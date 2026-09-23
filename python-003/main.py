import sys, json

def main():
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        req = json.loads(line)
        ops = req.get("ops", [])
        nodes = {"root": {"id":"root","parent":None,"name":"root","type":"folder"}}
        results = []
        for op in ops:
            results.append(apply(nodes, op))
        out_nodes = [nodes[k] for k in sorted(nodes)]
        print(json.dumps({"results": results, "nodes": out_nodes}))
