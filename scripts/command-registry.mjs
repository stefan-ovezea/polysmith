#!/usr/bin/env node
// Command registry generator (Refactoring Plan, Stage 4 item 3).
//
// Scans the app command handlers and emits a table of every
// `command.type == "..."` string alongside the payload keys it reads through
// the `read_*` helpers — the foundation for a single command surface (Lessons
// §5): generated from the code, so it cannot drift from the implementation.
//
// Commands whose whole payload is parsed by a `*_from_payload` helper are
// listed with `(from_payload)` instead of individual keys; those helpers carry
// the authoritative field lists.
//
// Usage:
//   node scripts/command-registry.mjs              # markdown table on stdout
//   node scripts/command-registry.mjs --json       # machine-readable JSON

import { readFileSync, readdirSync, statSync, writeFileSync } from "node:fs";
import { dirname, join, relative, resolve } from "node:path";
import { fileURLToPath } from "node:url";

const ROOT = resolve(dirname(fileURLToPath(import.meta.url)), "..");
const DIR = join(ROOT, "native", "cad-core", "src", "app");

function* files() {
  for (const entry of readdirSync(DIR)) {
    if (!entry.endsWith(".cpp")) continue;
    const path = join(DIR, entry);
    if (statSync(path).isFile()) yield path;
  }
}

// read_dimension(command.payload, "key")
const READ = /read_(\w+)\(command\.payload,\s*"([^"]+)"/g;
// <name>_from_payload(command.payload)  /  command.payload
const FROM_PAYLOAD = /(\w+_from_payload)\(command\.payload\)/;

const commands = new Map();

for (const file of files()) {
  const content = readFileSync(file, "utf8");
  const blocks = content.split(/if \(command\.type == /).slice(1);
  for (const block of blocks) {
    const command = (block.match(/^"([^"]+)"/) || [])[1];
    if (!command) continue;

    const reads = new Map(); // key -> reader kind
    for (const match of block.matchAll(READ)) {
      reads.set(match[2], match[1]);
    }
    const fromPayload = FROM_PAYLOAD.test(block);

    if (!commands.has(command)) {
      commands.set(command, { file: relative(ROOT, file).replace(/\\/g, "/"), reads: new Map(), fromPayload: false });
    }
    const entry = commands.get(command);
    for (const [key, kind] of reads) entry.reads.set(key, kind);
    entry.fromPayload = entry.fromPayload || fromPayload;
  }
}

const rows = [...commands.entries()].sort((a, b) => a[0].localeCompare(b[0]));

const writePath = (() => {
  const idx = process.argv.indexOf("--write");
  return idx >= 0 && idx + 1 < process.argv.length ? process.argv[idx + 1] : null;
})();

if (process.argv.includes("--json")) {
  const out = Object.fromEntries(rows.map(([command, entry]) => [
    command,
    {
      file: entry.file,
      keys: [...entry.reads.entries()].map(([key, kind]) => ({ key, read: kind })),
      from_payload: entry.fromPayload,
    },
  ]));
  console.log(JSON.stringify(out, null, 2));
} else {
  const lines = [];
  lines.push("# Command Reference (generated)");
  lines.push("");
  lines.push("> Auto-generated from the app command handlers by");
  lines.push("> `node scripts/command-registry.mjs --write <path>`.");
  lines.push("> Do not edit by hand — edits would drift from the code.");
  lines.push("");
  lines.push("| command | keys |");
  lines.push("|---|---|");
  for (const [command, entry] of rows) {
    const keys = [...entry.reads.entries()]
      .map(([key, kind]) => `${key} (${kind})`)
      .join(", ");
    const extra = entry.fromPayload ? (keys ? " + from_payload" : "from_payload") : "";
    lines.push(`| \`${command}\` | ${keys}${extra} |`);
  }
  const markdown = lines.join("\n") + "\n";
  if (writePath) {
    writeFileSync(join(ROOT, writePath), markdown, "utf8");
    console.error(`Wrote ${commands.size} commands to ${writePath}.`);
  } else {
    console.log(markdown);
  }
}

console.error(`\n${commands.size} commands scanned.`);
