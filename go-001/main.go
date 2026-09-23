package main

import (
    "bufio"
    "encoding/json"
    "fmt"
    "os"
    "sort"
)

type TaskInput struct {
    ID       string `json:"id"`
    Duration int    `json:"duration"`
    Failures int    `json:"failures"`
}

type Request struct {
    Tasks       []TaskInput `json:"tasks"`
    Concurrency int         `json:"concurrency"`
    MaxRetries  int         `json:"max_retries"`
    RetryDelay  int         `json:"retry_delay"`
}

type ErrorResponse struct {
    Error string `json:"error"`
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

type Task struct {
    ID         string
    Duration   int
    Failures   int
    Attempts   int
    Status     string
    FinishedAt int
}

type Running struct {
    TaskIndex  int
    Attempt    int
    FinishTime int
}

type Waiting struct {
    TaskIndex int
    Due       int
}

func simulate(req Request) (Response, string) {
    if req.Concurrency < 1 || req.Concurrency > 20 ||
        req.MaxRetries < 0 || req.MaxRetries > 5 ||
        req.RetryDelay < 0 {
        return Response{}, "CONFIG"
    }

    seen := make(map[string]bool)
    for _, t := range req.Tasks {
        if seen[t.ID] {
            return Response{}, "DUPLICATE_ID"
        }
        seen[t.ID] = true
    }

    for _, t := range req.Tasks {
        if t.Duration <= 0 || t.Failures < 0 {
            return Response{}, "TASK"
        }
    }

    n := len(req.Tasks)
    tasks := make([]Task, n)
    for i, t := range req.Tasks {
        tasks[i] = Task{
            ID:       t.ID,
            Duration: t.Duration,
            Failures: t.Failures,
            Status:   "pending",
        }
    }

    ready := make([]int, 0, n)
    for i := 0; i < n; i++ {
        ready = append(ready, i)
    }
    head := 0
    running := make([]Running, 0)
    waiting := make([]Waiting, 0)
    trace := make([]Event, 0)
    currentTime := 0
    doneCount := 0

    for {
        // Process finishes at currentTime
        if len(running) > 0 {
            finished := make([]Running, 0)
            remaining := make([]Running, 0, len(running))
            for _, r := range running {
                if r.FinishTime == currentTime {
                    finished = append(finished, r)
                } else {
                    remaining = append(remaining, r)
                }
            }
            if len(finished) > 0 {
                sort.Slice(finished, func(i, j int) bool {
                    return finished[i].TaskIndex < finished[j].TaskIndex
                })
                running = remaining
                for _, r := range finished {
                    t := &tasks[r.TaskIndex]
                    if r.Attempt <= t.Failures {
                        trace = append(trace, Event{
                            ID:      t.ID,
                            Event:   "failure",
                            At:      currentTime,
                            Attempt: r.Attempt,
                        })
                        if r.Attempt < req.MaxRetries+1 {
                            waiting = append(waiting, Waiting{
                                TaskIndex: r.TaskIndex,
                                Due:       currentTime + req.RetryDelay,
                            })
                        } else {
                            t.Status = "failed"
                            t.FinishedAt = currentTime
                            doneCount++
                        }
                    } else {
                        trace = append(trace, Event{
                            ID:      t.ID,
                            Event:   "success",
                            At:      currentTime,
                            Attempt: r.Attempt,
                        })
                        t.Status = "success"
                        t.FinishedAt = currentTime
                        doneCount++
                    }
                }
            }
        }

        // Process due retries
        if len(waiting) > 0 {
            due := make([]Waiting, 0)
            remaining := make([]Waiting, 0, len(waiting))
            for _, w := range waiting {
                if w.Due <= currentTime {
                    due = append(due, w)
                } else {
                    remaining = append(remaining, w)
                }
            }
            if len(due) > 0 {
                sort.Slice(due, func(i, j int) bool {
                    if due[i].Due != due[j].Due {
                        return due[i].Due < due[j].Due
                    }
                    return due[i].TaskIndex < due[j].TaskIndex
                })
                waiting = remaining
                for _, w := range due {
                    ready = append(ready, w.TaskIndex)
                }
            }
        }

        // Fill running slots
        for len(running) < req.Concurrency && head < len(ready) {
            idx := ready[head]
            head++
            t := &tasks[idx]
            t.Attempts++
            attempt := t.Attempts
            trace = append(trace, Event{
                ID:      t.ID,
                Event:   "start",
                At:      currentTime,
                Attempt: attempt,
            })
            running = append(running, Running{
                TaskIndex:  idx,
                Attempt:    attempt,
                FinishTime: currentTime + t.Duration,
            })
        }

        // Check if all done
        if doneCount == n && len(running) == 0 && head >= len(ready) && len(waiting) == 0 {
            break
        }

        // Determine next time
        nextTime := -1
        if len(running) > 0 {
            minFinish := running[0].FinishTime
            for _, r := range running[1:] {
                if r.FinishTime < minFinish {
                    minFinish = r.FinishTime
                }
            }
            nextTime = minFinish
        }
        if len(waiting) > 0 {
            minDue := waiting[0].Due
            for _, w := range waiting[1:] {
                if w.Due < minDue {
                    minDue = w.Due
                }
            }
            if nextTime == -1 || minDue < nextTime {
                nextTime = minDue
            }
        }
        if nextTime == -1 {
            break
        }
        currentTime = nextTime
    }

    results := make([]Result, n)
    for i, t := range tasks {
        results[i] = Result{
            ID:         t.ID,
            Status:     t.Status,
            Attempts:   t.Attempts,
            FinishedAt: t.FinishedAt,
        }
    }

    return Response{Trace: trace, Results: results}, ""
}

func main() {
    scanner := bufio.NewScanner(os.Stdin)
    buf := make([]byte, 1024*1024)
    scanner.Buffer(buf, 1024*1024*100)
    for scanner.Scan() {
        line := scanner.Text()
        var req Request
        if err := json.Unmarshal([]byte(line), &req); err != nil {
            fmt.Println("INVALID_JSON")
            continue
        }
        resp, errCode := simulate(req)
        if errCode != "" {
            out, _ := json.Marshal(ErrorResponse{Error: errCode})
            fmt.Println(string(out))
        } else {
            out, _ := json.Marshal(resp)
            fmt.Println(string(out))
        }
    }
}
