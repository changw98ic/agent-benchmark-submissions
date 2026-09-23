package main

import (
    "bufio"
    "bytes"
    "encoding/json"
    "io"
    "os"
)

type Event struct {
    EventID string          `json:"event_id"`
    Entity  string          `json:"entity"`
    Version int64           `json:"version"`
    Value   json.RawMessage `json:"value"`
}

type Request struct {
    Events []Event `json:"events"`
}

type Entity struct {
    Version int64           `json:"version"`
    Value   json.RawMessage `json:"value"`
}

type Response struct {
    Results  []string          `json:"results"`
    Entities map[string]Entity `json:"entities"`
}

func process(req Request) Response {
    results := make([]string, 0, len(req.Events))
    entities := make(map[string]Entity)
    seen := make(map[string]bool)
    for _, ev := range req.Events {
        if seen[ev.EventID] {
            results = append(results, "DUPLICATE")
            continue
        }
        if ev.Version < 1 {
            results = append(results, "INVALID")
            continue
        }
        seen[ev.EventID] = true
        cur, ok := entities[ev.Entity]
        if ok && ev.Version <= cur.Version {
            results = append(results, "STALE")
            continue
        }
        entities[ev.Entity] = Entity{Version: ev.Version, Value: ev.Value}
        results = append(results, "APPLIED")
    }
    return Response{Results: results, Entities: entities}
}

func main() {
    in := bufio.NewReader(os.Stdin)
    out := bufio.NewWriter(os.Stdout)
    defer out.Flush()

    for {
        line, err := in.ReadBytes(10)
        if len(line) > 0 {
            line = bytes.TrimSpace(line)
            if len(line) > 0 {
                var req Request
                if jerr := json.Unmarshal(line, &req); jerr == nil {
                    resp := process(req)
                    b, merr := json.Marshal(resp)
                    if merr == nil {
                        out.Write(b)
                        out.WriteByte(10)
                    }
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
