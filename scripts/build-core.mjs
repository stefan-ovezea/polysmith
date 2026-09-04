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
import { spawnSync } from "node:child_process";
import { dirname, join, resolve } from "node:path";
import { fileURLToPath } from "node:url";

// ---------------------------------------------------------------------------
// paths
// ---------------------------------------------------------------------------

const root = resolve(dirname(fileURLToPath(import.meta.url)), "..");
const coreBuild = join(root, "native", "cad-core", "build");
const isWindows = process.platform === "win32";

// ---------------------------------------------------------------------------
// helpers
// ---------------------------------------------------------------------------

function run(command, args, opts = {}) {
  const { cwd = root, env: extraEnv, silent = false } = opts;
  const env = { ...process.env, ...extraEnv };

  if (isWindows) {
    const quoted = args.map((a) => (a.includes(" ") ? `"${a}"` : a));
    const cmdline = [command, ...quoted].join(" ");
    console.log(`\n> ${cmdline}`);
    const result = spawnSync(cmdline, [], {
      cwd,
      env,
      stdio: silent ? "pipe" : "inherit",
      shell: true,
    });
    if (result.status !== 0) {
      console.error(`\n❌  Command failed with exit code ${result.status}`);
      process.exit(result.status ?? 1);
    }
    return result;
  }

  console.log(`\n> ${command} ${args.join(" ")}`);
  const result = spawnSync(command, args, {
    cwd,
    env,
    stdio: silent ? "pipe" : "inherit",
  });
  if (result.status !== 0) {
    console.error(`\n❌  Command failed with exit code ${result.status}`);
    process.exit(result.status ?? 1);
  }
  return result;
}

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

run("cmake", [
  "--build", coreBuild,
  "--config", "Release",
  "--parallel", String(jobs),
]);

console.log("\n✅  CAD core built successfully.");
