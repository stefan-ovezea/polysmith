import { create } from "zustand";
import type {
  CoreMessage,
  DocumentState,
  DocumentExportResult,
  LogEntry,
  SessionState,
  ViewportState,
} from "../types/ipc";
import {
  getDocumentFromMessage,
  getDocumentExportFromMessage,
  getErrorFromMessage,
  getViewportFromMessage,
} from "../lib/ipcProtocol";

export type ToastKind = "warning" | "error";

export interface ToastNotice {
  id: string;
  kind: ToastKind;
  message: string;
}

interface CadCoreStoreState {
  status: "idle" | "starting" | "connected" | "error" | "stopped";
  messages: string[];
  toasts: ToastNotice[];
  logs: LogEntry[];
  document: DocumentState | null;
  session: SessionState | null;
  viewport: ViewportState | null;
  lastExport: DocumentExportResult | null;
  lastEvent: CoreMessage | null;
  setStatus: (status: CadCoreStoreState["status"]) => void;
  handleCoreStopped: () => void;
  addMessage: (message: string) => void;
  addToast: (kind: ToastKind, message: string) => void;
  dismissToast: (id: string) => void;
  addLogEntry: (entry: LogEntry) => void;
  clearLogs: () => void;
  handleCoreMessage: (message: CoreMessage) => void;
}

function toastId() {
  return crypto.randomUUID();
}

// Wait until the store receives a new `document` snapshot that satisfies a
// predicate. The IPC bridge is fire-and-forget (commands write to cad_core
// stdin and return synchronously, while responses arrive later through the
// `cad-core-event` Tauri event). Callers that need to read post-command
// state from the store must wait for the next event instead of reading the
// store immediately after `await sendCoreCommand(...)`.
export function awaitDocumentChange(
  predicate: (next: DocumentState, previous: DocumentState | null) => boolean,
  timeoutMs = 4000,
): Promise<DocumentState> {
  return new Promise((resolve, reject) => {
    const initial = useCadCoreStore.getState().document;
    const timer = window.setTimeout(() => {
      unsubscribe();
      reject(new Error("awaitDocumentChange: timed out"));
    }, timeoutMs);
    const unsubscribe = useCadCoreStore.subscribe((state) => {
      const doc = state.document;
      if (!doc || doc === initial) {
        return;
      }
      if (predicate(doc, initial)) {
        window.clearTimeout(timer);
        unsubscribe();
        resolve(doc);
      }
    });
  });
}

export function awaitDocumentChangeOrCoreError(
  commandId: string,
  predicate: (next: DocumentState, previous: DocumentState | null) => boolean,
  timeoutMs = 4000,
): Promise<DocumentState> {
  return new Promise((resolve, reject) => {
    const initialDocument = useCadCoreStore.getState().document;
    const initialEvent = useCadCoreStore.getState().lastEvent;
    const timer = window.setTimeout(() => {
      unsubscribe();
      reject(new Error("Timed out waiting for cad_core to finish the command"));
    }, timeoutMs);
    const unsubscribe = useCadCoreStore.subscribe((state) => {
      const event = state.lastEvent;
      if (
        event &&
        event !== initialEvent &&
        event.type === "error" &&
        event.id === commandId
      ) {
        window.clearTimeout(timer);
        unsubscribe();
        reject(
          new Error(`${event.payload.code}: ${event.payload.message}`),
        );
        return;
      }

      const doc = state.document;
      if (!doc || doc === initialDocument) {
        return;
      }
      if (predicate(doc, initialDocument)) {
        window.clearTimeout(timer);
        unsubscribe();
        resolve(doc);
      }
    });
  });
}

export function awaitDocumentExport(
  predicate: (next: DocumentExportResult) => boolean,
  timeoutMs = 10000,
): Promise<DocumentExportResult> {
  return new Promise((resolve, reject) => {
    const initial = useCadCoreStore.getState().lastExport;
    const timer = window.setTimeout(() => {
      unsubscribe();
      reject(new Error("awaitDocumentExport: timed out"));
    }, timeoutMs);
    const unsubscribe = useCadCoreStore.subscribe((state) => {
      const exportResult = state.lastExport;
      if (!exportResult || exportResult === initial) {
        return;
      }
      if (predicate(exportResult)) {
        window.clearTimeout(timer);
        unsubscribe();
        resolve(exportResult);
      }
    });
  });
}

