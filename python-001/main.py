import sys
import json
import re
from datetime import datetime

TZ_RE = re.compile(r'(Z|[+-]\d{2}:\d{2})$')

def parse_ts(s):
    if not isinstance(s, str):
        raise ValueError("not string")
    if not TZ_RE.search(s):
        raise ValueError("no timezone")
    if s.endswith('Z'):
        s = s[:-1] + '+00:00'
    dt = datetime.fromisoformat(s)
    if dt.tzinfo is None:
        raise ValueError("no tzinfo")
    return dt

def process_request(req):
    text = req.get("text", "")
    if not isinstance(text, str):
        text = ""
    start_s = req.get("start")
    end_s = req.get("end")
    start = None
    end = None
    if start_s is not None:
        try:
            start = parse_ts(start_s)
        except Exception:
            return "TIME_RANGE"
    if end_s is not None:
        try:
            end = parse_ts(end_s)
        except Exception:
            return "TIME_RANGE"
    if start is not None and end is not None and start >= end:
        return "TIME_RANGE"

    levels = req.get("levels")
    if isinstance(levels, list) and len(levels) > 0:
        levels_set = set(levels)
    else:
        levels_set = None

    keyword = req.get("keyword")
    if not isinstance(keyword, str) or keyword == "":
        keyword = None

    if text == "":
        lines = []
    else:
        lines = text.split("\n")
        if lines and lines[-1] == "":
            lines.pop()

    read = len(lines)
    valid = 0
    bad_lines = []
    matched_lines = []
    errors = {}

    for i, line in enumerate(lines, 1):
        try:
            rec = json.loads(line)
        except Exception:
            bad_lines.append(i)
            continue
        if not isinstance(rec, dict):
            bad_lines.append(i)
            continue
        try:
            ts = parse_ts(rec.get("timestamp"))
        except Exception:
            bad_lines.append(i)
            continue
        level = rec.get("level")
        if level not in ("DEBUG", "INFO", "WARN", "ERROR"):
            bad_lines.append(i)
            continue
        msg = rec.get("message")
        if not isinstance(msg, str):
            bad_lines.append(i)
            continue
        et = rec.get("error_type", None)
        if et is not None and not isinstance(et, str):
            bad_lines.append(i)
            continue

        valid += 1

        if start is not None and ts < start:
            continue
        if end is not None and ts >= end:
            continue
        if levels_set is not None and level not in levels_set:
            continue
        if keyword is not None and keyword not in msg:
            continue

        matched_lines.append(i)
        if level == "ERROR":
            typ = et if isinstance(et, str) and et != "" else "unknown"
            errors[typ] = errors.get(typ, 0) + 1

    return {
        "read": read,
        "valid": valid,
        "bad_lines": bad_lines,
        "matched_lines": matched_lines,
        "errors": errors,
    }

def main():
    for raw_line in sys.stdin:
        line = raw_line.rstrip("\n")
        if line == "":
            continue
        try:
            req = json.loads(line)
        except Exception:
            # In case of invalid request line, skip? Or output error?
            # Protocol guarantees valid requests.
            continue
        res = process_request(req)
        sys.stdout.write(json.dumps(res, ensure_ascii=False) + "\n")

if __name__ == "__main__":
    main()
