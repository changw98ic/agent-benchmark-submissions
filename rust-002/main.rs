use std::io::{self, Read, Write};

const MAX_MS: i64 = 99*3600*1000 + 59*60*1000 + 59*1000 + 999; // 359999999

enum Error { InvalidSrt, InvalidTime, TimeRange }

struct Entry {
    start: i64,
    end: i64,
    body: Vec<String>,
}

fn is_blank(line: &str) -> bool {
    line.chars().all(|c| c == ' ' || c == '\t')
}

fn parse_time_part(s: &str) -> Option<i64> {
    if s.len() != 12 { return None; }
    let b = s.as_bytes();
    if b[2] != b':' || b[5] != b':' || b[8] != b',' { return None; }
    let hh = parse_two(&b[0..2])?;
    let mm = parse_two(&b[3..5])?;
    let ss = parse_two(&b[6..8])?;
    let mmm = parse_three(&b[9..12])?;
    if hh > 99 || mm > 59 || ss > 59 { return None; }
    Some((hh as i64 * 3600 + mm as i64 * 60 + ss as i64) * 1000 + mmm as i64)
}

fn parse_two(b: &[u8]) -> Option<u32> {
    if b.len() != 2 { return None; }
    let d1 = (b[0] as char).to_digit(10)?;
    let d2 = (b[1] as char).to_digit(10)?;
    Some(d1 * 10 + d2)
}

fn parse_three(b: &[u8]) -> Option<u32> {
    if b.len() != 3 { return None; }
    let d1 = (b[0] as char).to_digit(10)?;
    let d2 = (b[1] as char).to_digit(10)?;
    let d3 = (b[2] as char).to_digit(10)?;
    Some(d1 * 100 + d2 * 10 + d3)
}

fn parse_time_line(line: &str) -> Option<(i64, i64)> {
    if line.len() != 29 { return None; }
    if &line[12..17] != " --> " { return None; }
    let start = parse_time_part(&line[0..12])?;
    let end = parse_time_part(&line[17..29])?;
    Some((start, end))
}

fn parse_srt(input: &str) -> Result<Vec<Entry>, Error> {
    let input = input.strip_prefix('\u{FEFF}').unwrap_or(input);
    if input.trim().is_empty() {
        return Ok(Vec::new());
    }
    let lines: Vec<&str> = input.lines().collect();
    if lines.is_empty() {
        return Ok(Vec::new());
    }
    if is_blank(lines[0]) {
        return Err(Error::InvalidSrt);
    }
    let mut entries = Vec::new();
    let mut i = 0;
    let n = lines.len();
    while i < n {
        // Skip blank lines (should only happen after previous block, but we already skip)
        while i < n && is_blank(lines[i]) {
            i += 1;
        }
        if i >= n { break; }
        // index
        let idx_line = lines[i];
        if idx_line.is_empty() || !idx_line.chars().all(|c| c.is_ascii_digit()) {
            return Err(Error::InvalidSrt);
        }
        i += 1;
        if i >= n { return Err(Error::InvalidSrt); }
        let time_line = lines[i];
        let (start, end) = parse_time_line(time_line).ok_or(Error::InvalidSrt)?;
        i += 1;
        // body
        let mut body = Vec::new();
        while i < n && !is_blank(lines[i]) {
            body.push(lines[i].to_string());
            i += 1;
        }
        if body.is_empty() {
            return Err(Error::InvalidSrt);
        }
        entries.push(Entry { start, end, body });
    }
    Ok(entries)
}

fn format_time(ms: i64) -> String {
    let hh = ms / 3600000;
    let ms = ms % 3600000;
    let mm = ms / 60000;
    let ms = ms % 60000;
    let ss = ms / 1000;
    let mmm = ms % 1000;
    format!("{:02}:{:02}:{:02},{:03}", hh, mm, ss, mmm)
}

fn json_escape(s: &str) -> String {
    let mut out = String::new();
    out.push('"');
    for c in s.chars() {
        match c {
            '"' => out.push_str("\\\""),
            '\\' => out.push_str("\\\\"),
            '\n' => out.push_str("\\n"),
            '\r' => out.push_str("\\r"),
            '\t' => out.push_str("\\t"),
            '\u{0008}' => out.push_str("\\b"),
            '\u{000C}' => out.push_str("\\f"),
            c if (c as u32) < 0x20 => {
                out.push_str(&format!("\\u{:04x}", c as u32));
            }
            c => out.push(c),
        }
    }
    out.push('"');
    out
}

