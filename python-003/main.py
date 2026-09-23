import sys
import json

def is_valid_name(name):
    return isinstance(name, str) and name.strip() != '' and '/' not in name

def handle_add(op, nodes):
    id_ = op['id']
    parent = op['parent']
    name = op['name']
    typ = op['type']
    if id_ in nodes:
        return 'DUPLICATE_ID'
    p = nodes.get(parent)
    if p is None or p['type'] != 'folder':
        return 'PARENT'
    if not is_valid_name(name):
        return 'NAME'
    for n in nodes.values():
        if n['parent'] == parent and n['name'] == name:
            return 'NAME_CONFLICT'
    nodes[id_] = {'id': id_, 'parent': parent, 'name': name, 'type': typ}
    return 'OK'

def handle_rename(op, nodes):
    id_ = op['id']
    name = op['name']
    if id_ == 'root':
        return 'ROOT'
    node = nodes.get(id_)
    if node is None:
        return 'NOT_FOUND'
    if not is_valid_name(name):
        return 'NAME'
    parent = node['parent']
    for n in nodes.values():
        if n['id'] != id_ and n['parent'] == parent and n['name'] == name:
            return 'NAME_CONFLICT'
    node['name'] = name
    return 'OK'

def handle_move(op, nodes):
    id_ = op['id']
    new_parent = op['parent']
    if id_ == 'root':
        return 'ROOT'
    node = nodes.get(id_)
    if node is None:
        return 'NOT_FOUND'
    p = nodes.get(new_parent)
    if p is None or p['type'] != 'folder':
        return 'PARENT'
    cur = new_parent
    while cur is not None:
        if cur == id_:
            return 'CYCLE'
        cur_node = nodes.get(cur)
        if cur_node is None:
            break
        cur = cur_node['parent']
    for n in nodes.values():
        if n['id'] != id_ and n['parent'] == new_parent and n['name'] == node['name']:
            return 'NAME_CONFLICT'
    node['parent'] = new_parent
    return 'OK'

def handle_delete(op, nodes):
    id_ = op['id']
    if id_ == 'root':
        return 'ROOT'
    if id_ not in nodes:
        return 'NOT_FOUND'
    to_delete = []
    stack = [id_]
    while stack:
        cur = stack.pop()
        to_delete.append(cur)
        for n in nodes.values():
            if n['parent'] == cur:
                stack.append(n['id'])
    for nid in to_delete:
        del nodes[nid]
    return 'OK'

def process_ops(ops):
    nodes = {
        'root': {'id': 'root', 'parent': None, 'name': 'root', 'type': 'folder'}
    }
    results = []
    for op in ops:
        kind = op.get('op')
        if kind == 'add':
            result = handle_add(op, nodes)
        elif kind == 'rename':
            result = handle_rename(op, nodes)
        elif kind == 'move':
            result = handle_move(op, nodes)
        elif kind == 'delete':
            result = handle_delete(op, nodes)
        else:
            result = 'UNKNOWN'
        results.append(result)
    sorted_nodes = [nodes[k] for k in sorted(nodes.keys())]
    return {'results': results, 'nodes': sorted_nodes}

def main():
    for line in sys.stdin:
        line = line.strip()
        if not line:
            print('INVALID_JSON')
            continue
        try:
            req = json.loads(line)
        except Exception:
            print('INVALID_JSON')
            continue
        if not isinstance(req, dict) or not isinstance(req.get('ops'), list):
            print('INVALID_JSON')
            continue
        resp = process_ops(req['ops'])
        print(json.dumps(resp, ensure_ascii=False, separators=(',', ':')))

if __name__ == '__main__':
    main()
