import type { CamOperation } from "../layout/CamOperationPanel";
import type { CamOperationType } from "../layout/header/CamToolbar";
import { projectNameFromPath } from "../lib";
import type { DocumentState } from "../types";
import type { SavedDocumentBaseline } from "./appState";

export function buildCamOperations(
  document: DocumentState | null,
): CamOperation[] {
  return (document?.cam.operations ?? []).map((operation) => ({
    id: operation.op_id,
    setupId: operation.setup_id ?? "",
    name: operation.name,
    type: coreCamOperationTypeToUi(operation.type),
    mode:
      operation.type === "laser_cut"
        ? operation.parameters.laser?.mode
        : undefined,
    status: operation.status,
    statusMessage: operation.status_message,
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

// Core operation `type` strings → the UI's display-level operation
// kinds (CamOperationType in layout/header/CamToolbar.tsx).
function coreCamOperationTypeToUi(type: string): CamOperationType {
  switch (type) {
    case "face_milling":
      return "faceMilling";
    case "laser_cut":
      return "laserCut";
    case "pocket_2d":
      return "pocket";
    case "drilling":
      return "drill";
    default:
      return "profile";
  }
}
