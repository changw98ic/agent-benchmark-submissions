import sys
import json

class Tree:
    def __init__(self):
        self.nodes = {
            "root": {"id": "root", "parent": None, "name": "root", "type": "folder"}
        }

    def _valid_name(self, name):
        return isinstance(name, str) and name.strip() != "" and "/" not in name

    def _has_conflict(self, parent, name, exclude_id=None):
        for nid, node in self.nodes.items():
            if nid == exclude_id:
                continue
            if node["parent"] == parent and node["name"] == name:
                return True
        return False

    def _is_descendant(self, node_id, ancestor_id):
        cur = node_id
        while cur is not None:
            if cur == ancestor_id:
                return True
            cur = self.nodes[cur]["parent"]
        return False

    def add(self, op):
        node_id = op["id"]
        if node_id in self.nodes:
            return "DUPLICATE_ID"
        parent = op["parent"]
        if parent not in self.nodes or self.nodes[parent]["type"] != "folder":
            return "PARENT"
        name = op["name"]
        if not self._valid_name(name):
            return "NAME"
        if self._has_conflict(parent, name):
            return "NAME_CONFLICT"
        self.nodes[node_id] = {
            "id": node_id,
            "parent": parent,
            "name": name,
            "type": op["type"]
        }
        return "OK"

    def rename(self, op):
        node_id = op["id"]
        if node_id == "root":
            return "ROOT"
        if node_id not in self.nodes:
            return "NOT_FOUND"
        name = op["name"]
        if not self._valid_name(name):
            return "NAME"
        parent = self.nodes[node_id]["parent"]
        if self._has_conflict(parent, name, exclude_id=node_id):
            return "NAME_CONFLICT"
        self.nodes[node_id]["name"] = name
        return "OK"

    def move(self, op):
        node_id = op["id"]
        if node_id == "root":
            return "ROOT"
        if node_id not in self.nodes:
            return "NOT_FOUND"
        new_parent = op["parent"]
        if new_parent not in self.nodes or self.nodes[new_parent]["type"] != "folder":
            return "PARENT"
        if new_parent == node_id or self._is_descendant(new_parent, node_id):
            return "CYCLE"
        name = self.nodes[node_id]["name"]
        if self._has_conflict(new_parent, name, exclude_id=node_id):
            return "NAME_CONFLICT"
        self.nodes[node_id]["parent"] = new_parent
        return "OK"

    def delete(self, op):
        node_id = op["id"]
        if node_id == "root":
            return "ROOT"
        if node_id not in self.nodes:
            return "NOT_FOUND"
        to_delete = []
        stack = [node_id]
        while stack:
            cur = stack.pop()
            to_delete.append(cur)
            for nid, node in self.nodes.items():
                if node["parent"] == cur:
                    stack.append(nid)
        for nid in to_delete:
            del self.nodes[nid]
        return "OK"

    def apply(self, ops):
        results = []
        for op in ops:
            op_type = op.get("op")
            if op_type == "add":
                status = self.add(op)
            elif op_type == "rename":
                status = self.rename(op)
            elif op_type == "move":
                status = self.move(op)
            elif op_type == "delete":
                status = self.delete(op)
            else:
                status = "INVALID_OP"
            results.append(status)
        return results

    def get_nodes(self):
        return [self.nodes[nid] for nid in sorted(self.nodes.keys())]


def process_line(line):
    try:
        data = json.loads(line)
    except json.JSONDecodeError:
        return "INVALID_JSON"
    if not isinstance(data, dict):
        return "INVALID_JSON"
    ops = data.get("ops")
    if not isinstance(ops, list):
        return "INVALID_JSON"
    tree = Tree()
    results = tree.apply(ops)
    return json.dumps({"results": results, "nodes": tree.get_nodes()}, ensure_ascii=False)


def main():
    for line in sys.stdin:
        line = line.rstrip("\n")
        if line.endswith("\r"):
            line = line[:-1]
        output = process_line(line)
        print(output)


if __name__ == "__main__":
    main()
