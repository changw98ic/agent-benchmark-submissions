use std::io::{self, BufRead, Write};

const MAX_MS: i64 = 99*3600*1000 + 59*60*1000 + 59*1000 + 999;

#[derive(Debug)]
enum SrtError {
    InvalidSrt,
    InvalidTime,
    TimeRange,
}

struct Entry {
    start: i64,
    end: i64,
    body: Vec<String>,
}

fn main() {
    let stdin = io::stdin();
    let stdout = io::stdout();
    let mut out = io::BufWriter::new(stdout.lock());
    for line in stdin.lock().lines() {
        let line = match line {
            Ok(l) => l,
            Err(_) => continue, // or break?
        };
        // parse JSON
        match parse_request(&line) {
            Some((srt, offset)) => {
                let res = process_srt(&srt, offset);
                match res {
                    Ok((out_srt, overlaps)) => {
                        // write JSON
                        let escaped = json_escape(&out_srt);
                        write!(out, "{{\"srt\":\"{}\",\"overlaps\":[", escaped).unwrap();
                        for (idx, (i,j)) in overlaps.iter().enumerate() {
                            if idx > 0 { write!(out, ",").unwrap(); }
                            write!(out, "[{},{}]", i, j).unwrap();
                        }
                        writeln!(out, "]}}").unwrap();
                    }
                    Err(e) => {
                        let err = match e {
                            SrtError::InvalidSrt => "INVALID_SRT",
                            SrtError::InvalidTime => "INVALID_TIME",
                            SrtError::TimeRange => "TIME_RANGE",
                        };
                        writeln!(out, "{{\"error\":\"{}\"}}", err).unwrap();
                    }
                }
            }
            None => {
                writeln!(out, "INVALID_JSON").unwrap();
            }
        }
    }
}
