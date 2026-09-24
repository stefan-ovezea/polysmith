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
import { fileURLToPath } from "node:url";
import { cmake } from "./find-cmake.mjs";
import { run, findVcpkgRoot } from "./cmake-utils.mjs";

// ---------------------------------------------------------------------------
// paths
// ---------------------------------------------------------------------------

const root = resolve(dirname(fileURLToPath(import.meta.url)), "..");
const coreSrc = join(root, "native", "cad-core");
const coreBuild = join(root, "native", "cad-core", "build");
const isWindows = process.platform === "win32";

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
  // the known standalone locations win over the environment variable.
  vcpkgRoot = findVcpkgRoot();
  if (!vcpkgRoot) {
    console.error(
      "\n❌  vcpkg not found.\n" +
      "    The CAD core needs Boost + Eigen3 from vcpkg on Windows:\n" +
      "      git clone https://github.com/microsoft/vcpkg C:/vcpkg\n" +
      "      C:/vcpkg/bootstrap-vcpkg.bat\n" +
      "      C:/vcpkg/vcpkg.exe install boost eigen3 --triplet x64-windows\n" +
      "    (or set VCPKG_ROOT to an existing vcpkg checkout).\n" +
      "    See CLAUDE.md Build Troubleshooting."
    );
    process.exit(1);
  }
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
run(cmake, args, isWindows ? { cwd: root, env: { VCPKG_ROOT: vcpkgRoot } } : { cwd: root });

console.log("\n✅  CAD core configured successfully.");
console.log("    Next: pnpm core:build");
