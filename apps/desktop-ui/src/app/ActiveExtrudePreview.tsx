import type { Dispatch, SetStateAction } from "react";

import { ExtrudePreviewPanel } from "../layout/ExtrudePreviewPanel";
import type {
  ExtrudeAdvancedParameters,
  ExtrudeMode,
} from "../types";
import type { DocumentState, ViewportState } from "../types/ipc";
import type { ActiveExtrudeAction } from "./appState";
import { activeExtrudeAdvancedParameters } from "./extrudeCreationState";
import type { ExtrudeUpdateCallbacks } from "./extrudeUpdateCallbacks";

type AsyncVoid = () => Promise<void>;
type AnyAsync = () => Promise<unknown>;
type RunAction = (action: AsyncVoid) => Promise<void>;

export interface ActiveExtrudePreviewProps extends ExtrudeUpdateCallbacks {
  extrudeAction: ActiveExtrudeAction | null;
  document: DocumentState | null;
  viewport: ViewportState | null;
  disabled: boolean;
  runAction: RunAction;
  recreateNewProfileExtrudePreview: (
    action: ActiveExtrudeAction,
    depth: number,
    mode: ExtrudeMode,
    targetBodyId: string | null,
    parameters: ExtrudeAdvancedParameters | null,
  ) => Promise<boolean>;
  setExtrudeAction: Dispatch<SetStateAction<ActiveExtrudeAction | null>>;
  setHiddenFeatureIds: Dispatch<SetStateAction<Set<string>>>;
  clearSelection: AsyncVoid;
  restoreTimelineCursorAfterEdit: AsyncVoid;
  cancelActiveTool: AnyAsync;
}

export function ActiveExtrudePreview({
  extrudeAction,
  document,
  viewport,
  disabled,
  runAction,
  updateExtrudeDepth,
  updateExtrudeMode,
  updateExtrudeTargetBody,
  updateExtrudeParameters,
  recreateNewProfileExtrudePreview,
  setExtrudeAction,
  setHiddenFeatureIds,
  clearSelection,
  restoreTimelineCursorAfterEdit,
  cancelActiveTool,
}: ActiveExtrudePreviewProps) {
  if (extrudeAction?.phase !== "active" || !extrudeAction.featureId) {
    return null;
  }

  const activeExtrudeFeatureId = extrudeAction.featureId;
  const activeExtrudeFeatureIds = activeExtrudeFeatureIdsFor(extrudeAction);
  const activeExtrudeFeature = findFeature(document, activeExtrudeFeatureId);
  const extrudePreviewError = previewErrorFor(activeExtrudeFeature);

  return (
    <ExtrudePreviewPanel
      initialDepth={extrudeAction.initialDepth}
      initialMode={extrudeAction.initialMode}
      initialParameters={extrudeAction.initialParameters}
      selectedProfileCount={extrudeAction.profileCount}
      canCombineWithExistingBody={extrudeAction.canCombineWithExistingBody}
      availableTargetBodies={availableTargetBodiesFor(
        viewport,
        activeExtrudeFeatureIds,
      )}
      selectedFaceTargetId={document?.selected_face_id ?? null}
      initialTargetBodyId={extrudeAction.initialTargetBodyId}
      previewError={extrudePreviewError}
      disabled={disabled}
      onPreviewDepth={async (depth) => {
        await runAction(async () => {
          await updateExtrudeFeatures(activeExtrudeFeatureIds, (featureId) =>
            updateExtrudeDepth(featureId, depth),
          );
        });
        setExtrudeAction((current) =>
          current?.phase === "active" &&
          current.featureId === activeExtrudeFeatureId
            ? { ...current, initialDepth: depth }
            : current,
        );
      }}
      onPreviewMode={async (mode) => {
        const recreated = await recreateNewProfileExtrudePreview(
          extrudeAction,
          activeExtrudeFeature?.extrude_parameters?.depth ??
            extrudeAction.initialDepth,
          mode,
          activeExtrudeTargetBodyId(
            mode,
            activeExtrudeFeature,
            extrudeAction.initialTargetBodyId,
          ),
          activeExtrudeAdvancedParameters(
            activeExtrudeFeature?.extrude_parameters,
            mode,
          ),
        );
        if (recreated) {
          return;
        }
        await runAction(async () => {
          await updateExtrudeFeatures(activeExtrudeFeatureIds, (featureId) =>
            updateExtrudeMode(featureId, mode),
          );
        });
      }}
      onPreviewTargetBody={async (targetBodyId) => {
        await runAction(async () => {
          await updateExtrudeFeatures(activeExtrudeFeatureIds, (featureId) =>
            updateExtrudeTargetBody(featureId, targetBodyId),
          );
        });
      }}
      onPreviewParameters={async (parameters) => {
        if (!("sketch_feature_id" in parameters)) {
          return;
        }
        await runAction(async () => {
          await updateExtrudeFeatures(activeExtrudeFeatureIds, (featureId) =>
            updateExtrudeParameters(featureId, parameters),
          );
        });
      }}
      onConfirm={async () => {
        await confirmActiveExtrude({
          document,
          activeExtrudeFeatureId,
          setHiddenFeatureIds,
          setExtrudeAction,
          runAction,
          clearSelection,
          restoreTimelineCursorAfterEdit,
        });
      }}
      onCancel={async () => {
        await cancelActiveTool();
      }}
    />
  );
}

