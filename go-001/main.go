package main

import (
    "bufio"
    "bytes"
    "container/heap"
    "encoding/json"
    "fmt"
    "os"
    "sort"
)

type Request struct {
    Tasks      []TaskInput `json:"tasks"`
    Concurrency int         `json:"concurrency"`
    MaxRetries  int         `json:"max_retries"`
    RetryDelay  int64       `json:"retry_delay"`
}

type TaskInput struct {
    ID       string `json:"id"`
    Duration int64  `json:"duration"`
    Failures int64  `json:"failures"`
}

type Task struct {
    ID         string
    Duration   int64
    Failures   int64
    Attempts   int
    Status     string
    FinishedAt int64
    Index      int
}

type Event struct {
    ID      string `json:"id"`
    Event   string `json:"event"`
    At      int64  `json:"at"`
    Attempt int    `json:"attempt"`
}

type Result struct {
    ID         string `json:"id"`
    Status     string `json:"status"`
    Attempts   int    `json:"attempts"`
    FinishedAt int64  `json:"finished_at"`
}

type Response struct {
    Trace   []Event  `json:"trace"`
    Results []Result `json:"results"`
}

type ErrorResponse struct {
    Error string `json:"error"`
}

type WaitingRetry struct {
    TaskIndex int
    Due       int64
}

type WaitingHeap []WaitingRetry

func (h WaitingHeap) Len() int { return len(h) }
func (h WaitingHeap) Less(i, j int) bool {
    if h[i].Due != h[j].Due {
        return h[i].Due < h[j].Due
    }
    return h[i].TaskIndex < h[j].TaskIndex
}
func (h WaitingHeap) Swap(i, j int) { h[i], h[j] = h[j], h[i] }
func (h *WaitingHeap) Push(x interface{}) { *h = append(*h, x.(WaitingRetry)) }
func (h *WaitingHeap) Pop() interface{} {
    old := *h
    n := len(old)
    x := old[n-1]
    *h = old[:n-1]
    return x
}

func simulate(req Request) (Response, error) {
    if req.Concurrency < 1 || req.Concurrency > 20 {
        return Response{}, fmt.Errorf("CONFIG")
    }
    if req.MaxRetries < 0 || req.MaxRetries > 5 {
        return Response{}, fmt.Errorf("CONFIG")
    }
    if req.RetryDelay < 0 {
        return Response{}, fmt.Errorf("CONFIG")
    }

    seen := make(map[string]bool)
    for _, t := range req.Tasks {
        if seen[t.ID] {
            return Response{}, fmt.Errorf("DUPLICATE_ID")
        }
        seen[t.ID] = true
    }

    for _, t := range req.Tasks {
        if t.Duration <= 0 || t.Failures < 0 {
            return Response{}, fmt.Errorf("TASK")
        }
    }

    n := len(req.Tasks)
    tasks := make([]Task, n)
    for i, t := range req.Tasks {
        tasks[i] = Task{
            ID:       t.ID,
            Duration: t.Duration,
            Failures: t.Failures,
            Index:    i,
            Status:   "pending",
        }
    }

    trace := []Event{}
    results := make([]Result, n)

    ready := make([]int, 0, n)
    for i := 0; i < n; i++ {
        ready = append(ready, i)
    }

    type RunningAttempt struct {
        TaskIndex int
        Attempt   int
        Finish    int64
    }
    running := []RunningAttempt{}

    waiting := &WaitingHeap{}
    heap.Init(waiting)

    t := int64(0)
    for {
        // Process endings at t
        var ending []RunningAttempt
        var remaining []RunningAttempt
        for _, r := range running {
            if r.Finish == t {
                ending = append(ending, r)
            } else {
                remaining = append(remaining, r)
            }
        }
        sort.Slice(ending, func(i, j int) bool {
            return tasks[ending[i].TaskIndex].Index < tasks[ending[j].TaskIndex].Index
        })
        running = remaining

        for _, r := range ending {
            task := &tasks[r.TaskIndex]
            if int64(r.Attempt) <= task.Failures {
                trace = append(trace, Event{
                    ID:      task.ID,
                    Event:   "failure",
                    At:      t,
                    Attempt: r.Attempt,
                })
                if r.Attempt <= req.MaxRetries {
                    due := t + req.RetryDelay
                    heap.Push(waiting, WaitingRetry{
                        TaskIndex: r.TaskIndex,
                        Due:       due,
                    })
                    task.Status = "waiting"
                } else {
                    task.Status = "failed"
                    task.FinishedAt = t
                }
            } else {
                trace = append(trace, Event{
                    ID:      task.ID,
                    Event:   "success",
                    At:      t,
                    Attempt: r.Attempt,
                })
                task.Status = "success"
                task.FinishedAt = t
            }
        }

        // Move due retries to ready queue
        for waiting.Len() > 0 && (*waiting)[0].Due <= t {
            w := heap.Pop(waiting).(WaitingRetry)
            ready = append(ready, w.TaskIndex)
        }

        // Fill running slots
        for len(running) < req.Concurrency && len(ready) > 0 {
            taskIdx := ready[0]
            ready = ready[1:]
            task := &tasks[taskIdx]
            task.Attempts++
            attempt := task.Attempts
            trace = append(trace, Event{
                ID:      task.ID,
                Event:   "start",
                At:      t,
                Attempt: attempt,
            })
            task.Status = "running"
            running = append(running, RunningAttempt{
                TaskIndex: taskIdx,
                Attempt:   attempt,
                Finish:    t + task.Duration,
            })
        }

        if len(running) == 0 && waiting.Len() == 0 && len(ready) == 0 {
            break
        }

        var nextT int64 = -1
        for _, r := range running {
            if nextT == -1 || r.Finish < nextT {
                nextT = r.Finish
            }
        }
        if waiting.Len() > 0 {
            if nextT == -1 || (*waiting)[0].Due < nextT {
                nextT = (*waiting)[0].Due
            }
        }
        if nextT == -1 {
            break
        }
        t = nextT
    }

    for i := 0; i < n; i++ {
        task := tasks[i]
        status := task.Status
        if status != "success" && status != "failed" {
            status = "failed"
        }
        results[i] = Result{
            ID:         task.ID,
            Status:     status,
            Attempts:   task.Attempts,
            FinishedAt: task.FinishedAt,
        }
    }

    return Response{Trace: trace, Results: results}, nil
}

func main() {
    scanner := bufio.NewScanner(os.Stdin)
    buf := make([]byte, 1024*1024)
    scanner.Buffer(buf, 10*1024*1024)
    encoder := json.NewEncoder(os.Stdout)

    for scanner.Scan() {
        line := scanner.Bytes()
        if len(bytes.TrimSpace(line)) == 0 {
            continue
        }
        var req Request
        if err := json.Unmarshal(line, &req); err != nil {
            encoder.Encode(ErrorResponse{Error: "INVALID_JSON"})
            continue
        }
        resp, err := simulate(req)
        if err != nil {
            encoder.Encode(ErrorResponse{Error: err.Error()})
        } else {
            encoder.Encode(resp)
        }
    }
    if err := scanner.Err(); err != nil {
        fmt.Fprintln(os.Stderr, "error reading input:", err)
    }
}
