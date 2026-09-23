package main

import (
    "bufio"
    "encoding/json"
    "fmt"
    "os"
    "sort"
)

type InputTask struct {
    ID       string `json:"id"`
    Duration int    `json:"duration"`
    Failures int    `json:"failures"`
}

type Request struct {
    Tasks      []InputTask `json:"tasks"`
    Concurrency int        `json:"concurrency"`
    MaxRetries  int        `json:"max_retries"`
    RetryDelay  int        `json:"retry_delay"`
}

type Event struct {
    ID      string `json:"id"`
    Event   string `json:"event"`
    At      int    `json:"at"`
    Attempt int    `json:"attempt"`
}

type Result struct {
    ID         string `json:"id"`
    Status     string `json:"status"`
    Attempts   int    `json:"attempts"`
    FinishedAt int    `json:"finished_at"`
}

type Response struct {
    Trace   []Event  `json:"trace"`
    Results []Result `json:"results"`
}

type ErrorResponse struct {
    Error string `json:"error"`
}

type Task struct {
    ID       string
    Duration int
    Failures int
    Index    int
    AttemptsStarted int
    Status   string // "ready", "running", "waiting", "success", "failed"
    Due      int
    End      int
    FinishedAt int
}

func main() {
    scanner := bufio.NewScanner(os.Stdin)
    // Increase buffer just in case
    scanner.Buffer(make([]byte, 1024), 1024*1024)
    for scanner.Scan() {
        line := scanner.Text()
        var req Request
        if err := json.Unmarshal([]byte(line), &req); err != nil {
            fmt.Println("INVALID_JSON")
            continue
        }
        // Validate config
        if req.Concurrency < 1 || req.Concurrency > 20 ||
            req.MaxRetries < 0 || req.MaxRetries > 5 ||
            req.RetryDelay < 0 {
            printError("CONFIG")
            continue
        }
        // Check duplicate IDs
        seen := make(map[string]bool)
        dup := false
        for _, t := range req.Tasks {
            if seen[t.ID] {
                dup = true
                break
            }
            seen[t.ID] = true
        }
        if dup {
            printError("DUPLICATE_ID")
            continue
        }
        // Validate tasks
        taskInvalid := false
        for _, t := range req.Tasks {
            if t.Duration <= 0 || t.Failures < 0 {
                taskInvalid = true
                break
            }
        }
        if taskInvalid {
            printError("TASK")
            continue
        }

        resp := simulate(req)
        b, _ := json.Marshal(resp)
        fmt.Println(string(b))
    }
}

func printError(code string) {
    b, _ := json.Marshal(ErrorResponse{Error: code})
    fmt.Println(string(b))
}

func simulate(req Request) Response {
    n := len(req.Tasks)
    tasks := make([]*Task, n)
    for i, it := range req.Tasks {
        tasks[i] = &Task{
            ID:       it.ID,
            Duration: it.Duration,
            Failures: it.Failures,
            Index:    i,
            Status:   "ready",
        }
    }
    var trace []Event
    ready := make([]int, n)
    for i := 0; i < n; i++ {
        ready[i] = i
    }
    var running []int
    var waiting []int

    currentTime := 0
    concurrency := req.Concurrency
    maxRetries := req.MaxRetries
    retryDelay := req.RetryDelay

    for {
        // Step 1: process completed attempts at currentTime
        // collect running tasks with End == currentTime
        var completed []int
        for _, idx := range running {
            if tasks[idx].End == currentTime {
                completed = append(completed, idx)
            }
        }
        if len(completed) > 0 {
            sort.Slice(completed, func(i, j int) bool {
                return tasks[completed[i]].Index < tasks[completed[j]].Index
            })
        }
        // Remove completed from running
        if len(completed) > 0 {
            newRunning := running[:0]
            completedSet := make(map[int]bool)
            for _, idx := range completed {
                completedSet[idx] = true
            }
            for _, idx := range running {
                if !completedSet[idx] {
                    newRunning = append(newRunning, idx)
                }
            }
            running = newRunning
        }
        // Process each completed
        for _, idx := range completed {
            t := tasks[idx]
            attemptNum := t.AttemptsStarted // this is the attempt that just finished
            var eventType string
            if attemptNum <= t.Failures {
                eventType = "failure"
            } else {
                eventType = "success"
            }
            trace = append(trace, Event{
                ID:      t.ID,
                Event:   eventType,
                At:      currentTime,
                Attempt: attemptNum,
            })
            if eventType == "success" {
                t.Status = "success"
                t.FinishedAt = currentTime
            } else {
                // failure
                if attemptNum > maxRetries {
                    // no more retries
                    t.Status = "failed"
                    t.FinishedAt = currentTime
                } else {
                    t.Status = "waiting"
                    t.Due = currentTime + retryDelay
                    waiting = append(waiting, idx)
                }
            }
        }

        // Step 2: process due retries
        if len(waiting) > 0 {
            var due []int
            for _, idx := range waiting {
                if tasks[idx].Due <= currentTime {
                    due = append(due, idx)
                }
            }
            if len(due) > 0 {
                sort.Slice(due, func(i, j int) bool {
                    ti, tj := tasks[due[i]], tasks[due[j]]
                    if ti.Due != tj.Due {
                        return ti.Due < tj.Due
                    }
                    return ti.Index < tj.Index
                })
                // Remove due from waiting
                dueSet := make(map[int]bool)
                for _, idx := range due {
                    dueSet[idx] = true
                }
                newWaiting := waiting[:0]
                for _, idx := range waiting {
                    if !dueSet[idx] {
                        newWaiting = append(newWaiting, idx)
                    }
                }
                waiting = newWaiting
                // Append to ready queue
                for _, idx := range due {
                    tasks[idx].Status = "ready"
                    ready = append(ready, idx)
                }
            }
        }

        // Step 3: fill running slots
        for len(running) < concurrency && len(ready) > 0 {
            idx := ready[0]
            ready = ready[1:]
            t := tasks[idx]
            t.AttemptsStarted++
            t.Status = "running"
            t.End = currentTime + t.Duration
            running = append(running, idx)
            trace = append(trace, Event{
                ID:      t.ID,
                Event:   "start",
                At:      currentTime,
                Attempt: t.AttemptsStarted,
            })
        }

        // Check if done
        done := true
        for _, t := range tasks {
            if t.Status != "success" && t.Status != "failed" {
                done = false
                break
            }
        }
        if done {
            break
        }

        // Advance time
        nextTime := -1
        for _, idx := range running {
            end := tasks[idx].End
            if nextTime == -1 || end < nextTime {
                nextTime = end
            }
        }
        for _, idx := range waiting {
            due := tasks[idx].Due
            if nextTime == -1 || due < nextTime {
                nextTime = due
            }
        }
        if nextTime == -1 {
            // No running, no waiting, but not done? Should not happen if ready not empty? But if ready not empty and no running, concurrency would allow fill. So impossible.
            break
        }
        currentTime = nextTime
    }

    // Build results
    results := make([]Result, n)
    for i, t := range tasks {
        results[i] = Result{
            ID:         t.ID,
            Status:     t.Status,
            Attempts:   t.AttemptsStarted,
            FinishedAt: t.FinishedAt,
        }
    }
    return Response{Trace: trace, Results: results}
}
