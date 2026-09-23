use std::collections::BTreeMap;
use std::io::{self, BufRead, Write};

#[derive(Clone, Debug, PartialEq)]
enum JsonValue {
    Null,
    Bool(bool),
    Number(i64),
    String(String),
    Array(Vec<JsonValue>),
    Object(BTreeMap<String, JsonValue>),
}

struct Parser<'a> {
    bytes: &'a [u8],
    pos: usize,
}

impl<'a> Parser<'a> {
    fn new(s: &'a str) -> Self {
        Parser { bytes: s.as_bytes(), pos: 0 }
    }

    fn parse_value(&mut self) -> Result<JsonValue, ()> {
        self.skip_ws();
        if self.pos >= self.bytes.len() {
            return Err(());
        }
        match self.bytes[self.pos] {
            b'n' => {
                self.expect_literal("null")?;
                Ok(JsonValue::Null)
            }
            b't' => {
                self.expect_literal("true")?;
                Ok(JsonValue::Bool(true))
            }
            b'f' => {
                self.expect_literal("false")?;
                Ok(JsonValue::Bool(false))
            }
            b'"' => {
                let s = self.parse_string()?;
                Ok(JsonValue::String(s))
            }
            b'[' => self.parse_array(),
            b'{' => self.parse_object(),
            b'-' | b'0'..=b'9' => self.parse_number(),
            _ => Err(()),
        }
    }

    fn skip_ws(&mut self) {
        while self.pos < self.bytes.len() {
            match self.bytes[self.pos] {
                b' ' | b'\n' | b'\r' | b'\t' => self.pos += 1,
                _ => break,
            }
        }
    }

    fn expect_literal(&mut self, lit: &str) -> Result<(), ()> {
        let b = lit.as_bytes();
        if self.pos + b.len() <= self.bytes.len() && &self.bytes[self.pos..self.pos + b.len()] == b {
            self.pos += b.len();
            Ok(())
        } else {
            Err(())
        }
    }

    fn parse_string(&mut self) -> Result<String, ()> {
        if self.bytes[self.pos] != b'"' {
            return Err(());
        }
        self.pos += 1;
        let mut out = String::new();
        loop {
            if self.pos >= self.bytes.len() {
                return Err(());
            }
            let c = self.bytes[self.pos];
            if c == b'"' {
                self.pos += 1;
                return Ok(out);
            } else if c == b'\\' {
                self.pos += 1;
                if self.pos >= self.bytes.len() {
                    return Err(());
                }
                let esc = self.bytes[self.pos];
                self.pos += 1;
                match esc {
                    b'"' => out.push('"'),
                    b'\\' => out.push('\\'),
                    b'/' => out.push('/'),
                    b'b' => out.push('\u{0008}'),
                    b'f' => out.push('\u{000C}'),
                    b'n' => out.push('\n'),
                    b'r' => out.push('\r'),
                    b't' => out.push('\t'),
                    b'u' => {
                        let cp = self.parse_hex4()?;
                        if (0xD800..=0xDBFF).contains(&cp) {
                            if self.pos + 2 <= self.bytes.len()
                                && self.bytes[self.pos] == b'\\'
                                && self.bytes[self.pos + 1] == b'u'
                            {
                                self.pos += 2;
                                let low = self.parse_hex4()?;
                                if (0xDC00..=0xDFFF).contains(&low) {
                                    let c = 0x10000 + ((cp - 0xD800) << 10) + (low - 0xDC00);
                                    out.push(char::from_u32(c).ok_or(())?);
                                } else {
                                    return Err(());
                                }
                            } else {
                                return Err(());
                            }
                        } else if (0xDC00..=0xDFFF).contains(&cp) {
                            return Err(());
                        } else {
                            out.push(char::from_u32(cp).ok_or(())?);
                        }
                    }
                    _ => return Err(()),
                }
            } else if c < 0x20 {
                return Err(());
            } else {
                let start = self.pos;
                while self.pos < self.bytes.len() {
                    let b = self.bytes[self.pos];
                    if b == b'"' || b == b'\\' {
                        break;
                    }
                    if b < 0x20 {
                        return Err(());
                    }
                    self.pos += 1;
                }
                let s = std::str::from_utf8(&self.bytes[start..self.pos]).map_err(|_| ())?;
                out.push_str(s);
            }
        }
    }

