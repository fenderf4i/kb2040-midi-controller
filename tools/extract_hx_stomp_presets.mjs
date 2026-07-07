import fs from "node:fs/promises";
import path from "node:path";
import { FileBlob, SpreadsheetFile } from "@oai/artifact-tool";

const workspace = "C:/Users/mike_/Documents/Codex/2026-05-16/i-want-to-start-a-project";
const workbookPath = path.join(workspace, "outputs", "hx_stomp_midi_cc", "HX_Stomp_MIDI_CC.xlsx");
const outputPath = path.join(workspace, "outputs", "hx_stomp_midi_cc", "hx_stomp_presets.json");

const input = await FileBlob.load(workbookPath);
const workbook = await SpreadsheetFile.importXlsx(input);

const overview = await workbook.inspect({
  kind: "sheet,table",
  tableMaxRows: 8,
  tableMaxCols: 8,
  maxChars: 8000,
});
console.log(overview.ndjson);

const sheet = workbook.worksheets.getItem("HX Stomp MIDI CC");
const used = sheet.getUsedRange(true);
const values = used.values;
const headers = values[0].map((header) => String(header ?? "").trim());

function findHeader(name) {
  return headers.findIndex((header) => header.toLowerCase() === name.toLowerCase());
}

const sectionIndex = findHeader("Section");
const ccIndex = findHeader("CC#");
const valueIndex = findHeader("Value");
const functionIndex = findHeader("Function");

if (sectionIndex < 0 || ccIndex < 0 || valueIndex < 0 || functionIndex < 0) {
  throw new Error(`Required headers not found. Found headers: ${headers.join(", ")}`);
}

const presets = values.slice(1)
  .map((row, index) => ({
    sourceRow: index + 2,
    section: String(row[sectionIndex] ?? "").trim(),
    cc: String(row[ccIndex] ?? "").trim(),
    value: String(row[valueIndex] ?? "").trim(),
    function: String(row[functionIndex] ?? "").trim(),
  }))
  .filter((preset) => preset.section || preset.cc || preset.value || preset.function);

await fs.writeFile(outputPath, JSON.stringify({ headers, presets }, null, 2), "utf8");
console.log(`Saved ${outputPath}`);
console.log(JSON.stringify(presets.slice(0, 8), null, 2));
