// Headless driver for the native CAD core. Spawns the real `cad_core`
// binary (stdin/stdout NDJSON) with the same environment recipe the Tauri
// shell applies in `apps/desktop-ui/src-tauri/src/cad_core.rs`, and exposes
// id-correlated send() calls plus the latest document/viewport snapshots.
// Used by the AI sketch-generation integration tests — never by the app UI,
// which talks to the core through the Tauri bridge instead.
import { spawn, type ChildProcessWithoutNullStreams } from "node:child_process";
import { existsSync, mkdtempSync } from "node:fs";
import { tmpdir } from "node:os";
import { dirname, join } from "node:path";
import { createInterface } from "node:readline";
import { fileURLToPath } from "node:url";

import { parseCoreMessage } from "../ipcProtocol";
import type { CoreCommand, CoreMessage, DocumentState, ViewportState } from "@/types";

const COMMAND_TIMEOUT_MS = 15000;

// The repo root is the first ancestor directory containing `third_party`.
// Never trust process.cwd() — vitest workers run from arbitrary directories.
export function findRepoRoot(): string {
  let dir = dirname(fileURLToPath(import.meta.url));
  for (let depth = 0; depth < 8; depth++) {
    if (existsSync(join(dir, "third_party"))) {
      return dir;
    }
    const parent = dirname(dir);
    if (parent === dir) {
      break;
    }
    dir = parent;
  }
  throw new Error("Could not locate the polysmith repo root (no third_party marker found).");
}

export function coreBinaryPath(): string {
  if (process.env.POLYSMITH_CORE_BIN) {
    return process.env.POLYSMITH_CORE_BIN;
  }
  const binaryName = process.platform === "win32" ? "cad_core.exe" : "cad_core";
  return join(
    findRepoRoot(),
    "native",
    "cad-core",
    "build",
    "Release",
    binaryName,
  );
}

// Replicates the child environment from src-tauri/src/cad_core.rs: prepends
// the OCCT and 3rdparty DLL directories to PATH, points CSF_OCCTResourcePath
// at the built OCCT resource tree, and gives the core a scratch posts dir.
export function buildCoreEnv(): NodeJS.ProcessEnv {
  const root = findRepoRoot();
  const thirdParty = join(root, "third_party");
  const env: NodeJS.ProcessEnv = { ...process.env };

  const occtBin = join(thirdParty, "occt8-build/win64/vc14/bin");
  const dylibDirs = [
    occtBin,
    join(thirdParty, "3rdparty-vc14-64/freetype-2.13.3-x64/bin"),
    join(thirdParty, "3rdparty-vc14-64/zlib-1.2.8-vc14-64/bin"),
    join(thirdParty, "3rdparty-vc14-64/tbb-2021.13.0-x64/bin"),
    join(thirdParty, "3rdparty-vc14-64/jemalloc-vc14-64/bin"),
    join(thirdParty, "3rdparty-vc14-64/freeimage-3.18.0-x64/bin"),
    join(thirdParty, "3rdparty-vc14-64/lzma-5.2.2-vc14-64/bin"),
  ].filter((dir) => existsSync(dir));

  if (process.platform === "win32") {
    if (dylibDirs.length > 0) {
      env.PATH = [...dylibDirs, env.PATH ?? ""].join(";");
    }
  } else if (existsSync(occtBin)) {
    const existing = env.LD_LIBRARY_PATH ?? "";
    env.LD_LIBRARY_PATH = existing ? `${occtBin}:${existing}` : occtBin;
  }

  const occtSrc = join(thirdParty, "occt8-build/src");
  if (existsSync(occtSrc)) {
    env.CSF_OCCTResourcePath = occtSrc;
  }
  env.POLYSMITH_POSTS_DIR = mkdtempSync(join(tmpdir(), "polysmith-posts-"));
  return env;
}

interface PendingRequest {
  resolve: (message: CoreMessage) => void;
  reject: (error: Error) => void;
  timer: NodeJS.Timeout;
}

function messageId(message: CoreMessage): string | undefined {
  return (message as { id?: string }).id;
}

export class CadCoreHarness {
  private readonly child: ChildProcessWithoutNullStreams;
  private readonly pending = new Map<string, PendingRequest>();
  private lastDocumentState: DocumentState | null = null;
  private lastViewportState: ViewportState | null = null;
  // Core error events that could not be correlated to a pending command id
  // (malformed commands and other global failures). Diagnostics only.
  readonly unroutedErrors: string[] = [];

  constructor(binaryPath: string = coreBinaryPath()) {
    this.child = spawn(binaryPath, [], {
      stdio: ["pipe", "pipe", "pipe"],
      env: buildCoreEnv(),
      windowsHide: true,
    });

    const lines = createInterface({ input: this.child.stdout });
    lines.on("line", (line) => this.onLine(line));
    // Structured core logs land on stderr; the harness does not consume them.
    this.child.stderr.on("data", () => {});
    this.child.on("error", (error) => {
      for (const request of this.pending.values()) {
        clearTimeout(request.timer);
        request.reject(error instanceof Error ? error : new Error(String(error)));
      }
      this.pending.clear();
    });
  }

  private onLine(line: string) {
    let message: CoreMessage;
    try {
      message = parseCoreMessage(JSON.parse(line));
    } catch {
      return;
    }
    if (message.type === "document_state") {
      this.lastDocumentState = message.payload as DocumentState;
    }
    if (message.type === "document_created") {
      this.lastDocumentState = message.payload as DocumentState;
    }
    if (message.type === "viewport_state") {
      this.lastViewportState = message.payload as ViewportState;
    }
    if (message.type === "error") {
      const id = messageId(message);
      const request = id ? this.pending.get(id) : undefined;
      if (request) {
        clearTimeout(request.timer);
        this.pending.delete(id);
        request.reject(
          new Error(`core error: ${message.payload.code}: ${message.payload.message}`),
        );
      } else {
        this.unroutedErrors.push(
          `unrouted core error: ${message.payload.code}: ${message.payload.message}`,
        );
      }
      return;
    }
    const request = this.pending.get(messageId(message) ?? "");
    if (request) {
      clearTimeout(request.timer);
      this.pending.delete(messageId(message) ?? "");
      request.resolve(message);
    }
  }

  // Sends one command line and resolves with the correlated response event.
  // Rejects on a core error event for that id or after COMMAND_TIMEOUT_MS.
  send(type: CoreCommand["type"], payload: object = {}): Promise<CoreMessage> {
    const id = crypto.randomUUID();
    return new Promise<CoreMessage>((resolve, reject) => {
      const timer = setTimeout(() => {
        this.pending.delete(id);
        reject(new Error(`timed out waiting for ${type}`));
      }, COMMAND_TIMEOUT_MS);
      this.pending.set(id, { resolve, reject, timer });
      this.child.stdin.write(`${JSON.stringify({ id, type, payload })}\n`);
    });
  }

  latestDocument(): DocumentState | null {
    return this.lastDocumentState;
  }

  latestViewport(): ViewportState | null {
    return this.lastViewportState;
  }

  // The documented terminate path: there is no working shutdown command, so
  // close stdin and wait for exit, killing the process as a fallback.
  async close(): Promise<void> {
    this.child.stdin.end();
    await new Promise<void>((resolve) => {
      const killer = setTimeout(() => {
        this.child.kill();
        resolve();
      }, 5000);
      this.child.on("exit", () => {
        clearTimeout(killer);
        resolve();
      });
    });
  }
}
