// Kills whatever process still listens on the dev-server port before
// starting a new one.  On Windows, tauri's beforeDevCommand (vite)
// can orphan when the app exits — the next `pnpm dev` would otherwise
// start a NEW vite on a different port while the app keeps talking to
// the stale server (devUrl is pinned to 1420).  Run this first and
// `pnpm dev` always starts clean.

import { execSync } from "node:child_process";
import { existsSync, rmSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";

const port = process.argv[2] ?? "1420";

// Vite's transform cache can go stale on Windows (the watcher misses
// edits and a fresh server keeps serving old transforms).  Clear it on
// every dev start — a few extra seconds of transform time buys a
// guaranteed-fresh UI.
const repoRoot = join(dirname(fileURLToPath(import.meta.url)), "..");
const viteCache = join(repoRoot, "apps", "desktop-ui", "node_modules", ".vite");
if (existsSync(viteCache)) {
  rmSync(viteCache, { recursive: true, force: true });
  console.log("kill-dev-port: cleared the vite transform cache");
}

function killWindows(port) {
  const lines = execSync(`netstat -ano`, { encoding: "utf8" }).split(/\r?\n/);
  const pids = new Set();
  for (const line of lines) {
    // LISTENING lines look like: TCP    [::1]:1420    ...    LISTENING   21224
    const match = line.trim().match(/^\S+\s+\S+:(\d+)\s+.*?LISTENING\s+(\d+)\s*$/);
    if (match && match[1] === port) {
      pids.add(match[2]);
    }
  }
  for (const pid of pids) {
    try {
      execSync(`taskkill //F //PID ${pid}`, { stdio: "ignore" });
      console.log(`kill-dev-port: killed stale dev server (pid ${pid}) on port ${port}`);
    } catch {
      // process already gone
    }
  }
}

function killPosix(port) {
  try {
    execSync(`lsof -ti tcp:${port} | xargs -r kill -9`, { stdio: "ignore" });
    console.log(`kill-dev-port: cleared port ${port}`);
  } catch {
    // nothing listening — fine
  }
}

try {
  if (process.platform === "win32") {
    killWindows(port);
  } else {
    killPosix(port);
  }
} catch (error) {
  console.warn(`kill-dev-port: could not check port ${port}: ${error.message}`);
}
