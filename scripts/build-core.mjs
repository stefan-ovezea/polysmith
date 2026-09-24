#!/usr/bin/env node

/**
 * build-core.mjs — cross-platform CAD core build (Release).
 *
 * Invokes `cmake --build` with parallel jobs defaulting to the CPU count
 * (override with CAD_CORE_JOBS, e.g. CAD_CORE_JOBS=6 pnpm core:build for
 * constrained machines).  Deliberately does NOT pass --target: with the
 * Visual Studio generator `all` is not a solution project (MSB1009) and
 * the default ALL_BUILD target is what we want.
 *
 * Called from `pnpm core:build`.
 */

import os from "node:os";
import { dirname, join, resolve } from "node:path";
import { fileURLToPath } from "node:url";
import { runCmake } from "./cmake-utils.mjs";

// ---------------------------------------------------------------------------
// paths
// ---------------------------------------------------------------------------

const root = resolve(dirname(fileURLToPath(import.meta.url)), "..");
const coreBuild = join(root, "native", "cad-core", "build");

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------

console.log("=== PolySmith — CAD core build ===\n");
console.log(`Platform : ${process.platform}`);

const requested = Number.parseInt(process.env.CAD_CORE_JOBS ?? "", 10);
const jobs = Number.isFinite(requested) && requested > 0
  ? requested
  : Math.max(1, os.cpus().length);
console.log(`Jobs     : ${jobs}`);

runCmake([
  "--build", coreBuild,
  "--config", "Release",
  "--parallel", String(jobs),
], { cwd: root });

console.log("\n✅  CAD core built successfully.");
