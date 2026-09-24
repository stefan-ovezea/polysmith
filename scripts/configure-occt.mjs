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

import { existsSync, mkdirSync, readFileSync, rmSync } from "node:fs";
import { dirname, join, resolve } from "node:path";
import { spawnSync } from "node:child_process";
import { fileURLToPath } from "node:url";
import { cmake } from "./find-cmake.mjs";
import { run } from "./cmake-utils.mjs";

// ---------------------------------------------------------------------------
// paths
// ---------------------------------------------------------------------------

const root = resolve(dirname(fileURLToPath(import.meta.url)), "..");
const isWindows = process.platform === "win32";

const occtSrc = join(root, "third_party", "occt8");
const occtBuild = join(root, "third_party", "occt8-build");
const occtInstall = join(root, "third_party", "occt8-install");
const freetypeSrc = join(root, "third_party", "freetype");
const freetypeBuild = join(root, "third_party", "freetype-build");
const freetypeInstall = join(root, "third_party", "freetype-install");

// ---------------------------------------------------------------------------
// helpers
// ---------------------------------------------------------------------------

/**
 * Pick the Visual Studio generator for the OCCT configure step.
 *
 * Re-configures must reuse the generator recorded in an existing cache
 * (a machine that first built with VS 2022 must keep using it even if a
 * newer VS is installed side by side — CMake refuses a generator change
 * on a populated build dir).  On a fresh build dir, ask vswhere which
 * VS is actually installed instead of assuming 2022.
 */
function visualStudioGenerator() {
  const cacheFile = join(occtBuild, "CMakeCache.txt");
  if (existsSync(cacheFile)) {
    const cached = /^CMAKE_GENERATOR:INTERNAL=(.*)$/m.exec(readFileSync(cacheFile, "utf-8"));
    if (cached && cached[1]) return cached[1];
  }

  const vswhere = "C:\\Program Files (x86)\\Microsoft Visual Studio\\Installer\\vswhere.exe";
  if (existsSync(vswhere)) {
    const version = spawnSync(vswhere, ["-latest", "-products", "*", "-property", "installationVersion"], {
      encoding: "utf8",
    });
    const year = spawnSync(vswhere, ["-latest", "-products", "*", "-property", "catalog_productLineVersion"], {
      encoding: "utf8",
    });
    const major = (version.stdout ?? "").trim().split(".")[0];
    const productYear = (year.stdout ?? "").trim();
    if (version.status === 0 && /^\d+$/.test(major) && productYear) {
      return `Visual Studio ${major} ${productYear}`;
    }
  }

  // Last-resort default for machines where vswhere is missing.
  return "Visual Studio 17 2022";
}

function cmakeConfigure(srcDir, buildDir, defines = {}, extraArgs = []) {
  // Nuke stale cache from a different generator to avoid "does not match"
  // errors when switching between NMake and Visual Studio.
  const cacheFile = join(buildDir, "CMakeCache.txt");
  if (existsSync(cacheFile)) {
    const cache = readFileSync(cacheFile, "utf-8");
    if (isWindows && cache.includes("NMake Makefiles")) {
      console.log(`  →  Cleaning stale NMake cache in ${buildDir}`);
      rmSync(buildDir, { recursive: true, force: true });
    }
  }

  const args = [
    "-S", srcDir,
    "-B", buildDir,
  ];

  // generator — Visual Studio on Windows (produces .lib + .dll),
  // default (Unix Makefiles) elsewhere
  if (isWindows) {
    args.push("-G", visualStudioGenerator());
    args.push("-A", "x64");
  }

  for (const [key, value] of Object.entries(defines)) {
    args.push(`-D${key}=${value}`);
  }

  args.push(...extraArgs);
  run(cmake, args, { cwd: root });
}

function cmakeBuild(buildDir, config = "Release") {
  run(cmake, ["--build", buildDir, "--config", config, "--parallel"], { cwd: root });
}

function cmakeInstall(buildDir, config = "Release") {
  run(cmake, ["--install", buildDir, "--config", config], { cwd: root });
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

  cmakeConfigure(freetypeSrc, freetypeBuild, {
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

  // Neither the CAD core nor the desktop app use TCL/TK/Draw.
  USE_TCL: "OFF",
  USE_TK: "OFF",
  USE_VTK: "OFF",
  BUILD_MODULE_Draw: "OFF",

  // Disable 3rdparty deps not needed for PolySmith — avoids build
  // failures when they're not installed on the build machine.
  USE_TBB: "OFF",
  USE_FFMPEG: "OFF",
  USE_OPENVR: "OFF",
  USE_OPENGL: "OFF",
  USE_GLES2: "OFF",
  USE_D3D: "OFF",
  USE_XLIB: "OFF",

  // Only enable FreeType if we have it (system or vendored).
  USE_FREETYPE: freetypeDir || systemFreetypeAvailable() ? "ON" : "OFF",
  // FreeImage needed for image/texture support in data exchange.
  USE_FREEIMAGE: isWindows ? "OFF" : "OFF",
  // RapidJSON needed for glTF export — we don't use it but it's
  // pulled in by default in 8.0; disable to simplify the build.
  USE_RAPIDJSON: "OFF",
  // Draco mesh compression — not needed.
  USE_DRACO: "OFF",
};

const occtExtraArgs = [];
if (freetypeDir) {
  occtExtraArgs.push(`-D3RDPARTY_FREETYPE_DIR=${freetypeDir}`);
}

cmakeConfigure(occtSrc, occtBuild, occtDefines, occtExtraArgs);

console.log("\n✅  OCCT configured successfully.");
console.log("    Next: pnpm occt:build");
console.log("    Then: pnpm occt:install");
