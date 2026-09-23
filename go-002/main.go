package main

import (
    "bufio"
    "encoding/json"
    "os"
    "sort"
    "strings"
)

type Op struct {
    Op        string   `json:"op"`
    ID        string   `json:"id"`
    Start     int64    `json:"start"`
    End       int64    `json:"end"`
    Resources []string `json:"resources"`
}

type Request struct {
    Ops []Op `json:"ops"`
}

type Response struct {
    Results  []string     `json:"results"`
    Bookings []BookingOut `json:"bookings"`
}

type BookingOut struct {
    ID        string   `json:"id"`
    Start     int64    `json:"start"`
    End       int64    `json:"end"`
    Resources []string `json:"resources"`
}

type Booking struct {
    ID        string
    Start     int64
    End       int64
    Resources []string
    resSet    map[string]bool
}

type Engine struct {
    active map[string]*Booking
}

func NewEngine() *Engine {
    return &Engine{active: make(map[string]*Booking)}
}

func (e *Engine) Process(ops []Op) Response {
    results := make([]string, 0, len(ops))
    for _, op := range ops {
        switch op.Op {
        case "reserve":
            if _, exists := e.active[op.ID]; exists {
                results = append(results, "DUPLICATE_ID")
                continue
            }
            if op.Start >= op.End || len(op.Resources) == 0 {
                results = append(results, "INVALID")
                continue
            }
            resSet := make(map[string]bool, len(op.Resources))
            dup := false
            for _, r := range op.Resources {
                if resSet[r] {
                    dup = true
                    break
                }
                resSet[r] = true
            }
            if dup {
                results = append(results, "INVALID")
                continue
            }
            conflict := false
            for _, b := range e.active {
                if op.Start < b.End && b.Start < op.End {
                    for _, r := range op.Resources {
                        if b.resSet[r] {
                            conflict = true
                            break
                        }
                    }
                }
                if conflict {
                    break
                }
            }
            if conflict {
                results = append(results, "CONFLICT")
                continue
            }
            resCopy := append([]string(nil), op.Resources...)
            e.active[op.ID] = &Booking{
                ID:        op.ID,
                Start:     op.Start,
                End:       op.End,
                Resources: resCopy,
                resSet:    resSet,
            }
            results = append(results, "OK")
        case "cancel":
            if _, exists := e.active[op.ID]; !exists {
                results = append(results, "NOT_FOUND")
            } else {
                delete(e.active, op.ID)
                results = append(results, "OK")
            }
        default:
            results = append(results, "INVALID")
        }
    }

    bookings := make([]BookingOut, 0, len(e.active))
    for _, b := range e.active {
        res := append([]string(nil), b.Resources...)
        sort.Strings(res)
        bookings = append(bookings, BookingOut{
            ID:        b.ID,
            Start:     b.Start,
            End:       b.End,
            Resources: res,
        })
    }
    sort.Slice(bookings, func(i, j int) bool {
        return bookings[i].ID < bookings[j].ID
    })

    return Response{Results: results, Bookings: bookings}
}

func main() {
    scanner := bufio.NewScanner(os.Stdin)
    scanner.Buffer(make([]byte, 0, 64*1024), 1024*1024*10)
    writer := bufio.NewWriter(os.Stdout)
    defer writer.Flush()

    enc := json.NewEncoder(writer)
    enc.SetEscapeHTML(false)

    for scanner.Scan() {
        line := scanner.Bytes()
        if len(strings.TrimSpace(string(line))) == 0 {
            continue
        }
        var req Request
        if err := json.Unmarshal(line, &req); err != nil {
            enc.Encode(Response{Results: []string{"INVALID"}, Bookings: []BookingOut{}})
            continue
        }
        engine := NewEngine()
        resp := engine.Process(req.Ops)
        enc.Encode(resp)
    }
}
