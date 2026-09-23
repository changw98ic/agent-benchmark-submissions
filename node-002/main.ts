const DAY_MS = 86400000;
const OFFSET_MS = 8 * 3600 * 1000;
const MAX_TIME = 4102444800000;
const MAX_SPAN = 31 * DAY_MS;

type Entry = {
  id: string;
  project: string;
  start: number;
  end: number;
};

function isInvalidTime(entry: Entry): boolean {
  const { start, end } = entry;
  if (!Number.isInteger(start) || !Number.isInteger(end)) return true;
  if (start < 0 || end > MAX_TIME) return true;
  if (end <= start) return true;
  if (end - start > MAX_SPAN) return true;
  return false;
}

function processLine(line: string): string {
  let req: any;
  try {
    req = JSON.parse(line);
  } catch {
    return JSON.stringify({ error: 'INVALID_JSON' });
  }

  const entries: Entry[] = req && Array.isArray(req.entries) ? req.entries : [];

  const seen = new Set<string>();
  for (const entry of entries) {
    if (seen.has(entry.id)) {
      return JSON.stringify({ error: 'DUPLICATE_ID' });
    }
    seen.add(entry.id);
  }

  for (const entry of entries) {
    if (isInvalidTime(entry)) {
      return JSON.stringify({ error: 'INVALID_TIME' });
    }
  }

  const sorted = entries.slice().sort((a, b) => a.start - b.start || a.end - b.end);
  let maxEnd = -Infinity;
  for (const entry of sorted) {
    if (entry.start < maxEnd) {
      return JSON.stringify({ error: 'OVERLAP' });
    }
    if (entry.end > maxEnd) {
      maxEnd = entry.end;
    }
