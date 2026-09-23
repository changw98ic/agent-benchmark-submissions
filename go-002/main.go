package main

import (
    "bufio"
    "encoding/json"
    "fmt"
    "os"
    "sort"
)

type Operation struct {
    Op        string   `json:"op"`
    ID        string   `json:"id"`
    Start     int64    `json:"start"`
    End       int64    `json:"end"`
    Resources []string `json:"resources"`
}

type Request struct {
    Ops []Operation `json:"ops"`
}

type Booking struct {
    ID        string   `json:"id"`
    Start     int64    `json:"start"`
    End       int64    `json:"end"`
    Resources []string `json:"resources"`
}

type Response struct {
    Results  []string  `json:"results"`
    Bookings []Booking `json:"bookings"`
}

type State struct {
    bookings map[string]*Booking
}

func NewState() *State {
    return &State{bookings: make(map[string]*Booking)}
}

func (s *State) Reserve(op Operation) string {
    if _, exists := s.bookings[op.ID]; exists {
        return "DUPLICATE_ID"
    }
    if op.Start >= op.End {
        return "INVALID"
    }
    if len(op.Resources) == 0 {
        return "INVALID"
    }
    seen := make(map[string]bool, len(op.Resources))
    for _, r := range op.Resources {
        if seen[r] {
            return "INVALID"
        }
        seen[r] = true
    }
    for _, b := range s.bookings {
        if overlap(op.Start, op.End, b.Start, b.End) {
            for _, r := range op.Resources {
                if contains(b.Resources, r) {
                    return "CONFLICT"
                }
            }
        }
    }
    res := append([]string(nil), op.Resources...)
    sort.Strings(res)
    s.bookings[op.ID] = &Booking{
        ID:        op.ID,
        Start:     op.Start,
        End:       op.End,
        Resources: res,
    }
    return "OK"
}

func (s *State) Cancel(id string) string {
    if _, ok := s.bookings[id]; !ok {
        return "NOT_FOUND"
    }
    delete(s.bookings, id)
    return "OK"
}

func (s *State) Snapshot() []Booking {
    list := make([]Booking, 0, len(s.bookings))
    for _, b := range s.bookings {
        list = append(list, *b)
    }
    sort.Slice(list, func(i, j int) bool {
        return list[i].ID < list[j].ID
    })
    return list
}

func overlap(s1, e1, s2, e2 int64) bool {
    return s1 < e2 && s2 < e1
}

func contains(arr []string, target string) bool {
    for _, x := range arr {
        if x == target {
            return true
        }
    }
    return false
}

func process(req Request) Response {
    st := NewState()
    results := make([]string, 0, len(req.Ops))
    for _, op := range req.Ops {
        var status string
        switch op.Op {
        case "reserve":
            status = st.Reserve(op)
        case "cancel":
            status = st.Cancel(op.ID)
        default:
            status = "INVALID"
        }
        results = append(results, status)
    }
    return Response{
        Results:  results,
        Bookings: st.Snapshot(),
    }
}

func main() {
    scanner := bufio.NewScanner(os.Stdin)
    scanner.Buffer(make([]byte, 1024), 10*1024*1024)
    for scanner.Scan() {
        line := scanner.Bytes()
        var req Request
        if err := json.Unmarshal(line, &req); err != nil {
            fmt.Println("INVALID_JSON")
            continue
        }
        resp := process(req)
        out, err := json.Marshal(resp)
        if err != nil {
            fmt.Println("INVALID_JSON")
            continue
        }
        fmt.Println(string(out))
    }
}
