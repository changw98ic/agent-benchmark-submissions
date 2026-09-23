package main

import (
    "bufio"
    "encoding/json"
    "fmt"
    "os"
)

type Event struct {
    EventID string          `json:"event_id"`
    Entity  string          `json:"entity"`
    Version int             `json:"version"`
    Value   json.RawMessage `json:"value"`
}

type Request struct {
    Events []Event `json:"events"`
}

type EntityState struct {
    Version int             `json:"version"`
    Value   json.RawMessage `json:"value"`
}

type Response struct {
    Results  []string               `json:"results"`
    Entities map[string]EntityState `json:"entities"`
}

func main() {
    reader := bufio.NewReader(os.Stdin)
    enc := json.NewEncoder(os.Stdout)
    enc.SetEscapeHTML(false)

    for {
        line, err := reader.ReadString('\n')
        if len(line) > 0 {
            var req Request
            if uerr := json.Unmarshal([]byte(line), &req); uerr != nil {
                fmt.Fprintln(os.Stdout, "INVALID_JSON")
            } else {
                process(&req, enc)
            }
        }
        if err != nil {
            break
        }
    }
}

func process(req *Request, enc *json.Encoder) {
    seen := make(map[string]bool)
    entities := make(map[string]EntityState)
    results := make([]string, 0, len(req.Events))

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

        entities[ev.Entity] = EntityState{
            Version: ev.Version,
            Value:   ev.Value,
        }
        results = append(results, "APPLIED")
    }

    resp := Response{
        Results:  results,
        Entities: entities,
    }
    _ = enc.Encode(resp)
}
