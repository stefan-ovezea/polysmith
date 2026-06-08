import { useEffect, useRef } from "react";
import { matchesHotkey } from "../config";
import type { AppHotkeys } from "../config";
import type { SketchTool } from "../types";
import type { DocumentState } from "../types/ipc";
import { IS_MACOS } from "./appState";
import type { ActiveToolActions } from "./activeToolActions";

type AsyncAction = () => Promise<void>;
type AnyAsyncAction = () => Promise<unknown>;
type RunAction = (action: AsyncAction) => Promise<void>;

export interface AppHotkeyContext {
  event: KeyboardEvent;
  hotkeys: AppHotkeys;
  actions: ActiveToolActions;
  state: {
    activeSketchPlaneId: string | null;
    activeSketchTool: SketchTool;
    canCreateSketch: boolean;
    canUndo: boolean;
    canRedo: boolean;
    document: DocumentState | null;
  };
  callbacks: {
    cancelActiveTool: AnyAsyncAction;
    runAction: RunAction;
    saveCurrentDocument: AnyAsyncAction;
    clearSelection: AsyncAction;
    undo: AsyncAction;
    redo: AsyncAction;
    triggerExtrudeAction: AsyncAction;
    triggerEdgeOpAction: (kind: "fillet" | "chamfer") => Promise<void>;
    triggerCreateSketchAction: AsyncAction;
    setSketchTool: (tool: SketchTool) => Promise<void>;
  };
}

export type AppHotkeyRegistration = Omit<AppHotkeyContext, "event">;

export function useAppHotkeys(context: AppHotkeyRegistration) {
  const contextRef = useRef(context);
  contextRef.current = context;

  useEffect(() => {
    function handleKeyDown(event: KeyboardEvent) {
      handleAppHotkey({
        event,
        ...contextRef.current,
      });
    }

    window.addEventListener("keydown", handleKeyDown);
    return () => {
      window.removeEventListener("keydown", handleKeyDown);
    };
  }, []);
}

export function handleAppHotkey({
  event,
  hotkeys,
  actions,
  state,
  callbacks,
}: AppHotkeyContext) {
  if (event.defaultPrevented) {
    return false;
  }

  const hasCancelableTool = hasCancelableToolAction(actions);

  if (handleCancelableEscape(event, hasCancelableTool, callbacks)) {
    return true;
  }

  if (isTypingTarget(event.target)) {
    return false;
  }

  if (handleSaveHotkey(event, callbacks)) {
    return true;
  }

  if (handleSelectionEscape(event, hasCancelableTool, state, callbacks)) {
    return true;
  }

  if (handleHistoryHotkey(event, hotkeys, state, callbacks)) {
    return true;
  }

  if (handleToolbarHotkey(event, hotkeys, state, callbacks)) {
    return true;
  }

  return false;
}

function handleCancelableEscape(
  event: KeyboardEvent,
  hasCancelableTool: boolean,
  callbacks: AppHotkeyContext["callbacks"],
) {
  if (event.code !== "Escape" || !hasCancelableTool) {
    return false;
  }
  consumeEvent(event);
  void callbacks.cancelActiveTool();
  return true;
}

function handleSaveHotkey(
  event: KeyboardEvent,
  callbacks: AppHotkeyContext["callbacks"],
) {
  if (!isPlainSaveHotkey(event)) {
    return false;
  }
  event.preventDefault();
  void callbacks.runAction(async () => {
    await callbacks.saveCurrentDocument();
  });
  return true;
}

function handleSelectionEscape(
  event: KeyboardEvent,
  hasCancelableTool: boolean,
  state: AppHotkeyContext["state"],
  callbacks: AppHotkeyContext["callbacks"],
) {
  if (event.code !== "Escape" || state.activeSketchPlaneId || hasCancelableTool) {
    return false;
  }
  if (!hasDocumentSelection(state.document)) {
    return false;
  }
  event.preventDefault();
  void callbacks.runAction(callbacks.clearSelection);
  return true;
}

function handleHistoryHotkey(
  event: KeyboardEvent,
  hotkeys: AppHotkeys,
  state: AppHotkeyContext["state"],
  callbacks: AppHotkeyContext["callbacks"],
) {
  if (matchesHotkey(event, hotkeys.global.undo)) {
    event.preventDefault();
    if (state.canUndo) {
      void callbacks.runAction(callbacks.undo);
    }
    return true;
  }

  if (matchesHotkey(event, hotkeys.global.redo)) {
    event.preventDefault();
    if (state.canRedo) {
      void callbacks.runAction(callbacks.redo);
    }
    return true;
  }

  return false;
}

