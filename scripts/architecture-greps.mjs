#!/usr/bin/env node
// Executable architecture rules (Refactoring Plan, Stage 4).
//
// Three checks that turn "React must not own CAD state" and friends from
// prose into a tripwire CI can run:
//
//   1. no OCCT headers under `protocol/` or `core/document/`
//   2. no `make_document_state_event` from a command handler that only READ
//      the document (the sole legitimate read-only emitter is
//      `get_document_state`)
//
// The planegcs WASM solver in the UI is deliberately NOT checked: it is the
// correct interactive-constraint-solving design (over IPC it would be too
// slow for live drag-solving). The authoritative solve runs in the core.
//
// Usage:
//   node scripts/architecture-greps.mjs
//
// Exit code 0 when every check passes, 1 otherwise.

import { readFileSync, readdirSync, statSync } from "node:fs";
import { dirname, join, relative, resolve } from "node:path";
import { fileURLToPath } from "node:url";

const ROOT = resolve(dirname(fileURLToPath(import.meta.url)), "..");

function* walk(dir) {
  for (const entry of readdirSync(dir)) {
    const path = join(dir, entry);
    if (statSync(path).isDirectory()) {
      yield* walk(path);
    } else {
      yield path;
    }
  }
}

const rel = (p) => relative(ROOT, p).replace(/\\/g, "/");

function read(file) {
  return readFileSync(file, "utf8");
}

// ── Check 1: OCCT headers in protocol/ or core/document/ ────────────────
// OCCT headers are `.hxx` and start with an uppercase letter. The document
// layer should be pure data; OCCT geometry belongs in core/geometry/.
const OCCT = /#include\s*<[A-Z][A-Za-z0-9_]*\.hxx>/;
function checkOcctHeaders() {
  const dirs = [
    join(ROOT, "native", "cad-core", "src", "protocol"),
    join(ROOT, "native", "cad-core", "src", "core", "document"),
  ];
  const violations = [];
  for (const dir of dirs) {
    for (const file of walk(dir)) {
      read(file).split("\n").forEach((line, i) => {
        if (OCCT.test(line)) violations.push(`${rel(file)}:${i + 1}: ${line.trim()}`);
      });
    }
  }
  return violations;
}

