#!/usr/bin/env node

/**
 * configure-core.mjs — cross-platform CAD core CMake configuration.
 *
 * Invokes cmake with platform-appropriate toolchain settings:
 *  - Windows: vcpkg toolchain + prefix path
 *  - Linux / macOS: system packages (no extra cmake args needed)
 *
 * Called from `pnpm core:configure`.
 */

import { dirname, join, resolve } from "node:path";
import { spawnSync } from "node:child_process";
import { fileURLToPath } from "node:url";

// ---------------------------------------------------------------------------
// paths
// ---------------------------------------------------------------------------

const root = resolve(dirname(fileURLToPath(import.meta.url)), "..");
const coreSrc = join(root, "native", "cad-core");
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

console.log("=== PolySmith — CAD core configuration ===\n");
console.log(`Platform : ${process.platform}`);

const args = ["-S", coreSrc, "-B", coreBuild];

if (isWindows) {
  const vcpkgRoot = process.env.VCPKG_ROOT || "C:/SRC/vcpkg";
  const vcpkgInstalled = process.env.VCPKG_INSTALLED || "C:/SRC/vcpkg/installed/x64-windows";

  args.push(`-DCMAKE_TOOLCHAIN_FILE=${vcpkgRoot}/scripts/buildsystems/vcpkg.cmake`);
  args.push(`-DCMAKE_PREFIX_PATH=${vcpkgInstalled}`);
}

run("cmake", args);

console.log("\n✅  CAD core configured successfully.");
console.log("    Next: pnpm core:build");
