import fs from "node:fs/promises";
import path from "node:path";
import { SpreadsheetFile, Workbook } from "@oai/artifact-tool";

const workspace = "C:/Users/mike_/Documents/Codex/2026-05-16/i-want-to-start-a-project";
const outputDir = path.join(workspace, "outputs", "hx_stomp_midi_cc");
await fs.mkdir(outputDir, { recursive: true });

const rows = [
  ["Section", "CC#", "Value", "Function", "Device", "Notes"],
  ["Pedal and Footswitch Assignments", "1", "0-127", "Emulates EXP 1 Pedal", "Both", ""],
  ["Pedal and Footswitch Assignments", "2", "0-127", "Emulates EXP 2 Pedal", "Both", ""],
  ["Pedal and Footswitch Assignments", "49", "0-127", "Emulates FS1", "Both", ""],
  ["Pedal and Footswitch Assignments", "50", "0-127", "Emulates FS2", "Both", ""],
  ["Pedal and Footswitch Assignments", "51", "0-127", "Emulates FS3", "Both", ""],
  ["Pedal and Footswitch Assignments", "52", "0-127", "Emulates FS4", "Both", ""],
  ["Pedal and Footswitch Assignments", "53", "0-127", "Emulates FS5", "Both", ""],
  ["Pedal and Footswitch Assignments", "54", "0-127", "Emulates FS6", "HX Stomp XL", ""],
  ["Pedal and Footswitch Assignments", "55", "0-127", "Emulates FS7", "HX Stomp XL", ""],
  ["Pedal and Footswitch Assignments", "56", "0-127", "Emulates FS8", "HX Stomp XL", ""],
  ["One Switch Looper Controls", "60", "0-63: Overdub; 64-127: Record", "1 Switch Looper Record/Overdub", "", ""],
  ["One Switch Looper Controls", "61", "0-63: Stop; 64-127: Play", "1 Switch Looper Play/Stop", "", ""],
  ["One Switch Looper Controls", "62", "64-127", "1 Switch Looper Play Once", "", ""],
  ["One Switch Looper Controls", "63", "64-127", "1 Switch Looper Undo/Redo", "", ""],
  ["One Switch Looper Controls", "65", "0-63: Forward; 64-127: Reverse", "1 Switch Looper Forward/Reverse", "", ""],
  ["One Switch Looper Controls", "66", "0-63: Full; 64-127: Half", "Looper Full/Half Speed", "", ""],
  ["Additional Controls", "0", "N/A", "Bank MSB (unused but reserved)", "", ""],
  ["Additional Controls", "3", "0-127", "Additional global MIDI commands (reserved for future use)", "", ""],
  ["Additional Controls", "32", "N/A", "Bank LSB (unused but reserved)", "", ""],
  ["Additional Controls", "54-59", "0-127", "Additional global MIDI commands (reserved for future use)", "", ""],
  ["Additional Controls", "64", "64-127", "Tap Tempo", "", ""],
  ["Additional Controls", "67", "0-127", "Additional global MIDI commands (reserved for future use)", "", ""],
  ["Additional Controls", "68", "0-127", "Tuner screen on/off", "", ""],
  [
    "Additional Controls",
    "69",
    "0-2, 8 and 9",
    "Snapshot select",
    "",
    "0=Snapshot 1, 1=Snapshot 2, 2=Snapshot 3, 8=Next snapshot, 9=Previous snapshot",
  ],
  ["Additional Controls", "70", "0-63: Bypass; 64-127: On", "All Bypass", "", ""],
  [
    "Additional Controls",
    "71",
    "0-5",
    "Footswitch Mode",
    "",
    "0=Stomp, 1=Scroll, 2=Preset, 3=Snapshot, 4=Next footswitch mode, 5=Previous footswitch mode",
  ],
  ["Additional Controls", "72", "0-63, 64-127", "Previous Preset, Next Preset", "", ""],
  ["Additional Controls", "73-76", "0-127", "Additional global MIDI commands (reserved for future use)", "", ""],
  [
    "Additional Controls",
    "128",
    "0-63: Jealous; 64-127: Codependent",
    "HX Stomp and HX Stomp XL send late-night texts to all of your ex-significant others, professing your undying love to them and their cats",
    "",
    "As listed on source page; outside standard MIDI CC range",
  ],
];

