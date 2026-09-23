import * as readline from 'node:readline';

type Task = { id: string; title: string };
type ColumnName = 'todo' | 'doing' | 'done';
type Columns = { todo: Task[]; doing: Task[]; done: Task[] };

const COLUMN_NAMES: ColumnName[] = ['todo', 'doing', 'done'];

function trimAscii(s: string): string {
  let start = 0;
  let end = s.length;
  const isAsciiWs = (code: number): boolean =>
    code === 0x20 || code === 0x09 || code === 0x0a || code === 0x0d || code === 0x0c || code === 0x0b;
  while (start < end && isAsciiWs(s.charCodeAt(start))) start++;
  while (end > start && isAsciiWs(s.charCodeAt(end - 1))) end--;
  return s.slice(start, end);
}

function findTask(columns: Columns, id: string): { col: ColumnName; index: number; task: Task } | null {
  for (const col of COLUMN_NAMES) {
    const arr = columns[col];
    for (let i = 0; i < arr.length; i++) {
      if (arr[i].id === id) {
        return { col, index: i, task: arr[i] };
      }
    }
  }
  return null;
}

function isColumnName(value: any): value is ColumnName {
  return value === 'todo' || value === 'doing' || value === 'done';
}

function handleOps(ops: any[]): { results: string[]; columns: Columns } {
  const columns: Columns = { todo: [], doing: [], done: [] };
  const results: string[] = [];

  for (const op of ops) {
    if (!op || typeof op !== 'object') {
      results.push('INVALID_OP');
      continue;
    }

    switch (op.op) {
      case 'add': {
        const id = op.id;
        const title = op.title;
        if (findTask(columns, id)) {
          results.push('DUPLICATE_ID');
          break;
        }
        const trimmed = trimAscii(String(title));
        if (trimmed.length === 0) {
          results.push('TITLE');
          break;
        }
        columns.todo.push({ id, title: trimmed });
        results.push('OK');
        break;
      }
      case 'edit': {
        const id = op.id;
        const found = findTask(columns, id);
        if (!found) {
          results.push('NOT_FOUND');
          break;
        }
        const trimmed = trimAscii(String(op.title));
        if (trimmed.length === 0) {
          results.push('TITLE');
          break;
        }
        found.task.title = trimmed;
        results.push('OK');
        break;
      }
      case 'delete': {
        const id = op.id;
        const found = findTask(columns, id);
        if (!found) {
          results.push('NOT_FOUND');
          break;
        }
        columns[found.col].splice(found.index, 1);
        results.push('OK');
        break;
      }
      case 'move': {
        const id = op.id;
        const found = findTask(columns, id);
        if (!found) {
          results.push('NOT_FOUND');
          break;
        }
        const column = op.column;
        if (!isColumnName(column)) {
          results.push('COLUMN');
          break;
        }
        const index = op.index;
        if (typeof index !== 'number' || !Number.isInteger(index) || index < 0) {
          results.push('INDEX');
          break;
        }
        if (found.col === column) {
          const arr = columns[found.col];
          const lenAfter = arr.length - 1;
          if (index > lenAfter) {
            results.push('INDEX');
            break;
          }
          const [task] = arr.splice(found.index, 1);
          arr.splice(index, 0, task);
        } else {
          const sourceArr = columns[found.col];
          const targetArr = columns[column];
          if (index > targetArr.length) {
            results.push('INDEX');
            break;
          }
          const [task] = sourceArr.splice(found.index, 1);
          targetArr.splice(index, 0, task);
        }
        results.push('OK');
        break;
      }
      default:
        results.push('INVALID_OP');
    }
  }

  return { results, columns };
}

const rl = readline.createInterface({
  input: process.stdin,
  crlfDelay: Infinity,
});

rl.on('line', (line: string) => {
  let parsed: any;
  try {
    parsed = JSON.parse(line);
  } catch {
    process.stdout.write(JSON.stringify({ error: 'INVALID_JSON' }) + '\n');
    return;
  }

  if (!parsed || typeof parsed !== 'object' || !Array.isArray(parsed.ops)) {
    process.stdout.write(JSON.stringify({ error: 'INVALID_JSON' }) + '\n');
    return;
  }

  const { results, columns } = handleOps(parsed.ops);
  process.stdout.write(
    JSON.stringify({
      results,
      columns: {
        todo: columns.todo,
        doing: columns.doing,
        done: columns.done,
      },
    }) + '\n'
  );
});
