struct JsonParser<'a> {
    chars: Vec<char>,
    pos: usize,
}
impl<'a> JsonParser<'a> {
    fn new(s: &str) -> Self {
        JsonParser { chars: s.chars().collect(), pos: 0 }
    }
    fn peek(&self) -> Option<char> { self.chars.get(self.pos).copied() }
    fn next(&mut self) -> Option<char> { let c = self.peek(); if c.is_some() { self.pos += 1; } c }
    fn skip_ws(&mut self) {
        while let Some(c) = self.peek() {
            if c == ' ' || c == '\t' || c == '\n' || c == '\r' {
                self.pos += 1;
            } else { break; }
        }
    }
    fn parse_object_for_csv(&mut self) -> Option<Option<String>> {
        // returns Some(Some(csv)) if found, Some(None) if not found but valid, None if error
        self.skip_ws();
        if self.next()? != '{' { return None; }
        let mut csv = None;
        self.skip_ws();
        if self.peek() == Some('}') {
            self.next();
            return Some(csv);
        }
        loop {
            self.skip_ws();
            let key = self.parse_string()?;
            self.skip_ws();
            if self.next()? != ':' { return None; }
            self.skip_ws();
            if key == "csv" {
                let val = self.parse_string()?;
                csv = Some(val);
            } else {
                self.skip_value()?;
            }
            self.skip_ws();
            match self.next()? {
                ',' => { self.skip_ws(); continue; }
                '}' => { break; }
                _ => return None,
            }
        }
        Some(csv)
    }
    fn parse_string(&mut self) -> Option<String> {
        self.skip_ws();
        if self.next()? != '"' { return None; }
        let mut s = String::new();
        loop {
            let c = self.next()?;
            if c == '"' {
                return Some(s);
            }
            if c == '\\' {
                let esc = self.next()?;
                match esc {
                    '"' => s.push('"'),
                    '\\' => s.push('\\'),
                    '/' => s.push('/'),
                    'b' => s.push('\x08'),
                    'f' => s.push('\x0c'),
                    'n' => s.push('\n'),
                    'r' => s.push('\r'),
                    't' => s.push('\t'),
                    'u' => {
                        let u = self.parse_hex4()?;
                        if (0xD800..=0xDBFF).contains(&u) {
                            // high surrogate, expect \u low
                            if self.next()? != '\\' || self.next()? != 'u' { return None; }
                            let low = self.parse_hex4()?;
                            if !(0xDC00..=0xDFFF).contains(&low) { return None; }
                            let code = 0x10000 + ((u - 0xD800) << 10) + (low - 0xDC00);
                            s.push(char::from_u32(code)?);
                        } else if (0xDC00..=0xDFFF).contains(&u) {
                            return None; // lone low surrogate
                        } else {
                            s.push(char::from_u32(u)?);
                        }
                    }
                    _ => return None,
                }
            } else {
                if c < ' ' { return None; } // control chars invalid
                s.push(c);
            }
        }
    }
    fn parse_hex4(&mut self) -> Option<u32> {
        let mut val = 0;
        for _ in 0..4 {
            let c = self.next()?;
            let d = match c {
                '0'..='9' => c as u32 - '0' as u32,
                'a'..='f' => c as u32 - 'a' as u32 + 10,
                'A'..='F' => c as u32 - 'A' as u32 + 10,
                _ => return None,
            };
            val = val * 16 + d;
        }
        Some(val)
    }
    fn skip_value(&mut self) -> Option<()> {
        self.skip_ws();
        match self.peek()? {
            '"' => { self.parse_string()?; Some(()) }
            '{' => { self.skip_object()?; Some(()) }
            '[' => { self.skip_array()?; Some(()) }
            't' => { self.expect_literal("true")?; Some(()) }
            'f' => { self.expect_literal("false")?; Some(()) }
            'n' => { self.expect_literal("null")?; Some(()) }
            '-' | '0'..='9' => { self.skip_number()?; Some(()) }
            _ => None,
        }
    }
    fn skip_object(&mut self) -> Option<()> {
        if self.next()? != '{' { return None; }
        self.skip_ws();
        if self.peek() == Some('}') { self.next(); return Some(()); }
        loop {
            self.skip_ws();
            self.parse_string()?;
            self.skip_ws();
            if self.next()? != ':' { return None; }
            self.skip_ws();
            self.skip_value()?;
            self.skip_ws();
            match self.next()? {
                ',' => { self.skip_ws(); continue; }
                '}' => return Some(()),
                _ => return None,
            }
        }
    }
    fn skip_array(&mut self) -> Option<()> {
        if self.next()? != '[' { return None; }
        self.skip_ws();
        if self.peek() == Some(']') { self.next(); return Some(()); }
        loop {
            self.skip_ws();
            self.skip_value()?;
            self.skip_ws();
            match self.next()? {
                ',' => { self.skip_ws(); continue; }
                ']' => return Some(()),
                _ => return None,
            }
        }
    }
    fn expect_literal(&mut self, lit: &str) -> Option<()> {
        for expected in lit.chars() {
            if self.next()? != expected { return None; }
        }
        Some(())
    }
    fn skip_number(&mut self) -> Option<()> {
        // skip optional -
        if self.peek() == Some('-') { self.next(); }
        // integer
        if self.peek() == Some('0') {
            self.next();
        } else if let Some(c) = self.peek() {
            if c.is_ascii_digit() {
                while let Some(c) = self.peek() {
                    if c.is_ascii_digit() { self.next(); } else { break; }
                }
            } else {
                return None;
            }
        } else {
            return None;
        }
        // fraction
        if self.peek() == Some('.') {
            self.next();
            let mut has_digit = false;
            while let Some(c) = self.peek() {
                if c.is_ascii_digit() { self.next(); has_digit = true; } else { break; }
            }
            if !has_digit { return None; }
        }
        // exponent
        if let Some(c) = self.peek() {
            if c == 'e' || c == 'E' {
                self.next();
                if let Some(c) = self.peek() {
                    if c == '+' || c == '-' { self.next(); }
                }
                let mut has_digit = false;
                while let Some(c) = self.peek() {
                    if c.is_ascii_digit() { self.next(); has_digit = true; } else { break; }
                }
                if !has_digit { return None; }
            }
        }
        Some(())
    }
}
