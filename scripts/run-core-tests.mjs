// Runs every cad_core_*_test executable in the core build directory.
//
// The C++ suites are the project's regression safety net for profile
// detection, sketching, and extrusion logic — run them before
// committing any native change (`pnpm test:core`).  On Windows the
// OCCT DLLs are picked up from the build directory by prepending it
// to PATH; on POSIX the same directory is added to LD_LIBRARY_PATH.

import { spawnSync } from "node:child_process";
import { existsSync, readdirSync } from "node:fs";
import { dirname, delimiter, join, resolve } from "node:path";
import { fileURLToPath } from "node:url";

const root = resolve(dirname(fileURLToPath(import.meta.url)), "..");
const buildRoot = join(root, "native", "cad-core", "build");
const exeDir = join(buildRoot, "Release");

if (!existsSync(exeDir)) {
  console.error(`No test binaries at ${exeDir} — run pnpm core:build first.`);
  process.exit(1);
}

const isWindows = process.platform === "win32";
const pattern = isWindows ? /^cad_core_.*_test\.exe$/ : /^cad_core_.*_test$/;
const tests = readdirSync(exeDir)
  .filter((name) => pattern.test(name))
  .sort();

if (tests.length === 0) {
  console.error(`No cad_core_*_test executables found in ${exeDir}.`);
  process.exit(1);
}

const env = { ...process.env };
if (isWindows) {
  env.PATH = `${buildRoot}${delimiter}${env.PATH ?? ""}`;
} else {
  env.LD_LIBRARY_PATH = [buildRoot, env.LD_LIBRARY_PATH]
    .filter(Boolean)
    .join(delimiter);
}

// OCCT 8.0 resource files (STEP/IGES/STL export) — mirror the Tauri
// spawn env in cad_core.rs so writer tests run under the same
// conditions as the app.
const occtSrc = join(root, "third_party", "occt8-build", "src");
if (existsSync(occtSrc)) {
  env.CSF_OCCTResourcePath = occtSrc;
}

let failed = 0;
for (const test of tests) {
  const result = spawnSync(join(exeDir, test), [], { stdio: "inherit", env });
  const ok = result.status === 0;
  const status =
    result.status === null ? `signal ${result.signal}` : `exit ${result.status}`;
  console.log(`${ok ? "PASS" : "FAIL"}  ${test} (${status})`);
  if (!ok) failed += 1;
}

if (failed === 0) {
  console.log(`\nAll ${tests.length} test suites passed.`);
  process.exit(0);
}
console.error(`\n${failed}/${tests.length} suites FAILED.`);
process.exit(1);
