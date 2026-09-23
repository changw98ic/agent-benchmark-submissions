import * as readline from 'node:readline';

const MAX_TIME = 4102444800000;
const MAX_SPAN = 31 * 24 * 60 * 60 * 1000;
const DAY_MS = 24 * 60 * 60 * 1000;
const OFFSET = 8 * 60 * 60 * 1000;

function write(obj: any) {
  process.stdout.write(JSON.stringify(obj) + '\n');
}

function writeError(code: string) {
  write({ error: code });
}

function processLine(line: string) {
  let data: any;
  try {
    data = JSON.parse(line);
  } catch {
    writeError('INVALID_JSON');
    return;
  }

  if (!data || typeof data !== 'object' || !Array.isArray(data.entries)) {
    writeError('INVALID_JSON');
    return;
  }

  const entries = data.entries;

  const seenIds = new Set<string>();
  for (const e of entries) {
    if (seenIds.has(e.id)) {
      writeError('DUPLICATE_ID');
      return;
    }
    seenIds.add(e.id);
  }

  for (const e of entries) {
    const { start, end } = e;
    if (!Number.isInteger(start) || !Number.isInteger(end) ||
        start < 0 || end > MAX_TIME || end <= start ||
        (end - start) > MAX_SPAN) {
      writeError('INVALID_TIME');
      return;
    }
  }

  const sorted = [...entries].sort((a, b) => a.start - b.start || a.end - b.end);
  let maxEnd = -1;
  for (const e of sorted) {
    if (e.start < maxEnd) {
      writeError('OVERLAP');
      return;
    }
    if (e.end > maxEnd) maxEnd = e.end;
  }

  const dateMap = new Map<string, Map<string, number>>();
  let total = 0;

  for (const e of entries) {
    let cur = e.start;
    const project = e.project;
    while (cur < e.end) {
      const local = cur + OFFSET;
      const dayStartLocal = Math.floor(local / DAY_MS) * DAY_MS;
      const nextLocal = dayStartLocal + DAY_MS;
      const boundary = nextLocal - OFFSET;
      const segEnd = Math.min(e.end, boundary);
      const dur = segEnd - cur;
      const date = new Date(dayStartLocal).toISOString().slice(0, 10);

      let projMap = dateMap.get(date);
      if (!projMap) {
        projMap = new Map<string, number>();
        dateMap.set(date, projMap);
      }
      projMap.set(project, (projMap.get(project) || 0) + dur);
      total += dur;
      cur = segEnd;
    }
  }

  const daily: { date: string; project: string; duration_ms: number }[] = [];
  for (const [date, projMap] of dateMap) {
    for (const [project, duration] of projMap) {
      if (duration > 0) {
        daily.push({ date, project, duration_ms: duration });
      }
    }
  }
  daily.sort((a, b) => {
    if (a.date < b.date) return -1;
    if (a.date > b.date) return 1;
    if (a.project < b.project) return -1;
    if (a.project > b.project) return 1;
    return 0;
  });

  write({ daily, total_ms: total });
}

const rl = readline.createInterface({
  input: process.stdin,
  crlfDelay: Infinity,
});

rl.on('line', (line: string) => {
  processLine(line);
});
