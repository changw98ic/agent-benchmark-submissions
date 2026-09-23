fn extract_csv(line: &str) -> Option<String> {
    let mut chars = line.chars().peekable();
    // find "csv"
    while let Some(c) = chars.next() {
        if c == '"' {
            // read key
            let mut key = String::new();
            while let Some(&c2) = chars.peek() {
                if c2 == '"' {
                    chars.next();
                    break;
                } else if c2 == '\\' {
                    // handle escapes? keys are simple, but just in case
                    chars.next();
                    if let Some(&esc) = chars.peek() {
                        chars.next();
                        key.push(esc);
                    }
                } else {
                    key.push(c2);
                    chars.next();
                }
            }
            // skip whitespace
            while let Some(&c2) = chars.peek() {
                if c2.is_whitespace() { chars.next(); } else { break; }
            }
            if chars.peek() == Some(&':') {
                chars.next();
                // skip whitespace
                while let Some(&c2) = chars.peek() {
                    if c2.is_whitespace() { chars.next(); } else { break; }
                }
                if key == "csv" {
                    // parse string
                    if chars.peek() == Some(&'"') {
                        chars.next(); // opening quote
                        let mut val = String::new();
                        while let Some(c2) = chars.next() {
                            if c2 == '"' {
                                return Some(val);
                            } else if c2 == '\\' {
                                if let Some(esc) = chars.next() {
                                    match esc {
                                        '"' => val.push('"'),
                                        '\\' => val.push('\\'),
                                        '/' => val.push('/'),
                                        'b' => val.push('\u{0008}'),
                                        'f' => val.push('\u{000C}'),
                                        'n' => val.push('\n'),
                                        'r' => val.push('\r'),
                                        't' => val.push('\t'),
                                        'u' => {
                                            let mut hex = String::new();
                                            for _ in 0..4 {
                                                if let Some(h) = chars.next() {
                                                    hex.push(h);
                                                } else { break; }
                                            }
                                            if hex.len() == 4 {
                                                if let Ok(code) = u32::from_str_radix(&hex, 16) {
                                                    // handle surrogate pair
                                                    if (0xD800..=0xDBFF).contains(&code) {
                                                        // expect next \u
                                                        if chars.next() == Some('\\') && chars.next() == Some('u') {
                                                            let mut hex2 = String::new();
                                                            for _ in 0..4 {
                                                                if let Some(h) = chars.next() { hex2.push(h); } else { break; }
                                                            }
                                                            if hex2.len() == 4 {
                                                                if let Ok(code2) = u32::from_str_radix(&hex2, 16) {
                                                                    let c = 0x10000 + ((code - 0xD800) << 10) + (code2 - 0xDC00);
                                                                    if let Some(ch) = char::from_u32(c) { val.push(ch); }
                                                                }
                                                            }
                                                        }
                                                    } else if let Some(ch) = char::from_u32(code) {
                                                        val.push(ch);
                                                    }
                                                }
                                            }
                                        }
                                        _ => val.push(esc),
                                    }
                                }
                            } else {
                                val.push(c2);
                            }
                        }
                    }
                }
            }
        }
    }
    None
}
