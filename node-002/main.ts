import { createInterface } from 'node:readline';

interface Entry {
  id: string;
  project: string;
  start: number;
  end: number;
}

interface DailyItem {
  date: string;
  project: string;
  duration_ms: number;
}

interface Success {
  daily: DailyItem[];
  total_ms: number;
}

interface ErrorResponse {
  error: string;
}

const MAX_TIME = 4102444800000;
const DAY_MS = 86400000;
const MAX_SPAN = 31 * DAY_MS;
const OFFSET_MS = 8 * 3600 * 1000;

function formatLocalDate(ms: number): string {
  return new Date(ms + OFFSET_MS).toISOString().slice(0, 10);
}

function validate(entries: Entry[]): string | null {
  const seen = new Set<string>();
  for (const e of entries) {
    if (seen.has(e.id)) return 'DUPLICATE_ID';
    seen.add(e.id);
  }

  for (const e of entries) {
    if (
      !Number.isInteger(e.start) ||
      !Number.isInteger(e.end) ||
      e.end <= e.start ||
      e.start < 0 ||
      e.end > MAX_TIME ||
      e.end - e.start > MAX_SPAN
    ) {
      return 'INVALID_TIME';
    }
  }

  for (let i = 0; i < entries.length; i++) {
    const a = entries[i];
    for (let j = i + 1; j < entries.length; j++) {
      const b = entries[j];
      if (a.start < b.end && b.start < a.end) {
        return 'OVERLAP';
      }
    }
  }

  return null;
}

function aggregate(entries: Entry[]): Success {
  const map = new Map<string, number>();
  let total = 0;

  for (const e of entries) {
    total += e.end - e.start;
    let cur = e.start;

    while (cur < e.end) {
      const dayStart = Math.floor((cur + OFFSET_MS) / DAY_MS) * DAY_MS - OFFSET_MS;
      const dayEnd = dayStart + DAY_MS;
      const segEnd = Math.min(e.end, dayEnd);
      const duration = segEnd - cur;
      const date = formatLocalDate(dayStart);
      const key = `${date}\u0000${e.project}`;
      map.set(key, (map.get(key) ?? 0) + duration);
      cur = segEnd;
    }
  }

  const daily: DailyItem[] = [];
  for (const [key, duration_ms] of map) {
    if (duration_ms === 0) continue;
    const sep = key.indexOf('\u0000');
    const date = key.slice(0, sep);
    const project = key.slice(sep + 1);
    daily.push({ date, project, duration_ms });
  }

  daily.sort((a, b) => {
    if (a.date !== b.date) return a.date < b.date ? -1 : 1;
    if (a.project !== b.project) return a.project < b.project ? -1 : 1;
    return 0;
  });

  return { daily, total_ms: total };
}

const rl = createInterface({
  input: process.stdin,
  crlfDelay: Infinity,
});

rl.on('line', (line) => {
  const trimmed = line.trim();
  if (!trimmed) return;

  let response: Success | ErrorResponse;
  try {
    const req = JSON.parse(trimmed);
    const entries: Entry[] = req.entries;
    if (!Array.isArray(entries)) {
      response = { error: 'INVALID_INPUT' };
    } else {
      const err = validate(entries);
      if (err) {
        response = { error: err };
      } else {
        response = aggregate(entries);
      }
    }
  } catch {
    response = { error: 'INVALID_INPUT' };
  }

  console.log(JSON.stringify(response));
});