export function awaitDocumentSaved(
  predicate: (filePath: string) => boolean,
  timeoutMs = 4000,
): Promise<string> {
  return new Promise((resolve, reject) => {
    const initial = useCadCoreStore.getState().lastEvent;
    const timer = window.setTimeout(() => {
      unsubscribe();
      reject(new Error("awaitDocumentSaved: timed out"));
    }, timeoutMs);
    const unsubscribe = useCadCoreStore.subscribe((state) => {
      const event = state.lastEvent;
      if (!event || event === initial || event.type !== "document_saved") {
        return;
      }
      const filePath = event.payload.file_path;
      if (predicate(filePath)) {
        window.clearTimeout(timer);
        unsubscribe();
        resolve(filePath);
      }
    });
  });
}

export const useCadCoreStore = create<CadCoreStoreState>((set) => ({
  status: "idle",
  messages: [],
  toasts: [],
  logs: [],
  document: null,
  session: null,
  viewport: null,
  lastExport: null,
  lastEvent: null,
  setStatus: (status) => set({ status }),
  handleCoreStopped: () =>
    set({
      status: "stopped",
      document: null,
      session: null,
      viewport: null,
      lastExport: null,
      lastEvent: null,
    }),
  addMessage: (message) =>
    set((state) => ({ messages: [...state.messages, message] })),
  addToast: (kind, message) =>
    set((state) => ({
      toasts: [...state.toasts, { id: toastId(), kind, message }].slice(-5),
    })),
  dismissToast: (id) =>
    set((state) => ({
      toasts: state.toasts.filter((toast) => toast.id !== id),
    })),
  addLogEntry: (entry) =>
    set((state) => ({ logs: [...state.logs, entry].slice(-500) })),
  clearLogs: () => set({ logs: [] }),
  handleCoreMessage: (message) =>
    set((state) => {
      const nextState: Partial<CadCoreStoreState> = {
        lastEvent: message,
        status: getStatusFromMessage(message, state.status),
      };

      const document = getDocumentFromMessage(message);
      if (document) {
        nextState.document = document;
      }

      if (message.type === "session_state") {
        nextState.session = message.payload;
      }

      const viewport = getViewportFromMessage(message);
      if (viewport) {
        nextState.viewport = viewport;
      }

      const documentExport = getDocumentExportFromMessage(message);
      if (documentExport) {
        nextState.lastExport = documentExport;
      }

      if (message.type === "log") {
        nextState.logs = [...state.logs, message.payload].slice(-500);
      }

      const error = getErrorFromMessage(message);
      const renderedMessage =
        message.type === "error"
          ? `error: ${error?.payload.code} - ${error?.payload.message}`
          : message.type === "log"
            ? `log: ${message.payload.level} - ${message.payload.message}`
          : message.type === "document_exported"
            ? `event: document_exported - ${message.payload.file_path}`
            : `event: ${message.type}`;

      const toasts = [...state.toasts];
      if (message.type === "error" && error) {
        toasts.push({
          id: toastId(),
          kind: "error",
          message: `${error.payload.code}: ${error.payload.message}`,
        });
      } else if (
        message.type === "log" &&
        (message.payload.level === "warn" || message.payload.level === "error")
      ) {
        toasts.push({
          id: toastId(),
          kind: message.payload.level === "error" ? "error" : "warning",
          message: message.payload.message,
        });
      }

      return {
        ...nextState,
        messages: [...state.messages, renderedMessage],
        toasts: toasts.slice(-5),
      } as CadCoreStoreState;
    }),
}));

function getStatusFromMessage(
  message: CoreMessage,
  prevStatus: CadCoreStoreState["status"],
): CadCoreStoreState["status"] {
  if (message.type === "error") {
    return "error";
  }
  if (
    message.type === "hello" ||
    message.type === "pong" ||
    message.type === "document_created" ||
    message.type === "document_state" ||
    message.type === "viewport_state" ||
    message.type === "document_exported" ||
    message.type === "document_saved" ||
    message.type === "log"
  ) {
    return "connected";
  }
  return prevStatus;
}
