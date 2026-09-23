use std::collections::BTreeMap;
use std::io::{self, BufRead, Write};

#[derive(Clone, PartialEq, Eq)]
enum Value {
    Null,
    Bool(bool),
    Number(i64),
    String(String),
    Array(Vec<Value>),
    Object(BTreeMap<String, Value>),
}

struct Parser<'a> {
    input: &'a [u8],
    pos: usize,
}

impl<'a> Parser<'a> {
    fn new(s: &'a str) -> Self {
        Parser { input: s.as_bytes(), pos: 0 }
    }
    fn peek(&self) -> Option<u8> {
        self.input.get(self.pos).copied()
    }
    fn next(&mut self) -> Option<u8> {
        let b = self.peek();
        if b.is_some() {
            self.pos += 1;
        }
        b
    }
    fn skip_ws(&mut self) {
        while let Some(b) = self.peek() {
            if b == b' ' || b == b'\t' || b == b'\n' || b == b'\r' {
                self.pos += 1;
            } else {
                break;
            }
        }
    }
    fn parse_value(&mut self) -> Result<Value, String> {
        self.skip_ws();
        match self.peek() {
            Some(b'n') => self.parse_null(),
            Some(b't') => self.parse_true(),
            Some(b'f') => self.parse_false(),
            Some(b'"') => self.parse_string().map(Value::String),
            Some(b'[') => self.parse_array(),
            Some(b'{') => self.parse_object(),
            Some(b'-') | Some(b'0'..=b'9') => self.parse_number(),
            _ => Err("invalid value".to_string()),
        }
    }
    fn parse_null(&mut self) -> Result<Value, String> {
        if self.input[self.pos..].starts_with(b"null") {
            self.pos += 4;
            Ok(Value::Null)
        } else {
            Err("expected null".to_string())
        }
    }
    fn parse_true(&mut self) -> Result<Value, String> {
        if self.input[self.pos..].starts_with(b"true") {
            self.pos += 4;
            Ok(Value::Bool(true))
        } else {
            Err("expected true".to_string())
        }
    }
    fn parse_false(&mut self) -> Result<Value, String> {
        if self.input[self.pos..].starts_with(b"false") {
            self.pos += 5;
            Ok(Value::Bool(false))
        } else {
            Err("expected false".to_string())
        }
    }
    fn parse_string(&mut self) -> Result<String, String> {
        if self.next() != Some(b'"') {
            return Err("expected quote".to_string());
        }
        let mut bytes = Vec::new();
        loop {
            let b = self.next().ok_or("unexpected end")?;
            match b {
                b'"' => break,
                b'\\' => {
                    let esc = self.next().ok_or("unexpected end")?;
                    match esc {
                        b'"' => bytes.push(b'"'),
                        b'\\' => bytes.push(b'\\'),
                        b'/' => bytes.push(b'/'),
                        b'b' => bytes.push(0x08),
                        b'f' => bytes.push(0x0C),
                        b'n' => bytes.push(b'\n'),
                        b'r' => bytes.push(b'\r'),
                        b't' => bytes.push(b'\t'),
                        b'u' => {
                            let cp = self.parse_hex4()?;
                            if (0xD800..=0xDBFF).contains(&cp) {
                                if self.peek() == Some(b'\\') {
                                    self.pos += 1;
                                    if self.peek() == Some(b'u') {
                                        self.pos += 1;
                                        let low = self.parse_hex4()?;
                                        if (0xDC00..=0xDFFF).contains(&low) {
                                            let c = 0x10000 + ((cp - 0xD800) << 10) + (low - 0xDC00);
                                            let ch = char::from_u32(c).unwrap_or('\u{FFFD}');
                                            let mut buf = [0; 4];
                                            bytes.extend_from_slice(ch.encode_utf8(&mut buf).as_bytes());
                                        } else {
                                            bytes.extend_from_slice("\u{FFFD}".as_bytes());
                                            bytes.extend_from_slice("\u{FFFD}".as_bytes());
                                        }
                                    } else {
                                        bytes.extend_from_slice("\u{FFFD}".as_bytes());
                                    }
                                } else {
                                    bytes.extend_from_slice("\u{FFFD}".as_bytes());
                                }
                            } else if (0xDC00..=0xDFFF).contains(&cp) {
                                bytes.extend_from_slice("\u{FFFD}".as_bytes());
                            } else {
                                let ch = char::from_u32(cp).unwrap_or('\u{FFFD}');
                                let mut buf = [0; 4];
                                bytes.extend_from_slice(ch.encode_utf8(&mut buf).as_bytes());
                            }
                        }
                        _ => return Err("invalid escape".to_string()),
                    }
                }
                _ => bytes.push(b),
            }
        }
        String::from_utf8(bytes).map_err(|e| e.to_string())
    }
    fn parse_hex4(&mut self) -> Result<u32, String> {
        let mut val = 0u32;
        for _ in 0..4 {
            let b = self.next().ok_or("unexpected end")?;
            let digit = match b {
                b'0'..=b'9' => b - b'0',
                b'a'..=b'f' => b - b'a' + 10,
                b'A'..=b'F' => b - b'A' + 10,
                _ => return Err("invalid hex".to_string()),
            };
            val = val * 16 + digit as u32;
        }
        Ok(val)
    }
    fn parse_number(&mut self) -> Result<Value, String> {
        let start = self.pos;
        if self.peek() == Some(b'-') {
            self.pos += 1;
        }
        if self.peek() == Some(b'0') {
            self.pos += 1;
        } else if matches!(self.peek(), Some(b'1'..=b'9')) {
            while matches!(self.peek(), Some(b'0'..=b'9')) {
                self.pos += 1;
            }
        } else {
            return Err("invalid number".to_string());
        }
        // Spec says integers only. If there's a decimal or exponent, we don't handle.
        let num_str = std::str::from_utf8(&self.input[start..self.pos]).unwrap();
        num_str.parse::<i64>().map(Value::Number).map_err(|e| e.to_string())
    }
    fn parse_array(&mut self) -> Result<Value, String> {
        if self.next() != Some(b'[') {
            return Err("expected [".to_string());
        }
        let mut vec = Vec::new();
        self.skip_ws();
        if self.peek() == Some(b']') {
            self.pos += 1;
            return Ok(Value::Array(vec));
        }
        loop {
            self.skip_ws();
            let val = self.parse_value()?;
            vec.push(val);
            self.skip_ws();
            match self.next() {
                Some(b',') => continue,
                Some(b']') => break,
                _ => return Err("expected , or ]".to_string()),
            }
        }
        Ok(Value::Array(vec))
    }
    fn parse_object(&mut self) -> Result<Value, String> {
        if self.next() != Some(b'{') {
            return Err("expected {".to_string());
        }
        let mut map = BTreeMap::new();
        self.skip_ws();
        if self.peek() == Some(b'}') {
            self.pos += 1;
            return Ok(Value::Object(map));
        }
        loop {
            self.skip_ws();
            let key = self.parse_string()?;
            self.skip_ws();
            if self.next() != Some(b':') {
                return Err("expected :".to_string());
            }
            self.skip_ws();
            let val = self.parse_value()?;
            map.insert(key, val);
            self.skip_ws();
            match self.next() {
                Some(b',') => continue,
                Some(b'}') => break,
                _ => return Err("expected , or }".to_string()),
            }
        }
        Ok(Value::Object(map))
    }
}

