use std::collections::{BTreeMap, BTreeSet};
use std::io::{self, BufRead, Write};

#[derive(Clone, PartialEq)]
enum Json {
    Null,
    Bool(bool),
    Num(i64),
    Str(String),
    Arr(Vec<Json>),
    Obj(BTreeMap<String, Json>),
}

enum Change {
    Add { path: String, value: Json },
    Remove { path: String, value: Json },
    Replace { path: String, before: Json, after: Json },
}

struct Parser {
    chars: Vec<char>,
    pos: usize,
}

impl Parser {
    fn new(s: &str) -> Self {
        Parser {
            chars: s.chars().collect(),
            pos: 0,
        }
    }
    fn peek(&self) -> Option<char> {
        self.chars.get(self.pos).copied()
    }
    fn next(&mut self) -> Option<char> {
        let c = self.peek();
        if c.is_some() {
            self.pos += 1;
        }
        c
    }
    fn skip_ws(&mut self) {
        while let Some(c) = self.peek() {
            if c == ' ' || c == '\n' || c == '\r' || c == '\t' {
                self.pos += 1;
            } else {
                break;
            }
        }
    }
    fn expect_str(&mut self, s: &str) -> Result<(), String> {
        for expected in s.chars() {
            match self.next() {
                Some(c) if c == expected => {}
                _ => return Err("expected string".to_string()),
            }
        }
        Ok(())
    }
    fn parse_value(&mut self) -> Result<Json, String> {
        self.skip_ws();
        match self.peek() {
            Some('{') => self.parse_object(),
            Some('[') => self.parse_array(),
            Some('"') => self.parse_string().map(Json::Str),
            Some('t') => {
                self.expect_str("true")?;
                Ok(Json::Bool(true))
            }
            Some('f') => {
                self.expect_str("false")?;
                Ok(Json::Bool(false))
            }
            Some('n') => {
                self.expect_str("null")?;
                Ok(Json::Null)
            }
            Some(c) if c == '-' || c.is_ascii_digit() => self.parse_number(),
            _ => Err("unexpected character".to_string()),
        }
    }
    fn parse_number(&mut self) -> Result<Json, String> {
        let mut s = String::new();
        if self.peek() == Some('-') {
            s.push('-');
            self.next();
        }
        while let Some(c) = self.peek() {
            if c.is_ascii_digit() {
                s.push(c);
                self.next();
            } else {
                break;
            }
        }
        s.parse::<i64>().map(Json::Num).map_err(|e| e.to_string())
    }
    fn parse_string(&mut self) -> Result<String, String> {
        self.next();
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
                                let hex = self.next().ok_or("eof")?;
                                let val = hex.to_digit(16).ok_or("bad hex")?;
                                code = code * 16 + val;
                            }
                            if (0xD800..=0xDBFF).contains(&code) {
                                if self.next() != Some('\\') || self.next() != Some('u') {
                                    return Err("invalid surrogate".to_string());
                                }
                                let mut code2 = 0u32;
                                for _ in 0..4 {
                                    let hex = self.next().ok_or("eof")?;
                                    let val = hex.to_digit(16).ok_or("bad hex")?;
                                    code2 = code2 * 16 + val;
                                }
                                if !(0xDC00..=0xDFFF).contains(&code2) {
                                    return Err("invalid low surrogate".to_string());
                                }
                                let c = 0x10000 + ((code - 0xD800) << 10) + (code2 - 0xDC00);
                                s.push(char::from_u32(c).ok_or("bad char")?);
                            } else {
                                s.push(char::from_u32(code).ok_or("bad char")?);
                            }
                        }
                        _ => return Err("bad escape".to_string()),
                    }
                }
                _ => s.push(c),
            }
        }
        Err("unterminated string".to_string())
    }
    fn parse_object(&mut self) -> Result<Json, String> {
        self.next();
        let mut map = BTreeMap::new();
        self.skip_ws();
        if self.peek() == Some('}') {
            self.next();
            return Ok(Json::Obj(map));
        }
        loop {
            self.skip_ws();
            let key = self.parse_string()?;
            self.skip_ws();
            if self.next() != Some(':') {
                return Err("expected colon".to_string());
            }
            let value = self.parse_value()?;
            map.insert(key, value
