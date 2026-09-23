package main

import (
    "bufio"
    "encoding/json"
    "fmt"
    "io"
    "os"
)

type event struct {
    EventID string          `json:"event_id"`
    Entity  string          `json:"entity"`
    Version int64           `json:"version"`
    Value   json.RawMessage `json:"value"`
}

type request struct {
    Events []event `json:"events"`
}

type entityState struct {
    Version int64           `json:"version"`
    Value   json.RawMessage `json:"value"`
}

type response struct {
    Results  []string               `json:"results"`
    Entities map[string]entityState `json:"entities"`
}

func main() {
    reader := bufio.NewReader(os.Stdin)
    for {
        line, err := reader.ReadBytes('\n')
        if len(line) > 0 {
            if line[len(line)-1] == '\n' {
                line = line[:len(line)-1]
            }
            if len(line) > 0 && line[len(line)-1] == '\r' {
                line = line[:len(line)-1]
            }
            handleLine(line)
        }
        if err != nil {
            if err == io.EOF {
                break
            }
            break
        }
    }
}

func handleLine(line []byte) {
    var req request
    if err := json.Unmarshal(line, &req); err != nil {
        fmt.Println("INVALID_JSON")
        return
    }

    results := make([]string, 0, len(req.Events))
    entities := make(map[string]entityState)
    seen := make(map[string]struct{})

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

        if cur, ok := entities[ev.Entity]; ok && ev.Version <= cur.Version {
            results = append(results, "STALE")
            continue
        }

        entities[ev.Entity] = entityState{Version: ev.Version, Value: ev.Value}
        results = append(results, "APPLIED")
    }

    resp := response{Results: results, Entities: entities}
    out, err := json.Marshal(resp)
    if err != nil {
        fmt.Println("INVALID_JSON")
        return
    }
    fmt.Println(string(out))
}
