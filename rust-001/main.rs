use std::io::{self, BufRead, Write};

#[derive(Debug, Clone)]
enum Json {
    Null,
    Bool(bool),
    Num(i64),
    Str(String),
    Array(Vec<Json>),
    Object(Vec<(String, Json)>),
}

struct Parser<'a> {
    s: &'a str,
    bytes: &'a [u8],
    pos: usize,
}

impl<'a> Parser<'a> {
    fn new(s: &'a str) -> Self {
        Parser { s, bytes: s.as_bytes(), pos: 0 }
    }

    fn skip_ws(&mut self) {
        while self.pos < self.bytes.len() {
            match self.bytes[self.pos] {
                b' ' | b'\t' | b'\n' | b'\r' => self.pos += 1,
                _ => break,
            }
        }
    }

    fn parse_value(&mut self) -> Json {
        self.skip_ws();
        if self.pos >= self.bytes.len() {
            panic!("unexpected end");
        }
        match self.bytes[self.pos] {
            b'n' => { self.expect_lit("null"); Json::Null }
            b't' => { self.expect_lit("true"); Json::Bool(true) }
            b'f' => { self.expect_lit("false"); Json::Bool(false) }
            b'"' => Json::Str(self.parse_string()),
            b'[' => self.parse_array(),
            b'{' => self.parse_object(),
            b'-' | b'0'..=b'9' => Json::Num(self.parse_number()),
            _ => panic!("invalid json"),
        }
    }

    fn expect_lit(&mut self, lit: &str) {
        if self.s[self.pos..].starts_with(lit) {
            self.pos += lit.len();
        } else {
            panic!("invalid literal");
        }
    }

    fn parse_number(&mut self) -> i64 {
        let start = self.pos;
        if self.bytes[self.pos] == b'-' {
            self.pos += 1;
        }
        while self.pos < self.bytes.len() && self.bytes[self.pos].is_ascii_digit() {
            self.pos += 1;
        }
        // ignore fraction/exponent? Not needed. But if present, parse just integer part? Better panic.
        self.s[start..self.pos].parse::<i64>().unwrap()
    }

    fn parse_string(&mut self) -> String {
        self.pos += 1; // skip "
        let mut out = String::new();
        while self.pos < self.bytes.len() {
            let b = self.bytes[self.pos];
            if b == b'"' {
                self.pos += 1;
                return out;
            } else if b == b'\\' {
                self.pos += 1;
                if self.pos >= self.bytes.len() { panic!("bad escape"); }
                match self.bytes[self.pos] {
                    b'"' => { out.push('"'); self.pos += 1; }
                    b'\\' => { out.push('\\'); self.pos += 1; }
                    b'/' => { out.push('/'); self.pos += 1; }
                    b'b' => { out.push('\u{0008}'); self.pos += 1; }
                    b'f' => { out.push('\u{000C}'); self.pos += 1; }
                    b'n' => { out.push('\n'); self.pos += 1; }
                    b'r' => { out.push('\r'); self.pos += 1; }
                    b't' => { out.push('\t'); self.pos += 1; }
                    b'u' => {
                        self.pos += 1;
                        let c = self.parse_hex4();
                        if (0xD800..=0xDBFF).contains(&c) {
                            // high surrogate
                            if self.pos + 6 <= self.bytes.len() && self.bytes[self.pos] == b'\\' && self.bytes[self.pos+1] == b'u' {
                                self.pos += 2;
                                let c2 = self.parse_hex4();
                                if (0xDC00..=0xDFFF).contains(&c2) {
                                    let code = 0x10000 + (((c - 0xD800) as u32) << 10) + ((c2 - 0xDC00) as u32);
                                    if let Some(ch) = char::from_u32(code) {
                                        out.push(ch);
                                    }
                                } else {
                                    // invalid, push replacement? Just push high surrogate as char? Not possible.
                                    if let Some(ch) = char::from_u32(c as u32) { out.push(ch); }
                                    if let Some(ch) = char::from_u32(c2 as u32) { out.push(ch); }
                                }
                            } else {
                                if let Some(ch) = char::from_u32(c as u32) { out.push(ch); }
                            }
                        } else if (0xDC00..=0xDFFF).contains(&c) {
                            if let Some(ch) = char::from_u32(c as u32) { out.push(ch); }
                        } else {
                            if let Some(ch) = char::from_u32(c as u32) { out.push(ch); }
                        }
                    }
                    _ => panic!("bad escape"),
                }
            } else {
                // regular char, possibly UTF-8
                let remaining = &self.s[self.pos..];
                let ch = remaining.chars().next().unwrap();
                out.push(ch);
                self.pos += ch.len_utf8();
            }
        }
        panic!("unterminated string");
    }