// ── Check 2: document-state event from a read-only handler ────────────────
// A handler that only called GETTERS on the document manager and then emitted a
// full `make_document_state_event` did not change anything; the sole legitimate
// read-only emitter is the explicit `get_document_state` fetcher. The handlers
// now live as promoted `app/*.cpp` units, each taking a `DocumentManager&`.
const GETTER = /manager\.(get_|find_|is_|has_|current_|active_|session_state|document\b|compiled|resolve|compute)/;
function checkReadOnlyEmitter() {
  const dir = join(ROOT, "native", "cad-core", "src", "app");
  const violations = [];
  for (const file of walk(dir)) {
    if (!file.endsWith(".cpp")) continue;
    const content = read(file);
    // Split into per-command handler blocks so one file can hold many handlers.
    const blocks = content.split(/if \(command\.type == /).slice(1);
    for (const block of blocks) {
      const commandType = (block.match(/^"([^"]+)"/) || [])[1];
      if (!block.includes("make_document_state_event")) continue;
      if (commandType === "get_document_state") continue;

      const calls = block.match(/manager\.[A-Za-z_]\w*/g) || [];
      const mutates = calls.some((call) => !GETTER.test(call)) ||
                      /\b(push_undo_state|clear_redo_stack|bump_geometry_revision)\b/.test(block);
      const reads = block.includes("manager.get_document()");
      if (!mutates && reads) {
        violations.push(`${rel(file)}: ${commandType}`);
      }
    }
  }
  return violations;
}

// ── Check 3: dispatched commands match protocol/schema/commands.schema.json ──
// `CadCoreApp::handle_command_line` chains the per-area handlers with no
// fallback, so the JSON schema and the dispatch table are two halves of one
// contract:
//   * a command the schema allows but nothing handles produces no event at all,
//     and the UI's awaited promise never settles;
//   * a command the app sends but the schema rejects fails any validating
//     client (the five sketch commands this check was written for).
// Both directions are checked. `shutdown` is special-cased in
// protocol/ipc.cpp (empty-id carve-out) rather than dispatched, so it is
// allowed to appear in the schema only.
//
// The scan is global rather than block-split so that `||` chains (one handler
// covering two commands) and `!=` early-return guards (trim_preview) are both
// seen; block-splitting on `if (command.type == ` missed those.
const SCHEMA_SPECIAL_CASES = new Set(["shutdown"]);

function checkCommandSchema() {
  const dir = join(ROOT, "native", "cad-core", "src", "app");
  const dispatched = new Set();
  for (const file of walk(dir)) {
    if (!file.endsWith(".cpp")) continue;
    const content = read(file);
    for (const match of content.matchAll(/command\.type\s*(?:==|!=)\s*"([a-z0-9_]+)"/g)) {
      dispatched.add(match[1]);
    }
  }

  const schemaPath = join(ROOT, "protocol", "schema", "commands.schema.json");
  const schema = JSON.parse(read(schemaPath));
  const allowed = new Set(schema.properties.type.enum);

  const violations = [];
  for (const name of [...dispatched].sort()) {
    if (!allowed.has(name)) {
      violations.push(`dispatched but missing from commands.schema.json: ${name}`);
    }
  }
  for (const name of [...allowed].sort()) {
    if (!dispatched.has(name) && !SCHEMA_SPECIAL_CASES.has(name)) {
      violations.push(`in commands.schema.json but never dispatched: ${name}`);
    }
  }
  return violations;
}

// ── Check 4: horizontal option rows must be wrap-safe ────────────────────
// Panels are narrow (the sketch tool panel is w-72 = 288 px) and labels come
// from i18n, so an English layout that fits can clip in German or Japanese.
// A row of buttons/inputs laid out with `flex gap-2` must therefore allow
// wrapping instead of overflowing. This is the class of bug that pushed
// "Infinite" out of the line-type row (2026-10-24).
const OPTION_ROW = /(^|\s)flex(\s|$)/;
// Rows of side-by-side choices: a plain flex row with a 0.5rem gap. Checkbox
// rows (items-center), right-aligned dialog footers (justify-end), context-menu
// items, stacked columns and toasts are not option rows and are skipped — their
// labels are short or they wrap on their own.
const NOT_OPTION_ROW =
  /flex-col|items-center|items-end|items-start|justify-end|justify-between|cad-context-menu-item|pointer-events-none/;
function checkWrappingOptionRows() {
  const dir = join(ROOT, "apps", "desktop-ui", "src", "layout");
  const violations = [];
  for (const file of walk(dir)) {
    if (!file.endsWith(".tsx")) continue;
    const lines = read(file).split("\n");
    lines.forEach((line, i) => {
      for (const match of line.matchAll(/className=(?:"([^"]*)"|`([^`]*)`)/g)) {
        const classes = match[1] ?? match[2] ?? "";
        if (!OPTION_ROW.test(classes)) continue;
        if (!(/(^|\s)gap-2(\s|$)/.test(classes))) continue;
        if (NOT_OPTION_ROW.test(classes)) continue;
        if (/\bflex-wrap\b/.test(classes)) continue;
        violations.push(`${rel(file)}:${i + 1}: ${classes}`);
      }
    });
  }
  return violations;
}

const checks = [
  ["OCCT headers in protocol/ or core/document/", checkOcctHeaders],
  ["make_document_state_event from a read-only handler", checkReadOnlyEmitter],
  ["command dispatch matches commands.schema.json", checkCommandSchema],
  ["layout option rows are wrap-safe (i18n overflow)", checkWrappingOptionRows],
];

let failed = false;
for (const [label, run] of checks) {
  const violations = run();
  if (violations.length === 0) {
    console.log(`PASS  ${label}`);
  } else {
    failed = true;
    console.log(`FAIL  ${label}  (${violations.length})`);
    for (const v of violations.slice(0, 40)) {
      console.log(`        ${v}`);
    }
    if (violations.length > 40) {
      console.log(`        ... and ${violations.length - 40} more`);
    }
  }
}

process.exit(failed ? 1 : 0);
