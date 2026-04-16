import { create } from "zustand";
import type {
  CoreMessage,
  DocumentState,
  SessionState,
  ViewportState,
} from "../types/ipc";
import {
  getDocumentFromMessage,
  getErrorFromMessage,
  getViewportFromMessage,
} from "../lib/ipcProtocol";

interface CadCoreStoreState {
  status: "idle" | "starting" | "connected" | "error" | "stopped";
  messages: string[];
  document: DocumentState | null;
  session: SessionState | null;
  viewport: ViewportState | null;
  lastEvent: CoreMessage | null;
  setStatus: (status: CadCoreStoreState["status"]) => void;
  addMessage: (message: string) => void;
  handleCoreMessage: (message: CoreMessage) => void;
}

export const useCadCoreStore = create<CadCoreStoreState>((set) => ({
  status: "idle",
  messages: [],
  document: null,
  session: null,
  viewport: null,
  lastEvent: null,
  setStatus: (status) => set({ status }),
  addMessage: (message) =>
    set((state) => ({ messages: [...state.messages, message] })),
  handleCoreMessage: (message) =>
    set((state) => {
      const nextState: Partial<CadCoreStoreState> = {
        lastEvent: message,
        status:
          message.type === "error"
            ? "error"
            : message.type === "hello" || message.type === "pong" || message.type === "document_created" || message.type === "document_state" || message.type === "viewport_state"
              ? "connected"
              : state.status,
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

      const error = getErrorFromMessage(message);
      const renderedMessage =
        message.type === "error"
          ? `error: ${error?.payload.code} - ${error?.payload.message}`
          : `event: ${message.type}`;

      return {
        ...nextState,
        messages: [...state.messages, renderedMessage],
      } as CadCoreStoreState;
    }),
}));