fn process(input: &str) -> Result<String, Error> {
    let entries = parse_srt(input)?;
    // check original end > start
    for e in &entries {
        if e.end <= e.start {
            return Err(Error::InvalidTime);
        }
    }
    // apply offset? We need offset from request. Wait parse_srt doesn't take offset.
    // We need to parse offset from JSON first.
    // Let's restructure: parse JSON to get srt and offset, then parse_srt, then check time, then apply offset.
}

Actually process should take srt and offset and return result string.

Let's define:
fn process(srt: &str, offset: i64) -> Result<String, Error> {
    let mut entries = parse_srt(srt)?;
    for e in &entries {
        if e.end <= e.start {
            return Err(Error::InvalidTime);
        }
    }
    for e in &mut entries {
        e.start += offset;
        e.end += offset;
        if e.start < 0 || e.end < 0 || e.start > MAX_MS || e.end > MAX_MS {
            return Err(Error::TimeRange);
        }
    }
    // overlaps
    let mut overlaps = Vec::new();
    for i in 0..entries.len() {
        for j in (i+1)..entries.len() {
            if entries[i].start < entries[j].end && entries[j].start < entries[i].end {
                overlaps.push((i+1, j+1));
            }
        }
    }
    // build srt
    let mut out_srt = String::new();
    for (idx, e) in entries.iter().enumerate() {
        if idx > 0 {
            out_srt.push('\n'); // With block ending with \n, this adds extra \n => two LF between blocks
        }
        out_srt.push_str(&format!("{}\n", idx+1));
        out_srt.push_str(&format!("{} --> {}\n", format_time(e.start), format_time(e.end)));
        for (li, line) in e.body.iter().enumerate() {
            if li > 0 { out_srt.push('\n'); }
            out_srt.push_str(line);
        }
        out_srt.push('\n');
    }
    // Wait: If we do out_srt.push('\n') before each block except first, and each block ends with \n, then between block1 and block2:
    // block1 ends with \n. Then we push '\n' -> \n\n. Then block2 starts. Correct.
    // For last block, ends with \n. No extra. Good.
    // But careful: The above loop: for idx > 0 push('\n'). Then append index line with \n, time line with \n, body lines, then push('\n').
    // Let's trace: block1: idx=0: no extra. push "1\n". push "time\n". push body "Hello". push '\n'. Result: "1\ntime\nHello\n".
    // block2: idx=1: push '\n' -> now "1\ntime\nHello\n\n". push "2\n". push "time2\n". push body. push '\n'. Result: "1\ntime\nHello\n\n2\ntime2\nWorld\n". Correct.
    // If empty entries: out_srt is empty string. Correct.
    // But wait: For multiple body lines, we join with \n. The body lines are pushed without trailing \n, then we push final \n. So body line1\nline2\n. That's correct.
    // However, if body has multiple lines, the final push('\n') adds newline after last body line. So block ends with \n. Good.
    // Now JSON output
    let mut json = String::new();
    json.push_str("{\"srt\":");
    json.push_str(&json_escape(&out_srt));
    json.push_str(",\"overlaps\":[");
    for (k, (i,j)) in overlaps.iter().enumerate() {
        if k > 0 { json.push(','); }
        json.push_str(&format!("[{},{}]", i, j));
    }
    json.push_str("]}");
    Ok(json)
}

But wait: parse_srt returns Vec<Entry> with start/end original. Then we check end > start. Then apply offset. That's correct.

However, what about offset causing range error: we must check after applying offset to all? The spec: "任何结果小于 0 或大于 99:59:59,999 时整体返回 TIME_RANGE." So if any start or end out of range, return TIME_RANGE. Our loop checks each as we apply. If one fails, we return TimeRange. That's fine.

But note: "先完成全部原始条目的解析和有效时间检查，再应用偏移". So if there is an invalid time (end <= start) and also offset out of range, we should return INVALID_TIME. Our code checks all end > start before applying offset. Good.

Now JSON input parsing. Let's implement Parser and parse_input.

We need to parse the whole line. Let's write:
