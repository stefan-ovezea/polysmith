import type { CamOperation } from "../layout/CamOperationPanel";
import type { CamOperationType } from "../layout/header/CamToolbar";
import { projectNameFromPath } from "../lib";
import type { DocumentState } from "../types";
import type { SavedDocumentBaseline } from "./appState";

export function buildCamOperations(
  document: DocumentState | null,
): CamOperation[] {
  return ((document?.cam as any)?.operations ?? []).map((operation: any) => ({
    id: operation.id,
    name: operation.name,
    type: coreCamOperationTypeToUi(operation.type),
  }));
}

export function computeDocumentUiState({
  document,
  currentProjectPath,
  savedDocumentBaseline,
  untitledName,
}: {
  document: DocumentState | null;
  currentProjectPath: string | null;
  savedDocumentBaseline: SavedDocumentBaseline | null;
  untitledName: string;
}) {
  const activeSketchFeature =
    document?.feature_history.find(
      (entry) => entry.feature_id === document?.active_sketch_feature_id,
    ) ?? null;
  const hasDocumentContent =
    document !== null &&
    (document.feature_history.some((feature) => feature.kind !== "root_part") ||
      document.parameters.length > 0);
  const isDocumentDirty =
    document !== null &&
    (hasDocumentContent || currentProjectPath !== null) &&
    (savedDocumentBaseline?.documentId !== document.document_id ||
      savedDocumentBaseline.revision !== document.revision);
  const currentDocumentName = currentProjectPath
    ? projectNameFromPath(currentProjectPath)
    : document?.name || untitledName;
  const pendingMirror =
    activeSketchFeature?.sketch_parameters?.pending_mirror ?? null;

  return {
    activeSketchFeature,
    isDocumentDirty,
    currentDocumentName,
    windowDocumentTitle: `${currentDocumentName}${isDocumentDirty ? "*" : ""} - Polysmith`,
    pendingMirror,
    isMirrorToolOpen: pendingMirror !== null,
  };
}

function coreCamOperationTypeToUi(type: number): CamOperationType {
  switch (type) {
    case 0:
      return "faceMilling";
    case 1:
      return "pocket";
    case 2:
      return "drill";
    default:
      return "profile";
  }
}
