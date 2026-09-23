import sys
import json

INVALID_CHARS = ("/", "\\", "\x00")

def split_ext(name):
    idx = name.rfind(".")
    if idx > 0:
        return name[:idx], name[idx:]
    return name, ""

def process(req):
    files = req.get("files")
    rule = req.get("rule")
    if not isinstance(files, list) or not isinstance(rule, dict):
        return {"error": "INVALID_RULE"}
    if not all(isinstance(x, str) for x in files):
        return {"error": "INVALID_RULE"}
    rule_type = rule.get("type")
    if rule_type == "prefix":
        value = rule.get("value")
        if not isinstance(value, str):
            return {"error": "INVALID_RULE"}
    elif rule_type == "replace":
        old = rule.get("old")
        new = rule.get("new")
        if not isinstance(old, str) or not isinstance(new, str):
            return {"error": "INVALID_RULE"}
        if old == "":
            return {"error": "INVALID_RULE"}
    elif rule_type == "number":
        prefix = rule.get("prefix")
        width = rule.get("width")
        if not isinstance(prefix, str):
            return {"error": "INVALID_RULE"}
        if not isinstance(width, int) or isinstance(width, bool) or width < 1 or width > 6:
            return {"error": "INVALID_RULE"}
    else:
        return {"error": "INVALID_RULE"}

    if len(set(files)) != len(files):
        return {"error": "DUPLICATE_SOURCE"}

    sorted_names = sorted(files)
    targets = {}
    if rule_type == "prefix":
        value = rule["value"]
        for name in sorted_names:
            stem, ext = split_ext(name)
            targets[name] = value + stem + ext
    elif rule_type == "replace":
        old = rule["old"]
        new = rule["new"]
        for name in sorted_names:
            stem, ext = split_ext(name)
            targets[name] = stem.replace(old, new) + ext
    else:
        prefix = rule["prefix"]
        width = rule["width"]
        for i, name in enumerate(sorted_names, 1):
            stem, ext = split_ext(name)
            targets[name] = prefix + str(i).zfill(width) + ext

    for name in sorted_names:
        target = targets[name]
        if target == "" or target == "." or target == "..":
            return {"error": "INVALID_NAME"}
        if any(ch in target for ch in INVALID_CHARS):
            return {"error": "INVALID_NAME"}

    seen = {}
    for name in sorted_names:
        target = targets[name]
        if target in seen:
            return {"error": "COLLISION"}
        seen[target] = name

    source_set = set(files)
    for name in sorted_names:
        target = targets[name]
        if target in source_set and target != name:
            return {"error": "TARGET_EXISTS"}

    plan = [{"from": name, "to": targets[name]} for name in sorted_names]
    return {"plan": plan}

def main():
    for line in sys.stdin:
        try:
            req = json.loads(line)
        except Exception:
            print("INVALID_JSON", flush=True)
            continue
        try:
            resp = process(req)
        except Exception:
            resp = {"error": "INVALID_RULE"}
        print(json.dumps(resp, ensure_ascii=False, separators=(",", ":")), flush=True)

if __name__ == "__main__":
    main()
