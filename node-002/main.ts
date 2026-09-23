import * as readline from 'node:readline';

const OFFSET_MS = 8 * 60 * 60 * 1000;
const MAX_TIME = 4102444800000;
const MAX_SPAN = 31 * 24 * 60 * 60 * 1000;
const DAY_MS = 24 * 60 * 60 * 1000;

interface Entry {
  id: string;
  project: string;
  start: number;
  end: number;
}

type DailyMap = Map<string, Map<string, number>>;

function emit(obj: unknown): void {
  process.stdout.write(JSON.stringify(obj) + '\n');
}

function error(code: string): void {
  emit({ error: code });
}

function isValidEntry(e: Entry): boolean {
  if (!Number.isInteger(e.start) || !Number.isInteger(e.end)) return false;
  if (e.start < 0 || e.end > MAX_TIME) return false;
  if (e.end <= e.start) return false;
  if (e.end - e.start > MAX_SPAN) return false;
  return true;
}

function processRequest(req: any): void {
  if (typeof req !== 'object' || req === null || !Array.isArray(req.entries)) {
    error('INVALID_JSON');
    return;
  }
  const entries: Entry[] = req.entries;
  const ids = new Set<string>();
  for (const e of entries) {
    if (ids.has(e.id)) {
      error('DUPLICATE_ID');
      return;
    }
    ids.add(e.id);
  }
  for (const e of entries) {
    if (!isValidEntry(e)) {
      error('INVALID_TIME');
      return;
    }
  }
  for (let i = 0; i < entries.length; i++) {
    const a = entries[i];
    for (let j = i + 1; j < entries.length; j++) {
      const b = entries[j];
      if (a.start < b.end && b.start < a.end) {
        error('OVERLAP');
        return;
      }
    }
  }
  const dailyMap: DailyMap = new Map();
  let total = 0;
  for (const e of entries) {
    total += e.end - e.start;
    let cur = e.start;
    while (cur < e.end) {
      const localDate = new Date(cur + OFFSET_MS).toISOString().slice(0, 10);
      const [y, m, d] = localDate.split('-').map(Number);
      const dayStartUtc = Date.UTC(y, m - 1, d) - OFFSET_MS;
      const nextBoundary = dayStartUtc + DAY_MS;
      const segEnd = Math.min(e.end, nextBoundary);
      const dur = segEnd - cur;
      if (dur > 0) {
        let projMap = dailyMap.get(localDate);
        if (!projMap) {
          projMap = new Map();
          dailyMap.set(localDate, projMap);
        }
        projMap.set(e.project, (projMap.get(e.project) ?? 0) + dur);
      }
      cur = segEnd;
    }
  }
  const daily: { date: string; project: string; duration_ms: number }[] = [];
  const dates = Array.from(dailyMap.keys()).sort();
  for (const date of dates) {
    const projMap = dailyMap.get(date)!;
    const projects = Array.from(projMap.keys()).sort();
    for (const project of projects) {
      const duration_ms = projMap.get(project)!;
      if (duration_ms !== 0) {
        daily.push({ date, project, duration_ms });
      }
    }
  }
  emit({ daily, total_ms: total });
}

const rl = readline.createInterface({
  input: process.stdin,
  crlfDelay: Infinity,
});

rl.on('line', (line) => {
  const trimmed = line.trim();
  if (trimmed === '') {
    error('INVALID_JSON');
    return;
  }
  let req: any;
  try {
    req = JSON.parse(trimmed);
  } catch {
    error('INVALID_JSON');
    return;
  }
  try {
    processRequest(req);
  } catch {
    error('INVALID_JSON');
  }
});
