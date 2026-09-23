import * as readline from 'node:readline';

type ColumnName = 'todo' | 'doing' | 'done';
interface Task { id: string; title: string; }
interface Columns { todo: Task[]; doing: Task[]; done: Task[]; }
type Status = 'OK' | 'DUPLICATE_ID' | 'TITLE' | 'NOT_FOUND' | 'COLUMN' | 'INDEX';

const COLUMNS: ColumnName[] = ['todo', 'doing', 'done'];

function isColumn(s: any): s is ColumnName {
  return s === 'todo' || s === 'doing' || s === 'done';
}

function trimAscii(s: string): string {
  let start = 0;
  let end = s.length;
  const isWs = (c: number) => c === 9 || c === 10 || c === 11 || c === 12 || c === 13 || c === 32;
  while (start < end && isWs(s.charCodeAt(start))) start++;
  while (end > start && isWs(s.charCodeAt(end - 1))) end--;
  return s.slice(start, end);
}

function findTask(columns: Columns, id: string): { task: Task; col: ColumnName; index: number } | null {
  for (const col of COLUMNS) {
    const arr = columns[col];
    for (let i = 0; i < arr.length; i++) {
      if (arr[i].id === id) {
        return { task: arr[i], col, index: i };
      }
    }
  }
  return null;
}

function processReq(req: any): { results: Status[]; columns: Columns } {
  const columns: Columns = { todo: [], doing: [], done: [] };
  const results: Status[] = [];
  const ops = req && Array.isArray(req.ops) ? req.ops : [];

  for (const op of ops) {
    const type = op.op;

    if (type === 'add') {
      const id = op.id;
      const title = op.title;
      let duplicate = false;
      for (const col of COLUMNS) {
        if (columns[col].some(t => t.id === id)) {
          duplicate = true;
          break;
        }
      }
      if (duplicate) {
        results.push('DUPLICATE_ID');
        continue;
      }
      const trimmed = trimAscii(title);
      if (trimmed.length === 0) {
        results.push('TITLE');
        continue;
      }
      columns.todo.push({ id, title: trimmed });
      results.push('OK');
    } else if (type === 'edit') {
      const id = op.id;
      const title = op.title;
      const found = findTask(columns, id);
      if (!found) {
        results.push('NOT_FOUND');
        continue;
      }
      const trimmed = trimAscii(title);
      if (trimmed.length === 0) {
        results.push('TITLE');
        continue;
      }
      found.task.title = trimmed;
      results.push('OK');
    } else if (type === 'delete') {
      const id = op.id;
      const found = findTask(columns, id);
      if (!found) {
        results.push('NOT_FOUND');
        continue;
      }
      columns[found.col].splice(found.index, 1);
      results.push('OK');
    } else if (type === 'move') {
      const id = op.id;
      const column = op.column;
      const index = op.index;
      const found = findTask(columns, id);
      if (!found) {
        results.push('NOT_FOUND');
        continue;
      }
      if (!isColumn(column)) {
        results.push('COLUMN');
        continue;
      }
      let targetLen = columns[column].length;
      if (found.col === column) {
        targetLen -= 1;
      }
      if (typeof index !== 'number' || !Number.isInteger(index) || index < 0 || index > targetLen) {
        results.push('INDEX');
        continue;
      }
      columns[found.col].splice(found.index, 1);
      columns[column].splice(index, 0, found.task);
      results.push('OK');
    } else {
      results.push('OK');
    }
  }

  return { results, columns };
}

const rl = readline.createInterface({ input: process.stdin });

rl.on('line', (line) => {
  let req: any;
  try {
    req = JSON.parse(line);
  } catch {
    process.stdout.write(JSON.stringify({ error: 'INVALID_JSON' }) + '\n');
    return;
  }
  const out = processReq(req);
  process.stdout.write(JSON.stringify(out) + '\n');
});
