enum CsvState {
    StartField,
    InUnquoted,
    InQuoted,
    AfterQuote,
}

fn parse_csv(input: &str) -> Result<Vec<Vec<String>>, ()> {
    let mut bytes = input.as_bytes();
    if bytes.starts_with(&[0xEF,0xBB,0xBF]) {
        bytes = &bytes[3..];
    }
    let mut records = Vec::new();
    let mut record: Vec<String> = Vec::new();
    let mut field: Vec<u8> = Vec::new();
    let mut state = CsvState::StartField;
    let mut i = 0;
    while i < bytes.len() {
        let b = bytes[i];
        match state {
            CsvState::StartField => {
                if b == b'"' {
                    state = CsvState::InQuoted;
                    i += 1;
                } else if b == b',' {
                    record.push(String::new()); // empty field
                    i += 1;
                } else if b == b'\n' {
                    record.push(String::new());
                    records.push(std::mem::take(&mut record));
                    i += 1;
                } else if b == b'\r' && i+1 < bytes.len() && bytes[i+1] == b'\n' {
                    record.push(String::new());
                    records.push(std::mem::take(&mut record));
                    i += 2;
                } else {
                    field.push(b);
                    state = CsvState::InUnquoted;
                    i += 1;
                }
            }
            CsvState::InUnquoted => {
                if b == b',' {
                    record.push(String::from_utf8(std::mem::take(&mut field)).unwrap());
                    state = CsvState::StartField;
                    i += 1;
                } else if b == b'\n' {
                    record.push(String::from_utf8(std::mem::take(&mut field)).unwrap());
                    records.push(std::mem::take(&mut record));
                    state = CsvState::StartField;
                    i += 1;
                } else if b == b'\r' && i+1 < bytes.len() && bytes[i+1] == b'\n' {
                    record.push(String::from_utf8(std::mem::take(&mut field)).unwrap());
                    records.push(std::mem::take(&mut record));
                    state = CsvState::StartField;
                    i += 2;
                } else {
                    field.push(b);
                    i += 1;
                }
            }
            CsvState::InQuoted => {
                if b == b'"' {
                    if i+1 < bytes.len() && bytes[i+1] == b'"' {
                        field.push(b'"');
                        i += 2;
                    } else {
                        state = CsvState::AfterQuote;
                        i += 1;
                    }
                } else {
                    field.push(b);
                    i += 1;
                }
            }
            CsvState::AfterQuote => {
                if b == b',' {
                    record.push(String::from_utf8(std::mem::take(&mut field)).unwrap());
                    state = CsvState::StartField;
                    i += 1;
                } else if b == b'\n' {
                    record.push(String::from_utf8(std::mem::take(&mut field)).unwrap());
                    records.push(std::mem::take(&mut record));
                    state = CsvState::StartField;
                    i += 1;
                } else if b == b'\r' && i+1 < bytes.len() && bytes[i+1] == b'\n' {
                    record.push(String::from_utf8(std::mem::take(&mut field)).unwrap());
                    records.push(std::mem::take(&mut record));
                    state = CsvState::StartField;
                    i += 2;
                } else {
                    // Invalid char after closing quote. Could be space? 
                    // For strict, return Err. But maybe allow? We'll return Err.
                    return Err(());
                }
            }
        }
    }
    // EOF
    match state {
        CsvState::InQuoted => return Err(()),
        CsvState::StartField => {
            if !record.is_empty() || !field.is_empty() {
                record.push(String::from_utf8(field).unwrap());
                records.push(record);
            }
        }
        CsvState::InUnquoted | CsvState::AfterQuote => {
            record.push(String::from_utf8(field).unwrap());
            records.push(record);
        }
    }
    Ok(records)
}
