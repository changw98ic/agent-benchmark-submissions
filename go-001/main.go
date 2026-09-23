package main

import (
	"bufio"
	"bytes"
	"encoding/json"
	"os"
	"sort"
)

type Task struct {
	ID       string `json:"id"`
	Duration int    `json:"duration"`
	Failures int    `json:"failures"`
}

type Request struct {
	Tasks      []Task `json:"tasks"`
	Concurrency int    `json:"concurrency"`
	MaxRetries  int    `json:"max_retries"`
	RetryDelay  int    `json:"retry_delay"`
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

type ReadyItem struct {
	idx     int
	attempt int
}

type Running struct {
	idx     int
	attempt int
	end     int
}

type Waiting struct {
	idx     int
	attempt int
	due     int
}

func simulate(req Request) (Response, string) {
	if req.Concurrency < 1 || req.Concurrency > 20 {
		return Response{}, "CONFIG"
	}
	if req.MaxRetries < 0 || req.MaxRetries > 5 {
		return Response{}, "CONFIG"
	}
	if req.RetryDelay < 0 {
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
	results := make([]Result, n)
	for i, t := range req.Tasks {
		results[i].ID = t.ID
	}
	trace := make([]Event, 0)

	ready := make([]ReadyItem, 0, n)
	for i := 0; i < n; i++ {
		ready = append(ready, ReadyItem{idx: i, attempt: 1})
	}
	running := make([]Running, 0)
	waiting := make([]Waiting, 0)
	current := 0
	finished := 0

	for finished < n {
		// Process endings at current time
		ending := make([]Running, 0)
		remaining := make([]Running, 0)
		for _, r := range running {
			if r.end == current {
				ending = append(ending, r)
			} else {
				remaining = append(remaining, r)
			}
		}
		sort.Slice(ending, func(i, j int) bool {
			return ending[i].idx < ending[j].idx
		})
		running = remaining

		for _, r := range ending {
			task := req.Tasks[r.idx]
			if r.attempt <= task.Failures {
				trace = append(trace, Event{ID: task.ID, Event: "failure", At: current, Attempt: r.attempt})
				if r.attempt <= req.MaxRetries {
					waiting = append(waiting, Waiting{
						idx:     r.idx,
						attempt: r.attempt + 1,
						due:     current + req.RetryDelay,
					})
				} else {
					results[r.idx].Status = "failed"
					results[r.idx].Attempts = r.attempt
					results[r.idx].FinishedAt = current
					finished++
				}
			} else {
				trace = append(trace, Event{ID: task.ID, Event: "success", At: current, Attempt: r.attempt})
				results[r.idx].Status = "success"
				results[r.idx].Attempts = r.attempt
				results[r.idx].FinishedAt = current
				finished++
			}
		}

		// Add due retries
		dueList := make([]Waiting, 0)
		stillWaiting := make([]Waiting, 0)
		for _, w := range waiting {
			if w.due <= current {
				dueList = append(dueList, w)
			} else {
				stillWaiting = append(stillWaiting, w)
			}
		}
		sort.Slice(dueList, func(i, j int) bool {
			if dueList[i].due != dueList[j].due {
				return dueList[i].due < dueList[j].due
			}
			return dueList[i].idx < dueList[j].idx
		})
		waiting = stillWaiting
		for _, w := range dueList {
			ready = append(ready, ReadyItem{idx: w.idx, attempt: w.attempt})
		}

		// Fill slots
		for len(running) < req.Concurrency && len(ready) > 0 {
			item := ready[0]
			ready = ready[1:]
			task := req.Tasks[item.idx]
			trace = append(trace, Event{ID: task.ID, Event: "start", At: current, Attempt: item.attempt})
			running = append(running, Running{
				idx:     item.idx,
				attempt: item.attempt,
				end:     current + task.Duration,
			})
		}

		if finished == n {
			break
		}

		nextTime := -1
		for _, r := range running {
			if nextTime == -1 || r.end < nextTime {
				nextTime = r.end
			}
		}
		for _, w := range waiting {
			if nextTime == -1 || w.due < nextTime {
				nextTime = w.due
			}
		}
		if nextTime == -1 {
			// Should not happen with valid config, but avoid infinite loop.
			break
		}
		current = nextTime
	}

	return Response{Trace: trace, Results: results}, ""
}

func main() {
	scanner := bufio.NewScanner(os.Stdin)
	scanner.Buffer(make([]byte, 1024), 1024*1024*10)
	encoder := json.NewEncoder(os.Stdout)

	for scanner.Scan() {
		line := scanner.Bytes()
		if len(bytes.TrimSpace(line)) == 0 {
			continue
		}
		var req Request
		if err := json.Unmarshal(line, &req); err != nil {
			encoder.Encode(ErrorResponse{Error: "CONFIG"})
			continue
		}
		resp, errCode := simulate(req)
		if errCode != "" {
			encoder.Encode(ErrorResponse{Error: errCode})
		} else {
			encoder.Encode(resp)
		}
	}
}
