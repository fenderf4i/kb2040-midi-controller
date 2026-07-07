import { mkdir, readFile, writeFile } from "node:fs/promises";
import { gzipSync } from "node:zlib";
import { fileURLToPath } from "node:url";
import { join } from "node:path";

const root = fileURLToPath(new URL(".", import.meta.url));
const dist = join(root, "dist");
const generated = join(root, "generated");

const [htmlSource, css, javascript] = await Promise.all([
  readFile(join(root, "index.html"), "utf8"),
  readFile(join(root, "styles.css"), "utf8"),
  readFile(join(root, "app.js"), "utf8")
]);

const html = htmlSource
  .replace("  <link rel=\"stylesheet\" href=\"styles.css\">", `  <style>${css}</style>`)
  .replace("  <script src=\"app.js\"></script>", `  <script>${javascript}</script>`);
const compressed = gzipSync(Buffer.from(html));
const bytes = [...compressed].map((value, index) => `${index % 16 === 0 ? "\n  " : ""}0x${value.toString(16).padStart(2, "0")}`).join(", ");
const header = `#pragma once

#include <Arduino.h>

const uint8_t WEB_CONFIG_INDEX_GZ[] PROGMEM = {${bytes}\n};
const size_t WEB_CONFIG_INDEX_GZ_SIZE = sizeof(WEB_CONFIG_INDEX_GZ);
`;

await mkdir(dist, { recursive: true });
await mkdir(generated, { recursive: true });
await writeFile(join(dist, "index.html"), html);
await writeFile(join(generated, "WebConfigIndex.h"), header);

console.log(`Embedded page: ${Buffer.byteLength(html)} bytes`);
console.log(`Gzip payload: ${compressed.length} bytes`);
