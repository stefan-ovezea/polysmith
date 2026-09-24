#!/usr/bin/env node

/**
 * cmake-run.mjs — run an arbitrary CMake command with the resolved
 * CMake binary (see find-cmake.mjs: the VS-bundled CMake on Windows
 * when no standalone CMake is on PATH).  Used by the `occt:build` and
 * `occt:install` package scripts so every CMake step in the build
 * chain resolves the toolchain the same way.
 *
 * Usage: node scripts/cmake-run.mjs <cmake args...>
 */

import { runCmake } from "./cmake-utils.mjs";

const args = process.argv.slice(2);
if (args.length === 0) {
  console.error("usage: node scripts/cmake-run.mjs <cmake args...>");
  process.exit(1);
}

runCmake(args);
