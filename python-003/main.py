import sys
import json

class VirtualTree:
    def __init__(self):
        self.nodes = {
            "root": {"id": "root", "parent": None, "name": "root", "type": "folder"}
        }

    def add(self, op):
        nid = op["id"]
        parent = op["parent"]
        name = op["name"]
        typ = op["type"]
        if nid in self.nodes:
            return "DUPLICATE_ID"
        p = self.nodes.get(parent)
        if p is None or p["type"] != "folder":
            return "PARENT"
        if name.strip() == "" or "/" in name:
            return "NAME"
        for n in self.nodes.values():
            if n["parent"] == parent and n["name"] == name:
                return "NAME_CONFLICT"
        self.nodes[nid] = {"id": nid, "parent": parent, "name": name, "type": typ}
        return "OK"

    def rename(self, op):
        nid = op["id"]
        name = op["name"]
        if nid == "root":
            return "ROOT"
        node = self.nodes.get(nid)
        if node is None:
            return "NOT_FOUND"
        if name.strip() == "" or "/" in name:
            return "NAME"
        parent = node["parent"]
        for n in self.nodes.values():
            if n["id"] != nid and n["parent"] == parent and n["name"] == name:
                return "NAME_CONFLICT"
        node["name"] = name
        return "OK"

    def move(self, op):
        nid = op["id"]
        parent = op["parent"]
        if nid == "root":
            return "ROOT"
        node = self.nodes.get(nid)
        if node is None:
            return "NOT_FOUND"
        p = self.nodes.get(parent)
        if p is None or p["type"] != "folder":
            return "PARENT"
        cur = parent
        while cur is not None:
            if cur == nid:
                return "CYCLE"
            cur_node = self.nodes.get(cur)
            if cur_node is None:
                break
            cur = cur_node["parent"]
        for n in self.nodes.values():
            if n["id"] != nid and n["parent"] == parent and n["name"] == node["name"]:
                return "NAME_CONFLICT"
        node["parent"] = parent
        return "OK"

    def delete(self, op):
        nid = op["id"]
        if nid == "root":
            return "ROOT"
        if nid not in self.nodes:
            return "NOT_FOUND"
        to_delete = set()
        for other_id, other in self.nodes.items():
            cur = other_id
            while cur is not None:
                if cur == nid:
                    to_delete.add(other_id)
                    break
                cur_node = self.nodes.get(cur)
                if cur_node is None:
                    break
                cur = cur_node["parent"]
        for d in to_delete:
            del self.nodes[d]
        return "OK"

    def snapshot(self):
        return [self.nodes[k] for k in sorted(self.nodes.keys())]


def process_request(req):
    tree = VirtualTree()
    results = []
    for op in req["ops"]:
        kind = op["op"]
        if kind == "add":
            results.append(tree.add(op))
        elif kind == "rename":
            results.append(tree.rename(op))
        elif kind == "move":
            results.append(tree.move(op))
        elif kind == "delete":
            results.append(tree.delete(op))
        else:
            results.append("INVALID_OP")
    return {"results": results, "nodes": tree.snapshot()}


def main():
    for line in sys.stdin:
        try:
            req = json.loads(line)
        except json.JSONDecodeError:
            print("INVALID_JSON")
            continue
        out = process_request(req)
        print(json.dumps(out, ensure_ascii=False, separators=(",", ":")))


if __name__ == "__main__":
    main()
