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

const checks = [
  ["OCCT headers in protocol/ or core/document/", checkOcctHeaders],
  ["make_document_state_event from a read-only handler", checkReadOnlyEmitter],
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
