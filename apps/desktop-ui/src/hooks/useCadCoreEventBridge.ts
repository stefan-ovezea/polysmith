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
import { useCadCoreStore, useToastStore } from "@/state";
import { reportCoreError } from "./coreLogReporting";

export function useCadCoreEventBridge() {
  const addMessage = useCadCoreStore((state) => state.addMessage);
  const addLogEntry = useCadCoreStore((state) => state.addLogEntry);
  const handleCoreMessage = useCadCoreStore((state) => state.handleCoreMessage);
  const handleCoreStopped = useCadCoreStore((state) => state.handleCoreStopped);
  const setStatus = useCadCoreStore((state) => state.setStatus);
  const pushToast = useToastStore((state) => state.pushToast);

  useEffect(() => {
    let disposed = false;
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
          reportCoreError(
            { addLogEntry, addMessage, setStatus },
            "desktop_ui",
            `parse error: ${String(error).slice(0, 4000)}\nraw: ${raw}`,
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
        if (disposed) {
          unlisten();
        } else {
          unlistenFns.push(unlisten);
        }
      }
    }

    void setupListeners();

    return () => {
      disposed = true;
      for (const unlisten of unlistenFns) {
        unlisten();
      }
    };
  }, [
    addLogEntry,
    addMessage,
    handleCoreMessage,
    handleCoreStopped,
    pushToast,
    setStatus,
  ]);
}
