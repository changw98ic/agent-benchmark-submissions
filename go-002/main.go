package main

import (
    "bufio"
    "encoding/json"
    "fmt"
    "io"
    "os"
    "sort"
    "strings"
)

type operation struct {
    Op        string   `json:"op"`
    ID        string   `json:"id"`
    Start     int64    `json:"start"`
    End       int64    `json:"end"`
    Resources []string `json:"resources"`
}

type request struct {
    Ops []operation `json:"ops"`
}

type booking struct {
    ID        string   `json:"id"`
    Start     int64    `json:"start"`
    End       int64    `json:"end"`
    Resources []string `json:"resources"`
}

type response struct {
    Results  []string  `json:"results"`
    Bookings []booking `json:"bookings"`
}

func main() {
    reader := bufio.NewReader(os.Stdin)
    for {
        line, err := reader.ReadString('\n')
        if len(line) > 0 {
            line = strings.TrimRight(line, "\r\n")
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

func handleLine(line string) {
    var req request
    if err := json.Unmarshal([]byte(line), &req); err != nil {
        fmt.Println("INVALID_JSON")
        return
    }
    resp := process(req)
    out, err := json.Marshal(resp)
    if err != nil {
        fmt.Println("INVALID_JSON")
        return
    }
    fmt.Println(string(out))
}

func process(req request) response {
    active := make(map[string]booking)
    results := make([]string, 0, len(req.Ops))
    for _, op := range req.Ops {
        switch op.Op {
        case "reserve":
            results = append(results, reserve(active, op))
        case "cancel":
            if _, exists := active[op.ID]; exists {
                delete(active, op.ID)
                results = append(results, "OK")
            } else {
                results = append(results, "NOT_FOUND")
            }
        default:
            results = append(results, "INVALID")
        }
    }
    bookings := make([]booking, 0, len(active))
    for _, b := range active {
        bookings = append(bookings, b)
    }
    sort.Slice(bookings, func(i, j int) bool {
        return bookings[i].ID < bookings[j].ID
    })
    for i := range bookings {
        sort.Strings(bookings[i].Resources)
    }
    return response{Results: results, Bookings: bookings}
}

func reserve(active map[string]booking, op operation) string {
    if _, exists := active[op.ID]; exists {
        return "DUPLICATE_ID"
    }
    if op.Start >= op.End || len(op.Resources) == 0 {
        return "INVALID"
    }
    seen := make(map[string]bool, len(op.Resources))
    for _, r := range op.Resources {
        if seen[r] {
            return "INVALID"
        }
        seen[r] = true
    }
    for _, b := range active {
        if op.Start < b.End && op.End > b.Start && sharesResource(seen, b.Resources) {
            return "CONFLICT"
        }
    }
    resources := append([]string(nil), op.Resources...)
    sort.Strings(resources)
    active[op.ID] = booking{
        ID:        op.ID,
        Start:     op.Start,
        End:       op.End,
        Resources: resources,
    }
    return "OK"
}

func sharesResource(seen map[string]bool, resources []string) bool {
    for _, r := range resources {
        if seen[r] {
            return true
        }
    }
    return false
}
