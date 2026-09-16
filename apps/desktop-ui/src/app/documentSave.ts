import { awaitDocumentSaved } from "../state";
import { writeProjectThumbnail } from "../lib";
import type { DocumentState } from "../types";
import type { SavedDocumentBaseline } from "./appState";
import { pickSaveDocumentPath, type DialogTranslate } from "./documentDialogs";
import { directoryFromFilePath } from "./lastUsedDirectory";

interface SaveCurrentDocumentContext {
  document: DocumentState | null;
  currentProjectPath: string | null;
  parentFolderId?: string | null;
  // When true (Save As), always open the picker instead of reusing
  // currentProjectPath.
  forcePick?: boolean;
  translate: DialogTranslate;
  addMessage: (message: string) => void;
  saveDocument: (filePath: string) => Promise<void>;
  captureProjectThumbnail: () => string | null;
  getCurrentDocument: () => DocumentState | null;
  setCurrentProjectPath: (filePath: string) => void;
  setSavedDocumentBaseline: (baseline: SavedDocumentBaseline) => void;
  recordRecentProject: (
    filePath: string,
    thumbnailDataUrl: string | null,
    parentFolderId?: string | null,
  ) => Promise<void>;
}

export async function saveCurrentDocumentFromContext({
  document,
  currentProjectPath,
  parentFolderId,
  forcePick = false,
  translate,
  addMessage,
  saveDocument,
  captureProjectThumbnail,
  getCurrentDocument,
  setCurrentProjectPath,
  setSavedDocumentBaseline,
  recordRecentProject,
}: SaveCurrentDocumentContext) {
  if (!document) {
    return false;
  }

  const filePath = await resolveSavePath({
    currentProjectPath,
    forcePick,
    document,
    translate,
    addMessage,
  });
  if (!filePath) {
    return false;
  }

  await saveDocumentAndWait(filePath, saveDocument);
  const thumbnailDataUrl = captureProjectThumbnail();
  await writeProjectThumbnail(filePath, thumbnailDataUrl);

  const savedDocument = getCurrentDocument() ?? document;
  setCurrentProjectPath(filePath);
  setSavedDocumentBaseline({
    documentId: savedDocument.document_id,
    revision: savedDocument.revision,
  });
  await recordRecentProjectSafely({
    filePath,
    thumbnailDataUrl,
    parentFolderId,
    addMessage,
    recordRecentProject,
  });
  addMessage(`saved: ${filePath}`);
  return true;
}

async function resolveSavePath({
  currentProjectPath,
  forcePick,
  document,
  translate,
  addMessage,
}: Pick<
  SaveCurrentDocumentContext,
  | "currentProjectPath"
  | "forcePick"
  | "document"
  | "translate"
  | "addMessage"
>) {
  if (!forcePick && currentProjectPath) {
    return currentProjectPath;
  }
  return pickSaveDocumentPath({
    translate,
    documentName: document?.name,
    addMessage,
    // Save As starts next to the current file; a first save falls back
    // to the remembered directory inside the picker.
    directory: currentProjectPath
      ? directoryFromFilePath(currentProjectPath)
      : undefined,
  });
}

async function saveDocumentAndWait(
  filePath: string,
  saveDocument: (filePath: string) => Promise<void>,
) {
  const savedPromise = awaitDocumentSaved((savedPath) => savedPath === filePath);
  try {
    await saveDocument(filePath);
    await savedPromise;
  } catch (error) {
    void savedPromise.catch(() => {});
    throw error;
  }
}

async function recordRecentProjectSafely({
  filePath,
  thumbnailDataUrl,
  parentFolderId,
  addMessage,
  recordRecentProject,
}: Pick<
  SaveCurrentDocumentContext,
  "parentFolderId" | "addMessage" | "recordRecentProject"
> & {
  filePath: string;
  thumbnailDataUrl: string | null;
}) {
  try {
    await recordRecentProject(filePath, thumbnailDataUrl, parentFolderId);
  } catch (error) {
    addMessage(`recent projects save error: ${String(error)}`);
  }
}
