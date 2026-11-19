import { invoke } from "@tauri-apps/api/core";
import { listen, type UnlistenFn } from "@tauri-apps/api/event";
import type { CoreCommand, CoreMessage } from "@/types";
import { markAtomicCommand } from "../state/cadCoreStore";

enum CadCoreCommandType {
  SendCoreCommand = "send_core_command",
  StartCadCore = "start_cad_core",
  CadCoreEvent = "cad-core-event",
  CadCoreLog = "cad-core-log",
  CadCoreError = "cad-core-error",
  CadCoreExited = "cad-core-exited",
}

export async function startCadCore(): Promise<string> {
  return invoke(CadCoreCommandType.StartCadCore);
}

export async function sendCoreCommand(
  command: CoreCommand & { include_viewport?: boolean; id?: string },
): Promise<void> {
  // Atomic commit: ask the core to follow this reply with a viewport_state
  // bearing the same id, then remember the id so the store can apply the
  // document and its geometry in one update (no stale-viewport frame).
  if (command.include_viewport && command.id) {
    markAtomicCommand(command.id);
  }
  return invoke(CadCoreCommandType.SendCoreCommand, {
    command: JSON.stringify(command),
  });
}

// ── request/response correlation ───────────────────────────────────
// Commands are fire-and-forget: sendCoreCommand resolves when the
// command reaches the core's stdin, while responses arrive later as
// cad-core-event messages. Tools that need to know a specific command
// was applied (instead of guessing from the latest document_state)
// can await the matching response here. The core echoes the command id
// on its response events, so correlation is by id — no ordering
// assumptions.

interface PendingCommand {
  resolve: (message: Record<string, unknown>) => void;
  reject: (reason: Error) => void;
  type: string;
  sentAt: number;
}

const pendingCommands = new Map<string, PendingCommand>();

// A command is failed when the CORE goes quiet, not when the wall clock passes a
// fixed mark. The core handles commands strictly in arrival order, so a short
// command sent while a long one runs waits for it: importing this board's STEP
// takes ~12 s (measured), and a sketch's `undo_end_group` sent during that
// import was answered 11.9 s after it was sent — 100 ms after the import
// finished. A flat 10 s timeout failed it with "core did not respond to
// undo_end_group" while the core was perfectly healthy.
//
// Any inbound message proves the core is alive, so it resets the idle window.
// The absolute cap still catches a core that keeps chatting but never answers
// this particular command.
const CORE_IDLE_TIMEOUT_MS = 10_000;
const CORE_COMMAND_ABSOLUTE_TIMEOUT_MS = 300_000;

let lastCoreActivityAt = Date.now();
let watchdogId: number | null = null;
const activityListeners = new Set<() => void>();

// Pure so the timeout rule can be tested without a core process.
export function shouldFailPendingCommand({
  now,
  sentAt,
  lastActivityAt,
  idleTimeoutMs = CORE_IDLE_TIMEOUT_MS,
  absoluteTimeoutMs = CORE_COMMAND_ABSOLUTE_TIMEOUT_MS,
}: {
  now: number;
  sentAt: number;
  lastActivityAt: number;
  idleTimeoutMs?: number;
  absoluteTimeoutMs?: number;
}): boolean {
  // A command sent AFTER the last activity still gets a full idle window.
  const quietSince = Math.max(lastActivityAt, sentAt);
  return (
    now - quietSince >= idleTimeoutMs || now - sentAt >= absoluteTimeoutMs
  );
}

// Called for every message the core sends, including its progress logs.
export function noteCoreActivity(): void {
  lastCoreActivityAt = Date.now();
  for (const listener of activityListeners) {
    listener();
  }
}

// Lets other waiters (the store's "wait for the next document/viewport" helpers)
// reuse the same "the core is still working" signal instead of inventing their
// own wall-clock timeout.
export function subscribeCoreActivity(listener: () => void): () => void {
  activityListeners.add(listener);
  return () => {
    activityListeners.delete(listener);
  };
}

