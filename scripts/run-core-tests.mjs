// Runs every cad_core_*_test executable in the core build directory.
//
// The C++ suites are the project's regression safety net for profile
// detection, sketching, and extrusion logic — run them before
// committing any native change (`pnpm test:core`).  On Windows the
// OCCT DLLs are picked up from the build directory by prepending it
// to PATH; on POSIX the same directory is added to LD_LIBRARY_PATH.
//
// Suites run concurrently (bounded by CAD_CORE_TEST_JOBS, default CPU
// count; CAD_CORE_TEST_JOBS=1 reproduces the old serial run).  Each
// suite gets a private temp dir exported as TMP/TEMP (Windows) or
// TMPDIR (POSIX): several suites create fixed-name subdirs under
// temp_directory_path() and remove_all() them on entry, so sharing
// the real temp root would race.  Output is buffered per suite and
// printed on completion to avoid interleaving.

import { spawn } from "node:child_process";
import { existsSync, mkdtempSync, readdirSync, rmSync } from "node:fs";
import os from "node:os";
import { delimiter, dirname, join, resolve } from "node:path";
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

const baseEnv = { ...process.env };
if (isWindows) {
  baseEnv.PATH = `${buildRoot}${delimiter}${baseEnv.PATH ?? ""}`;
} else {
  baseEnv.LD_LIBRARY_PATH = [buildRoot, baseEnv.LD_LIBRARY_PATH]
    .filter(Boolean)
    .join(delimiter);
}

// OCCT 8.0 resource files (STEP/IGES/STL export) — mirror the Tauri
// spawn env in cad_core.rs so writer tests run under the same
// conditions as the app.
const occtSrc = join(root, "third_party", "occt8-build", "src");
if (existsSync(occtSrc)) {
  baseEnv.CSF_OCCTResourcePath = occtSrc;
}

const requested = Number.parseInt(process.env.CAD_CORE_TEST_JOBS ?? "", 10);
const jobs = Number.isFinite(requested) && requested > 0
  ? Math.min(requested, tests.length)
  : Math.max(1, Math.min(os.cpus().length, tests.length));

let next = 0;
let running = 0;
let done = 0;
let failed = 0;

function spawnOne(test) {
  // std::filesystem::temp_directory_path() → GetTempPathW (checks TMP,
  // then TEMP) on Windows, TMPDIR on POSIX — point both at the private
  // root so fixed-name temp subdirs can never collide across suites.
  const privateTmp = mkdtempSync(join(os.tmpdir(), "polysmith-core-test-"));
  const env = { ...baseEnv };
  if (isWindows) {
    env.TMP = privateTmp;
    env.TEMP = privateTmp;
  } else {
    env.TMPDIR = privateTmp;
  }

  const child = spawn(join(exeDir, test), [], {
    env,
    stdio: ["ignore", "pipe", "pipe"],
  });
  let out = "";
  child.stdout.on("data", (chunk) => { out += chunk; });
  child.stderr.on("data", (chunk) => { out += chunk; });
  child.on("close", (code, signal) => {
    rmSync(privateTmp, { recursive: true, force: true });
    const ok = code === 0;
    const status = code === null ? `signal ${signal}` : `exit ${code}`;
    console.log(`${ok ? "PASS" : "FAIL"}  ${test} (${status})`);
    if (out.trim() !== "") {
      console.log(out.trimEnd());
    }
    if (!ok) failed += 1;
    running -= 1;
    done += 1;
    if (done === tests.length) {
      if (failed === 0) {
        console.log(`\nAll ${tests.length} test suites passed.`);
        process.exit(0);
      }
      console.error(`\n${failed}/${tests.length} suites FAILED.`);
      process.exit(1);
    }
    pump();
  });
  running += 1;
}

function pump() {
  while (running < jobs && next < tests.length) {
    spawnOne(tests[next]);
    next += 1;
  }
}

pump();
