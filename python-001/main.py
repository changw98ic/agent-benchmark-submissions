import sys
import json
from datetime import datetime

def parse_timestamp(s):
    """Parse ISO 8601 timestamp with timezone. Returns datetime or None."""
    if not isinstance(s, str):
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
    start_str = req.get("start")
    end_str = req.get("end")
    levels = req.get("levels")
    keyword = req.get("keyword")
    
    # Parse time range
    start_dt = None
    end_dt = None
    
    if start_str is not None:
        start_dt = parse_timestamp(start_str)
        if start_dt is None:
            return {"read": 0, "valid": 0, "bad_lines": [], "matched_lines": [], "errors": {"TIME_RANGE": 1}}
    
    if end_str is not None:
        end_dt = parse_timestamp(end_str)
        if end_dt is None:
            return {"read": 0, "valid": 0, "bad_lines": [], "matched_lines": [], "errors": {"TIME_RANGE": 1}}
    
    if start_dt is not None and end_dt is not None and start_dt >= end_dt:
        return {"read": 0, "valid": 0, "bad_lines": [], "matched_lines": [], "errors": {"TIME_RANGE": 1}}
    
    # Split text into lines
    if not text:
        lines = []
    else:
        lines = text.split('\n')
        if lines and lines[-1] == '':
            lines.pop()
    
    read = len(lines)
    valid = 0
    bad_lines = []
    matched_lines = []
    errors = {}
    
    # Determine level filter
    level_filter = None
    if levels and isinstance(levels, list) and len(levels) > 0:
        level_filter = set(levels)
    
    # Determine keyword filter
    keyword_filter = None
    if keyword and isinstance(keyword, str) and len(keyword) > 0:
        keyword_filter = keyword
    
    for i, line in enumerate(lines):
        line_num = i + 1
        
        # Empty line is bad
        if not line.strip():
            bad_lines.append(line_num)
            continue
        
        # Parse JSON
        try:
            record = json.loads(line)
        except (json.JSONDecodeError, ValueError):
            bad_lines.append(line_num)
            continue
        
        # Must be object
        if not isinstance(record, dict):
            bad_lines.append(line_num)
            continue
        
        # Check required fields
        if 'timestamp' not in record or 'level' not in record or 'message' not in record:
            bad_lines.append(line_num)
            continue
        
        ts = record['timestamp']
        level = record['level']
        message = record['message']
        
        # Type checks
        if not isinstance(ts, str) or not isinstance(level, str) or not isinstance(message, str):
            bad_lines.append(line_num)
            continue
        
        # Level validity
        if level not in ('DEBUG', 'INFO', 'WARN', 'ERROR'):
            bad_lines.append(line_num)
            continue
        
        # Parse timestamp
        ts_dt = parse_timestamp(ts)
        if ts_dt is None:
            bad_lines.append(line_num)
            continue
        
        # error_type
        error_type = record.get('error_type')
        if error_type is not None and not isinstance(error_type, str):
            bad_lines.append(line_num)
            continue
        
        if error_type is None or error_type == '':
            error_type = 'unknown'
        
        # Valid record
        valid += 1
        
        # Apply filters
        match = True
        
        # Time range filter [start, end)
        if start_dt is not None and ts_dt < start_dt:
            match = False
        if end_dt is not None and ts_dt >= end_dt:
            match = False
        
        # Level filter
        if match and level_filter is not None and level not in level_filter:
            match = False
        
        # Keyword filter
        if match and keyword_filter is not None and keyword_filter not in message:
            match = False
        
        if match:
            matched_lines.append(line_num)
            if level == 'ERROR':
                errors[error_type] = errors.get(error_type, 0) + 1
    
    return {
        "read": read,
        "valid": valid,
        "bad_lines": bad_lines,
        "matched_lines": matched_lines,
        "errors": errors
    }

def main():
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        try:
            req = json.loads(line)
        except json.JSONDecodeError:
            # Invalid request JSON - output error?
            # Per schema, we should still output valid JSON
            print(json.dumps({"read": 0, "valid": 0, "bad_lines": [], "matched_lines": [], "errors": {}}))
            continue
        
        result = process_request(req)
        print(json.dumps(result))

if __name__ == "__main__":
    main()
