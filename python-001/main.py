import sys
import json
from datetime import datetime

LEVELS = {"DEBUG", "INFO", "WARN", "ERROR"}

def parse_ts(ts):
    if not isinstance(ts, str):
        return None
    if ts.endswith("Z"):
        pass
    else:
        if len(ts) < 6:
            return None
        suffix = ts[-6:]
        if suffix[0] not in "+-" or suffix[3] != ":":
            return None
        if not (suffix[1:3].isdigit() and suffix[4:6].isdigit()):
            return None
    try:
        dt = datetime.fromisoformat(ts)
    except Exception:
        return None
    if dt.tzinfo is None or dt.utcoffset() is None:
        return None
    return dt

def process(req):
    text = req.get("text")
    if not isinstance(text, str):
        return {"error": "INVALID_REQUEST"}
    start_raw = req.get("start")
    end_raw = req.get("end")
    start_dt = None
    if start_raw is not None:
        start_dt = parse_ts(start_raw)
        if start_dt is None:
            return {"error": "TIME_RANGE"}
    end_dt = None
    if end_raw is not None:
        end_dt = parse_ts(end_raw)
        if end_dt is None:
            return {"error": "TIME_RANGE"}
    if start_dt is not None and end_dt is not None and start_dt >= end_dt:
        return {"error": "TIME_RANGE"}
    
    levels = req.get("levels")
    level_filter = None
    if levels is not None:
        if not isinstance(levels, list):
            return {"error": "INVALID_REQUEST"}
        if len(levels) > 0:
            level_filter = set(levels)
    keyword = req.get("keyword")
    keyword_filter = None
    if keyword is not None:
        if not isinstance(keyword, str):
            return {"error": "INVALID_REQUEST"}
        if keyword != "":
            keyword_filter = keyword
    
    if text == "":
        lines = []
    else:
        lines = text.splitlines()
    read = len(lines)
    valid = 0
    bad_lines = []
    matched_lines = []
    errors = {}
    for idx, line in enumerate(lines, 1):
        try:
            obj = json.loads(line)
        except Exception:
            bad_lines.append(idx)
            continue
        if not isinstance(obj, dict):
            bad_lines.append(idx)
            continue
        ts = parse_ts(obj.get("timestamp"))
        if ts is None:
            bad_lines.append(idx)
            continue
        lvl = obj.get("level")
        if not isinstance(lvl, str) or lvl not in LEVELS:
            bad_lines.append(idx)
            continue
        msg = obj.get("message")
        if not isinstance(msg, str):
            bad_lines.append(idx)
            continue
        et_raw = obj.get("error_type")
        if et_raw is None:
            et = "unknown"
        elif isinstance(et_raw, str):
            et = et_raw if et_raw != "" else "unknown"
        else:
            bad_lines.append(idx)
            continue
        valid += 1
        if start_dt is not None and ts < start_dt:
            continue
        if end_dt is not None and ts >= end_dt:
            continue
        if level_filter is not None and lvl not in level_filter:
            continue
        if keyword_filter is not None and keyword_filter not in msg:
            continue
        matched_lines.append(idx)
        if lvl == "ERROR":
            errors[et] = errors.get(et, 0) + 1
    return {
        "read": read,
        "valid": valid,
        "bad_lines": bad_lines,
        "matched_lines": matched_lines,
        "errors": errors
    }

def main():
    for line in sys.stdin:
        line = line.rstrip("\n")
        if line == "":
            print(json.dumps({"error": "INVALID_REQUEST"}))
            continue
        try:
            req = json.loads(line)
        except Exception:
            print(json.dumps({"error": "INVALID_REQUEST"}))
            continue
        if not isinstance(req, dict):
            print(json.dumps({"error": "INVALID_REQUEST"}))
            continue
        try:
            resp = process(req)
        except Exception:
            resp = {"error": "INVALID_REQUEST"}
        print(json.dumps(resp, ensure_ascii=False))

if __name__ == "__main__":
    main()
