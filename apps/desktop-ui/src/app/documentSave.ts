import { awaitDocumentSaved } from "../state";
import { writeProjectThumbnail } from "../lib";
import type { DocumentState } from "../types";
import type { SavedDocumentBaseline } from "./appState";
import { pickSaveDocumentPath, type DialogTranslate } from "./documentDialogs";

interface SaveCurrentDocumentContext {
  document: DocumentState | null;
  currentProjectPath: string | null;
  parentFolderId?: string | null;
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
  document,
  translate,
  addMessage,
}: Pick<
  SaveCurrentDocumentContext,
  "currentProjectPath" | "document" | "translate" | "addMessage"
>) {
  return (
    currentProjectPath ??
    (await pickSaveDocumentPath({
      translate,
      documentName: document?.name,
      addMessage,
    }))
  );
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
