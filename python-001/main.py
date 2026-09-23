import sys
import json
import re
from datetime import datetime

TZ_REGEX = re.compile(r'[+-]\d{2}:\d{2}$')
VALID_LEVELS = {"DEBUG", "INFO", "WARN", "ERROR"}

def parse_timestamp(s):
    if not isinstance(s, str):
        return None
    if not (s.endswith('Z') or TZ_REGEX.search(s)):
        return None
    try:
        dt = datetime.fromisoformat(s)
    except ValueError:
        return None
    if dt.tzinfo is None:
        return None
    return dt

def process_request(req):
    text = req.get("text", "")
    start = req.get("start")
    end = req.get("end")
    levels = req.get("levels")
    keyword = req.get("keyword")

    start_dt = None
    end_dt = None
    if start is not None:
        start_dt = parse_timestamp(start)
        if start_dt is None:
            return {"error": "TIME_RANGE"}
    if end is not None:
        end_dt = parse_timestamp(end)
        if end_dt is None:
            return {"error": "TIME_RANGE"}
    if start_dt is not None and end_dt is not None and start_dt >= end_dt:
        return {"error": "TIME_RANGE"}

    if levels is None or (isinstance(levels, list) and len(levels) == 0):
        levels_filter = None
    else:
        # assume list of strings
        levels_filter = set(levels)

    if keyword is None or keyword == "":
        keyword_filter = None
    else:
        keyword_filter = keyword

    if not isinstance(text, str):
        # should not happen
        text = ""

    lines = text.splitlines()
    read = len(lines)
    valid = 0
    bad_lines = []
    matched_lines = []
    errors = {}

    for idx, line in enumerate(lines, start=1):
        try:
            record = json.loads(line)
        except json.JSONDecodeError:
            bad_lines.append(idx)
            continue
        if not isinstance(record, dict):
            bad_lines.append(idx)
            continue

        if "timestamp" not in record or "level" not in record or "message" not in record:
            bad_lines.append(idx)
            continue

        ts = record["timestamp"]
        level = record["level"]
        message = record["message"]

        if not isinstance(ts, str) or not isinstance(level, str) or not isinstance(message, str):
            bad_lines.append(idx)
            continue

        if level not in VALID_LEVELS:
            bad_lines.append(idx)
            continue

        dt = parse_timestamp(ts)
        if dt is None:
            bad_lines.append(idx)
            continue

        error_type = record.get("error_type")
        if "error_type" in record:
            if error_type is not None and not isinstance(error_type, str):
                bad_lines.append(idx)
                continue
        # else error_type is None

        valid += 1

        if start_dt is not None and dt < start_dt:
            continue
        if end_dt is not None and dt >= end_dt:
            continue
        if levels_filter is not None and level not in levels_filter:
            continue
        if keyword_filter is not None and keyword_filter not in message:
            continue

        matched_lines.append(idx)
        if level == "ERROR":
            et = error_type
            if et is None or et == "":
                et = "unknown"
            errors[et] = errors.get(et, 0) + 1

    return {
        "read": read,
        "valid": valid,
        "bad_lines": bad_lines,
        "matched_lines": matched_lines,
        "errors": errors,
    }

def main():
    for line in sys.stdin:
        line = line.rstrip('\n')
        if line.endswith('\r'):
            line = line[:-1]
        if not line:
            print("INVALID_JSON")
            continue
        try:
            req = json.loads(line)
        except json.JSONDecodeError:
            print("INVALID_JSON")
            continue
        if not isinstance(req, dict):
            print("INVALID_JSON")
            continue
        resp = process_request(req)
        print(json.dumps(resp, ensure_ascii=False))

if __name__ == "__main__":
    main()
