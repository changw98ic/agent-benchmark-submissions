import sys
import json

INVALID_RULE = "INVALID_RULE"
DUPLICATE_SOURCE = "DUPLICATE_SOURCE"
INVALID_NAME = "INVALID_NAME"
COLLISION = "COLLISION"
TARGET_EXISTS = "TARGET_EXISTS"

class InvalidRule(Exception):
    pass

def split_ext(name):
    if not name:
        return "", ""
    idx = name.rfind(".")
    if idx > 0:
        return name[:idx], name[idx:]
    return name, ""

def handle(req):
    files = req.get("files")
    rule = req.get("rule")
    if not isinstance(files, list) or not isinstance(rule, dict):
        return {"error": INVALID_RULE}
    typ = rule.get("type")
    if typ == "prefix":
        if not isinstance(rule.get("value"), str):
            return {"error": INVALID_RULE}
    elif typ == "replace":
        old = rule.get("old")
        new = rule.get("new")
        if not isinstance(old, str) or not isinstance(new, str) or old == "":
            return {"error": INVALID_RULE}
    elif typ == "number":
        prefix = rule.get("prefix")
        width = rule.get("width")
        if not isinstance(prefix, str) or not isinstance(width, int) or width < 1 or width > 6:
            return {"error": INVALID_RULE}
    else:
        return {"error": INVALID_RULE}

    if len(set(files)) != len(files):
        return {"error": DUPLICATE_SOURCE}

    sorted_files = sorted(files)
    entries = []
    for i, name in enumerate(sorted_files, start=1):
        body, ext = split_ext(name)
        if typ == "prefix":
            new_body = rule["value"] + body
        elif typ == "replace":
            new_body = body.replace(rule["old"], rule["new"])
        else:
            new_body = rule["prefix"] + str(i).zfill(rule["width"])
        target = new_body + ext
        if target == "" or target == "." or target == ".." or "/" in target or "\\" in target or "\x00" in target:
            return {"error": INVALID_NAME}
        entries.append({"from": name, "to": target})

    seen = set()
    for e in entries:
        if e["to"] in seen:
            return {"error": COLLISION}
        seen.add(e["to"])

    source_set = set(sorted_files)
    for e in entries:
        if e["to"] != e["from"] and e["to"] in source_set:
            return {"error": TARGET_EXISTS}

    return {"plan": entries}

def main():
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        try:
            req = json.loads(line)
        except json.JSONDecodeError:
            continue
        resp = handle(req)
        sys.stdout.write(json.dumps(resp, ensure_ascii=False) + "\n")

if __name__ == "__main__":
    main()