    fn parse_hex4(&mut self) -> Result<u32, ()> {
        if self.pos + 4 > self.bytes.len() {
            return Err(());
        }
        let mut val = 0u32;
        for _ in 0..4 {
            let b = self.bytes[self.pos];
            let d = match b {
                b'0'..=b'9' => (b - b'0') as u32,
                b'a'..=b'f' => (b - b'a' + 10) as u32,
                b'A'..=b'F' => (b - b'A' + 10) as u32,
                _ => return Err(()),
            };
            val = val * 16 + d;
            self.pos += 1;
        }
        Ok(val)
    }

    fn parse_number(&mut self) -> Result<JsonValue, ()> {
        let start = self.pos;
        if self.bytes[self.pos] == b'-' {
            self.pos += 1;
        }
        if self.pos >= self.bytes.len() {
            return Err(());
        }
        if self.bytes[self.pos] == b'0' {
            self.pos += 1;
        } else if self.bytes[self.pos].is_ascii_digit() {
            while self.pos < self.bytes.len() && self.bytes[self.pos].is_ascii_digit() {
                self.pos += 1;
            }
        } else {
            return Err(());
        }
        let s = std::str::from_utf8(&self.bytes[start..self.pos]).map_err(|_| ())?;
        let num: i64 = s.parse().map_err(|_| ())?;
        Ok(JsonValue::Number(num))
    }

    fn parse_array(&mut self) -> Result<JsonValue, ()> {
        self.pos += 1;
        let mut arr = Vec::new();
        self.skip_ws();
        if self.pos < self.bytes.len() && self.bytes[self.pos] == b']' {
            self.pos += 1;
            return Ok(JsonValue::Array(arr));
        }
        loop {
            let val = self.parse_value()?;
            arr.push(val);
            self.skip_ws();
            if self.pos >= self.bytes.len() {
                return Err(());
            }
            match self.bytes[self.pos] {
                b',' => {
                    self.pos += 1;
                    self.skip_ws();
                }
                b']' => {
                    self.pos += 1;
                    break;
                }
                _ => return Err(()),
            }
        }
        Ok(JsonValue::Array(arr))
    }

    fn parse_object(&mut self) -> Result<JsonValue, ()> {
        self.pos += 1;
        let mut map = BTreeMap::new();
        self.skip_ws();
        if self.pos < self.bytes.len() && self.bytes[self.pos] == b'}' {
            self.pos += 1;
            return Ok(JsonValue::Object(map));
        }
        loop {
            self.skip_ws();
            if self.pos >= self.bytes.len() || self.bytes[self.pos] != b'"' {
                return Err(());
            }
            let key = self.parse_string()?;
            self.skip_ws();
            if self.pos >= self.bytes.len() || self.bytes[self.pos] != b':' {
                return Err(());
            }
            self.pos += 1;
            let val = self.parse_value()?;
            map.insert(key, val);
            self.skip_ws();
            if self.pos >= self.bytes.len() {
                return Err(());
            }
            match self.bytes[self.pos] {
                b',' => {
                    self.pos += 1;
                    self.skip_ws();
                }
                b'}' => {
                    self.pos += 1;
                    break;
                }
                _ => return Err(()),
            }
        }
        Ok(JsonValue::Object(map))
    }
}

fn parse_json(s: &str) -> Result<JsonValue, ()> {
    let mut p = Parser::new(s);
    let v = p.parse_value()?;
    p.skip_ws();
    if p.pos != p.bytes.len() {
        return Err(());
    }
    Ok(v)
}

fn escape_json_string(s: &str, out: &mut String) {
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
            c if c < '\u{0020}' => {
                let hex = format!("\\u{:04x}", c as u32);
                out.push_str(&hex);
            }
            _ => out.push(c),
        }
    }
    out.push('"');
}

fn serialize_json(v: &JsonValue, out: &mut String) {
    match v {
        JsonValue::Null => out.push_str("null"),
        JsonValue::Bool(b) => out.push_str(if *b { "true" } else { "false" }),
        JsonValue::Number(n) => out.push_str(&n.to_string()),
        JsonValue::String(s) => escape_json_string(s, out),
        JsonValue::Array(arr) => {
            out.push('[');
            for (i, item) in arr.iter().enumerate() {
                if i > 0 {
                    out.push(',');
                }
                serialize_json(item, out);
            }
            out.push(']');
        }
        JsonValue::Object(map) => {
            out.push('{');
            let mut first = true;
            for (k, v) in map {
                if !first {
                    out.push(',');
                }
                first = false;
                escape_json_string(k, out);
                out.push(':');
                serialize_json(v, out);
            }
            out.push('}');
        }
    }
}

