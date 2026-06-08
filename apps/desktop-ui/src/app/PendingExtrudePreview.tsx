import type { Dispatch, SetStateAction } from "react";

import { ExtrudePreviewPanel } from "../layout";
import type { ExtrudeAdvancedParameters, ExtrudeMode } from "../types";
import type { DocumentState, ViewportState } from "../types/ipc";
import type { ActiveExtrudeAction } from "./appState";
import type { DefaultExtrudeSettings } from "./extrudeDefaults";

type AnyAsync = () => Promise<unknown>;

interface PendingExtrudePreviewProps {
  extrudeAction: ActiveExtrudeAction | null;
  document: DocumentState | null;
  viewport: ViewportState | null;
  disabled: boolean;
  selectedExtrudableFaceId: string | null;
  selectedSketchEntityIds: string[];
  selectedSketchProfileIds: string[];
  createExtrudeFromSelectedFace: (
    faceId: string,
    depth: number,
    mode: ExtrudeMode | null,
    targetBodyId: string | null,
    parameters?: ExtrudeAdvancedParameters | null,
  ) => Promise<void>;
  createExtrudeFromSelectedProfiles: (
    depth: number,
    mode: ExtrudeMode | null,
    targetBodyId: string | null,
    parameters?: ExtrudeAdvancedParameters | null,
  ) => Promise<void>;
  createThinExtrudeFromSelectedEntities: (
    depth: number,
    mode: ExtrudeMode | null,
    targetBodyId: string | null,
    parameters: ExtrudeAdvancedParameters,
  ) => Promise<void>;
  getDefaultExtrudeSettings: (
    profileIds: readonly string[],
  ) => DefaultExtrudeSettings;
  setExtrudeAction: Dispatch<SetStateAction<ActiveExtrudeAction | null>>;
  onCancelActiveTool: AnyAsync;
}

export function PendingExtrudePreview({
  extrudeAction,
  document,
  viewport,
  disabled,
  selectedExtrudableFaceId,
  selectedSketchEntityIds,
  selectedSketchProfileIds,
  createExtrudeFromSelectedFace,
  createExtrudeFromSelectedProfiles,
  createThinExtrudeFromSelectedEntities,
  getDefaultExtrudeSettings,
  setExtrudeAction,
  onCancelActiveTool,
}: PendingExtrudePreviewProps) {
  if (extrudeAction?.phase !== "pending") {
    return null;
  }

  return (
    <ExtrudePreviewPanel
      phase="pending"
      initialDepth={extrudeAction.initialDepth}
      initialMode={extrudeAction.initialMode}
      initialParameters={extrudeAction.initialParameters}
      selectedProfileCount={extrudeAction.profileCount}
      canCombineWithExistingBody={extrudeAction.canCombineWithExistingBody}
      availableTargetBodies={viewport?.bodies ?? []}
      selectedFaceTargetId={document?.selected_face_id ?? null}
      initialTargetBodyId={extrudeAction.initialTargetBodyId}
      disabled={disabled}
      onPreviewDepth={async (depth) => {
        setExtrudeAction((current) =>
          current?.phase === "pending"
            ? { ...current, initialDepth: depth }
            : current,
        );
      }}
      onPreviewMode={async (mode) => {
        setExtrudeAction((current) =>
          current?.phase === "pending"
            ? {
                ...current,
                automaticMode: false,
                initialMode: mode,
                initialTargetBodyId:
                  mode === "new_body"
                    ? null
                    : current.initialTargetBodyId ??
                      getDefaultExtrudeSettings(selectedSketchProfileIds)
                        .targetBodyId,
              }
            : current,
        );
      }}
      onPreviewTargetBody={async (targetBodyId) => {
        setExtrudeAction((current) =>
          current?.phase === "pending"
            ? { ...current, initialTargetBodyId: targetBodyId }
            : current,
        );
      }}
      onPreviewParameters={async (parameters) => {
        if ("sketch_feature_id" in parameters) {
          return;
        }
        setExtrudeAction((current) =>
          current?.phase === "pending"
            ? {
                ...current,
                automaticMode: false,
                initialParameters: null,
              }
            : current,
        );
      }}
      onConfirm={async (depth, mode, targetBodyId, parameters) => {
        if (
          parameters.thin.enabled &&
          selectedSketchProfileIds.length === 0 &&
          selectedExtrudableFaceId === null &&
          selectedSketchEntityIds.length > 0
        ) {
          await createThinExtrudeFromSelectedEntities(
            depth,
            mode,
            targetBodyId,
            parameters,
          );
          return;
        }
        if (selectedSketchProfileIds.length > 0) {
          await createExtrudeFromSelectedProfiles(
            depth,
            mode,
            targetBodyId,
            parameters,
          );
          return;
        }
        if (selectedExtrudableFaceId) {
          await createExtrudeFromSelectedFace(
            selectedExtrudableFaceId,
            depth,
            mode,
            targetBodyId,
            parameters,
          );
        }
      }}
      onCancel={async () => {
        await onCancelActiveTool();
      }}
    />
  );
}
