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
import { useCadCoreStore } from "@/state";

export function useCadCoreEventBridge() {
  const addMessage = useCadCoreStore((state) => state.addMessage);
  const addLogEntry = useCadCoreStore((state) => state.addLogEntry);
  const handleCoreMessage = useCadCoreStore((state) => state.handleCoreMessage);
  const handleCoreStopped = useCadCoreStore((state) => state.handleCoreStopped);
  const setStatus = useCadCoreStore((state) => state.setStatus);

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
            window.dispatchEvent(
              new CustomEvent("polysmith-trim-preview", {
                detail: message.payload,
              }),
            );
          }
          handleCoreMessage(message);
        } catch (error) {
          const entry = makeUiLogEntry(
            "error",
            "desktop_ui",
            `parse error: ${String(error)}`,
          );
          writeLogToConsole(entry);
          addLogEntry(entry);
          addMessage(entry.message);
          setStatus("error");
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
      });

      const unlistenError = await onCadCoreError((message) => {
        const entry = makeUiLogEntry("error", "tauri_bridge", message);
        writeLogToConsole(entry);
        addLogEntry(entry);
        addMessage(`bridge error: ${message}`);
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
  }, [addLogEntry, addMessage, handleCoreMessage, handleCoreStopped, setStatus]);
}
