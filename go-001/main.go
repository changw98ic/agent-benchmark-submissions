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
	Tasks      []TaskInput `json:"tasks"`
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
	ID         string
	Duration   int
	Failures   int
	Attempts   int
	Status     int // 0=not started, 2=success, 3=failed
	FinishedAt int
}

type Running struct {
	taskIdx  int
	finishAt int
}

type Waiting struct {
	taskIdx  int
	readyAt  int
}

func process(req Request) interface{} {
	// Validate config
	if req.Concurrency < 1 || req.Concurrency > 20 {
		return ErrorResponse{Error: "CONFIG"}
	}
	if req.MaxRetries < 0 || req.MaxRetries > 5 {
		return ErrorResponse{Error: "CONFIG"}
	}
	if req.RetryDelay < 0 {
		return ErrorResponse{Error: "CONFIG"}
	}

	// Check duplicate IDs
	seen := make(map[string]bool)
	for _, t := range req.Tasks {
		if seen[t.ID] {
			return ErrorResponse{Error: "DUPLICATE_ID"}
		}
		seen[t.ID] = true
	}

	// Check task validity
	for _, t := range req.Tasks {
		if t.Duration <= 0 || t.Failures < 0 {
			return ErrorResponse{Error: "TASK"}
		}
	}

	// Initialize tasks
	tasks := make([]Task, len(req.Tasks))
	for i, t := range req.Tasks {
		tasks[i] = Task{
			ID:       t.ID,
			Duration: t.Duration,
			Failures: t.Failures,
		}
	}

	// Scheduler state
	var trace []Event
	var running []Running
	var waiting []Waiting
	ready := make([]int, len(tasks))
	for i := range tasks {
		ready[i] = i
	}

	t := 0
	for {
		// Process completions at time t
		var completed []int
		var remaining []Running
		for _, r := range running {
			if r.finishAt <= t {
				completed = append(completed, r.taskIdx)
			} else {
				remaining = append(remaining, r)
			}
		}
		running = remaining
		sort.Ints(completed)
		for _, idx := range completed {
			task := &tasks[idx]
			attempt := task.Attempts
			if attempt <= task.Failures {
				// failure
				trace = append(trace, Event{ID: task.ID, Event: "failure", At: t, Attempt: attempt})
				if attempt < req.MaxRetries+1 {
					waiting = append(waiting, Waiting{taskIdx: idx, readyAt: t + req.RetryDelay})
				} else {
					task.Status = 3
					task.FinishedAt = t
				}
			} else {
				// success
				trace = append(trace, Event{ID: task.ID, Event: "success", At: t, Attempt: attempt})
				task.Status = 2
				task.FinishedAt = t
			}
		}

		// Process due retries
		var due []Waiting
		var stillWaiting []Waiting
		for _, w := range waiting {
			if w.readyAt <= t {
				due = append(due, w)
			} else {
				stillWaiting = append(stillWaiting, w)
			}
		}
		waiting = stillWaiting
		sort.Slice(due, func(i, j int) bool {
			if due[i].readyAt != due[j].readyAt {
				return due[i].readyAt < due[j].readyAt
			}
			return due[i].taskIdx < due[j].taskIdx
		})
		for _, w := range due {
			ready = append(ready, w.taskIdx)
		}

		// Fill slots
		for len(running) < req.Concurrency && len(ready) > 0 {
			idx := ready[0]
			ready = ready[1:]
			task := &tasks[idx]
			task.Attempts++
			trace = append(trace, Event{ID: task.ID, Event: "start", At: t, Attempt: task.Attempts})
			running = append(running, Running{taskIdx: idx, finishAt: t + task.Duration})
		}

		// Check if done
		if len(running) == 0 && len(waiting) == 0 && len(ready) == 0 {
			break
		}

		// Compute next time
		nextT := -1
		for _, r := range running {
			if nextT == -1 || r.finishAt < nextT {
				nextT = r.finishAt
			}
		}
		for _, w := range waiting {
			if nextT == -1 || w.readyAt < nextT {
				nextT = w.readyAt
			}
		}
		if nextT == -1 {
			break
		}
		t = nextT
	}

	// Build results
	results := make([]Result, len(tasks))
	for i, task := range tasks {
		status := "failed"
		if task.Status == 2 {
			status = "success"
		}
		results[i] = Result{
			ID:         task.ID,
			Status:     status,
			Attempts:   task.Attempts,
			FinishedAt: task.FinishedAt,
		}
	}

	return Response{Trace: trace, Results: results}
}

func main() {
	scanner := bufio.NewScanner(os.Stdin)
	encoder := json.NewEncoder(os.Stdout)
	for scanner.Scan() {
		line := scanner.Bytes()
		if len(line) == 0 {
			continue
		}
		var req Request
		if err := json.Unmarshal(line, &req); err != nil {
			encoder.Encode(ErrorResponse{Error: "CONFIG"})
			continue
		}
		resp := process(req)
		encoder.Encode(resp)
	}
}
