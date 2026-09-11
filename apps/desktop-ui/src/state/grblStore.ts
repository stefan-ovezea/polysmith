import { listen, type UnlistenFn } from "@tauri-apps/api/event";
import { create } from "zustand";

import { makeUiLogEntry } from "@/lib";
import type { GrblSetting, GrblStreamEvent } from "@/lib/grblClient";
import { useCadCoreStore } from "./cadCoreStore";
import { useToastStore } from "./toastStore";

// UI-side mirror of the shell GRBL transport state (gcode_sender.rs).
// The shell is the source of truth; this store only renders what the
// `grbl-stream` events report.  Errors surface in both the Logs panel
// and as toasts — serial feedback is invisible otherwise.

interface GrblStoreState {
  connected: boolean;
  portName: string | null;
  baudRate: number | null;
  streaming: boolean;
  paused: boolean;
  completed: boolean;
  linesSent: number;
  linesTotal: number;
  percent: number;
  machineState: string | null;
  mpos: [number, number, number] | null;
  wpos: [number, number, number] | null;
  lastError: string | null;
  lastMessage: string | null;
  settings: GrblSetting[] | null;
  applyEvent: (event: GrblStreamEvent) => void;
}

function log(level: "info" | "warn" | "error", message: string) {
  useCadCoreStore
    .getState()
    .addLogEntry(makeUiLogEntry(level, "grbl_stream", message));
}

export const useGrblStore = create<GrblStoreState>((set) => ({
  connected: false,
  portName: null,
  baudRate: null,
  streaming: false,
  paused: false,
  completed: false,
  linesSent: 0,
  linesTotal: 0,
  percent: 0,
  machineState: null,
  mpos: null,
  wpos: null,
  lastError: null,
  lastMessage: null,
  settings: null,
  applyEvent: (event) => {
    switch (event.kind) {
      case "connected":
        // TCP links have no baud rate — omit it from the log line.
        log(
          "info",
          event.baudRate
            ? `connected to ${event.portName ?? ""} @ ${event.baudRate}`
            : `connected to ${event.portName ?? ""}`,
        );
        set({
          connected: true,
          portName: event.portName ?? null,
          baudRate: event.baudRate ?? null,
          streaming: false,
          paused: false,
          completed: false,
          linesSent: 0,
          linesTotal: 0,
          percent: 0,
          machineState: null,
          mpos: null,
          wpos: null,
          lastError: null,
          lastMessage: event.message,
          settings: null,
        });
        break;
      case "disconnected":
        log("info", "GRBL disconnected");
        set({
          connected: false,
          portName: null,
          baudRate: null,
          streaming: false,
          paused: false,
          completed: false,
          linesSent: 0,
          linesTotal: 0,
          percent: 0,
          machineState: null,
          mpos: null,
          wpos: null,
          lastError: null,
          lastMessage: event.message,
          settings: null,
        });
        break;
      case "progress":
        set({
          streaming: true,
          completed: false,
          linesSent: event.linesSent ?? 0,
          linesTotal: event.linesTotal ?? 0,
          percent: event.percent ?? 0,
          lastMessage: event.message,
        });
        break;
      case "completed":
        log("info", "stream complete");
        set({
          streaming: false,
          completed: true,
          percent: 100,
          lastMessage: event.message,
        });
        break;
      case "status":
        set({
          machineState: event.state ?? null,
          mpos: event.mpos ?? null,
          wpos: event.wpos ?? null,
        });
        break;
      case "zeroed":
        // The worker derives the result of G92 X0 Y0 Z0 locally
        // (WPos = 0,0,0 at the last known MPos) so the DRO and the
        // toolpath shift instantly — FluidNC can take seconds to
        // publish the new WCO in its status reports.  The next real
        // status report confirms or corrects these values.
        log("info", event.message);
        set((state) => ({
          mpos: event.mpos ?? state.mpos,
          wpos: event.wpos ?? state.wpos,
          lastMessage: event.message,
        }));
        break;
      case "paused":
        set({ paused: true, lastMessage: event.message });
        break;
      case "resumed":
        set({ paused: false, lastMessage: event.message });
        break;
      case "error":
        log("error", event.message);
        useToastStore.getState().pushToast("error", event.message);
        set({
          streaming: false,
          completed: false,
          linesSent: 0,
          linesTotal: 0,
          percent: 0,
          lastError: event.message,
          lastMessage: event.message,
        });
        break;
      case "reset":
        // User-initiated abort — clear job progress, no toast.
        log("info", event.message);
        set({
          streaming: false,
          completed: false,
          paused: false,
          linesSent: 0,
          linesTotal: 0,
          percent: 0,
          lastError: null,
          lastMessage: event.message,
        });
        break;
      case "settings":
        // `$$` dump result — feeds the settings dialog.
        log("info", event.message);
        set({ settings: event.settings ?? [] });
        break;
    }
  },
}));

let grblStreamUnlisten: UnlistenFn | null = null;

// Subscribes to shell GRBL events exactly once for the app's lifetime.
// Called from the GRBL panel on mount; the guard makes repeated mounts
// free.
export function initGrblStreamListener(): void {
  if (grblStreamUnlisten !== null) {
    return;
  }
  void listen<GrblStreamEvent>("grbl-stream", (event) => {
    useGrblStore.getState().applyEvent(event.payload);
  }).then((unlisten) => {
    grblStreamUnlisten = unlisten;
  });
}
