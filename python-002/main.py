import sys
import json

def split_name(name):
    last_dot = name.rfind('.')
    if last_dot > 0:
        return name[:last_dot], name[last_dot:]
    return name, ""

def apply_rule(stem, rule):
    rtype = rule.get("type")
    if rtype == "prefix":
        return rule["value"] + stem
    elif rtype == "replace":
        old = rule["old"]
        new = rule["new"]
        if old == "":
            raise ValueError("INVALID_RULE")
        return stem.replace(old, new)
    elif rtype == "number":
        # handled globally
        pass
    else:
        raise ValueError("INVALID_RULE")

def process_request(req):
    files = req["files"]
    rule = req["rule"]
    # Validate rule
    rtype = rule.get("type")
    if rtype == "prefix":
        if "value" not in rule or not isinstance(rule["value"], str):
            return {"error": "INVALID_RULE"}
    elif rtype == "replace":
        if "old" not in rule or "new" not in rule or not isinstance(rule["old"], str) or not isinstance(rule["new"], str):
            return {"error": "INVALID_RULE"}
        if rule["old"] == "":
            return {"error": "INVALID_RULE"}
    elif rtype == "number":
        if "prefix" not in rule or "width" not in rule or not isinstance(rule["prefix"], str) or not isinstance(rule["width"], int):
            return {"error": "INVALID_RULE"}
        if not (1 <= rule["width"] <= 6):
            return {"error": "INVALID_RULE"}
    else:
        return {"error": "INVALID_RULE"}

    # Sort files by Unicode code points
    sorted_files = sorted(files)
    # Check duplicate sources
    if len(sorted_files) != len(set(sorted_files)):
        return {"error": "DUPLICATE_SOURCE"}

    # Generate targets
    plan = []
    if rtype == "number":
        width = rule["width"]
        prefix = rule["prefix"]
        for idx, name in enumerate(sorted_files, start=1):
            stem, ext = split_name(name)
            new_stem = prefix + str(idx).zfill(width)
            target = new_stem + ext
            plan.append((name, target))
    else:
        for name in sorted_files:
            stem, ext = split_name(name)
            if rtype == "prefix":
                new_stem = rule["value"] + stem
            elif rtype == "replace":
                new_stem = stem.replace(rule["old"], rule["new"])
            target = new_stem + ext
            plan.append((name, target))

    # Check target validity
    for _, target in plan:
        if target == "" or target == "." or target == "..":
            return {"error": "INVALID_NAME"}
        if "/" in target or "\\" in target or "\x00" in target:
            return {"error": "INVALID_NAME"}

    # Check target duplicates
    targets = [t for _, t in plan]
    if len(targets) != len(set(targets)):
        return {"error": "COLLISION"}

    # Check target exists as another source
    source_to_idx = {name: i for i, (name, _) in enumerate(plan)}
    for i, (_, target) in enumerate(plan):
        if target in source_to_idx:
            if source_to_idx[target] != i:
                return {"error": "TARGET_EXISTS"}

    # Success
    return {"plan": [{"from": f, "to": t} for f, t in plan]}

def main():
    for line in sys.stdin:
        line = line.rstrip('\n')
        if not line:
            # empty line? treat as malformed? If line is empty, json.loads fails.
            print("INVALID_JSON")
            continue
        try:
            req = json.loads(line)
        except json.JSONDecodeError:
            print("INVALID_JSON")
            continue
        try:
            resp = process_request(req)
        except Exception:
            # If any unexpected error, maybe print INVALID_JSON? But spec says only malformed JSON.
            # However, to avoid crash, we can print INVALID_JSON. But might hide bugs.
            # Since input structure guaranteed, shouldn't happen.
            print("INVALID_JSON")
            continue
        print(json.dumps(resp, ensure_ascii=False))

if __name__ == "__main__":
    main()
