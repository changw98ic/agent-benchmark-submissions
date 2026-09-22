import json
import sys
from datetime import datetime

VALID_LEVELS = {"DEBUG", "INFO", "WARN", "ERROR"}


def parse_iso(value):
    """Parse an ISO 8601 string that must carry a timezone offset.
    Return an aware datetime, or None if invalid/naive."""
    if not isinstance(value, str):
        return None
    s = value
    if s.endswith(("Z", "z")):
        s = s[:-1] + "+00:00"
    try:
        dt = datetime.fromisoformat(s)
    except ValueError:
        return None
    if dt.tzinfo is None:
        return None
    return dt


def parse_record(line):
    """Validate one physical log line.
    Return (timestamp, level, message, error_type) or None for a bad line."""
    try:
        obj = json.loads(line)
    except ValueError:
        return None
    if not isinstance(obj, dict):
        return None
    ts = parse_iso(obj.get("timestamp"))
    if ts is None:
        return None
    level = obj.get("level")
    if not isinstance(level, str) or level not in VALID_LEVELS:
        return None
    message = obj.get("message")
    if not isinstance(message, str):
        return None
    error_type = obj.get("error_type")
    if error_type is None or error_type == "":
        error_type = "unknown"
    elif not isinstance(error_type, str):
        return None
    return ts, level, message, error_type


def split_physical_lines(text):
    """Split on newlines; a single trailing newline does not add a line."""
    if not text:
        return []
    lines = text.split("\n")
    if lines and lines[-1] == "":
        lines.pop()
    return lines


def handle_request(req):
    if not isinstance(req, dict):
        return {"error": "BAD_REQUEST"}
    text = req.get("text")
    if not isinstance(text, str):
        text =
