import * as readline from 'node:readline';

type ColumnName = 'todo' | 'doing' | 'done';
interface Task { id: string; title: string; }
type Columns = Record<ColumnName, Task[]>;

const COLUMN_NAMES: ColumnName[] = ['todo', 'doing', 'done'];

function trimAscii(s: string): string {
  return s.replace(/^[\x09-\x0D\x20]+/, '').replace(/[\x09-\x0D\x20]+$/, '');
}

function findTask(columns: Columns, id: string): { task: Task; column: Task[]; columnName: ColumnName; index: number } | null {
  for (const colName of COLUMN_NAMES) {
    const arr = columns[colName];
    const idx = arr.findIndex(t => t.id === id);
    if (idx !== -1) {
      return { task: arr[idx], column: arr, columnName: colName, index: idx };
    }
  }
  return null;
}

function processRequest(req: any): { results: string[]; columns: Columns } {
  const columns: Columns = { todo: [], doing: [], done: [] };
  const results: string[] = [];
  const ops = req.ops;
  for (const op of ops) {
    const opName = op.op;
    if (opName === 'add') {
      const id = op.id;
      const title = typeof op.title === 'string' ? op.title : '';
      const exists = COLUMN_NAMES.some(col => columns[col].some(t => t.id === id));
      if (exists) {
        results.push('DUPLICATE_ID');
        continue;
      }
      const trimmed = trimAscii(title);
      if (trimmed === '') {
        results.push('TITLE');
        continue;
      }
      columns.todo.push({ id, title: trimmed });
      results.push('OK');
    } else if (opName === 'edit') {
      const id = op.id;
      const found = findTask(columns, id);
      if (!found) {
        results.push('NOT_FOUND');
        continue;
      }
      const title = typeof op.title === 'string' ? op.title : '';
      const trimmed = trimAscii(title);
      if (trimmed === '') {
        results.push('TITLE');
        continue;
      }
      found.task.title = trimmed;
      results.push('OK');
    } else if (opName === 'delete') {
      const id = op.id;
      const found = findTask(columns, id);
      if (!found) {
        results.push('NOT_FOUND');
        continue;
      }
      found.column.splice(found.index, 1);
      results.push('OK');
    } else if (opName === 'move') {
      const id = op.id;
      const found = findTask(columns, id);
      if (!found) {
        results.push('NOT_FOUND');
        continue;
      }
      const colName = op.column;
      if (colName !== 'todo' && colName !== 'doing' && colName !== 'done') {
        results.push('COLUMN');
        continue;
      }
      const index = op.index;
      let targetLen: number;
      if (colName === found.columnName) {
        targetLen = found.column.length - 1;
      } else {
        targetLen = columns[colName].length;
      }
      if (!Number.isInteger(index) || index < 0 || index > targetLen) {
        results.push('INDEX');
        continue;
      }
      found.column.splice(found.index, 1);
      columns[colName].splice(index, 0, found.task);
      results.push('OK');
    } else {
      results.push('CONFIG');
    }
  }
  return { results, columns };
}

const rl = readline.createInterface({
  input: process.stdin,
  crlfDelay: Infinity
});

rl.on('line', (line) => {
  let parsed: any;
  try {
    parsed = JSON.parse(line);
  } catch {
    process.stdout.write(JSON.stringify({ error: 'INVALID_JSON' }) + '\n');
    return;
  }
  if (!parsed || typeof parsed !== 'object' || !Array.isArray(parsed.ops)) {
    process.stdout.write(JSON.stringify({ error: 'CONFIG' }) + '\n');
    return;
  }
  const output = processRequest(parsed);
  process.stdout.write(JSON.stringify(output) + '\n');
});
