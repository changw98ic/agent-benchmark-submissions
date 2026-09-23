import json, sys

def split_name(name):
    idx = name.rfind('.')
    if idx > 0:
        return name[:idx], name[idx:]
    return name, ''

def apply_rule(stem, ext, rule):
    t = rule.get("type")
    if t == "prefix":
        return rule["value"] + stem + ext
    if t == "replace":
        old = rule["old"]; new = rule["new"]
        if old == "":
            return None  # INVALID_RULE
        return stem.replace(old, new) + ext
    if t == "number":
        width = rule["width"]
        if not isinstance(width, int) or isinstance(width, bool) or not (1 <= width <= 6):
            return None
        return rule  # handled separately
    return None