enum Change {
    Add { path: String, value: Value },
    Remove { path: String, value: Value },
    Replace { path: String, before: Value, after: Value },
}

fn escape_key(s: &str) -> String {
    let mut out = String::new();
    for c in s.chars() {
        match c {
            '~' => out.push_str("~0"),
            '/' => out.push_str("~1"),
            _ => out.push(c),
        }
    }
    out
}

fn join_path(base: &str, key: &str) -> String {
    let escaped = escape_key(key);
    if base.is_empty() {
        format!("/{}", escaped)
    } else {
        format!("{}/{}", base, escaped)
    }
}

fn join_path_index(base: &str, index: usize) -> String {
    if base.is_empty() {
        format!("/{}", index)
    } else {
        format!("{}/{}", base, index)
    }
}

fn diff(before: &Value, after: &Value, path: &str, changes: &mut Vec<Change>) {
    if before == after {
        return;
    }
    match (before, after) {
        (Value::Object(b), Value::Object(a)) => {
            let mut b_iter = b.iter();
            let mut a_iter = a.iter();
            let mut b_next = b_iter.next();
            let mut a_next = a_iter.next();
            while b_next.is_some() || a_next.is_some() {
                match (b_next, a_next) {
                    (Some((k, v)), Some((k2, v2))) => {
                        if k == k2 {
                            let new_path = join_path(path, k);
                            diff(v, v2, &new_path, changes);
                            b_next = b_iter.next();
                            a_next = a_iter.next();
                        } else if k < k2 {
                            let new_path = join_path(path, k);
                            changes.push(Change::Remove { path: new_path, value: v.clone() });
                            b_next = b_iter.next();
                        } else {
                            let new_path = join_path(path, k2);
                            changes.push(Change::Add { path: new_path, value: v2.clone() });
                            a_next = a_iter.next();
                        }
                    }
                    (Some((k, v)), None) => {
                        let new_path = join_path(path, k);
                        changes.push(Change::Remove { path: new_path, value: v.clone() });
                        b_next = b_iter.next();
                    }
                    (None, Some((k2, v2))) => {
                        let new_path = join_path(path, k2);
                        changes.push(Change::Add { path: new_path, value: v2.clone() });
                        a_next = a_iter.next();
                    }
                    (None, None) => break,
                }
            }
        }
        (Value::Array(b), Value::Array(a)) => {
            let max_len = b.len().max(a.len());
            for i in 0..max_len {
                let new_path = join_path_index(path, i);
                if i < b.len() && i < a.len() {
                    diff(&b[i], &a[i], &new_path, changes);
                } else if i < b.len() {
                    changes.push(Change::Remove { path: new_path, value: b[i].clone() });
                } else {
                    changes.push(Change::Add { path: new_path, value: a[i].clone() });
                }
            }
        }
        _ => {
            changes.push(Change::Replace {
                path: path.to_string(),
                before: before.clone(),
                after: after.clone(),
            });
        }
    }
}

