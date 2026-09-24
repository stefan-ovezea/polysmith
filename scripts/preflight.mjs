#!/usr/bin/env node

/**
 * preflight.mjs — environment check that runs before the bootstrap
 * chain.  Fresh installs fail most often on environment mismatches
 * (wrong pnpm, missing git/CMake, uninitialized submodules, missing
 * vcpkg on Windows) — and on a machine without the prerequisites the
 * failure can surface far from the real cause (e.g. a native crash in
 * `pnpm install`, or a mid-build CMake error half an hour in).  This
 * reports each requirement with the actual resolved value BEFORE any
 * build step starts, so the failure is self-describing.
 *
 * Exit code 1 = hard blocker (no git / no CMake / no vcpkg on
 * Windows).  Everything else is reported as a warning and left to the
 * real step.
 */

import { existsSync, readFileSync, readdirSync } from "node:fs";
import { spawnSync } from "node:child_process";
import { dirname, join, resolve } from "node:path";
import { fileURLToPath } from "node:url";
import { cmake } from "./find-cmake.mjs";
import { findVcpkgRoot } from "./cmake-utils.mjs";

const root = resolve(dirname(fileURLToPath(import.meta.url)), "..");
const isWindows = process.platform === "win32";

let failed = false;

function fail(msg) {
  console.error(`❌  ${msg}`);
  failed = true;
}

function warn(msg) {
  console.log(`⚠️  ${msg}`);
}

function versionOf(command, args = ["--version"]) {
  const result = spawnSync(command, args, { encoding: "utf8" });
  if (result.error) return ""; // not on PATH
  return (result.stdout ?? "").trim().split(/\r?\n/)[0];
}

console.log("=== PolySmith bootstrap preflight ===\n");

console.log(`platform : ${process.platform} ${process.arch}`);
console.log(`node     : ${versionOf("node") || "?"}`);

// pnpm — report what the bootstrap chain will actually run, and flag a
// version that ignores the packageManager pin (corepack resolves the
// pin; a globally installed pnpm runs its own version).
const pmField = JSON.parse(readFileSync(join(root, "package.json"), "utf-8")).packageManager;
const pnpmVersion = versionOf("pnpm");
if (pnpmVersion) {
  console.log(`pnpm     : ${pnpmVersion}`);
  if (!pnpmVersion.startsWith(pmField.split("@").join(" "))) {
    warn(`running pnpm ${pnpmVersion}, package.json pins ${pmField} — ` +
      `enable corepack (corepack enable) to use the pinned version.`);
  }
} else {
  warn("no pnpm on PATH — corepack will resolve the packageManager pin.");
}

// git + submodules (the deps:sync step).
const gitVersion = versionOf("git");
if (!gitVersion) {
  fail("git not found on PATH — `pnpm deps:sync` (submodules) cannot run.");
} else {
  console.log(`git      : ${gitVersion}`);
  const gitmodules = readFileSync(join(root, ".gitmodules"), "utf-8");
  const submodulePaths = [...gitmodules.matchAll(/^\s*path\s*=\s*(.+)$/gm)].map((m) => m[1].trim());
  const empty = submodulePaths.filter((p) => {
    const dir = join(root, p);
    return !existsSync(dir) || readdirSync(dir).length === 0;
  });
  if (empty.length > 0) {
    warn(`submodules not initialized: ${empty.join(", ")} — pnpm deps:sync will pull them.`);
  } else {
    console.log(`✅  all ${submodulePaths.length} submodules initialized`);
  }
}

// CMake — the same resolution the build scripts use (VS-bundled CMake
// on Windows when no standalone CMake is on PATH).
console.log(`cmake    : ${cmake} (${versionOf(cmake)})`);

if (isWindows) {
  const vcpkg = findVcpkgRoot();
  if (!vcpkg) {
    fail("vcpkg not found — the core needs Boost + Eigen3 on Windows.\n" +
      "         Install vcpkg (see CLAUDE.md Build Troubleshooting) or set VCPKG_ROOT.");
  } else {
    console.log(`vcpkg    : ${vcpkg}`);
  }

  const vswhere = "C:\\Program Files (x86)\\Microsoft Visual Studio\\Installer\\vswhere.exe";
  if (!existsSync(vswhere)) {
    fail("Visual Studio not found (vswhere.exe missing) — OCCT and the core need the MSVC toolchain.");
  }
}

if (failed) {
  console.error("\nPreflight failed — fix the ❌ items above, then re-run pnpm bootstrap.");
  process.exit(1);
}

console.log("\nPreflight OK — starting the bootstrap chain.\n");
