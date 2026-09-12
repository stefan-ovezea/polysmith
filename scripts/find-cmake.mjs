#!/usr/bin/env node

/**
 * find-cmake.mjs — resolves a usable CMake binary (>= 3.20) without
 * hardcoding machine-specific paths.
 *
 * Resolution order:
 *   1. `cmake` on PATH
 *   2. Windows only: the CMake bundled with Visual Studio (the "C++ CMake
 *      tools for Windows" component), located via vswhere.exe, which ships
 *      with every VS install at a well-known path.
 *
 * Imported by the build scripts (configure-core / build-core /
 * configure-occt) so the build works from any shell on any workstation,
 * including machines where no standalone CMake is on PATH.
 */

import { existsSync } from "node:fs";
import { spawnSync } from "node:child_process";

const MIN_MAJOR = 3;
const MIN_MINOR = 20;

function versionOk(raw) {
  const m = /(\d+)\.(\d+)\.(\d+)/.exec(raw ?? "");
  if (!m) return false;
  const major = Number(m[1]);
  const minor = Number(m[2]);
  return major > MIN_MAJOR || (major === MIN_MAJOR && minor >= MIN_MINOR);
}

function versionOf(cmake) {
  const result = spawnSync(cmake, ["--version"], { encoding: "utf8" });
  return result.status === 0 ? result.stdout : "";
}

function resolveCmake() {
  if (versionOk(versionOf("cmake"))) {
    return "cmake";
  }

  if (process.platform === "win32") {
    const vswhere = "C:\\Program Files (x86)\\Microsoft Visual Studio\\Installer\\vswhere.exe";
    if (existsSync(vswhere)) {
      const result = spawnSync(vswhere, ["-latest", "-property", "installationPath"], {
        encoding: "utf8",
      });
      const vsRoot = (result.stdout ?? "").trim();
      if (result.status === 0 && vsRoot) {
        const candidate = `${vsRoot}\\Common7\\IDE\\CommonExtensions\\Microsoft\\CMake\\CMake\\bin\\cmake.exe`;
        if (existsSync(candidate) && versionOk(versionOf(candidate))) {
          return candidate;
        }
      }
    }
  }

  console.error(
    "\n❌  No CMake >= 3.20 found.\n" +
      "    Install CMake (https://cmake.org/download/) or, on Windows, install\n" +
      "    the 'C++ CMake tools for Windows' component in Visual Studio."
  );
  process.exit(1);
}

export const cmake = resolveCmake();
