import { useEffect } from "react";
import {
  onCadCoreError,
  onCadCoreEvent,
  onCadCoreExited,
  onCadCoreLog,
  sendCoreCommand,
  startCadCore,
} from "../lib/cadCoreClient";
import {
  makeCreateDocumentCommand,
  makeAddBoxFeatureCommand,
  makeClearSelectionCommand,
  makeDeleteFeatureCommand,
  makeGetDocumentStateCommand,
  makeGetSessionStateCommand,
  makeGetViewportStateCommand,
  makePingCommand,
  makeRedoCommand,
  makeRenameFeatureCommand,
  makeSelectFeatureCommand,
  makeUndoCommand,
  makeUpdateBoxFeatureCommand,
  parseCoreMessage,
} from "../lib/ipcProtocol";
import { useCadCoreStore } from "../state/cadCoreStore";

export function useCadCore() {
  const addMessage = useCadCoreStore((state) => state.addMessage);
  const handleCoreMessage = useCadCoreStore((state) => state.handleCoreMessage);
  const setStatus = useCadCoreStore((state) => state.setStatus);

  useEffect(() => {
    let disposed = false;
    const unlistenFns: Array<() => void> = [];

    async function setupListeners() {
      const unlistenEvent = await onCadCoreEvent((payload) => {
        try {
          const message = parseCoreMessage(payload);
          handleCoreMessage(message);
        } catch (error) {
          addMessage(`parse error: ${String(error)}`);
          setStatus("error");
        }
      });

      const unlistenLog = await onCadCoreLog((line) => {
        addMessage(`log: ${line}`);
      });

      const unlistenError = await onCadCoreError((message) => {
        addMessage(`bridge error: ${message}`);
        setStatus("error");
      });

      const unlistenExited = await onCadCoreExited((message) => {
        addMessage(`exit: ${message}`);
        setStatus("stopped");
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
  }, [addMessage, handleCoreMessage, setStatus]);

  return {
    start: async () => {
      setStatus("starting");
      const result = await startCadCore();
      addMessage(`start: ${result}`);
    },
    ping: async () => {
      await sendCoreCommand(makePingCommand());
    },
    createDocument: async () => {
      await sendCoreCommand(makeCreateDocumentCommand());
      await sendCoreCommand(makeGetSessionStateCommand());
      await sendCoreCommand(makeGetViewportStateCommand());
    },
    refreshDocument: async () => {
      await sendCoreCommand(makeGetDocumentStateCommand());
    },
    refreshSession: async () => {
      await sendCoreCommand(makeGetSessionStateCommand());
    },
    refreshViewport: async () => {
      await sendCoreCommand(makeGetViewportStateCommand());
    },
    addBoxFeature: async (width: number, height: number, depth: number) => {
      await sendCoreCommand(makeAddBoxFeatureCommand(width, height, depth));
      await sendCoreCommand(makeGetViewportStateCommand());
    },
    updateBoxFeature: async (
      featureId: string,
      width: number,
      height: number,
      depth: number,
    ) => {
      await sendCoreCommand(
        makeUpdateBoxFeatureCommand(featureId, width, height, depth),
      );
      await sendCoreCommand(makeGetSessionStateCommand());
      await sendCoreCommand(makeGetViewportStateCommand());
    },
    renameFeature: async (featureId: string, name: string) => {
      await sendCoreCommand(makeRenameFeatureCommand(featureId, name));
      await sendCoreCommand(makeGetSessionStateCommand());
      await sendCoreCommand(makeGetViewportStateCommand());
    },
    deleteFeature: async (featureId: string) => {
      await sendCoreCommand(makeDeleteFeatureCommand(featureId));
      await sendCoreCommand(makeGetSessionStateCommand());
      await sendCoreCommand(makeGetViewportStateCommand());
    },
    undo: async () => {
      await sendCoreCommand(makeUndoCommand());
      await sendCoreCommand(makeGetSessionStateCommand());
      await sendCoreCommand(makeGetViewportStateCommand());
    },
    redo: async () => {
      await sendCoreCommand(makeRedoCommand());
      await sendCoreCommand(makeGetSessionStateCommand());
      await sendCoreCommand(makeGetViewportStateCommand());
    },
    selectFeature: async (featureId: string) => {
      await sendCoreCommand(makeSelectFeatureCommand(featureId));
      await sendCoreCommand(makeGetViewportStateCommand());
    },
    clearSelection: async () => {
      await sendCoreCommand(makeClearSelectionCommand());
      await sendCoreCommand(makeGetViewportStateCommand());
    },
  };
}
