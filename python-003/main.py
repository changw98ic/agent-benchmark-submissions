import sys
import json

class DirTree:
    def __init__(self):
        self.nodes = {}
        self.nodes['root'] = {'id': 'root', 'parent': None, 'name': 'root', 'type': 'folder'}

    def is_folder(self, node_id):
        node = self.nodes.get(node_id)
        return node is not None and node['type'] == 'folder'

    def name_valid(self, name):
        return name.strip() != '' and '/' not in name

    def has_conflict(self, parent_id, name, exclude=None):
        for node_id, node in self.nodes.items():
            if node_id == exclude:
                continue
            if node['parent'] == parent_id and node['name'] == name:
                return True
        return False

    def is_descendant(self, possible_descendant, ancestor):
        current = possible_descendant
        while current is not None:
            if current == ancestor:
                return True
            node = self.nodes.get(current)
            if node is None:
                return False
            current = node['parent']
        return False

    def delete_subtree(self, root_id):
        to_delete = []
        stack = [root_id]
        while stack:
            node_id = stack.pop()
            to_delete.append(node_id)
            for child_id, node in self.nodes.items():
                if node['parent'] == node_id:
                    stack.append(child_id)
        for node_id in to_delete:
            del self.nodes[node_id]

def process_ops(ops):
    tree = DirTree()
    results = []
    for op in ops:
        op_type = op.get('op')
        if op_type == 'add':
            node_id = op['id']
            parent_id = op['parent']
            name = op['name']
            node_type = op['type']
            if node_id in tree.nodes:
                results.append('DUPLICATE_ID')
                continue
            if parent_id not in tree.nodes or not tree.is_folder(parent_id):
                results.append('PARENT')
                continue
            if not tree.name_valid(name):
                results.append('NAME')
                continue
            if tree.has_conflict(parent_id, name):
                results.append('NAME_CONFLICT')
                continue
            tree.nodes[node_id] = {'id': node_id, 'parent': parent_id, 'name': name, 'type': node_type}
            results.append('OK')
        elif op_type == 'rename':
            node_id = op['id']
            name = op['name']
            if node_id == 'root':
                results.append('ROOT')
                continue
            if node_id not in tree.nodes:
                results.append('NOT_FOUND')
                continue
            if not tree.name_valid(name):
                results.append('NAME')
                continue
            parent_id = tree.nodes[node_id]['parent']
            if tree.has_conflict(parent_id, name, exclude=node_id):
                results.append('NAME_CONFLICT')
                continue
            tree.nodes[node_id]['name'] = name
            results.append('OK')
        elif op_type == 'move':
            node_id = op['id']
            parent_id = op['parent']
            if node_id == 'root':
                results.append('ROOT')
                continue
            if node_id not in tree.nodes:
                results.append('NOT_FOUND')
                continue
            if parent_id not in tree.nodes or not tree.is_folder(parent_id):
                results.append('PARENT')
                continue
            if tree.is_descendant(parent_id, node_id):
                results.append('CYCLE')
                continue
            name = tree.nodes[node_id]['name']
            if tree.has_conflict(parent_id, name, exclude=node_id):
                results.append('NAME_CONFLICT')
                continue
            tree.nodes[node_id]['parent'] = parent_id
            results.append('OK')
        elif op_type == 'delete':
            node_id = op['id']
            if node_id == 'root':
                results.append('ROOT')
                continue
            if node_id not in tree.nodes:
                results.append('NOT_FOUND')
                continue
            tree.delete_subtree(node_id)
            results.append('OK')
        else:
            results.append('INVALID_OP')
    nodes_out = [tree.nodes[node_id] for node_id in sorted(tree.nodes.keys())]
    return {'results': results, 'nodes': nodes_out}

def main():
    for line in sys.stdin:
        try:
            obj = json.loads(line)
        except json.JSONDecodeError:
            print('INVALID_JSON')
            continue
        if not isinstance(obj, dict) or not isinstance(obj.get('ops'), list):
            print('INVALID_JSON')
            continue
        response = process_ops(obj['ops'])
        print(json.dumps(response, ensure_ascii=False, separators=(',', ':')))

if __name__ == '__main__':
    main()
