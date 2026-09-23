import sys
import json


def new_state():
    return {
        'root': {'id': 'root', 'parent': None, 'name': 'root', 'type': 'folder'}
    }


def valid_name(name):
    if not isinstance(name, str):
        return False
    return name.strip() != '' and '/' not in name


def process(ops):
    nodes = new_state()
    results = []
    for op in ops:
        kind = op.get('op')
        if kind == 'add':
            oid = op.get('id')
            parent = op.get('parent')
            name = op.get('name')
            typ = op.get('type')
            if oid in nodes:
                results.append('DUPLICATE_ID')
            elif parent not in nodes or nodes[parent]['type'] != 'folder':
                results.append('PARENT')
            elif not valid_name(name):
                results.append('NAME')
            elif any(n['parent'] == parent and n['name'] == name for n in nodes.values()):
                results.append('NAME_CONFLICT')
            else:
                nodes[oid] = {'id': oid, 'parent': parent, 'name': name, 'type': typ}
                results.append('OK')
        elif kind == 'rename':
            oid = op.get('id')
            name = op.get('name')
            if oid == 'root':
                results.append('ROOT')
            elif oid not in nodes:
                results.append('NOT_FOUND')
            elif not valid_name(name):
                results.append('NAME')
            else:
                parent = nodes[oid]['parent']
                conflict = any(
                    n['id'] != oid and n['parent'] == parent and n['name'] == name
                    for n in nodes.values()
                )
                if conflict:
                    results.append('NAME_CONFLICT')
                else:
                    nodes[oid]['name'] = name
                    results.append('OK')
        elif kind == 'move':
            oid = op.get('id')
            parent = op.get('parent')
            if oid == 'root':
                results.append('ROOT')
            elif oid not in nodes:
                results.append('NOT_FOUND')
            elif parent not in nodes or nodes[parent]['type'] != 'folder':
                results.append('PARENT')
            else:
                cur = parent
                cycle = False
                while cur is not None:
                    if cur == oid:
                        cycle = True
                        break
                    cur = nodes[cur]['parent']
                if cycle:
                    results.append('CYCLE')
                else:
                    name = nodes[oid]['name']
                    conflict = any(
                        n['id'] != oid and n['parent'] == parent and n['name'] == name
                        for n in nodes.values()
                    )
                    if conflict:
                        results.append('NAME_CONFLICT')
                    else:
                        nodes[oid]['parent'] = parent
                        results.append('OK')
        elif kind == 'delete':
            oid = op.get('id')
            if oid == 'root':
                results.append('ROOT')
            elif oid not in nodes:
                results.append('NOT_FOUND')
            else:
                stack = [oid]
                to_delete = set()
                while stack:
                    cur = stack.pop()
                    if cur in to_delete:
                        continue
                    to_delete.add(cur)
                    for n in nodes.values():
                        if n['parent'] == cur:
                            stack.append(n['id'])
                for did in to_delete:
                    del nodes[did]
                results.append('OK')
        else:
            results.append('UNKNOWN')
    sorted_nodes = []
    for oid in sorted(nodes.keys()):
        n = nodes[oid]
        sorted_nodes.append({
            'id': n['id'],
            'parent': n['parent'],
            'name': n['name'],
            'type': n['type'],
        })
    return {'results': results, 'nodes': sorted_nodes}


def main():
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        try:
            req = json.loads(line)
            ops = req.get('ops', [])
            resp = process(ops)
        except Exception:
            resp = {'results': [], 'nodes': []}
        sys.stdout.write(json.dumps(resp, ensure_ascii=False) + '\n')
        sys.stdout.flush()


if __name__ == '__main__':
    main()
