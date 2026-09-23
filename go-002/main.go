package main

import (
	"bufio"
	"bytes"
	"encoding/json"
	"os"
	"sort"
)

type Operation struct {
	Op        string   `json:"op"`
	ID        string   `json:"id"`
	Start     int      `json:"start"`
	End       int      `json:"end"`
	Resources []string `json:"resources"`
}

type Request struct {
	Ops []Operation `json:"ops"`
}

type Booking struct {
	ID        string   `json:"id"`
	Start     int      `json:"start"`
	End       int      `json:"end"`
	Resources []string `json:"resources"`
}

type Response struct {
	Results  []string  `json:"results"`
	Bookings []Booking `json:"bookings"`
}

func main() {
	scanner := bufio.NewScanner(os.Stdin)
	scanner.Buffer(make([]byte, 1024), 10*1024*1024)
	enc := json.NewEncoder(os.Stdout)
	for scanner.Scan() {
		line := bytes.TrimSpace(scanner.Bytes())
		if len(line) == 0 {
			continue
		}
		var req Request
		if err := json.Unmarshal(line, &req); err != nil {
			enc.Encode(Response{Results: []string{"INVALID"}, Bookings: []Booking{}})
			continue
		}
		enc.Encode(process(req))
	}
}

func process(req Request) Response {
	resp := Response{Results: make([]string, 0), Bookings: make([]Booking, 0)}
	active := make(map[string]Booking)
	for _, op := range req.Ops {
		switch op.Op {
		case "reserve":
			if _, ok := active[op.ID]; ok {
				resp.Results = append(resp.Results, "DUPLICATE_ID")
				continue
			}
			if op.Start >= op.End || len(op.Resources) == 0 {
				resp.Results = append(resp.Results, "INVALID")
				continue
			}
			seen := make(map[string]struct{}, len(op.Resources))
			invalid := false
			for _, r := range op.Resources {
				if _, ok := seen[r]; ok {
					invalid = true
					break
				}
				seen[r] = struct{}{}
			}
			if invalid {
				resp.Results = append(resp.Results, "INVALID")
				continue
			}
			conflict := false
			for _, b := range active {
				if op.Start < b.End && op.End > b.Start {
					for _, r := range op.Resources {
						for _, br := range b.Resources {
							if r == br {
								conflict = true
								break
							}
						}
						if conflict {
							break
						}
					}
				}
				if conflict {
					break
				}
			}
			if conflict {
				resp.Results = append(resp.Results, "CONFLICT")
				continue
			}
			resCopy := append([]string(nil), op.Resources...)
			active[op.ID] = Booking{ID: op.ID, Start: op.Start, End: op.End, Resources: resCopy}
			resp.Results = append(resp.Results, "OK")
		case "cancel":
			if _, ok := active[op.ID]; ok {
				delete(active, op.ID)
				resp.Results = append(resp.Results, "OK")
			} else {
				resp.Results = append(resp.Results, "NOT_FOUND")
			}
		default:
			resp.Results = append(resp.Results, "INVALID")
		}
	}
	for _, b := range active {
		sort.Strings(b.Resources)
		resp.Bookings = append(resp.Bookings, b)
	}
	sort.Slice(resp.Bookings, func(i, j int) bool {
		return resp.Bookings[i].ID < resp.Bookings[j].ID
	})
	return resp
}
