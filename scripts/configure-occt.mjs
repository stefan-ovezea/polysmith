#!/usr/bin/env node

/**
 * configure-occt.mjs — cross-platform OCCT CMake configuration.
 *
 * Handles:
 *  - Disabling TCL/TK (not needed by the CAD core, avoids build errors on all
 *    platforms)
 *  - Building the vendored FreeType on Windows (system FreeType is rarely
 *    available) and pointing OCCT at it
 *  - Using system FreeType on Linux / macOS
 *
 * Called from `pnpm occt:configure`.
 */

import { existsSync, mkdirSync } from "node:fs";
import { dirname, join, resolve } from "node:path";
import { spawnSync } from "node:child_process";
import { fileURLToPath } from "node:url";

// ---------------------------------------------------------------------------
// paths
// ---------------------------------------------------------------------------

const root = resolve(dirname(fileURLToPath(import.meta.url)), "..");
const isWindows = process.platform === "win32";

const occtSrc = join(root, "third_party", "occt");
const occtBuild = join(root, "third_party", "occt-build");
const occtInstall = join(root, "third_party", "occt-install");
const freetypeSrc = join(root, "third_party", "freetype");
const freetypeBuild = join(root, "third_party", "freetype-build");
const freetypeInstall = join(root, "third_party", "freetype-install");

// ---------------------------------------------------------------------------
// helpers
// ---------------------------------------------------------------------------

function run(command, args, opts = {}) {
  const { cwd = root, env: extraEnv, silent = false } = opts;
  const env = { ...process.env, ...extraEnv };

  if (isWindows) {
    // Manual quoting for cmd.exe: arguments with spaces need double-quotes.
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

function cmake(srcDir, buildDir, defines = {}, extraArgs = []) {
  const args = [
    "-S", srcDir,
    "-B", buildDir,
  ];

  // generator — NMake on Windows, default (Unix Makefiles) elsewhere
  if (isWindows) {
    args.push("-G", "NMake Makefiles");
  }

  for (const [key, value] of Object.entries(defines)) {
    args.push(`-D${key}=${value}`);
  }

  args.push(...extraArgs);
  run("cmake", args);
}

function cmakeBuild(buildDir, config = "Release") {
  run("cmake", ["--build", buildDir, "--config", config]);
}

function cmakeInstall(buildDir, config = "Release") {
  run("cmake", ["--install", buildDir, "--config", config]);
}

// ---------------------------------------------------------------------------
// platform helpers
// ---------------------------------------------------------------------------

/**
 * Check whether a system FreeType is available (Linux / macOS only).
 */
function systemFreetypeAvailable() {
  if (isWindows) return false;

  // try pkg-config first
  const pkg = spawnSync("pkg-config", ["--exists", "freetype2"], {
    cwd: root,
    stdio: "pipe",
    shell: false,
  });
  if (pkg.status === 0) return true;

  // fallback: check for the header in common locations
  const headerPaths = [
    "/usr/include/ft2build.h",
    "/usr/local/include/ft2build.h",
    "/usr/include/freetype2/ft2build.h",
  ];
  for (const p of headerPaths) {
    if (existsSync(p)) return true;
  }

  return false;
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------

console.log("=== PolySmith — OCCT configuration ===\n");
console.log(`Platform : ${process.platform}`);
console.log(`OCCT src : ${occtSrc}`);
console.log(`Build    : ${occtBuild}`);
console.log(`Install  : ${occtInstall}`);

// ---- FreeType -----------------------------------------------------------

let freetypeDir = "";

if (systemFreetypeAvailable()) {
  console.log("\n📦  Using system FreeType.");
  // Leave freetypeDir empty — OCCT's built-in search will find it.
} else {
  console.log("\n📦  Building vendored FreeType 2.14.3 …");

  if (!existsSync(freetypeSrc)) {
    console.error("❌  Vendored FreeType not found at", freetypeSrc);
    console.error("    Run `pnpm deps:sync` first to pull the FreeType submodule.");
    process.exit(1);
  }

  mkdirSync(freetypeBuild, { recursive: true });

  cmake(freetypeSrc, freetypeBuild, {
    CMAKE_BUILD_TYPE: "Release",
    CMAKE_INSTALL_PREFIX: freetypeInstall,
    // static lib — OCCT can link against it on both Windows and Linux
    BUILD_SHARED_LIBS: "OFF",
  });

  cmakeBuild(freetypeBuild);
  cmakeInstall(freetypeBuild);
  freetypeDir = freetypeInstall;

  console.log("✅  FreeType built and installed to", freetypeInstall);
}

// ---- OCCT ---------------------------------------------------------------

console.log("\n⚙️  Configuring OpenCascade …");

const occtDefines = {
  CMAKE_BUILD_TYPE: "Release",
  CMAKE_INSTALL_PREFIX: occtInstall,

  // Neither the CAD core nor the desktop app use TCL/TK.
  // Disabling avoids build errors on platforms where TCL libraries
  // are missing or incompatible.
  USE_TCL: "OFF",
  USE_TK: "OFF",
  BUILD_MODULE_Draw: "OFF",

  // Only enable FreeType if we have it (system or vendored).
  USE_FREETYPE: freetypeDir || systemFreetypeAvailable() ? "ON" : "OFF",
};

const occtExtraArgs = [];
if (freetypeDir) {
  occtExtraArgs.push(`-D3RDPARTY_FREETYPE_DIR=${freetypeDir}`);
}

cmake(occtSrc, occtBuild, occtDefines, occtExtraArgs);

console.log("\n✅  OCCT configured successfully.");
console.log("    Next: pnpm occt:build");
console.log("    Then: pnpm occt:install");
