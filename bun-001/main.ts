const ALLOWED_METHODS = new Set(['GET', 'POST', 'PUT', 'PATCH', 'DELETE']);

function isValidRule(rule: any): boolean {
  if (typeof rule !== 'object' || rule === null || Array.isArray(rule)) return false;
  const { method, path, status, body } = rule;
  if (typeof method !== 'string' || !ALLOWED_METHODS.has(method)) return false;
  if (typeof path !== 'string' || !path.startsWith('/') || path.includes('?') || path.includes('#')) return false;
  if (typeof status !== 'number' || !Number.isInteger(status) || status < 200 || status > 599) return false;
  if (status === 204 || status === 304) {
    if (body !== null) return false;
  } else {
    if (!('body' in rule)) return false;
  }
  return true;
}

function getPath(target: string): string {
  let s = target;
  const hashIdx = s.indexOf('#');
  if (hashIdx !== -1) s = s.slice(0, hashIdx);
  const queryIdx = s.indexOf('?');
  if (queryIdx !== -1) s = s.slice(0, queryIdx);
  return s;
}

function processLine(line: string): void {
  let parsed: any;
  try {
    parsed = JSON.parse(line);
  } catch {
    console.log(JSON.stringify({ error: 'INVALID_JSON' }));
    return;
  }

  if (typeof parsed !== 'object' || parsed === null || Array.isArray(parsed)) {
    console.log(JSON.stringify({ error: 'CONFIG' }));
    return;
  }

  const { routes, requests } = parsed;
  if (!Array.isArray(routes) || !Array.isArray(requests)) {
    console.log(JSON.stringify({ error: 'CONFIG' }));
    return;
  }

  const seen = new Set<string>();
  for (const rule of routes) {
    if (!isValidRule(rule)) {
      console.log(JSON.stringify({ error: 'CONFIG' }));
      return;
    }
    const key = rule.method + '\0' + rule.path;
    if (seen.has(key)) {
      console.log(JSON.stringify({ error: 'DUPLICATE_ROUTE' }));
      return;
    }
    seen.add(key);
  }

  const responses: Array<{ status: number; body: any }> = [];
  for (const req of requests) {
    const method =
