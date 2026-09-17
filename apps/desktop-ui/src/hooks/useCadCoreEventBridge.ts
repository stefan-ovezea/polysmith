import { useEffect } from "react";

import {
  onCadCoreError,
  onCadCoreEvent,
  onCadCoreExited,
  onCadCoreLog,
  makeUiLogEntry,
  parseCoreMessage,
  writeLogToConsole,
} from "@/lib";
import { setCamSchemaRescueReporter } from "@/lib/schemas/ipc/camSchema";
import { useCadCoreStore, useToastStore } from "@/state";
import { reportCoreError } from "./coreLogReporting";

// The bridge is a PROCESS-WIDE singleton — exactly one Tauri listener
// per channel no matter how many components call useCadCore().  A
// second listener (e.g. the tool library dialog mounting the hook)
// used to parse and dispatch every core event twice, doubling the UI
// work per event.
let bridgeInstalled = false;

export function useCadCoreEventBridge() {
  const addMessage = useCadCoreStore((state) => state.addMessage);
  const addLogEntry = useCadCoreStore((state) => state.addLogEntry);
  const handleCoreMessage = useCadCoreStore((state) => state.handleCoreMessage);
  const handleCoreStopped = useCadCoreStore((state) => state.handleCoreStopped);
  const setStatus = useCadCoreStore((state) => state.setStatus);
  const pushToast = useToastStore((state) => state.pushToast);

  // The CAM subtree rescue (see camSchema.ts) reports through here so a
  // swallowed mismatch is LOUD — toast + log with the exact field path.
  // Deduped: the same mismatch on every document_state must not toast
  // forever.
  useEffect(() => {
    let lastReported = "";
    setCamSchemaRescueReporter((issues) => {
      const first = issues[0] as
        | { path?: unknown[]; message?: string; expected?: unknown; received?: unknown }
        | undefined;
      const path = Array.isArray(first?.path)
        ? first.path.map(String).join(".")
        : "?";
      const detail = first
        ? ` ${first.message ?? ""}${"received" in first ? ` (received: ${JSON.stringify(first.received)})` : ""}`
        : "";
      const signature = `${path}:${first?.message ?? ""}`;
      // The toast itself carries the failing field + value so the
      // answer is on screen even when the app is otherwise frozen.
      const message = `CAM parse failed at ${path}.${detail} CAM view emptied — paste this toast.`;
      addLogEntry(makeUiLogEntry("error", "desktop_ui", message));
      if (signature !== lastReported) {
        lastReported = signature;
        pushToast("error", message);
        addMessage(message);
      }
    });
    // Process-lifetime reporter — a later unmount (dialog close) must
    // not disarm it while the app bridge is still alive.
  }, [addLogEntry, addMessage, pushToast]);

  useEffect(() => {
    // Singleton: only the FIRST mount installs the Tauri listeners —
    // they live for the process.  Every later mount (any component
    // calling useCadCore()) reuses them instead of adding another
    // per-event parse+dispatch.
    if (bridgeInstalled) {
      return;
    }
    bridgeInstalled = true;

    const unlistenFns: Array<() => void> = [];

    async function setupListeners() {
      const unlistenEvent = await onCadCoreEvent((payload) => {
        try {
          const message = parseCoreMessage(payload);
          if (message.type === "log") {
            writeLogToConsole(message.payload);
          }
          if (message.type === "trim_preview_result") {
            // Echo the command id so the viewport can reject responses
            // that are not the newest request (hover previews are
            // coalesced but can still arrive out of order).
            window.dispatchEvent(
              new CustomEvent("polysmith-trim-preview", {
                detail: { ...message.payload, id: message.id },
              }),
            );
          }
          if (message.type === "corner_trim_preview_result") {
            // Same coalescing contract as the trim preview: the
            // viewport rejects responses that are not the newest
            // request by id.
            window.dispatchEvent(
              new CustomEvent("polysmith-corner-trim-preview", {
                detail: { ...message.payload, id: message.id },
              }),
            );
          }
          handleCoreMessage(message);
        } catch (error) {
          // Include the raw message so a schema gap can be identified
          // from the log alone — zod's union error alone does not say
          // which message failed.
          let rawText: string;
          try {
            rawText = JSON.stringify(payload);
          } catch {
            rawText = String(payload);
          }
          const raw =
            rawText.length > 2000 ? rawText.slice(0, 2000) : rawText;
          const stack =
            error instanceof Error && error.stack
              ? `\nstack: ${error.stack.slice(0, 6000)}`
              : "";
          reportCoreError(
            { addLogEntry, addMessage, setStatus },
            "desktop_ui",
            `parse error: ${String(error).slice(0, 4000)}${stack}\nraw: ${raw}`,
          );
        }
      });

      const unlistenLog = await onCadCoreLog((line) => {
        const level = line.startsWith("ERROR")
          ? "error"
          : line.startsWith("WARN")
            ? "warn"
            : "info";
        const entry = makeUiLogEntry(level, "cad_core_stderr", line);
        writeLogToConsole(entry);
        addLogEntry(entry);
        addMessage(`log: ${line}`);
        if (level === "error") {
          pushToast("error", line);
        }
      });

      const unlistenError = await onCadCoreError((message) => {
        const entry = makeUiLogEntry("error", "tauri_bridge", message);
        writeLogToConsole(entry);
        addLogEntry(entry);
        addMessage(`bridge error: ${message}`);
        pushToast("error", message);
        setStatus("error");
      });

      const unlistenExited = await onCadCoreExited((message) => {
        const entry = makeUiLogEntry("warn", "cad_core", message);
        writeLogToConsole(entry);
        addLogEntry(entry);
        addMessage(`exit: ${message}`);
        handleCoreStopped();
      });

      for (const unlisten of [
        unlistenEvent,
        unlistenLog,
        unlistenError,
        unlistenExited,
      ]) {
        unlistenFns.push(unlisten);
      }
    }

    void setupListeners();

    // The listeners are process-lifetime — no cleanup.  Later mounts
    // short-circuit on bridgeInstalled before reaching here.
  }, [
    addLogEntry,
    addMessage,
    handleCoreMessage,
    handleCoreStopped,
    pushToast,
    setStatus,
  ]);
}
