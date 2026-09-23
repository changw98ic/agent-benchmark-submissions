def process(req):
    files = req["files"]
    rule = req["rule"]
    if len(files) != len(set(files)):
        return {"error":"DUPLICATE_SOURCE"}
    sorted_files = sorted(files)
    # validate rule
    rtype = rule.get("type")
    if rtype == "prefix":
        value = rule["value"]
        def transform(body, idx):
            return value + body
    elif rtype == "replace":
        old = rule["old"]
        new = rule["new"]
        if old == "":
            return {"error":"INVALID_RULE"}
        def transform(body, idx):
            return body.replace(old, new)
    elif rtype == "number":
        prefix = rule["prefix"]
        width = rule["width"]
        # width guaranteed 1-6, but validate
        if not isinstance(width, int) or not (1 <= width <= 6):
            return {"error":"INVALID_RULE"}
        def transform(body, idx):
            return prefix + str(idx).zfill(width)
    else:
        return {"error":"INVALID_RULE"}
    targets = []
    for i, name in enumerate(sorted_files, start=1):
        body, ext = split_name(name)
        new_body = transform(body, i)
        target = new_body + ext
        if target == "" or target == "." or target == ".." or any(c in target for c in "/\\\0"):
            return {"error":"INVALID_NAME"}
        targets.append((name, target))
    # collision
    seen = set()
    for name, target in targets:
        if target in seen:
            return {"error":"COLLISION"}
        seen.add(target)
    # target exists
    source_set = set(sorted_files)
    for name, target in targets:
        if target != name and target in source_set:
            return {"error":"TARGET_EXISTS"}
    plan = [{"from": name, "to": target} for name, target in targets]
    return {"plan": plan}