fn escape_path_segment(s: &str) -> String {
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

enum Change {
    Add { path: String, value: JsonValue },
    Remove { path: String, value: JsonValue },
    Replace { path: String, before: JsonValue, after: JsonValue },
}

fn serialize_change(c: &Change, out: &mut String) {
    match c {
        Change::Add { path, value } => {
            out.push_str("{\"op\":\"add\",\"path\":");
            escape_json_string(path, out);
            out.push_str(",\"value\":");
            serialize_json(value, out);
            out.push('}');
        }
        Change::Remove { path, value } => {
            out.push_str("{\"op\":\"remove\",\"path\":");
            escape_json_string(path, out);
            out.push_str(",\"value\":");
            serialize_json(value, out);
            out.push('}');
        }
        Change::Replace { path, before, after } => {
            out.push_str("{\"op\":\"replace\",\"path\":");
            escape_json_string(path, out);
            out.push_str(",\"before\":");
            serialize_json(before, out);
            out.push_str(",\"after\":");
            serialize_json(after, out);
            out.push('}');
        }
    }
}

fn diff(before: &JsonValue, after: &JsonValue, path: &str, changes: &mut Vec<Change>) {
    if std::mem::discriminant(before) != std::mem::discriminant(after) {
        changes.push(Change::Replace {
            path: path.to_string(),
            before: before.clone(),
            after: after.clone(),
        });
        return;
    }
    match (before, after) {
        (JsonValue::Null, JsonValue::Null) => {}
        (JsonValue::Bool(a), JsonValue::Bool(b)) => {
            if a != b {
                changes.push(Change::Replace {
                    path: path.to_string(),
                    before: before.clone(),
                    after: after.clone(),
                });
            }
        }
        (JsonValue::Number(a), JsonValue::Number(b)) => {
            if a != b {
                changes.push(Change::Replace {
                    path: path.to_string(),
                    before: before.clone(),
                    after: after.clone(),
                });
            }
        }
        (JsonValue::String(a), JsonValue::String(b)) => {
            if a != b {
                changes.push(Change::Replace {
                    path: path.to_string(),
                    before: before.clone(),
                    after: after.clone(),
                });
            }
        }
        (JsonValue::Array(a), JsonValue::Array(b)) => {
            let max = a.len().max(b.len());
            for i in 0..max {
                let child_path = format!("{}/{}", path, i);
                if i < a.len() && i < b.len() {
                    diff(&a[i], &b[i], &child_path, changes);
                } else if i < a.len() {
                    changes.push(Change::Remove {
                        path: child_path,
                        value: a[i].clone(),
                    });
                } else {
                    changes.push(Change::Add {
                        path: child_path,
                        value: b[i].clone(),
                    });
                }
            }
        }
        (JsonValue::Object(a), JsonValue::Object(b)) => {
            let mut keys: Vec<&String> = a.keys().chain(b.keys()).collect();
            keys.sort();
            keys.dedup();
            for key in keys {
                let child_path = format!("{}/{}", path, escape_path_segment(key));
                match (a.get(key), b.get(key)) {
                    (Some(av), Some(bv)) => diff(av, bv, &child_path, changes),
                    (Some(av), None) => changes.push(Change::Remove {
                        path: child_path,
                        value: av.clone(),
                    }),
                    (None, Some(bv)) => changes.push(Change::Add {
                        path: child_path,
                        value: bv.clone(),
                    }),
                    (None, None) => unreachable!(),
                }
            }
        }
        _ => unreachable!(),
    }
}

fn main() {
    let stdin = io::stdin();
    let mut stdout = io::BufWriter::new(io::stdout());
    for line in stdin.lock().lines() {
        match line {
            Ok(line) => {
                match parse_json(&line) {
                    Ok(JsonValue::Object(obj)) => {
                        let before = obj.get("before");
                        let after = obj.get("after");
                        match (before, after) {
                            (Some(b), Some(a)) => {
                                let mut changes = Vec::new();
                                diff(b, a, "", &mut changes);
                                let mut out = String::from("{\"changes\":[");
                                for (i, c) in changes.iter().enumerate() {
                                    if i > 0 {
                                        out.push(',');
                                    }
                                    serialize_change(c, &mut out);
                                }
                                out.push_str("]}");
                                writeln!(stdout, "{}", out).unwrap();
                            }
                            _ => {
                                writeln!(stdout, "INVALID_JSON").unwrap();
                            }
                        }
                    }
                    _ => {
                        writeln!(stdout, "INVALID_JSON").unwrap();
                    }
                }
            }
            Err(_) => break,
        }
    }
}
