fn main() {
    let stdin = std::io::stdin();
    for line in stdin.lock().lines() {
        let line = match line {
            Ok(l) => l,
            Err(_) => continue,
        };
        let line = line.trim();
        if line.is_empty() {
            continue;
        }
        let (srt_input, offset_ms) = match parse_request(line) {
            Some(v) => v,
            None => {
                println!("{{\"error\":\"INVALID_SRT\"}}");
                continue;
            }
        };
        let result = process(&srt_input, offset_ms);
        println!("{}", result);
    }
}

fn process(srt_input: &str, offset_ms: i64) -> String {
    let subtitles = match parse_srt(srt_input) {
        Ok(v) => v,
        Err(e) => return format!("{{\"error\":\"{}\"}}", e),
    };
    if subtitles.is_empty() {
        return "{\"srt\":\"\",\"overlaps\":[]}".to_string();
    }
    // apply offset and check range
    let max_ms = 359999999; // 99:59:59,999
    let mut adjusted = Vec::new();
    for sub in &subtitles {
        let start = sub.start + offset_ms;
        let end = sub.end + offset_ms;
        if start < 0 || end < 0 || start > max_ms || end > max_ms {
            return "{\"error\":\"TIME_RANGE\"}".to_string();
        }
        adjusted.push(Subtitle {
            start,
            end,
            body: sub.body.clone(),
        });
    }
    // detect overlaps
    let mut overlaps: Vec<(usize, usize)> = Vec::new();
    let n = adjusted.len();
    for i in 0..n {
        for j in (i+1)..n {
            if adjusted[i].start < adjusted[j].end && adjusted[j].start < adjusted[i].end {
                overlaps.push((i+1, j+1));
            }
        }
    }
    // format SRT
    let mut srt_out = String::new();
    for (idx, sub) in adjusted.iter().enumerate() {
        if idx > 0 {
            srt_out.push('\n');
        }
        srt_out.push_str(&format!("{}\n", idx + 1));
        srt_out.push_str(&format!("{} --> {}\n", format_time(sub.start), format_time(sub.end)));
        for (i, line) in sub.body.iter().enumerate() {
            if i > 0 {
                srt_out.push('\n');
            }
            srt_out.push_str(line);
        }
        srt_out.push('\n'); // end of block? Wait, we need exactly two LFs between blocks.
    }
    // Wait, the above logic: for each block, we append number, time, body, then '\n'. But between blocks we need two LFs. If we just append '\n' at end of each block, then blocks are separated by one LF? Let's trace:
    // Block1: "1\ntime\nbody1\n" (ends with \n)
    // Block2: "2\ntime\nbody2\n"
    // Concatenated: "1\ntime\nbody1\n2\ntime\nbody2\n". Between body1 and 2 there is only one \n? Actually body1 ends with \n, then next block starts with '2'. So there is one \n between body1 and 2. But we need two LFs between blocks. So we need an extra \n between blocks.
    // The spec: "块之间恰好两个 LF". So between block1 and block2, there should be "\n\n". The last block ends with exactly one LF.
    // So structure: for i in 0..n:
    //   if i > 0 { append "\n" } // extra LF to make two between blocks? Let's see.
    //   append block content (number, time, body lines) without trailing LF? Actually block content lines are separated by LF. Let's define block content as: number + "\n" + time + "\n" + body lines joined by "\n". No trailing LF on body.
    // Then for i=0..n:
    //   if i > 0 { srt_out.push('\n'); } // this gives one LF separator? Wait, if we append block content without trailing LF, then between block1 and block2 we need two LFs. So we should append "\n\n" between blocks.
    // Let's build correctly:
    // For each block, create string: number + "\n" + time + "\n" + body_joined. No trailing newline.
    // Then join blocks with "\n\n". Finally append "\n" at the end.
    // Let's do that.
    let mut blocks_out: Vec<String> = Vec::new();
    for (idx, sub) in adjusted.iter().enumerate() {
        let mut block = String::new();
        block.push_str(&format!("{}\n", idx + 1));
        block.push_str(&format!("{} --> {}\n", format_time(sub.start), format_time(sub.end)));
        block.push_str(&sub.body.join("\n"));
        blocks_out.push(block);
    }
    let srt_out = blocks_out.join("\n\n") + "\n";
    // But wait: if there are no blocks, we already handled empty. If there is one block, join gives the block, then + "\n" -> block + "\n". That matches example: block is "1\ntime\nHello", then + "\n" -> "1\ntime\nHello\n". Correct.
    // For multiple blocks: block1 + "\n\n" + block2 + ... + "\n". Between block1 and block2: block1 ends with body, then "\n\n", then block2 starts with number. So there are two LFs between body and next number. Correct.
    // Now overlaps JSON
    let overlaps_json = if overlaps.is_empty() {
        "[]".to_string()
    } else {
        let mut s = String::from("[");
        for (i, (a, b)) in overlaps.iter().enumerate() {
            if i > 0 { s.push(','); }
            s.push_str(&format!("[{},{}]", a, b));
        }
        s.push(']');
        s
    };
    // escape srt_out for JSON
    let escaped_srt = json_escape(&srt_out);
    format!("{{\"srt\":\"{}\",\"overlaps\":{}}}", escaped_srt, overlaps_json)
}

fn json_escape(s: &str) -> String {
    let mut out = String::new();
    for c in s.chars() {
        match c {
            '"' => out.push_str("\\\""),
            '\\' => out.push_str("\\\\"),
            '\n' => out.push_str("\\n"),
            '\r' => out.push_str("\\r"),
            '\t' => out.push_str("\\t"),
            '\u{0008}' => out.push_str("\\b"),
            '\u{000C}' => out.push_str("\\f"),
            c if c < ' ' => {
                out.push_str(&format!("\\u{:04x}", c as u32));
            }
            _ => out.push(c),
        }
    }
    out
}
