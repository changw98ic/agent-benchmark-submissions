type Route = {
  method: string;
  path: string;
  status: number;
  body: unknown;
};

type Request = {
  method: string;
  target: string;
};

const VALID_METHODS = new Set(["GET", "POST", "PUT", "PATCH", "DELETE"]);

function processInput(input: any): any {
  const routes = input?.routes;
  const requests = input?.requests;

  if (!Array.isArray(routes) || !Array.isArray(requests)) {
    return { error: "CONFIG" };
  }

  const index = new Map<string, { status: number; body: unknown }>();

  for (const r of routes) {
    if (typeof r !== "object" || r === null) {
      return { error: "CONFIG" };
    }
    const { method, path, status, body } = r as any;
    if (typeof method !== "string" || !VALID_METHODS.has(method)) {
      return { error: "CONFIG" };
    }
    if (
      typeof path !== "string" ||
      !path.startsWith("/") ||
      path.includes("?") ||
      path.includes("#")
    ) {
      return { error: "CONFIG" };
    }
    if (
      typeof status !== "number" ||
      !Number.isInteger(status) ||
      status < 200 ||
      status > 599
    ) {
      return { error: "CONFIG" };
    }
    if (!("body" in r)) {
      return { error: "CONFIG" };
    }
    if ((status === 204 || status === 304) && body !== null) {
      return { error: "CONFIG" };
    }

    const key = `${method}\0${path}`;
    if (index.has(key)) {
      return { error: "DUPLICATE_ROUTE" };
    }
    index.set(key, { status, body });
  }

  const responses: Array<{ status: number; body: unknown }> = [];

  for (const req of requests) {
    if (typeof req !== "object" || req === null) {
      responses.push({ status: 404, body: { error: "NOT_FOUND" } });
      continue;
    }
    const { method, target } = req as any;
    if (typeof method !== "string" || typeof target !== "string") {
      responses.push({ status: 404, body: { error: "NOT_FOUND" } });
      continue;
    }

    let path = target;
    const hashIndex = path.indexOf("#");
    if (hashIndex !== -1) {
      path = path.slice(0, hashIndex);
    }
    const queryIndex = path.indexOf("?");
    if (queryIndex !== -1) {
      path = path.slice(0, queryIndex);
    }

    const key = `${method}\0${path}`;
    const route = index.get(key);
    if (route) {
      responses.push({ status: route.status, body: route.body });
    } else {
      responses.push({ status: 404, body: { error: "NOT_FOUND" } });
    }
  }

  return { responses };
}

async function main() {
  const decoder = new TextDecoder();
  let buffer = "";

  for await (const chunk of Bun.stdin.stream()) {
    buffer += decoder.decode(chunk, { stream: true });
    const lines = buffer.split("\n");
    buffer = lines.pop() ?? "";

    for (const line of lines) {
      handleLine(line);
    }
  }

  if (buffer.length > 0) {
    handleLine(buffer);
  }
}

function handleLine(line: string) {
  let json: any;
  try {
    json = JSON.parse(line);
  } catch {
    console.log(JSON.stringify({ error: "INVALID_JSON" }));
    return;
  }

  try {
    const result = processInput(json);
    console.log(JSON.stringify(result));
  } catch {
    console.log(JSON.stringify({ error: "CONFIG" }));
  }
}

main();
