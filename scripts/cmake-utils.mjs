#!/usr/bin/env node

/**
 * cmake-utils.mjs — shared helpers for the build scripts.
 *
 * `run()` deliberately does NOT use shell:true on Windows.  Node builds
 * a correctly quoted CreateProcess command line for .exe targets itself
 * (paths with spaces, &, ^, % are all safe), while shell:true + manual
 * cmd.exe quoting is the classic quoting trap — one path containing "&"
 * or "%" silently corrupts the command.  `find-cmake.mjs` already
 * resolves cmake to a full .exe path (the VS-bundled one on Windows),
 * so the command is always directly spawnable.
 *
 * Imported by configure-core.mjs / build-core.mjs / configure-occt.mjs /
 * cmake-run.mjs / preflight.mjs.
 */

import { existsSync } from "node:fs";
import { spawnSync } from "node:child_process";
import { join } from "node:path";
import { cmake } from "./find-cmake.mjs";

export function run(command, args, opts = {}) {
  const { cwd = process.cwd(), env: extraEnv, silent = false } = opts;
  const env = { ...process.env, ...extraEnv };

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

export function runCmake(args, opts = {}) {
  return run(cmake, args, opts);
}

/**
 * Locate the vcpkg root that provides the packages the CAD core needs
 * (Boost, Eigen3) on Windows.  VS 2022 auto-injects its own bundled
 * vcpkg (via VCPKG_ROOT) which lacks packages — so the well-known
 * standalone locations win over the environment variable.
 *
 * Returns "" when no usable vcpkg is found (the caller decides how to
 * fail — configure-core and preflight both want their own message).
 */
export function findVcpkgRoot() {
  if (process.platform !== "win32") return "";
  const candidates = [
    "C:/vcpkg",
    "C:/SRC/vcpkg",
    process.env.VCPKG_ROOT,
  ].filter(Boolean);
  return candidates.find((dir) =>
    existsSync(join(dir, "scripts", "buildsystems", "vcpkg.cmake"))
  ) ?? "";
}
