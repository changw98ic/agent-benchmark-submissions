struct Parser<'a> {
    chars: std::iter::Peekable<std::str::Chars<'a>>,
}

impl<'a> Parser<'a> {
    fn new(s: &'a str) -> Self {
        Self { chars: s.chars().peekable() }
    }
    fn skip_ws(&mut self) {
        while let Some(&c) = self.chars.peek() {
            if c.is_whitespace() {
                self.chars.next();
            } else {
                break;
            }
        }
    }
    fn peek(&mut self) -> Option<char> {
        self.chars.peek().copied()
    }
    fn next(&mut self) -> Option<char> {
        self.chars.next()
    }
    fn expect(&mut self, c: char) -> Result<(), ()> {
        if self.next() == Some(c) { Ok(()) } else { Err(()) }
    }
    fn parse_string(&mut self) -> Result<String, ()> {
        self.skip_ws();
        if self.next() != Some('"') { return Err(()); }
        let mut s = String::new();
        while let Some(c) = self.next() {
            match c {
                '"' => return Ok(s),
                '\\' => {
                    match self.next() {
                        Some('"') => s.push('"'),
                        Some('\\') => s.push('\\'),
                        Some('/') => s.push('/'),
                        Some('b') => s.push('\u{0008}'),
                        Some('f') => s.push('\u{000C}'),
                        Some('n') => s.push('\n'),
                        Some('r') => s.push('\r'),
                        Some('t') => s.push('\t'),
                        Some('u') => {
                            let mut code = 0u32;
                            for _ in 0..4 {
                                let c = self.next().ok_or(())?;
                                let digit = c.to_digit(16).ok_or(())?;
                                code = code * 16 + digit;
                            }
                            if (0xD800..=0xDBFF).contains(&code) {
                                // high surrogate
                                if self.next() != Some('\\') { return Err(()); }
                                if self.next() != Some('u') { return Err(()); }
                                let mut low = 0u32;
                                for _ in 0..4 {
                                    let c = self.next().ok_or(())?;
                                    let digit = c.to_digit(16).ok_or(())?;
                                    low = low * 16 + digit;
                                }
                                if !(0xDC00..=0xDFFF).contains(&low) { return Err(()); }
                                let code = 0x10000 + ((code - 0xD800) << 10) + (low - 0xDC00);
                                if let Some(ch) = char::from_u32(code) {
                                    s.push(ch);
                                } else {
                                    return Err(());
                                }
                            } else if (0xDC00..=0xDFFF).contains(&code) {
                                return Err(());
                            } else {
                                if let Some(ch) = char::from_u32(code) {
                                    s.push(ch);
                                } else {
                                    return Err(());
                                }
                            }
                        }
                        _ => return Err(()),
                    }
                }
                c if c < '\u{20}' => return Err(()),
                c => s.push(c),
            }
        }
        Err(())
    }
    fn skip_value(&mut self) -> Result<(), ()> {
        self.skip_ws();
        match self.peek() {
            Some('"') => { self.parse_string()?; Ok(()) }
            Some('{') => self.skip_object(),
            Some('[') => self.skip_array(),
            Some('t') => self.expect_literal("true"),
            Some('f') => self.expect_literal("false"),
            Some('n') => self.expect_literal("null"),
            Some(c) if c == '-' || c.is_ascii_digit() => self.skip_number(),
            _ => Err(()),
        }
    }
    fn expect_literal(&mut self, lit: &str) -> Result<(), ()> {
        for expected in lit.chars() {
            if self.next() != Some(expected) {
                return Err(());
            }
        }
        Ok(())
    }
    fn skip_number(&mut self) -> Result<(), ()> {
        // simple number parser
        let mut has_digit = false;
        if self.peek() == Some('-') {
            self.next();
        }
        while let Some(c) = self.peek() {
            if c.is_ascii_digit() {
                has_digit = true;
                self.next();
            } else {
                break;
            }
        }
        if !has_digit { return Err(()); }
        if self.peek() == Some('.') {
            self.next();
            let mut has_frac = false;
            while let Some(c) = self.peek() {
                if c.is_ascii_digit() {
                    has_frac = true;
                    self.next();
                } else {
                    break;
                }
            }
            if !has_frac { return Err(()); }
        }
        if let Some(c) = self.peek() {
            if c == 'e' || c == 'E' {
                self.next();
                if let Some(c) = self.peek() {
                    if c == '+' || c == '-' {
                        self.next();
                    }
                }
                let mut has_exp = false;
                while let Some(c) = self.peek() {
                    if c.is_ascii_digit() {
                        has_exp = true;
                        self.next();
                    } else {
                        break;
                    }
                }
                if !has_exp { return Err(()); }
            }
        }
        Ok(())
    }
    fn skip_object(&mut self) -> Result<(), ()> {
        self.expect('{')?;
        self.skip_ws();
        if self.peek() == Some('}') {
            self.next();
            return Ok(());
        }
        loop {
            self.skip_ws();
            self.parse_string()?; // key
            self.skip_ws();
            self.expect(':')?;
            self.skip_ws();
            self.skip_value()?;
            self.skip_ws();
            match self.next() {
                Some(',') => continue,
                Some('}') => return Ok(()),
                _ => return Err(()),
            }
        }
    }
    fn skip_array(&mut self) -> Result<(), ()> {
        self.expect('[')?;
        self.skip_ws();
        if self.peek() == Some(']') {
            self.next();
            return Ok(());
        }
        loop {
            self.skip_ws();
            self.skip_value()?;
            self.skip_ws();
            match self.next() {
                Some(',') => continue,
                Some(']') => return Ok(()),
                _ => return Err(()),
            }
        }
    }
}