function checkPendingCommands(): void {
  const now = Date.now();
  for (const [id, pending] of pendingCommands) {
    if (
      shouldFailPendingCommand({
        now,
        sentAt: pending.sentAt,
        lastActivityAt: lastCoreActivityAt,
      })
    ) {
      rejectPendingCommand(
        id,
        new Error(`core did not respond to ${pending.type} (${id})`),
      );
    }
  }
  if (pendingCommands.size === 0 && watchdogId !== null) {
    window.clearInterval(watchdogId);
    watchdogId = null;
  }
}

export function resolvePendingCommand(
  id: string,
  message: Record<string, unknown>,
): boolean {
  const pending = pendingCommands.get(id);
  if (pending === undefined) return false;
  pendingCommands.delete(id);
  pending.resolve(message);
  return true;
}

export function rejectPendingCommand(id: string, reason: Error): boolean {
  const pending = pendingCommands.get(id);
  if (pending === undefined) return false;
  pendingCommands.delete(id);
  pending.reject(reason);
  return true;
}

export async function sendCoreCommandAwaited(
  command: CoreCommand & { id: string },
): Promise<Record<string, unknown>> {
  const id = command.id;
  const response = new Promise<Record<string, unknown>>((resolve, reject) => {
    pendingCommands.set(id, {
      resolve,
      reject,
      type: command.type,
      sentAt: Date.now(),
    });
  });
  if (watchdogId === null) {
    watchdogId = window.setInterval(checkPendingCommands, 1000);
  }
  try {
    await sendCoreCommand(command);
  } catch (error) {
    rejectPendingCommand(id, error as Error);
  }
  return response;
}

// The Rust bridge gzip-compresses events larger than 64KB (see
// protocol.rs emit_core_event). Decompress before handing the message
// to the handler; events are processed strictly in arrival order so
// document/viewport state updates never reorder.
async function decodeCoreEventPayload(
  payload: unknown,
): Promise<CoreMessage | Record<string, unknown>> {
  const wrapper = payload as { _gz?: string } | null;
  if (
    wrapper !== null &&
    typeof wrapper === "object" &&
    typeof wrapper._gz === "string"
  ) {
    const bytes = Uint8Array.from(atob(wrapper._gz), (char) =>
      char.charCodeAt(0),
    );
    const stream = new Blob([bytes])
      .stream()
      .pipeThrough(new DecompressionStream("gzip"));
    const text = await new Response(stream).text();
    return JSON.parse(text) as CoreMessage;
  }
  return payload as CoreMessage | Record<string, unknown>;
}

let eventChain: Promise<void> = Promise.resolve();

export async function onCadCoreEvent(
  handler: (message: CoreMessage | Record<string, unknown>) => void,
): Promise<UnlistenFn> {
  return listen<unknown>(CadCoreCommandType.CadCoreEvent, (event) => {
    eventChain = eventChain
      .then(() => decodeCoreEventPayload(event.payload))
      .then((message) => {
        // Every event is proof of life: a long operation narrates itself with
        // log events, and those keep the awaited commands in the queue from
        // being failed by the clock (see CORE_IDLE_TIMEOUT_MS).
        noteCoreActivity();
        // The core echoes the command id on every response event —
        // settle any awaited command BEFORE the handler processes the
        // message, so awaiters never race their own UI update.
        const id =
          typeof message === "object" && message !== null
            ? (message as { id?: unknown }).id
            : undefined;
        if (typeof id === "string" && pendingCommands.has(id)) {
          resolvePendingCommand(id, message as Record<string, unknown>);
        }
        handler(message);
      })
      .catch((error) => {
        console.error("failed to decode cad-core-event payload:", error);
      });
  });
}

export async function onCadCoreLog(
  handler: (line: string) => void,
): Promise<UnlistenFn> {
  return listen<string>(CadCoreCommandType.CadCoreLog, (event) => {
    noteCoreActivity();
    handler(event.payload);
  });
}

export async function onCadCoreError(
  handler: (message: string) => void,
): Promise<UnlistenFn> {
  return listen<string>(CadCoreCommandType.CadCoreError, (event) => {
    noteCoreActivity();
    handler(event.payload);
  });
}

export async function onCadCoreExited(
  handler: (message: string) => void,
): Promise<UnlistenFn> {
  return listen<string>(CadCoreCommandType.CadCoreExited, (event) => {
    handler(event.payload);
  });
}
