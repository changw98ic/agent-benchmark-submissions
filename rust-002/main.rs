struct JsonParser<'a> {
    s: &'a str,
    pos: usize,
}

enum JsonValue {
    String(String),
    Integer(i64),
    Object(Vec<(String, JsonValue)>),
    Other,
}

impl<'a> JsonParser<'a> {
    fn new(s: &'a str) -> Self { Self { s, pos: 0 } }

    fn peek_char(&self) -> Option<char> {
        self.s[self.pos..].chars().next()
    }

    fn next_char(&mut self) -> Option<char> {
        let c = self.peek_char()?;
        self.pos += c.len_utf8();
        Some(c)
    }

    fn skip_ws(&mut self) {
        while let Some(c) = self.peek_char() {
            if c == ' ' || c == '\n' || c == '\r' || c == '\t' {
                self.pos += c.len_utf8();
            } else {
                break;
            }
        }
    }

    fn expect_str(&mut self, expected: &str) -> Result<(), ()> {
        for c in expected.chars() {
            if self.next_char() != Some(c) {
                return Err(());
            }
        }
        Ok(())
    }

    fn parse_hex4(&mut self) -> Result<u32, ()> {
        let mut val = 0u32;
        for _ in 0..4 {
            let c = self.next_char().ok_or(())?;
            let d = c.to_digit(16).ok_or(())?;
            val = val * 16 + d;
        }
        Ok(val)
    }

    fn parse_string(&mut self) -> Result<String, ()> {
        if self.next_char() != Some('"') { return Err(()); }
        let mut out = String::new();
        loop {
            let c = self.next_char().ok_or(())?;
            match c {
                '"' => break,
                '\\' => {
                    let e = self.next_char().ok_or(())?;
                    match e {
                        '"' => out.push('"'),
                        '\\' => out.push('\\'),
                        '/' => out.push('/'),
                        'b' => out.push('\u{0008}'),
                        'f' => out.push('\u{000C}'),
                        'n' => out.push('\n'),
                        'r' => out.push('\r'),
                        't' => out.push('\t'),
                        'u' => {
                            let u1 = self.parse_hex4()?;
                            if (0xD800..=0xDBFF).contains(&u1) {
                                if self.next_char() != Some('\\') { return Err(()); }
                                if self.next_char() != Some('u') { return Err(()); }
                                let u2 = self.parse_hex4()?;
                                if !(0xDC00..=0xDFFF).contains(&u2) { return Err(()); }
                                let code = 0x10000 + ((u1 - 0xD800) << 10) + (u2 - 0xDC00);
                                out.push(char::from_u32(code).ok_or(())?);
                            } else if (0xDC00..=0xDFFF).contains(&u1) {
                                return Err(());
                            } else {
                                out.push(char::from_u32(u1).ok_or(())?);
                            }
                        }
                        _ => return Err(()),
                    }
                }
                _ => {
                    if (c as u32) < 0x20 { return Err(()); }
                    out.push(c);
                }
            }
        }
        Ok(out)
    }

    fn parse_number(&mut self) -> Result<JsonValue, ()> {
        let start = self.pos;
        if self.peek_char() == Some('-') {
            self.next_char();
        }
        let mut has_digits = false;
        while let Some(c) = self.peek_char() {
            if c.is_ascii_digit() {
                has_digits = true;
                self.next_char();
            } else {
                break;
            }
        }
        if !has_digits { return Err(()); }
        let mut is_int = true;
        if self.peek_char() == Some('.') {
            is_int = false;
            self.next_char();
            let mut frac_digits = false;
            while let Some(c) = self.peek_char() {
                if c.is_ascii_digit() {
                    frac_digits = true;
                    self.next_char();
                } else {
                    break;
                }
            }
            if !frac_digits { return Err(()); }
        }
        if let Some(c) = self.peek_char() {
            if c == 'e' || c == 'E' {
                is_int = false;
                self.next_char();
                if let Some(c) = self.peek_char() {
                    if c == '+' || c == '-' {
                        self.next_char();
                    }
                } else {
                    return Err(());
                }
                let mut exp_digits = false;
                while let Some(c) = self.peek_char() {
                    if c.is_ascii_digit() {
                        exp_digits = true;
                        self.next_char();
                    } else {
                        break;
                    }
                }
                if !exp_digits { return Err(()); }
            }
        }
        let num_str = &self.s[start..self.pos];
        if is_int {
            match num_str.parse::<i64>() {
                Ok(n) => Ok(JsonValue::Integer(n)),
                Err(_) => Err(()),
            }
        } else {
            Ok(JsonValue::Other)
        }
    }

    fn parse_array(&mut self) -> Result<JsonValue, ()> {
        if self.next_char() != Some('[') { return Err(()); }
        self.skip_ws();
        if self.peek_char() == Some(']') {
            self.next_char();
            return Ok(JsonValue::Other);
        }
        loop {
            self.skip_ws();
            self.parse_value()?;
            self.skip_ws();
            match self.next_char() {
                Some(',') => { self.skip_ws(); continue; }
                Some(']') => break,
                _ => return Err(()),
            }
        }
        Ok(JsonValue::Other)
    }

    fn parse_object(&mut self) -> Result<JsonValue, ()> {
        if self.next_char() != Some('{') { return Err(()); }
        self.skip_ws();
        let mut obj = Vec::new();
        if self.peek_char() == Some('}') {
            self.next_char();
            return Ok(JsonValue::Object(obj));
        }
        loop {
            self.skip_ws();
            let key = self.parse_string()?;
            self.skip_ws();
            if self.next_char() != Some(':') { return Err(()); }
            self.skip_ws();
            let val = self.parse_value()?;
            obj.push((key, val));
            self.skip_ws();
            match self.next_char() {
                Some(',') => { self.skip_ws(); continue; }
                Some('}') => break,
                _ => return Err(()),
            }
        }
        Ok(JsonValue::Object(obj))
    }

    fn parse_value(&mut self) -> Result<JsonValue, ()> {
        self.skip_ws();
        match self.peek_char() {
            Some('"') => Ok(JsonValue::String(self.parse_string()?)),
            Some('{') => self.parse_object(),
            Some('[') => self.parse_array(),
            Some('t') => { self.expect_str("true")?; Ok(JsonValue::Other) }
            Some('f') => { self.expect_str("false")?; Ok(JsonValue::Other) }
            Some('n') => { self.expect_str("null")?; Ok(JsonValue::Other) }
            Some(c) if c == '-' || c.is_ascii_digit() => self.parse_number(),
            _ => Err(()),
        }
    }
}
