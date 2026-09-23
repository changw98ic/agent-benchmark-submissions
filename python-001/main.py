#!/usr/bin/env python3
import sys
import json
import re
from datetime import datetime

ISO_RE = re.compile(
    r'^\d{4}-\d{2}-\d{2}T\d{2}:\d{2}'
    r'(?::\d{2}(?:\.\d+)?)?'
    r'(?:Z|[+-]\d{2}:\d{2})$'
)
LEVELS = {"DEBUG", "INFO", "WARN", "ERROR"}

def parse_ts(s):
    if not isinstance(s, str):
        return None
    if not ISO_RE.match(s):
        return None
    try:
        dt = datetime.fromisoformat(s.replace('Z', '+00:00'))
    except ValueError:
        return None
    if dt.tzinfo is None:
        return None
    return dt

def split_text(text):
    if text == "":
        return []
    parts = text.split('\n')
    if parts and parts[-1] == '':
        parts.pop()
    return [p[:-1] if p.endswith('\r') else p for p in parts]

def parse_record(obj):
    if not isinstance(obj, dict):
        return None
    ts = obj.get("timestamp")
    dt = parse_ts(ts)
    if dt is None:
        return None
    level = obj.get("level")
    if not isinstance(level, str) or level not in LEVELS:
        return None
    msg = obj.get("message")
    if not isinstance(msg, str):
        return None
    et = obj.get("error_type")
    if "error_type" in obj and et is not None and not isinstance(et, str):
        return None
    return dt, level, msg, et

def process_request(req):
    if not isinstance(req, dict) or "text" not in req:
        return None  # signal invalid
    text = req["text"]
    if not isinstance(text, str):
        return None
    # parse time range
    start_dt = None
    end_dt = None
    if "start" in req:
        start_dt = parse_ts(req["start"])
        if start_dt is None:
            return {"error": "TIME_RANGE"}
    if "end" in req:
        end_dt = parse_ts(req["end"])
        if end_dt is None:
            return {"error": "TIME_RANGE"}
    if start_dt is not None and end_dt is not None and start_dt >= end_dt:
        return {"error": "TIME_RANGE"}
    
    levels = req.get("levels")
    levels_set = None
    if isinstance(levels, list) and len(levels) > 0:
        levels_set = set(levels)
    keyword = req.get("keyword")
    use_keyword = isinstance(keyword, str) and keyword != ""
    
    lines = split_text(text)
    read = len(lines)
    valid = 0
    bad_lines = []
    matched_lines = []
    errors = {}
    
    for idx, line in enumerate(lines, 1):
        if line == "":
            bad_lines.append(idx)
            continue
        try:
            obj = json.loads(line)
        except json.JSONDecodeError:
            bad_lines.append(idx)
            continue
        rec = parse_record(obj)
        if rec is None:
            bad_lines.append(idx)
            continue
        valid += 1
        dt, level, msg, et = rec
        # filters
        if start_dt is not None and dt < start_dt:
            continue
        if end_dt is not None and dt >= end_dt:
            continue
        if levels_set is not None and level not in levels_set:
            continue
        if use_keyword and keyword not in msg:
            continue
        matched_lines.append(idx)
        if level == "ERROR":
            key = et if isinstance(et, str) and et != "" else "unknown"
            errors[key] = errors.get(key, 0) + 1
    
    return {
        "read": read,
        "valid": valid,
        "bad_lines": bad_lines,
        "matched_lines": matched_lines,
        "errors": errors
    }

def main():
    for raw in sys.stdin:
        line = raw.rstrip('\n')
        if line.endswith('\r'):
            line = line[:-1]
        if not line:
            # empty line is malformed JSON
            print("INVALID_JSON")
            continue
        try:
            req = json.loads(line)
        except json.JSONDecodeError:
            print("INVALID_JSON")
            continue
        res = process_request(req)
        if res is None:
            print("INVALID_JSON")
        else:
            print(json.dumps(res, ensure_ascii=False, separators=(',', ':')))

if __name__ == "__main__":
    main()