function handleToolbarHotkey(
  event: KeyboardEvent,
  hotkeys: AppHotkeys,
  state: AppHotkeyContext["state"],
  callbacks: AppHotkeyContext["callbacks"],
) {
  if (handleExtrudeHotkey(event, hotkeys, callbacks)) {
    return true;
  }

  if (handleFilletHotkey(event, hotkeys, state, callbacks)) {
    return true;
  }

  if (handleCreateSketchHotkey(event, hotkeys, state, callbacks)) {
    return true;
  }

  if (handleProjectHotkey(event, hotkeys, state, callbacks)) {
    return true;
  }

  return false;
}

function handleExtrudeHotkey(
  event: KeyboardEvent,
  hotkeys: AppHotkeys,
  callbacks: AppHotkeyContext["callbacks"],
) {
  if (!matchesHotkey(event, hotkeys.toolbar.extrude)) {
    return false;
  }
  event.preventDefault();
  void callbacks.triggerExtrudeAction();
  return true;
}

function handleFilletHotkey(
  event: KeyboardEvent,
  hotkeys: AppHotkeys,
  state: AppHotkeyContext["state"],
  callbacks: AppHotkeyContext["callbacks"],
) {
  if (!matchesHotkey(event, hotkeys.toolbar.fillet)) {
    return false;
  }
  if (state.activeSketchPlaneId) {
    return false;
  }
  event.preventDefault();
  void callbacks.triggerEdgeOpAction("fillet");
  return true;
}

function handleCreateSketchHotkey(
  event: KeyboardEvent,
  hotkeys: AppHotkeys,
  state: AppHotkeyContext["state"],
  callbacks: AppHotkeyContext["callbacks"],
) {
  if (!matchesHotkey(event, hotkeys.sketchToolbar.createSketch)) {
    return false;
  }
  if (state.activeSketchPlaneId || !state.canCreateSketch) {
    return false;
  }
  event.preventDefault();
  void callbacks.triggerCreateSketchAction();
  return true;
}

function handleProjectHotkey(
  event: KeyboardEvent,
  hotkeys: AppHotkeys,
  state: AppHotkeyContext["state"],
  callbacks: AppHotkeyContext["callbacks"],
) {
  if (!matchesHotkey(event, hotkeys.toolbar.project)) {
    return false;
  }
  if (!state.activeSketchPlaneId) {
    return false;
  }
  event.preventDefault();
  const nextTool = state.activeSketchTool === "project" ? "select" : "project";
  void callbacks.runAction(async () => {
    await callbacks.setSketchTool(nextTool);
  });
  return true;
}

function hasCancelableToolAction(actions: AppHotkeyContext["actions"]) {
  return hasCancelableAppAction(
    [
      actions.extrudeAction,
      actions.loftAction,
      actions.revolveAction,
      actions.sweepAction,
      actions.moveAction,
      actions.edgeOpAction,
      actions.shellAction,
      actions.holeAction,
      actions.offsetPlaneAction,
      actions.anglePlaneAction,
      actions.midplaneAction,
      actions.tangentPlaneAction,
      actions.constructionAxisAction,
      actions.constructionPointAction,
      actions.threadAction,
      actions.fastenerAction,
      actions.helixAction,
      actions.editingFeatureId,
    ],
    actions.materialsPanelOpen,
  );
}

function consumeEvent(event: KeyboardEvent) {
  event.preventDefault();
  event.stopPropagation();
  event.stopImmediatePropagation();
}

function isTypingTarget(target: EventTarget | null) {
  return (
    target instanceof HTMLInputElement ||
    target instanceof HTMLTextAreaElement ||
    target instanceof HTMLSelectElement ||
    (target instanceof HTMLElement && target.isContentEditable)
  );
}

function isPlainSaveHotkey(event: KeyboardEvent) {
  return (
    event.code === "KeyS" &&
    (IS_MACOS ? event.metaKey : event.ctrlKey) &&
    !event.altKey &&
    !event.shiftKey
  );
}

function hasCancelableAppAction(
  actions: readonly unknown[],
  materialsPanelOpen: boolean,
) {
  return actions.some(Boolean) || materialsPanelOpen;
}

function hasDocumentSelection(documentState: DocumentState | null) {
  return Boolean(
    documentState &&
      (documentState.selected_feature_id ||
        documentState.selected_reference_id ||
        documentState.selected_face_id ||
        documentState.selected_edge_ids.length > 0 ||
        documentState.selected_vertex_ids.length > 0),
  );
}