const wb = Workbook.create();
const sheet = wb.worksheets.add("HX Stomp MIDI CC");
sheet.showGridLines = false;
sheet.getRangeByIndexes(0, 0, rows.length, rows[0].length).values = rows;

sheet.getRangeByIndexes(0, 0, rows.length, rows[0].length).format = {
  font: { name: "Aptos", size: 10 },
  wrapText: true,
  verticalAlignment: "top",
};
sheet.getRange("A1:F1").format = {
  fill: "#1F4E78",
  font: { bold: true, color: "#FFFFFF" },
  horizontalAlignment: "center",
  verticalAlignment: "middle",
  wrapText: true,
};
sheet.freezePanes.freezeRows(1);

const table = sheet.tables.add(`A1:F${rows.length}`, true, "HxStompMidiCcTable");
table.style = "TableStyleMedium2";
table.showFilterButton = true;

sheet.getRange("A:A").format.columnWidthPx = 205;
sheet.getRange("B:B").format.columnWidthPx = 70;
sheet.getRange("C:C").format.columnWidthPx = 215;
sheet.getRange("D:D").format.columnWidthPx = 345;
sheet.getRange("E:E").format.columnWidthPx = 105;
sheet.getRange("F:F").format.columnWidthPx = 365;
sheet.getRange("1:1").format.rowHeightPx = 28;
sheet.getRange(`A2:F${rows.length}`).format.rowHeightPx = 42;

const notes = wb.worksheets.add("Source Notes");
notes.showGridLines = false;
notes.getRange("A1:B5").values = [
  ["Source", "Helix Help - MIDI"],
  ["URL", "https://helixhelp.com/tips-and-guides/universal/midi#hx-stomp-hx-stomp-xl"],
  ["Section", "HX Stomp, HX Stomp XL > MIDI CC"],
  ["Created", new Date("2026-05-17T00:00:00")],
  ["Notes", "Rows are transcribed from the HX Stomp/HX Stomp XL MIDI CC tables. Device column is included where the source lists it."],
];
notes.getRange("A1:A5").format = {
  fill: "#1F4E78",
  font: { bold: true, color: "#FFFFFF" },
};
notes.getRange("A1:B5").format = { wrapText: true, verticalAlignment: "top" };
notes.getRange("A:A").format.columnWidthPx = 110;
notes.getRange("B:B").format.columnWidthPx = 700;

const tableCheck = await wb.inspect({
  kind: "table",
  range: "HX Stomp MIDI CC!A1:F30",
  include: "values",
  tableMaxRows: 35,
  tableMaxCols: 6,
  maxChars: 12000,
});
console.log(tableCheck.ndjson);

const errors = await wb.inspect({
  kind: "match",
  searchTerm: "#REF!|#DIV/0!|#VALUE!|#NAME\\?|#N/A",
  options: { useRegex: true, maxResults: 50 },
  summary: "formula error scan",
});
console.log(errors.ndjson);

const preview = await wb.render({
  sheetName: "HX Stomp MIDI CC",
  range: "A1:F30",
  scale: 1,
  format: "png",
});
await fs.writeFile(
  path.join(outputDir, "hx_stomp_midi_cc_preview.png"),
  new Uint8Array(await preview.arrayBuffer()),
);

const xlsx = await SpreadsheetFile.exportXlsx(wb);
const outPath = path.join(outputDir, "HX_Stomp_MIDI_CC.xlsx");
await xlsx.save(outPath);
console.log(`Saved ${outPath}`);