    fn parse_hex4(&mut self) -> u16 {
        let mut v: u16 = 0;
        for _ in 0..4 {
            if self.pos >= self.bytes.len() { panic!("bad unicode"); }
            let b = self.bytes[self.pos];
            let d = match b {
                b'0'..=b'9' => b - b'0',
                b'a'..=b'f' => b - b'a' + 10,
                b'A'..=b'F' => b - b'A' + 10,
                _ => panic!("bad hex"),
            };
            v = v * 16 + d as u16;
            self.pos += 1;
        }
        v
    }

    fn parse_array(&mut self) -> Json {
        self.pos += 1; // [
        let mut arr = Vec::new();
        self.skip_ws();
        if self.pos < self.bytes.len() && self.bytes[self.pos] == b']' {
            self.pos += 1;
            return Json::Array(arr);
        }
        loop {
            let v = self.parse_value();
            arr.push(v);
            self.skip_ws();
            if self.pos >= self.bytes.len() { panic!("bad array"); }
            match self.bytes[self.pos] {
                b',' => { self.pos += 1; }
                b']' => { self.pos += 1; break; }
                _ => panic!("bad array"),
            }
        }
        Json::Array(arr)
    }

    fn parse_object(&mut self) -> Json {
        self.pos += 1; // {
        let mut obj = Vec::new();
        self.skip_ws();
        if self.pos < self.bytes.len() && self.bytes[self.pos] == b'}' {
            self.pos += 1;
            return Json::Object(obj);
        }
        loop {
            self.skip_ws();
            if self.bytes[self.pos] != b'"' { panic!("bad object key"); }
            let key = self.parse_string();
            self.skip_ws();
            if self.bytes[self.pos] != b':' { panic!("bad object colon"); }
            self.pos += 1;
            let val = self.parse_value();
            obj.push((key, val));
            self.skip_ws();
            match self.bytes[self.pos] {
                b',' => { self.pos += 1; }
                b'}' => { self.pos += 1; break; }
                _ => panic!("bad object"),
            }
        }
        Json::Object(obj)
    }
}

fn parse_json(s: &str) -> Json {
    let mut p = Parser::new(s);
    let v = p.parse_value();
    p.skip_ws();
    if p.pos != p.bytes.len() {
        panic!("trailing data");
    }
    v
}

fn json_eq(a: &Json, b: &Json) -> bool {
    match (a, b) {
        (Json::Null, Json::Null) => true,
        (Json::Bool(x), Json::Bool(y)) => x == y,
        (Json::Num(x), Json::Num(y)) => x == y,
        (Json::Str(x), Json::Str(y)) => x == y,
        (Json::Array(xs), Json::Array(ys)) => {
            xs.len() == ys.len() && xs.iter().zip(ys.iter()).all(|(x, y)| json_eq(x, y))
        }
        (Json::Object(xs), Json::Object(ys)) => {
            if xs.len() != ys.len() { return false; }
            for (k, vx) in xs {
                let mut found = false;
                for (k2, vy) in ys {
                    if k == k2 {
                        if !json_eq(vx, vy) { return false; }
                        found = true;
                        break;
                    }
                }
                if !found { return false; }
            }
            true
        }
        _ => false,
    }
}

fn escape_segment(s: &str) -> String {
    let mut out = String::with_capacity(s.len());
    for c in s.chars() {
        match c {
            '~' => out.push_str("~0"),
            '/' => out.push_str("~1"),
            _ => out.push(c),
        }
    }
    out
}

fn write_json_string(s: &str, out: &mut String) {
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
            _ => out.push(c),
        }
    }
    out.push('"');
}

