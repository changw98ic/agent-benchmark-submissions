}
    fn skip_value(&mut self) -> Option<()> {
        self.skip_ws();
        match self.peek()? {
            b'"' => { self.parse_string()?; Some(()) }
            b'{' => {
                self.pos += 1;
                self.skip_ws();
                if self.consume(b'}') { return Some(()); }
                loop {
                    self.parse_string()?;
                    self.skip_ws();
                    if !self.consume(b':') { return None; }
                    self.skip_value()?;
                    self.skip_ws();
                    if self.consume(b',') { continue; }
                    if self.consume(b'}') { return Some(()); }
                    return None;
                }
            }
            b'[' => {
                self.pos += 1;
                self.skip_ws();
                if self.consume(b']') { return Some(()); }
                loop {
                    self.skip_value()?;
                    self.skip_ws();
                    if self.consume(b',') { continue; }
                    if self.consume(b']') { return Some(()); }
                    return None;
                }
            }
            b't' => { for &b in b"true" { if !self.consume(b) { return None; } } Some(()) }
            b'f' => { for &b in b"false" { if !self.consume(b) { return None; } } Some(()) }
            b'n' => { for &b in b"null" { if !self.consume(b) { return None; } } Some(()) }
            b'-' | b'0'..=b'9' => { self.parse_integer()?; Some(()) }
