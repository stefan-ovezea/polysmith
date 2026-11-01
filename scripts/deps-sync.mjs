#!/usr/bin/env node

/**
 * deps-sync.mjs — cross-platform git submodule sync with verification.
 *
 * Why this exists instead of a bare `git submodule update --init --recursive`
 * in package.json:
 *
 * git considers a submodule up to date when the submodule's HEAD matches the
 * SHA-1 recorded in the superproject.  It does NOT verify that the working
 * tree actually contains those files.  An interrupted checkout (Ctrl-C during
 * a slow clone, a dropped connection, an antivirus lock, a full disk) leaves
 * `.git/modules/<name>` fully populated with the right HEAD but the working
 * tree empty — and every later `git submodule update` then exits 0 without
 * touching it.  The damage surfaces much later, as a confusing CMake error:
 *
 *   CMake Error: The source directory ".../third_party/occt8" does not
 *   appear to contain CMakeLists.txt.
 *
 * So: update, then verify each submodule actually has tracked files in its
 * index, repair the ones that don't, and fail loudly if a repair does not
 * stick.  Pure Node + spawnSync(git) — identical behaviour on Windows,
 * macOS and Linux, no shell, no platform-specific quoting.
 *
 * Called from `pnpm deps:sync` and from the bootstrap chain.
 */

import { join, resolve, dirname } from "node:path";
import { existsSync } from "node:fs";
import { spawnSync } from "node:child_process";
import { fileURLToPath } from "node:url";

const root = resolve(dirname(fileURLToPath(import.meta.url)), "..");
const isWindows = process.platform === "win32";

/**
 * Run git, inheriting stdio so clone/checkout progress stays visible.
 * Returns the spawn result (never throws on a non-zero exit — callers decide).
 */
function git(args, { cwd = root, capture = false } = {}) {
  // Command-line config is propagated to the child git processes that
  // actually perform submodule checkouts (GIT_CONFIG_PARAMETERS), so this is
  // what makes long paths work *inside* third_party/occt8 — a repo-local
  // `core.longpaths` setting only covers the superproject.
  const effectiveArgs = isWindows ? ["-c", "core.longpaths=true", ...args] : args;

  return spawnSync("git", effectiveArgs, {
    cwd,
    encoding: "utf8",
    stdio: capture ? "pipe" : "inherit",
    // `git ls-files` in third_party/occt8 prints ~35k paths (>1.6 MB); the
    // 1 MB spawnSync default truncates it and fails the call with ENOBUFS.
    maxBuffer: 64 * 1024 * 1024,
  });
}

/**
 * How many files git believes are checked out in `dir`.
 *  -1  → not a git worktree at all (submodule never initialised)
 *   0  → worktree is empty (index empty, or every tracked file deleted)
 *  >0  → healthy
 *
 * The deleted-file probe matters: an interrupted checkout can also leave a
 * populated index with nothing on disk, and `ls-files` alone would call that
 * healthy while CMake still finds no CMakeLists.txt.
 */
function measureWorktree(dir) {
  if (!existsSync(dir)) return -1;

  const tracked = git(["ls-files"], { cwd: dir, capture: true });
  if (tracked.status !== 0) return -1;

  const files = (tracked.stdout ?? "").split(/\r?\n/).filter(Boolean);
  if (files.length === 0) return 0;

  const deleted = git(["ls-files", "--deleted"], { cwd: dir, capture: true });
  const missing = (deleted.stdout ?? "").split(/\r?\n/).filter(Boolean).length;
  if (missing >= files.length) return 0;

  return files.length;
}

/**
 * Parse `git submodule status --recursive`.
 * Line format: "<marker><sha1> <path> (<describe>)", marker is " " (ok),
 * "-" (not initialised) or "+" (checked out at a different commit).
 */
function listSubmodules() {
  const result = git(["submodule", "status", "--recursive"], { capture: true });
  if (result.status !== 0) return [];

  return (result.stdout ?? "")
    .split(/\r?\n/)
    .filter(Boolean)
    .map((line) => {
      const marker = line[0];
      const path = line.trim().split(/\s+/)[1] ?? "";
      return { marker, path };
    })
    .filter((m) => m.path);
}

function isBroken(module) {
  if (module.marker === "-") return true;
  return measureWorktree(join(root, module.path)) <= 0;
}

function repair(module) {
  const dir = join(root, module.path);
  console.log(`\n🔧  ${module.path} is registered but has no checked-out files — repairing`);

  // --force re-runs the checkout even when the recorded SHA-1 already matches
  // HEAD, which is exactly the state an interrupted clone leaves behind.
  const forced = git(["submodule", "update", "--init", "--force", "--checkout", module.path]);

  if (forced.status !== 0 || measureWorktree(dir) === 0) {
    // Belt and braces: rebuild the index from HEAD inside the submodule.
    console.log(`    → falling back to a hard reset in ${module.path}`);
    git(["reset", "--hard"], { cwd: dir });
  }
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------

console.log("=== PolySmith — dependency sync ===\n");

// Windows: OCCT and FreeType contain paths long enough to trip the legacy
// 260-character MAX_PATH limit when the repo is cloned deep in a user
// profile.  Repo-local so it never mutates the user's global git config.
if (isWindows) {
  git(["config", "core.longpaths", "true"]);
  const longPaths = git(
    ["config", "--get", "core.longpaths"],
    { capture: true }
  );
  console.log(`core.longpaths : ${(longPaths.stdout ?? "").trim() || "unset"}`);
}

const update = git(["submodule", "update", "--init", "--recursive"]);
if (update.status !== 0) {
  console.log("\n⚠️  `git submodule update` reported an error — verifying each submodule before giving up.");
}

let modules = listSubmodules();
let broken = modules.filter(isBroken);

if (broken.length > 0) {
  for (const module of broken) repair(module);
  modules = listSubmodules();
  broken = modules.filter(isBroken);
}

if (broken.length > 0) {
  console.error("\n❌  These submodules are still not checked out:");
  for (const module of broken) console.error(`      ${module.path}`);

  console.error(
    "\n    git can leave a submodule in this state when its checkout was interrupted:\n" +
      "    the recorded commit is fetched, HEAD points at it, but no files exist on disk.\n" +
      "    Repair one manually with:\n\n" +
      "        git submodule update --init --force --checkout <path>\n" +
      "        git -C <path> reset --hard\n"
  );

  if (isWindows) {
    console.error(
      "    Windows-specific causes worth ruling out:\n" +
        "      • path length — `git config --global core.longpaths true` and enable\n" +
        "        LongPathsEnabled (HKLM\\SYSTEM\\CurrentControlSet\\Control\\FileSystem)\n" +
        "      • antivirus locking files during checkout — exclude the repo directory\n" +
        "      • a full disk\n"
    );
  }

  process.exit(1);
}

console.log(`\n✅  ${modules.length} submodules checked out`);