fn write_string(s: &mut String, st: &str) {
    s.push('"');
    for c in st.chars() {
        match c {
            '"' => s.push_str("\\\""),
            '\\' => s.push_str("\\\\"),
            '\n' => s.push_str("\\n"),
            '\r' => s.push_str("\\r"),
            '\t' => s.push_str("\\t"),
            '\u{08}' => s.push_str("\\b"),
            '\u{0C}' => s.push_str("\\f"),
            c if c < ' ' => {
                s.push_str(&format!("\\u{:04x}", c as u32));
            }
            _ => s.push(c),
        }
    }
    s.push('"');
}

fn write_value(s: &mut String, v: &Value) {
    match v {
        Value::Null => s.push_str("null"),
        Value::Bool(b) => s.push_str(if *b { "true" } else { "false" }),
        Value::Number(n) => s.push_str(&n.to_string()),
        Value::String(st) => write_string(s, st),
        Value::Array(arr) => {
            s.push('[');
            for (i, item) in arr.iter().enumerate() {
                if i > 0 {
                    s.push(',');
                }
                write_value(s, item);
            }
            s.push(']');
        }
        Value::Object(map) => {
            s.push('{');
            for (i, (k, val)) in map.iter().enumerate() {
                if i > 0 {
                    s.push(',');
                }
                write_string(s, k);
                s.push(':');
                write_value(s, val);
            }
            s.push('}');
        }
    }
}

fn write_change(s: &mut String, c: &Change) {
    match c {
        Change::Add { path, value } => {
            s.push_str("{\"op\":\"add\",\"path\":");
            write_string(s, path);
            s.push_str(",\"value\":");
            write_value(s, value);
            s.push('}');
        }
        Change::Remove { path, value } => {
            s.push_str("{\"op\":\"remove\",\"path\":");
            write_string(s, path);
            s.push_str(",\"value\":");
            write_value(s, value);
            s.push('}');
        }
        Change::Replace { path, before, after } => {
            s.push_str("{\"op\":\"replace\",\"path\":");
            write_string(s, path);
            s.push_str(",\"before\":");
            write_value(s, before);
            s.push_str(",\"after\":");
            write_value(s, after);
            s.push('}');
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
        if line.trim().is_empty() {
            continue;
        }
        let mut parser = Parser::new(&line);
        let value = match parser.parse_value() {
            Ok(v) => v,
            Err(_) => {
                // On error, output empty changes
                let _ = out.write_all(b"{\"changes\":[]}\n");
                continue;
            }
        };
        if let Value::Object(map) = value {
            let before = map.get("before").cloned().unwrap_or(Value::Null);
            let after = map.get("after").cloned().unwrap_or(Value::Null);
            let mut changes = Vec::new();
            diff(&before, &after, "", &mut changes);
            let mut s = String::new();
            s.push_str("{\"changes\":[");
            for (i, c) in changes.iter().enumerate() {
                if i > 0 {
                    s.push(',');
                }
                write_change(&mut s, c);
            }
            s.push_str("]}\n");
            let _ = out.write_all(s.as_bytes());
        } else {
            let _ = out.write_all(b"{\"changes\":[]}\n");
        }
    }
}
