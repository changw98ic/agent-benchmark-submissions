import sys
import json
from datetime import datetime

def parse_timestamp(value):
    if not isinstance(value, str):
        return None
    # Check for Z or ±HH:MM suffix
    if value.endswith('Z'):
        pass
    elif len(value) >= 6 and value[-6] in ('+', '-') and value[-5:-3].isdigit() and value[-3] == ':' and value[-2:].isdigit():
        pass
    else:
        return None
    if value.endswith('Z'):
        iso = value[:-1] + '+00:00'
    else:
        iso = value
    try:
        dt = datetime.fromisoformat(iso)
    except ValueError:
        return None
    if dt.tzinfo is None:
        return None
    return dt

def split_lines(text):
    if text == "":
        return []
    lines = text.split("\n")
    if lines and lines[-1] == "":
        lines.pop()
    return lines

def process_request(req):
    # Validate start/end
    start = None
    end = None
    if "start" in req and req["start"] is not None:
        start = parse_timestamp(req["start"])
        if start is None:
            return {"error": "TIME_RANGE"}
    if "end" in req and req["end"] is not None:
        end = parse_timestamp(req["end"])
        if end is None:
            return {"error": "TIME_RANGE"}
    if start is not None and end is not None and start >= end:
        return {"error": "TIME_RANGE"}

    text = req.get("text", "")
    if not isinstance(text, str):
        # Should not happen per spec
        text = ""

    lines = split_lines(text)

    levels = req.get("levels")
    if levels is None or (isinstance(levels, list) and len(levels) == 0):
        level_set = None
    elif isinstance(levels, list):
        level_set = set(levels)
    else:
        level_set = None

    keyword = req.get("keyword")
    if keyword is None or keyword == "":
        keyword = None

    bad_lines = []
    matched_lines = []
    errors = {}
    valid_count = 0

    for idx, line in enumerate(lines, 1):
        try:
            obj = json.loads(line)
        except Exception:
            bad_lines.append(idx)
            continue
        if not isinstance(obj, dict):
            bad_lines.append(idx)
            continue
        if "timestamp" not in obj or "level" not in obj or "message" not in obj:
            bad_lines.append(idx)
            continue
        ts_val = obj["timestamp"]
        level = obj["level"]
        message = obj["message"]
        if not isinstance(ts_val, str) or not isinstance(level, str) or not isinstance(message, str):
            bad_lines.append(idx)
            continue
        if level not in ("DEBUG", "INFO", "WARN", "ERROR"):
            bad_lines.append(idx)
            continue
        dt = parse_timestamp(ts_val)
        if dt is None:
            bad_lines.append(idx)
            continue
        # optional error_type
        if "error_type" in obj:
            et = obj["error_type"]
            if et is not None and not isinstance(et, str):
                bad_lines.append(idx)
                continue
        else:
            et = None
        valid_count += 1
        # filters
        if start is not None and dt < start:
            continue
        if end is not None and dt >= end:
            continue
        if level_set is not None and level not in level_set:
            continue
        if keyword is not None and keyword not in message:
            continue
        matched_lines.append(idx)
        if level == "ERROR":
            if et is None or et == "":
                etype = "unknown"
            else:
                etype = et
            errors[etype] = errors.get(etype, 0) + 1

    return {
        "read": len(lines),
        "valid": valid_count,
        "bad_lines": bad_lines,
        "matched_lines": matched_lines,
        "errors": errors,
    }

def main():
    for raw in sys.stdin:
        line = raw.rstrip("\n")
        if line.endswith("\r"):
            line = line[:-1]
        try:
            req = json.loads(line)
        except Exception:
            sys.stdout.write("INVALID_JSON\n")
            continue
        if not isinstance(req, dict) or "text" not in req or not isinstance(req["text"], str):
            sys.stdout.write("INVALID_JSON\n")
            continue
        result = process_request(req)
        sys.stdout.write(json.dumps(result, ensure_ascii=False, separators=(",", ":")) + "\n")
    sys.stdout.flush()

if __name__ == "__main__":
    main()
