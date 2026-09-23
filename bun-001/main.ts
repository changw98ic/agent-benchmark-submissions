// @ts-nocheck
const ALLOWED_METHODS = new Set(['GET', 'POST', 'PUT', 'PATCH', 'DELETE']);

function isValidRule(rule: any): boolean {
  if (typeof rule !== 'object' || rule === null) return false;
  if (!Object.prototype.hasOwnProperty.call(rule, 'method')) return false;
  if (!Object.prototype.hasOwnProperty.call(rule, 'path')) return false;
  if (!Object.prototype.hasOwnProperty.call(rule, 'status')) return false;
  if (!Object.prototype.hasOwnProperty.call(rule, 'body')) return false;
  const method = rule.method;
  const path = rule.path;
  const status = rule.status;
  const body = rule.body;
  if (typeof method !== 'string' || !ALLOWED_METHODS.has(method)) return false;
  if (typeof path !== 'string' || !path.startsWith('/') || path.includes('?') || path.includes('#')) return false;
  if (typeof status !== 'number' || !Number.isInteger(status) || status < 200 || status > 599) return false;
  if (status === 204 || status === 304) {
    if (body !== null) return false;
  }
  return true;
}

function processInput(input: any): any {
  if (typeof input !== 'object' || input === null) return { error: 'CONFIG' };
  const routes = input.routes;
  const requests = input.requests;
  if (!Array.isArray(routes) || !Array.isArray(requests)) return { error: 'CONFIG' };

  const seen = new Set<string>();
  for (const rule of routes) {
    if (!isValidRule(rule)) return { error: 'CONFIG' };
    const key = JSON.stringify([rule.method, rule.path]);
    if (seen.has(key)) return { error: 'DUPLICATE_ROUTE' };
    seen.add(key);
  }

  const table = new Map<string, { status: number; body: any }>();
  for (const rule of routes) {
    table.set(JSON.stringify([rule.method, rule.path]), { status: rule.status, body: rule.body });
  }

  const responses: Array<{ status: number; body: any }> = [];
  for (const req of requests) {
    if (typeof req !== 'object' || req === null || typeof req.method !== 'string' || typeof req.target !== 'string') {
      responses.push({ status: 404, body: { error: 'NOT_FOUND' } });
      continue;
    }
    let target = req.target;
    const hashIndex = target.indexOf('#');
    if (hashIndex !== -1) target = target.slice(0, hashIndex);
    const queryIndex = target.indexOf('?');
    if (queryIndex !== -1) target = target.slice(0, queryIndex);
    const key = JSON.stringify([req.method, target]);
    const match = table.get(key);
    if (match) {
      responses.push({ status: match.status, body: match.body });
    } else {
      responses.push({ status: 404, body: { error: 'NOT_FOUND' } });
    }
  }
  return { responses };
}

const inputText = await Bun.stdin.text();
const lines = inputText.split(/\r?\n/);
for (const line of lines) {
  if (line.trim() === '') continue;
  let parsed: any;
  try {
    parsed = JSON.parse(line);
  } catch {
    process.stdout.write(JSON.stringify({ error: 'CONFIG' }) + '\n');
    continue;
  }
  const result = processInput(parsed);
  process.stdout.write(JSON.stringify(result) + '\n');
}
