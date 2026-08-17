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

import { existsSync } from "node:fs";
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
let vcpkgRoot = "";

if (isWindows) {
  // Auto-detect the vcpkg root that has the packages we need (Boost, Eigen3).
  // VS 2022 sets VCPKG_ROOT to its own bundled copy which lacks packages;
  // only use it as a last resort, not as the first choice.
  vcpkgRoot = (existsSync("C:/vcpkg/scripts/buildsystems/vcpkg.cmake") ? "C:/vcpkg" : "")
    || (existsSync("C:/SRC/vcpkg/scripts/buildsystems/vcpkg.cmake") ? "C:/SRC/vcpkg" : "")
    || process.env.VCPKG_ROOT
    || "";
  const vcpkgInstalled = process.env.VCPKG_INSTALLED || `${vcpkgRoot}/installed/x64-windows`;

  // VS 2022 auto-injects its bundled vcpkg toolchain, which can shadow
  // our explicit -DCMAKE_TOOLCHAIN_FILE.  Set VCPKG_ROOT in the
  // environment so the toolchain (whichever one loads) finds the right
  // installed packages.
  args.push(`-DCMAKE_TOOLCHAIN_FILE=${vcpkgRoot}/scripts/buildsystems/vcpkg.cmake`);
  args.push(`-DCMAKE_PREFIX_PATH=${vcpkgInstalled}`);
  // Belt-and-suspenders: explicit package hints so find_package works
  // even if the wrong vcpkg toolchain loads first.
  args.push(`-DBoost_DIR=${vcpkgInstalled}/share/boost`);
  args.push(`-DEigen3_DIR=${vcpkgInstalled}/share/eigen3`);
}

// VS 2022 auto-injects its bundled vcpkg toolchain.  Set VCPKG_ROOT
// in the process environment so the toolchain (whichever one loads)
// finds the correct installed packages.
run("cmake", args, isWindows ? { env: { VCPKG_ROOT: vcpkgRoot } } : {});

console.log("\n✅  CAD core configured successfully.");
console.log("    Next: pnpm core:build");
