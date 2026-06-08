import type { Dispatch, MutableRefObject, SetStateAction } from "react";
import { getCurrentWindow } from "@tauri-apps/api/window";

import { awaitDocumentChange, useCadCoreStore } from "../state";
import { readProjectThumbnail } from "../lib";
import type { CategoryId } from "../layout";
import type { DocumentState } from "../types";
import {
  defaultHiddenSketchIdsForLoadedDocument,
  documentHasSolidBody,
  type PendingUnsavedAction,
  type SavedDocumentBaseline,
  type SidebarTab,
} from "./appState";
import { saveCurrentDocumentFromContext } from "./documentSave";
import type { DialogTranslate } from "./documentDialogs";

interface DocumentLifecycleActionsContext {
  document: DocumentState | null;
  currentProjectPath: string | null;
  pendingUnsavedAction: PendingUnsavedAction | null;
  translate: DialogTranslate;
  createDocument: () => Promise<void>;
  loadDocument: (filePath: string) => Promise<void>;
  saveDocument: (filePath: string) => Promise<void>;
  recordRecentProject: (
    filePath: string,
    thumbnailDataUrl: string | null,
    parentFolderId?: string | null,
  ) => Promise<void>;
  setCurrentProjectPath: Dispatch<SetStateAction<string | null>>;
  setSavedDocumentBaseline: Dispatch<
    SetStateAction<SavedDocumentBaseline | null>
  >;
  setHiddenFeatureIds: Dispatch<SetStateAction<Set<string>>>;
  setHiddenCategories: Dispatch<SetStateAction<Set<CategoryId>>>;
  setSidebarTab: Dispatch<SetStateAction<SidebarTab>>;
  setPendingUnsavedAction: Dispatch<
    SetStateAction<PendingUnsavedAction | null>
  >;
  originVisibilityManuallyChangedRef: MutableRefObject<boolean>;
  snapshotCaptureRef: MutableRefObject<(() => string | null) | null>;
  allowAppCloseRef: MutableRefObject<boolean>;
  runAction: (action: () => Promise<void>) => Promise<void>;
  addMessage: (message: string) => void;
}

export function createDocumentLifecycleActions({
  document,
  currentProjectPath,
  pendingUnsavedAction,
  translate,
  createDocument,
  loadDocument,
  saveDocument,
  recordRecentProject,
  setCurrentProjectPath,
  setSavedDocumentBaseline,
  setHiddenFeatureIds,
  setHiddenCategories,
  setSidebarTab,
  setPendingUnsavedAction,
  originVisibilityManuallyChangedRef,
  snapshotCaptureRef,
  allowAppCloseRef,
  runAction,
  addMessage,
}: DocumentLifecycleActionsContext) {
  function captureProjectThumbnail() {
    return snapshotCaptureRef.current?.() ?? null;
  }

  async function saveCurrentDocument(parentFolderId?: string | null) {
    return saveCurrentDocumentFromContext({
      document,
      currentProjectPath,
      parentFolderId,
      translate,
      addMessage,
      saveDocument,
      captureProjectThumbnail,
      getCurrentDocument: () => useCadCoreStore.getState().document,
      setCurrentProjectPath,
      setSavedDocumentBaseline,
      recordRecentProject,
    });
  }

  async function performCreateDocument() {
    const documentPromise = awaitDocumentChange(
      (next, previous) => next.document_id !== previous?.document_id,
    );
    await createDocument();
    const nextDocument = await documentPromise;
    setCurrentProjectPath(null);
    setSavedDocumentBaseline(null);
    setHiddenFeatureIds(new Set<string>());
    setHiddenCategories(new Set<CategoryId>());
    originVisibilityManuallyChangedRef.current = false;
    addMessage(`created: ${nextDocument.name}`);
  }

  async function performCreateAndSaveProject(parentFolderId: string | null) {
    await performCreateDocument();
    await saveCurrentDocument(parentFolderId);
  }

  async function performLoadDocument(filePath: string) {
    const documentPromise = awaitDocumentChange(() => true);
    await loadDocument(filePath);
    const loadedDocument = await documentPromise;
    setCurrentProjectPath(filePath);
    setSavedDocumentBaseline({
      documentId: loadedDocument.document_id,
      revision: loadedDocument.revision,
    });
    setSidebarTab("hierarchy");
    const loadedDocumentHasSolidBody = documentHasSolidBody(loadedDocument);
    setHiddenFeatureIds(defaultHiddenSketchIdsForLoadedDocument(loadedDocument));
    setHiddenCategories(
      loadedDocumentHasSolidBody
        ? new Set<CategoryId>(["origin"])
        : new Set<CategoryId>(),
    );
    originVisibilityManuallyChangedRef.current = false;
    const thumbnailDataUrl = await readProjectThumbnail(filePath);
    try {
      await recordRecentProject(filePath, thumbnailDataUrl);
    } catch (error) {
      addMessage(`recent projects save error: ${String(error)}`);
    }
    addMessage(`loaded: ${filePath}`);
  }

  async function executePendingAction(action: PendingUnsavedAction) {
    if (action.kind === "quit") {
      allowAppCloseRef.current = true;
      await getCurrentWindow().destroy();
      return;
    }
    if (action.kind === "new") {
      await runAction(performCreateDocument);
      return;
    }
    if (action.kind === "newProject") {
      await runAction(async () => {
        await performCreateAndSaveProject(action.parentFolderId);
      });
      return;
    }
    await runAction(async () => {
      await performLoadDocument(action.filePath);
    });
  }

  async function saveThenContinuePendingAction() {
    if (!pendingUnsavedAction) {
      return;
    }
    const action = pendingUnsavedAction;
    await runAction(async () => {
      const didSave = await saveCurrentDocument();
      if (!didSave) {
        return;
      }
      setPendingUnsavedAction(null);
      await executePendingAction(action);
    });
  }

  function discardThenContinuePendingAction() {
    if (!pendingUnsavedAction) {
      return;
    }
    const action = pendingUnsavedAction;
    setPendingUnsavedAction(null);
    void executePendingAction(action);
  }

  return {
    discardThenContinuePendingAction,
    executePendingAction,
    saveCurrentDocument,
    saveThenContinuePendingAction,
  };
}
