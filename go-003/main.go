package main

import (
    "bufio"
    "encoding/json"
    "io"
    "os"
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

type entityState struct {
    Version int             `json:"version"`
    Value   json.RawMessage `json:"value"`
}

type response struct {
    Results  []string               `json:"results"`
    Entities map[string]entityState `json:"entities"`
}

func main() {
    in := bufio.NewReader(os.Stdin)
    out := bufio.NewWriter(os.Stdout)
    defer out.Flush()

    enc := json.NewEncoder(out)
    for {
        line, err := in.ReadBytes('\n')
        if len(line) > 0 {
            for len(line) > 0 {
                c := line[len(line)-1]
                if c == '\n' || c == '\r' || c == ' ' || c == '\t' {
                    line = line[:len(line)-1]
                } else {
                    break
                }
            }
            if len(line) > 0 {
                var req request
                if json.Unmarshal(line, &req) == nil {
                    _ = enc.Encode(process(req))
                }
            }
        }
        if err != nil {
            if err == io.EOF {
                break
            }
            break
        }
    }
}

func process(req request) response {
    seen := make(map[string]struct{})
    entities := make(map[string]entityState)
    results := make([]string, 0, len(req.Events))
    for _, ev := range req.Events {
        if _, ok := seen[ev.EventID]; ok {
            results = append(results, "DUPLICATE")
            continue
        }
        if ev.Version < 1 {
            results = append(results, "INVALID")
            continue
        }
        seen[ev.EventID] = struct{}{}
        cur, ok := entities[ev.Entity]
        if ok && ev.Version <= cur.Version {
            results = append(results, "STALE")
            continue
        }
        entities[ev.Entity] = entityState{Version: ev.Version, Value: ev.Value}
        results = append(results, "APPLIED")
    }
    return response{Results: results, Entities: entities}
}
