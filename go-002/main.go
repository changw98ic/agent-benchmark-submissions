package main

import (
    "bufio"
    "encoding/json"
    "os"
    "sort"
    "strings"
)

type Op struct {
    Op string `json:"op"`
    ID string `json:"id"`
    Start int `json:"start"`
    End int `json:"end"`
    Resources []string `json:"resources"`
}

type Request struct {
    Ops []Op `json:"ops"`
}

type Booking struct {
    ID string `json:"id"`
    Start int `json:"start"`
    End int `json:"end"`
    Resources []string `json:"resources"`
}

type Response struct {
    Results []string `json:"results"`
    Bookings []Booking `json:"bookings"`
}

type activeBooking struct {
    ID string
    Start int
    End int
    Resources []string
}

func process(req Request) Response {
    results := make([]string, 0, len(req.Ops))
    active := make(map[string]*activeBooking)
    for _, op := range req.Ops {
        switch op.Op {
        case "reserve":
            if _, exists := active[op.ID]; exists {
                results = append(results, "DUPLICATE_ID")
                continue
            }
            if op.Start >= op.End || len(op.Resources) == 0 {
                results = append(results, "INVALID")
                continue
            }
            seen := make(map[string]bool, len(op.Resources))
            invalid := false
            for _, r := range op.Resources {
                if seen[r] {
                    invalid = true
                    break
                }
                seen[r] = true
            }
            if invalid {
                results = append(results, "INVALID")
                continue
            }
            conflict := false
            for _, b := range active {
                if op.Start < b.End && b.Start < op.End {
                    for _, r := range b.Resources {
                        if seen[r] {
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
                results = append(results, "CONFLICT")
                continue
            }
            res := append([]string(nil), op.Resources...)
            sort.Strings(res)
            active[op.ID] = &activeBooking{ID: op.ID, Start: op.Start, End: op.End, Resources: res}
            results = append(results, "OK")
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
    bookings := make([]Booking, 0, len(active))
    for _, b := range active {
        bookings = append(bookings, Booking{ID: b.ID, Start: b.Start, End: b.End, Resources: append([]string(nil), b.Resources...)})
    }
    sort.Slice(bookings, func(i, j int) bool { return bookings[i].ID < bookings[j].ID })
    for i := range bookings {
        sort.Strings(bookings[i].Resources)
    }
    return Response{Results: results, Bookings: bookings}
}

func main() {
    reader := bufio.NewReader(os.Stdin)
    writer := bufio.NewWriter(os.Stdout)
    defer writer.Flush()
    for {
        line, err := reader.ReadString('\n')
        if len(line) > 0 {
            line = strings.TrimRight(line, "\r\n")
            var req Request
            if e := json.Unmarshal([]byte(line), &req); e != nil {
                writer.WriteString("INVALID_JSON\n")
            } else {
                resp := process(req)
                bytes, e := json.Marshal(resp)
                if e != nil {
                    writer.WriteString("INVALID_JSON\n")
                } else {
                    writer.Write(bytes)
                    writer.WriteByte('\n')
                }
            }
        }
        if err != nil {
            break
        }
    }
}