fn write_json(v: &Json, out: &mut String) {
    match v {
        Json::Null => out.push_str("null"),
        Json::Bool(b) => out.push_str(if *b { "true" } else { "false" }),
        Json::Num(n) => out.push_str(&n.to_string()),
        Json::Str(s) => write_json_string(s, out),
        Json::Array(arr) => {
            out.push('[');
            for (i, item) in arr.iter().enumerate() {
                if i > 0 { out.push(','); }
                write_json(item, out);
            }
            out.push(']');
        }
        Json::Object(obj) => {
            out.push('{');
            for (i, (k, v)) in obj.iter().enumerate() {
                if i > 0 { out.push(','); }
                write_json_string(k, out);
                out.push(':');
                write_json(v, out);
            }
            out.push('}');
        }
    }
}

fn change_add(path: &str, value: &Json) -> Json {
    Json::Object(vec![
        ("op".to_string(), Json::Str("add".to_string())),
        ("path".to_string(), Json::Str(path.to_string())),
        ("value".to_string(), value.clone()),
    ])
}

fn change_remove(path: &str, value: &Json) -> Json {
    Json::Object(vec![
        ("op".to_string(), Json::Str("remove".to_string())),
        ("path".to_string(), Json::Str(path.to_string())),
        ("value".to_string(), value.clone()),
    ])
}

fn change_replace(path: &str, before: &Json, after: &Json) -> Json {
    Json::Object(vec![
        ("op".to_string(), Json::Str("replace".to_string())),
        ("path".to_string(), Json::Str(path.to_string())),
        ("before".to_string(), before.clone()),
        ("after".to_string(), after.clone()),
    ])
}

fn diff(before: &Json, after: &Json, path: &str, changes: &mut Vec<Json>) {
    if json_eq(before, after) {
        return;
    }
    match (before, after) {
        (Json::Object(b), Json::Object(a)) => {
            let mut keys: Vec<&str> = Vec::new();
            for (k, _) in b {
                if !keys.contains(&k.as_str()) { keys.push(k); }
            }
            for (k, _) in a {
                if !keys.contains(&k.as_str()) { keys.push(k); }
            }
            keys.sort();
            for k in keys {
                let bv = b.iter().find(|(bk, _)| bk == k).map(|(_, v)| v);
                let av = a.iter().find(|(ak, _)| ak == k).map(|(_, v)| v);
                let child_path = if path.is_empty() {
                    format!("/{}", escape_segment(k))
                } else {
                    format!("{}/{}", path, escape_segment(k))
                };
                match (bv, av) {
                    (Some(v), None) => changes.push(change_remove(&child_path, v)),
                    (None, Some(v)) => changes.push(change_add(&child_path, v)),
                    (Some(bv), Some(av)) => diff(bv, av, &child_path, changes),
                    (None, None) => {}
                }
            }
        }
        (Json::Array(b), Json::Array(a)) => {
            let max = std::cmp::max(b.len(), a.len());
            for i in 0..max {
                let child_path = if path.is_empty() {
                    format!("/{}", i)
                } else {
                    format!("{}/{}", path, i)
                };
                if i < b.len() && i < a.len() {
                    diff(&b[i], &a[i], &child_path, changes);
                } else if i < b.len() {
                    changes.push(change_remove(&child_path, &b[i]));
                } else {
                    changes.push(change_add(&child_path, &a[i]));
                }
            }
        }
        _ => {
            changes.push(change_replace(path, before, after));
        }
    }
}

fn main() {
    let stdin = io::stdin();
    let stdout = io::stdout();
    let mut out = stdout.lock();
    for line in stdin.lock().lines() {
        let line = match line {
            Ok(l) => l,
            Err(_) => break,
        };
        let trimmed = line.trim();
        if trimmed.is_empty() {
            continue;
        }
        let req = parse_json(trimmed);
        let (before, after) = match req {
            Json::Object(ref fields) => {
                let before = fields.iter().find(|(k, _)| k == "before").map(|(_, v)| v).unwrap();
                let after = fields.iter().find(|(k, _)| k == "after").map(|(_, v)| v).unwrap();
                (before, after)
            }
            _ => panic!("request must be object"),
        };
        let mut changes = Vec::new();
        diff(before, after, "", &mut changes);
        let response = Json::Object(vec![("changes".to_string(), Json::Array(changes))]);
        let mut s = String::new();
        write_json(&response, &mut s);
        writeln!(out, "{}", s).unwrap();
    }
}
