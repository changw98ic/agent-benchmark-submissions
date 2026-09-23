package main

import (
    "bufio"
    "bytes"
    "encoding/json"
    "os"
    "sort"
)

type Op struct {
    Op        string
    ID        string
    Start     int64
    End       int64
    Resources []string
}

type Request struct {
    Ops []Op
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

func sortedCopy(in []string) []string {
    out := append([]string(nil), in...)
    sort.Strings(out)
    return out
}

func hasDuplicates(in []string) bool {
    seen := make(map[string]struct{}, len(in))
    for _, s := range in {
        if _, ok := seen[s]; ok {
            return true
        }
        seen[s] = struct{}{}
    }
    return false
}

func intervalsOverlap(aStart, aEnd, bStart, bEnd int64) bool {
    return aStart < bEnd && bStart < aEnd
}

func shareResource(a, b []string) bool {
    i, j := 0, 0
    for i < len(a) && j < len(b) {
        if a[i] == b[j] {
            return true
        }
        if a[i] < b[j] {
            i++
        } else {
            j++
        }
    }
    return false
}

func process(req Request) Response {
    results := make([]string, 0, len(req.Ops))
    active := make(map[string]Booking)

    for _, op := range req.Ops {
        switch op.Op {
        case "reserve":
            if _, exists := active[op.ID]; exists {
                results = append(results, "DUPLICATE_ID")
                continue
            }
            if op.Start >= op.End || len(op.Resources) == 0 || hasDuplicates(op.Resources) {
                results = append(results, "INVALID")
                continue
            }
            res := sortedCopy(op.Resources)
            conflict := false
            for _, b := range active {
                if intervalsOverlap(op.Start, op.End, b.Start, b.End) && shareResource(res, b.Resources) {
                    conflict = true
                    break
                }
            }
            if conflict {
                results = append(results, "CONFLICT")
                continue
            }
            active[op.ID] = Booking{ID: op.ID, Start: op.Start, End: op.End, Resources: res}
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
        b.Resources = sortedCopy(b.Resources)
        bookings = append(bookings, b)
    }
    sort.Slice(bookings, func(i, j int) bool {
        return bookings[i].ID < bookings[j].ID
    })

    return Response{Results: results, Bookings: bookings}
}

func main() {
    reader := bufio.NewReader(os.Stdin)
    writer := bufio.NewWriter(os.Stdout)
    defer writer.Flush()

    enc := json.NewEncoder(writer)

    for {
        line, err := reader.ReadBytes(10)
        if len(line) > 0 {
            line = bytes.TrimSpace(line)
            if len(line) > 0 {
                var req Request
                if json.Unmarshal(line, &req) == nil {
                    _ = enc.Encode(process(req))
                } else {
                    _ = enc.Encode(Response{Results: []string{}, Bookings: []Booking{}})
                }
            }
        }
        if err != nil {
            break
        }
    }
}
