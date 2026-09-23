import sys
import json

def do_add(nodes, op):
    node_id = op['id']
    parent = op['parent']
    name = op['name']
    node_type = op['type']
    if node_id in nodes:
        return 'DUPLICATE_ID'
    if parent not in nodes or nodes[parent]['type'] != 'folder':
        return 'PARENT'
    if name.strip() == '' or '/' in name:
        return 'NAME'
    for existing in nodes.values():
        if existing['parent'] == parent and existing['name'] == name:
            return 'NAME_CONFLICT'
    nodes[node_id] = {
        'id': node_id,
        'parent': parent,
        'name': name,
        'type': node_type,
    }
    return 'OK'

def do_rename(nodes, op):
    node_id = op['id']
    name = op['name']
    if node_id == 'root':
        return 'ROOT'
    if node_id not in nodes:
        return 'NOT_FOUND'
    if name.strip() == '' or '/' in name:
        return 'NAME'
    node = nodes[node_id]
    parent = node['parent']
    for other_id, other in nodes.items():
        if other_id != node_id and other['parent'] == parent and other['name'] == name:
            return 'NAME_CONFLICT'
    node['name'] = name
    return 'OK'

def do_move(nodes, op):
    node_id = op['id']
    new_parent = op['parent']
    if node_id == 'root':
        return 'ROOT'
    if node_id not in nodes:
        return 'NOT_FOUND'
    if new_parent not in nodes or nodes[new_parent]['type'] != 'folder':
        return 'PARENT'
    current = new_parent
    while current is not None:
        if current == node_id:
            return 'CYCLE'
        current = nodes[current]['parent'] if current in nodes else None
    node = nodes[node_id]
    for other_id, other in nodes.items():
        if other_id != node_id and other['parent'] == new_parent and other['name'] == node['name']:
            return 'NAME_CONFLICT'
    node['parent'] = new_parent
    return 'OK'

def do_delete(nodes, op):
    node_id = op['id']
    if node_id == 'root':
        return 'ROOT'
    if node_id not in nodes:
        return 'NOT_FOUND'
    to_delete = set()
    stack = [node_id]
    while stack:
        current = stack.pop()
        if current in to_delete:
            continue
        to_delete.add(current)
        for child_id, child in nodes.items():
            if child['parent'] == current:
                stack.append(child_id)
    for k in to_delete:
        del nodes[k]
    return 'OK'

def process_request(request):
    nodes = {
        'root': {
            'id': 'root',
            'parent': None,
            'name': 'root',
            'type': 'folder',
        }
    }
    results = []
    ops = request.get('ops', [])
    for op in ops:
        op_type = op.get('op')
        if op_type == 'add':
            status = do_add(nodes, op)
        elif op_type == 'rename':
            status = do_rename(nodes, op)
        elif op_type == 'move':
            status = do_move(nodes, op)
        elif op_type == 'delete':
            status = do_delete(nodes, op)
        else:
            status = 'INVALID_OP'
        results.append(status)
    sorted_nodes = [nodes[k] for k in sorted(nodes.keys())]
    return {'results': results, 'nodes': sorted_nodes}

def main():
    for line in sys.stdin:
        line = line.rstrip('\n')
        try:
            request = json.loads(line)
            if not isinstance(request, dict) or not isinstance(request.get('ops'), list):
                print('INVALID_JSON', flush=True)
                continue
            response = process_request(request)
            print(json.dumps(response, ensure_ascii=False, separators=(',', ':')), flush=True)
        except json.JSONDecodeError:
            print('INVALID_JSON', flush=True)
        except Exception:
            print('INVALID_JSON', flush=True)

if __name__ == '__main__':
    main()
