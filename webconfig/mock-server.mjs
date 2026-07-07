import { createServer } from "node:http";
import { readFile, writeFile } from "node:fs/promises";
import { extname, join, normalize } from "node:path";
import { fileURLToPath } from "node:url";

const root = fileURLToPath(new URL(".", import.meta.url));
const configPath = join(root, ".mock-config.json");
const defaultConfigPath = join(root, "default-config.json");
const port = Number(process.env.PORT || 4173);

let config;
try {
  config = JSON.parse(await readFile(configPath, "utf8"));
} catch {
  config = JSON.parse(await readFile(defaultConfigPath, "utf8"));
}

const contentTypes = {
  ".html": "text/html; charset=utf-8",
  ".css": "text/css; charset=utf-8",
  ".js": "text/javascript; charset=utf-8",
  ".json": "application/json; charset=utf-8"
};

function send(response, status, body, contentType = "application/json; charset=utf-8") {
  response.writeHead(status, {
    "Content-Type": contentType,
    "Cache-Control": "no-store",
    "Access-Control-Allow-Origin": "*"
  });
  response.end(body);
}

async function readBody(request) {
  const chunks = [];
  for await (const chunk of request) chunks.push(chunk);
  return Buffer.concat(chunks).toString("utf8");
}

createServer(async (request, response) => {
  const url = new URL(request.url, `http://${request.headers.host}`);

  if (url.pathname === "/api/config" && request.method === "GET") {
    send(response, 200, JSON.stringify(config));
    return;
  }

  if (url.pathname === "/api/config" && request.method === "PUT") {
    try {
      config = JSON.parse(await readBody(request));
      await writeFile(configPath, JSON.stringify(config, null, 2));
      send(response, 200, JSON.stringify({ ok: true }));
    } catch {
      send(response, 400, JSON.stringify({ ok: false, error: "Invalid JSON" }));
    }
    return;
  }

  if (url.pathname === "/api/reboot" && request.method === "POST") {
    send(response, 200, JSON.stringify({ ok: true, mode: "normal" }));
    return;
  }

  const requestedPath = url.pathname === "/" ? "index.html" : url.pathname.slice(1);
  const filePath = normalize(join(root, requestedPath));
  if (!filePath.startsWith(root)) {
    send(response, 403, "Forbidden", "text/plain; charset=utf-8");
    return;
  }

  try {
    const data = await readFile(filePath);
    send(response, 200, data, contentTypes[extname(filePath)] || "application/octet-stream");
  } catch {
    send(response, 404, "Not found", "text/plain; charset=utf-8");
  }
}).listen(port, "127.0.0.1", () => {
  console.log(`KB2040 web configurator: http://127.0.0.1:${port}`);
});
