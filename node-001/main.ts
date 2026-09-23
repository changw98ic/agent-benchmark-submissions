import * as readline from "node:readline";

type Task = { id: string; title: string };
type Columns = { todo: Task[]; doing: Task[]; done: Task[] };

type Op = {
  op: string;
  id?: string;
  title?: string;
  column?: string;
  index?: number;
};

function hasId(t: Columns, id: string): boolean {
  return (
    t.todo.some((x) => x.id === id) ||
    t.doing.some((x) => x.id === id) ||
    t.done.some((x) => x.id === id)
  );
}

function findCol(t: Columns, id: string): Task[] | null {
  if (t.todo.some((x) => x.id === id)) return t.todo;
  if (t.doing.some((x) => x.id === id)) return t.doing;
  if (t.done.some((x) => x.id === id)) return t.done;
  return null;
}

function getCol(t: Columns, name: string): Task[] | null {
  if (name === "todo") return t.todo;
  if (name === "doing") return t.doing;
  if (name === "done") return t.done;
  return null;
}

function processOp(t: Columns, op: Op): string {
  if (op.op === "add") {
    const id = op.id as string;
    if (hasId(t, id)) return "DUPLICATE_ID";
    const title = String(op.title ?? "").replace(/^[\t\n\v\f\r ]+|[\t\n\v\f\r ]+$/g, "");
    if (title.length === 0) return "TITLE";
    t.todo.push({ id, title });
    return "OK";
  }

  const id = op.id as string;
  const src = findCol(t, id);
  if (!src) return "NOT_FOUND";

  if (op.op === "edit") {
    const title = String(op.title ?? "").replace(/^[\t\n\v\f\r ]+|[\t\n\v\f\r ]+$/g, "");
    if (title.length === 0) return "TITLE";
    const task = src.find((x) => x.id === id) as Task;
    task.title = title;
    return "OK";
  }

  if (op.op === "delete") {
    const i = src.findIndex((x) => x.id === id);
    src.splice(i, 1);
    return "OK";
  }

  if (op.op === "move") {
    const dst = getCol(t, String(op.column));
    if (!dst) return "COLUMN";
    const idx = op.index as number;
    const i = src.findIndex((x) => x.id === id);
    const [task] = src.splice(i, 1);
    if (dst === src) {
      if (!Number.isInteger(idx) || idx < 0 || idx > dst.length) {
        src.splice(i, 0, task);
        return "INDEX";
      }
      dst.splice(idx, 0, task);
      return "OK";
    }
    if (!Number.isInteger(idx) || idx < 0 || idx > dst.length) {
      src.splice(i, 0, task);
      return "INDEX";
    }
    dst.splice(idx, 0, task);
    return "OK";
  }

  return "ERROR";
}

function handleLine(line: string): string | null {
  const trimmed = line.trim();
  if (trimmed.length === 0) return null;
  let req: { ops: Op[] };
  try {
    req = JSON.parse(trimmed);
  } catch {
    return null;
  }
  const columns: Columns = { todo: [], doing: [], done: [] };
  const results: string[] = [];
  const ops = Array.isArray(req.ops) ? req.ops : [];
  for (const op of ops) {
    results.push(processOp(columns, op));
  }
  return JSON.stringify({ results, columns });
}

const rl = readline.createInterface({ input: process.stdin, crlfDelay: Infinity });
rl.on("line", (line) => {
  const out = handleLine(line);
  if (out !== null) process.stdout.write(out + "\n");
});
rl.on("close", () => {
  process.exit(0);
});
