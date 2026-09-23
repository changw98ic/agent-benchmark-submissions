import sys
import json

class RuleError(Exception):
    def __init__(self, code):
        self.code = code

def split_name(name):
    idx = name.rfind('.')
    if idx <= 0:
        return name, ''
    return name[:idx], name[idx:]

def apply_rule(name, rule, number=None):
    typ = rule.get('type')
    if typ == 'prefix':
        value = rule.get('value', '')
        if not isinstance(value, str):
            raise RuleError('INVALID_RULE')
        body, ext = split_name(name)
        return value + body + ext
    if typ == 'replace':
        old = rule.get('old', '')
        new = rule.get('new', '')
        if not isinstance(old, str) or not isinstance(new, str) or old == '':
            raise RuleError('INVALID_RULE')
        body, ext = split_name(name)
        return body.replace(old, new) + ext
    if typ == 'number':
        prefix = rule.get('prefix', '')
        width = rule.get('width')
        if not isinstance(prefix, str) or not isinstance(width, int) or isinstance(width, bool) or width < 1 or width > 6:
            raise RuleError('INVALID_RULE')
        if number is None:
            raise RuleError('INVALID_RULE')
        body, ext = split_name(name)
        return prefix + str(number).zfill(width) + ext
    raise RuleError('INVALID_RULE')

def invalid_target(t):
    if t == '' or t == '.' or t == '..':
        return True
    if '/' in t or '\\' in t or '\x00' in t:
        return True
    return False

def process(req):
    files = req.get('files')
    rule = req.get('rule')
    if not isinstance(files, list) or not all(isinstance(x, str) for x in files):
        return {'error': 'INVALID_INPUT'}
    if not isinstance(rule, dict):
        return {'error': 'INVALID_RULE'}
    if len(set(files)) != len(files):
        return {'error': 'DUPLICATE_SOURCE'}
    sorted_files = sorted(files)
    typ = rule.get('type')
    if typ not in ('prefix', 'replace', 'number'):
        return {'error': 'INVALID_RULE'}
    if typ == 'prefix':
        value = rule.get('value', '')
        if not isinstance(value, str):
            return {'error': 'INVALID_RULE'}
    elif typ == 'replace':
        old = rule.get('old', '')
        new = rule.get('new', '')
        if not isinstance(old, str) or not isinstance(new, str) or old == '':
            return {'error': 'INVALID_RULE'}
    elif typ == 'number':
        prefix = rule.get('prefix', '')
        width = rule.get('width')
        if not isinstance(prefix, str) or not isinstance(width, int) or isinstance(width, bool) or width < 1 or width > 6:
            return {'error': 'INVALID_RULE'}
    targets = []
    for i, name in enumerate(sorted_files):
        try:
            target = apply_rule(name, rule, i + 1 if typ == 'number' else None)
        except RuleError as e:
            return {'error': e.code}
        if invalid_target(target):
            return {'error': 'INVALID_NAME'}
        targets.append((name, target))
    seen = set()
    for _, target in targets:
        if target in seen:
            return {'error': 'COLLISION'}
        seen.add(target)
    source_set = set(sorted_files)
    for name, target in targets:
        if target in source_set and target != name:
            return {'error': 'TARGET_EXISTS'}
    return {'plan': [{'from': n, 'to': t} for n, t in targets]}

def main():
    for line in sys.stdin:
        raw = line.rstrip('\n')
        try:
            req = json.loads(raw)
        except json.JSONDecodeError:
            print('INVALID_JSON')
            continue
        if not isinstance(req, dict):
            print('INVALID_JSON')
            continue
        print(json.dumps(process(req), ensure_ascii=False))

if __name__ == '__main__':
    main()
