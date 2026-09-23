let buffer = '';
process.stdin.setEncoding('utf8');
process.stdin.on('data', (chunk) => {
  buffer += chunk;
  let index;
  const newline = String.fromCharCode(10);
  while ((index = buffer.indexOf(newline)) !== -1) {
    const line = buffer.slice(0, index);
    buffer = buffer.slice(index + 1);
    handleLine(line);
  }
});
process.stdin.on('end', () => {
  if (buffer.length > 0) {
    handleLine(buffer);
  }
});

function handleLine(line) {
  const trimmed = line.trim();
  if (trimmed === '') return;
  let req;
  try {
    req = JSON.parse(trimmed);
  } catch {
    return;
  }
  const out = processRequest(req);
  process.stdout.write(JSON.stringify(out) + String.fromCharCode(10));
}

function trimAscii(s) {
  let start = 0;
  let end = s.length;
  while (start < end) {
    const c = s.charCodeAt(start);
    if (c === 32 || c === 9 || c === 10 || c === 11 || c === 12 || c === 13) start++;
    else break;
  }
  while (end > start) {
    const c = s.charCodeAt(end - 1);
    if (c === 32 || c === 9 || c === 10 || c === 11 || c === 12 || c === 13) end--;
    else break;
  }
  return s.slice(start, end);
}

function processRequest(req) {
  const columns = {
    todo: [],
    doing: [],
    done: []
  };
  const taskMap = new Map();
  const results = [];
  const ops = Array.isArray(req.ops) ? req.ops : [];
  for (const op of ops) {
    results.push(applyOp(op, columns, taskMap));
  }
  return { results, columns };
}

function applyOp(op, columns, taskMap) {
  const opType = op.op;
  if (opType === 'add') {
    const id = op.id;
    if (taskMap.has(id)) return 'DUPLICATE_ID';
    const title = trimAscii(String(op.title ?? ''));
    if (title === '') return 'TITLE';
    const task = { id, title };
    columns.todo.push(task);
    taskMap.set(id, { task, column: 'todo' });
    return 'OK';
  }

  const existing = taskMap.get(op.id);
  if (!existing) return 'NOT_FOUND';

  if (opType === 'edit') {
    const title = trimAscii(String(op.title ?? ''));
    if (title === '') return 'TITLE';
    existing.task.title = title;
    return 'OK';
  }

  if (opType === 'delete') {
    const col = existing.column;
    const arr = columns[col];
    const idx = arr.indexOf(existing.task);
    if (idx !== -1) arr.splice(idx, 1);
    taskMap.delete(op.id);
    return 'OK';
  }

  if (opType === 'move') {
    const targetCol = op.column;
    if (targetCol !== 'todo' && targetCol !== 'doing' && targetCol !== 'done') {
      return 'COLUMN';
    }
    const srcCol = existing.column;
    const srcArr = columns[srcCol];
    const srcIdx = srcArr.indexOf(existing.task);
    const targetArr = columns[targetCol];
    const maxIndex = srcCol === targetCol ? targetArr.length - 1 : targetArr.length;
    const index = op.index;
    if (!Number.isInteger(index) || index < 0 || index > maxIndex) {
      return 'INDEX';
    }
    if (srcIdx !== -1) srcArr.splice(srcIdx, 1);
    columns[targetCol].splice(index, 0, existing.task);
    taskMap.set(op.id, { task: existing.task, column: targetCol });
    return 'OK';
  }

  return 'NOT_FOUND';
}
