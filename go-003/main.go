package main

import (
	"bufio"
	"bytes"
	"encoding/json"
	"os"
	"strings"
)

type event struct {
	EventID string          `json:"event_id"`
	Entity  string          `json:"entity"`
	Version int             `json:"version"`
	Value   json.RawMessage `json:"value"`
}

type request struct {
	Events []event `json:"events"`
}

type entitySnapshot struct {
	Version int             `json:"version"`
	Value   json.RawMessage `json:"value"`
}

type response struct {
	Results  []string                  `json:"results"`
	Entities map[string]entitySnapshot `json:"entities"`
}

func compact(raw json.RawMessage) json.RawMessage {
	if len(raw) == 0 {
		return json.RawMessage("null")
	}
	var buf bytes.Buffer
	if err := json.Compact(&buf, raw); err != nil {
		return json.RawMessage("null")
	}
	return json.RawMessage(buf.Bytes())
}

func process(line []byte) ([]byte, bool) {
	var req request
	if err := json.Unmarshal(line, &req); err != nil {
		return nil, false
	}
	seen := make(map[string]bool)
	snapshots := make(map[string]*entitySnapshot)
	resp := response{Results: make([]string, 0, len(req.Events)), Entities: make(map[string]entitySnapshot)}
	for _, ev := range req.Events {
		if seen[ev.EventID] {
			resp.Results = append(resp.Results, "DUPLICATE")
			continue
		}
		if ev.Version < 1 {
			resp.Results = append(resp.Results, "INVALID")
			continue
		}
		seen[ev.EventID] = true
		cur, ok := snapshots[ev.Entity]
		if ok && ev.Version <= cur.Version {
			resp.Results = append(resp.Results, "STALE")
			continue
		}
		if !ok {
			cur = &entitySnapshot{}
			snapshots[ev.Entity] = cur
		}
		cur.Version = ev.Version
		cur.Value = compact(ev.Value)
		resp.Results = append(resp.Results, "APPLIED")
	}
	for id, snap := range snapshots {
		resp.Entities[id] = *snap
	}
	out, err := json.Marshal(resp)
	if err != nil {
		return nil, false
	}
	return out, true
}

func main() {
	in := bufio.NewReaderSize(os.Stdin, 1<<20)
	out := bufio.NewWriter(os.Stdout)
	defer out.Flush()
	for {
		line, err := in.ReadString('\n')
		trimmed := strings.TrimSpace(line)
		if trimmed == "" {
			if err != nil {
				break
			}
			continue
		}
		if !json.Valid([]byte(trimmed)) {
			out.WriteString("INVALID_JSON\n")
		} else if res, ok := process([]byte(trimmed)); ok {
			out.Write(res)
			out.WriteByte('\n')
		} else {
			out.WriteString("INVALID_JSON\n")
		}
		if err != nil {
			break
		}
	}
}