function activeExtrudeFeatureIdsFor(extrudeAction: ActiveExtrudeAction) {
  return extrudeAction.featureIds.length > 0
    ? extrudeAction.featureIds
    : [extrudeAction.featureId!];
}

function availableTargetBodiesFor(
  viewport: ViewportState | null,
  activeExtrudeFeatureIds: readonly string[],
) {
  return (viewport?.bodies ?? []).filter(
    (body) => !activeExtrudeFeatureIds.includes(body.id),
  );
}

function findFeature(
  document: DocumentState | null,
  featureId: string,
) {
  return (
    document?.feature_history.find((entry) => entry.feature_id === featureId) ??
    null
  );
}

function previewErrorFor(
  feature: DocumentState["feature_history"][number] | null,
) {
  return feature?.dependency_broken || feature?.status === "warning"
    ? feature.dependency_warning ?? null
    : null;
}

function activeExtrudeTargetBodyId(
  mode: ExtrudeMode,
  feature: DocumentState["feature_history"][number] | null,
  initialTargetBodyId: string | null,
) {
  return mode === "new_body"
    ? null
    : feature?.extrude_parameters?.target_body_id ?? initialTargetBodyId;
}

async function updateExtrudeFeatures(
  featureIds: readonly string[],
  updateFeature: (featureId: string) => Promise<void>,
) {
  for (const featureId of featureIds) {
    await updateFeature(featureId);
  }
}

async function confirmActiveExtrude({
  document,
  activeExtrudeFeatureId,
  setHiddenFeatureIds,
  setExtrudeAction,
  runAction,
  clearSelection,
  restoreTimelineCursorAfterEdit,
}: {
  document: DocumentState | null;
  activeExtrudeFeatureId: string;
  setHiddenFeatureIds: Dispatch<SetStateAction<Set<string>>>;
  setExtrudeAction: Dispatch<SetStateAction<ActiveExtrudeAction | null>>;
  runAction: RunAction;
  clearSelection: AsyncVoid;
  restoreTimelineCursorAfterEdit: AsyncVoid;
}) {
  const confirmedFeature = findFeature(document, activeExtrudeFeatureId);
  const sketchFeatureId =
    confirmedFeature?.extrude_parameters?.sketch_feature_id ?? null;
  const confirmedMode = confirmedFeature?.extrude_parameters?.mode ?? null;
  hideSourceSketchAfterConfirm(setHiddenFeatureIds, sketchFeatureId);
  setExtrudeAction(null);
  await clearCutSelectionAfterConfirm(confirmedMode, runAction, clearSelection);
  await restoreTimelineCursorAfterEdit();
}

function hideSourceSketchAfterConfirm(
  setHiddenFeatureIds: Dispatch<SetStateAction<Set<string>>>,
  sketchFeatureId: string | null,
) {
  if (!sketchFeatureId) {
    return;
  }
  setHiddenFeatureIds((current) => {
    if (current.has(sketchFeatureId)) {
      return current;
    }
    const next = new Set(current);
    next.add(sketchFeatureId);
    return next;
  });
}

async function clearCutSelectionAfterConfirm(
  confirmedMode: ExtrudeMode | null,
  runAction: RunAction,
  clearSelection: AsyncVoid,
) {
  if (confirmedMode !== "cut") {
    return;
  }
  await runAction(async () => {
    await clearSelection();
  });
}
