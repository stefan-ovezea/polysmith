import { create } from "zustand";
import type {
  CoreMessage,
  DocumentState,
  DocumentExportResult,
  SessionState,
  ViewportState,
} from "../types/ipc";
import {
  getDocumentFromMessage,
  getDocumentExportFromMessage,
  getErrorFromMessage,
  getViewportFromMessage,
} from "../lib/ipcProtocol";

interface CadCoreStoreState {
  status: "idle" | "starting" | "connected" | "error" | "stopped";
  messages: string[];
  document: DocumentState | null;
  session: SessionState | null;
  viewport: ViewportState | null;
  lastExport: DocumentExportResult | null;
  lastEvent: CoreMessage | null;
  setStatus: (status: CadCoreStoreState["status"]) => void;
  addMessage: (message: string) => void;
  handleCoreMessage: (message: CoreMessage) => void;
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

export const useCadCoreStore = create<CadCoreStoreState>((set) => ({
  status: "idle",
  messages: [],
  document: null,
  session: null,
  viewport: null,
  lastExport: null,
  lastEvent: null,
  setStatus: (status) => set({ status }),
  addMessage: (message) =>
    set((state) => ({ messages: [...state.messages, message] })),
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

      const error = getErrorFromMessage(message);
      const renderedMessage =
        message.type === "error"
          ? `error: ${error?.payload.code} - ${error?.payload.message}`
          : message.type === "document_exported"
            ? `event: document_exported - ${message.payload.file_path}`
            : `event: ${message.type}`;

      return {
        ...nextState,
        messages: [...state.messages, renderedMessage],
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
    message.type === "document_saved"
  ) {
    return "connected";
  }
  return prevStatus;
}
