type AllowedMethod = "GET" | "POST" | "PUT" | "PATCH" | "DELETE";

interface Route {
  method: AllowedMethod;
  path: string;
  status: number;
  body: unknown;
}

interface Response {
  status: number;
  body: unknown;
}

const ALLOWED_METHODS: ReadonlySet<string> = new Set([
  "GET",
  "POST",
  "PUT",
  "PATCH",
  "DELETE",
]);

function isObject(value: unknown): value is Record<string, unknown> {
  return typeof value === "object" && value !== null && !Array.isArray(value);
}

function validateRoute(
  route: unknown
): { ok: true; route: Route } | { ok: false; error: string } {
  if (!isObject(route)) {
    return { ok: false, error: "CONFIG" };
  }

  const { method, path, status, body } = route;

  if (typeof method !== "string" || !ALLOWED_METHODS.has(method)) {
    return { ok: false, error: "CONFIG" };
  }

  if (
    typeof path !== "string" ||
    !path.startsWith("/") ||
    path.includes("?") ||
    path.includes("#")
  ) {
    return { ok: false, error: "CONFIG" };
  }

  if (
    typeof status !== "number" ||
    !Number.isInteger(status) ||
    status < 200 ||
    status > 599
  ) {
    return { ok: false, error: "CONFIG" };
  }

  if ((status === 204 || status === 304) && body !== null) {
    return { ok: false, error: "CONFIG" };
  }

  if (!Object.prototype.hasOwnProperty.call(route, "body")) {
    return { ok: false, error: "CONFIG" };
  }

  return {
    ok: true,
    route: {
      method: method as AllowedMethod,
      path,
      status,
      body,
    },
  };
}

function extractPath(target: string): string {
  let path = target;
  const hashIndex = path.indexOf("#");
  if (hashIndex !== -1) {
    path = path.slice(0, hashIndex);
  }
  const queryIndex = path.indexOf("?");
  if (queryIndex !== -1) {
    path = path.slice(0, queryIndex);
  }
  return path;
}

function handleRequest(request: unknown, routes: Route[]): Response {
  if (!isObject(request)) {
    return { status: 404, body: { error: "NOT_FOUND" } };
  }

  const { method, target } = request;

  if (typeof method !== "string" || typeof target !== "string") {
    return { status: 404, body: { error: "NOT_FOUND" } };
  }

  const path = extractPath(target);

  for (const route of routes) {
    if (route.method === method && route.path === path) {
      return { status: route.status, body: route.body };
    }
  }

  return { status: 404, body: { error: "NOT_FOUND" } };
}

function processLine(line: string): string {
  let input: unknown;
  try {
    input = JSON.parse(line);
  } catch {
    return JSON.stringify({ error: "INVALID_JSON" });
  }

  if (!isObject(input)) {
    return JSON.stringify({ error: "CONFIG" });
  }

  const routesInput = input.routes;
  const requestsInput = input.requests;

  if (!Array.isArray(routesInput) || !Array.isArray(requestsInput)) {
    return JSON.stringify({ error: "CONFIG" });
  }

  const routes: Route[] = [];
  const seen = new Set<string>();

  for (const routeInput of routesInput) {
    const result = validateRoute(routeInput);
    if (!result.ok) {
      return JSON.stringify({ error: result.error });
    }

    const route = result.route;
    const key = `${route.method}\u0000${route.path}`;

    if (seen.has(key)) {
      return JSON.stringify({ error: "DUPLICATE_ROUTE" });
    }

    seen.add(key);
    routes.push(route);
  }

  const responses: Response[] = [];
  for (const requestInput of requestsInput) {
    responses.push(handleRequest(requestInput, routes));
  }

  return JSON.stringify({ responses });
}

const input = await Bun.stdin.text();
const lines = input.split(/\r?\n/);

for (const line of lines) {
  const trimmed = line.trim();
  if (trimmed === "") continue;
  console.log(processLine(trimmed));
}
