fn skip_ws(chars: &mut std::iter::Peekable<std::str::Chars>) {
    while let Some(&c) = chars.peek() {
        if c == ' ' || c == '\n' || c == '\r' || c == '\t' {
            chars.next();
        } else {
            break;
        }
    }
}

fn parse_json_string(chars: &mut std::iter::Peekable<std::str::Chars>) -> Result<String, ()> {
    skip_ws(chars);
    if chars.next() != Some('"') {
        return Err(());
    }
    let mut s = String::new();
    while let Some(c) = chars.next() {
        match c {
            '"' => return Ok(s),
            '\\' => {
                let esc = chars.next().ok_or(())?;
                match esc {
                    '"' => s.push('"'),
                    '\\' => s.push('\\'),
                    '/' => s.push('/'),
                    'b' => s.push('\u{0008}'),
                    'f' => s.push('\u{000C}'),
                    'n' => s.push('\n'),
                    'r' => s.push('\r'),
                    't' => s.push('\t'),
                    'u' => {
                        let code = read_hex4(chars)?;
                        let ch = if (0xD800..=0xDBFF).contains(&code) {
                            if chars.next() != Some('\\') || chars.next() != Some('u') {
                                return Err(());
                            }
                            let low = read_hex4(chars)?;
                            if !(0xDC00..=0xDFFF).contains(&low) {
                                return Err(());
                            }
                            let c = 0x10000 + ((code - 0xD800) << 10) + (low - 0xDC00);
                            char::from_u32(c).ok_or(())?
                        } else if (0xDC00..=0xDFFF).contains(&code) {
                            return Err(());
                        } else {
                            char::from_u32(code).ok_or(())?
                        };
                        s.push(ch);
                    }
                    _ => return Err(()),
                }
            }
            c => s.push(c),
        }
    }
    Err(())
}

fn read_hex4(chars: &mut std::iter::Peekable<std::str::Chars>) -> Result<u32, ()> {
    let mut v = 0u32;
    for _ in 0..4 {
        let c = chars.next().ok_or(())?;
        let d = c.to_digit(16).ok_or(())?;
        v = v * 16 + d;
    }
    Ok(v)
}

fn skip_json_value(chars: &mut std::iter::Peekable<std::str::Chars>) -> Result<(), ()> {
    skip_ws(chars);
    match chars.peek() {
        Some('"') => {
            parse_json_string(chars)?;
            Ok(())
        }
        Some('{') => skip_json_object(chars),
        Some('[') => skip_json_array(chars),
        Some('t') => {
            expect(chars, "true")
        }
        Some('f') => {
            expect(chars, "false")
        }
        Some('n') => {
            expect(chars, "null")
        }
        Some(c) if *c == '-' || c.is_ascii_digit() => skip_json_number(chars),
        _ => Err(()),
    }
}

fn skip_json_object(chars: &mut std::iter::Peekable<std::str::Chars>) -> Result<(), ()> {
    if chars.next() != Some('{') { return Err(()); }
    loop {
        skip_ws(chars);
        match chars.peek() {
            Some('}') => {
                chars.next();
                return Ok(());
            }
            Some('"') => {
                parse_json_string(chars)?;
                skip_ws(chars);
                if chars.next() != Some(':') { return Err(()); }
                skip_json_value(chars)?;
                skip_ws(chars);
                match chars.peek() {
                    Some(',') => { chars.next(); }
                    Some('}') => {
                        chars.next();
                        return Ok(());
                    }
                    _ => return Err(()),
                }
            }
            _ => return Err(()),
        }
    }
}

fn skip_json_array(chars: &mut std::iter::Peekable<std::str::Chars>) -> Result<(), ()> {
    if chars.next() != Some('[') { return Err(()); }
    loop {
        skip_ws(chars);
        match chars.peek() {
            Some(']') => {
                chars.next();
                return Ok(());
            }
            _ => {
                skip_json_value(chars)?;
                skip_ws(chars);
                match chars.peek() {
                    Some(',') => { chars.next(); }
                    Some(']') => {
                        chars.next();
