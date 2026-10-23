import { useEffect, type Dispatch, type SetStateAction } from "react";
import { useTranslation } from "react-i18next";

import { ExtrudePreviewPanel } from "../layout/ExtrudePreviewPanel";
import { reportUiError } from "../lib/uiErrorReporting";
import { useCadCoreStore } from "../state";
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
  /** Re-binds the feature to a new profile selection; used to heal a
   *  `dependency_broken` extrude when the user re-picks its faces. */
  updateExtrudeProfiles: (
    featureId: string,
    profileIds: readonly string[],
  ) => Promise<void>;
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
  updateExtrudeProfiles,
  recreateNewProfileExtrudePreview,
  setExtrudeAction,
  setHiddenFeatureIds,
  clearSelection,
  restoreTimelineCursorAfterEdit,
  cancelActiveTool,
}: ActiveExtrudePreviewProps) {
  const { t } = useTranslation();
  const activeFeatureId =
    extrudeAction?.phase === "active" ? extrudeAction.featureId : null;

  // D5 teardown (M13): the extrude panel session is ONE undo step, so
  // undo removes the feature while `extrudeAction` is still "active".
  // Drive the panel's lifecycle from the document — when the feature
  // disappears from the history, close the action instead of letting
  // a stale panel keep issuing updates against a deleted feature.
  useEffect(() => {
    if (!activeFeatureId || !document) {
      return;
    }
    const featureStillExists = document.feature_history.some(
      (feature) => feature.feature_id === activeFeatureId,
    );
    if (!featureStillExists) {
      setExtrudeAction(null);
    }
  }, [activeFeatureId, document, setExtrudeAction]);

  if (extrudeAction?.phase !== "active" || !extrudeAction.featureId) {
    return null;
  }

  const activeExtrudeFeatureId = extrudeAction.featureId;
  const activeExtrudeFeatureIds = activeExtrudeFeatureIdsFor(extrudeAction);
  const activeExtrudeFeature = findFeature(document, activeExtrudeFeatureId);
  const extrudePreviewError = previewErrorFor(activeExtrudeFeature);

  // A feature that lost a source region stays `dependency_broken`, and that
  // error used to disable Confirm permanently — even after the user re-picked
  // the surviving faces, which is the only way to repair it. Treat a non-empty
  // selection as the repair intent: keep the message visible, let Confirm
  // through, and re-bind the feature to the new selection when it is pressed.
  const selectedProfileIds = document?.selected_sketch_profile_ids ?? [];
  const canRepairWithSelection =
    activeExtrudeFeature?.dependency_broken === true &&
    selectedProfileIds.length > 0;

  // An *edit* session's input is the live selection; the action's `profileCount`
  // is the snapshot taken when the panel opened, so it kept reporting the old
  // count while Confirm read the live one (or the reverse) and the two
  // disagreed. Show what Confirm will actually use.
  const isEditSession = extrudeAction.originalSnapshot !== null;
  const displayProfileCount = isEditSession
    ? selectedProfileIds.length
    : extrudeAction.profileCount;

  return (
    <ExtrudePreviewPanel
      initialDepth={extrudeAction.initialDepth}
      initialMode={extrudeAction.initialMode}
      initialParameters={extrudeAction.initialParameters}
      selectedProfileCount={displayProfileCount}
      canCombineWithExistingBody={extrudeAction.canCombineWithExistingBody}
      availableTargetBodies={availableTargetBodiesFor(
        viewport,
        activeExtrudeFeatureIds,
      )}
      selectedFaceTargetId={document?.selected_face_id ?? null}
      initialTargetBodyId={extrudeAction.initialTargetBodyId}
      previewError={extrudePreviewError}
      previewErrorBlocksConfirm={!canRepairWithSelection}
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
        // A confirm that silently does nothing is the worst failure mode here:
        // the feature was rolled back to enter edit mode, so the viewport is
        // empty until the cursor is restored. Report anything thrown instead of
        // letting the panel look inert.
        try {
          // One line in ui-errors.log: what Confirm actually saw. A Confirm that
          // closes the panel without restoring the body is otherwise invisible.
          reportUiError(
            "extrude confirm",
            JSON.stringify({
              featureId: activeExtrudeFeatureId,
              edit: isEditSession,
              broken: activeExtrudeFeature?.dependency_broken ?? null,
              liveProfiles: selectedProfileIds.length,
              actionProfiles: extrudeAction.profileIds.length,
              repairing: canRepairWithSelection,
            }),
          );
          if (
            activeExtrudeFeature?.dependency_broken === true &&
            selectedProfileIds.length === 0
          ) {
            // Nothing to re-bind to. Closing here left the body broken with no
            // explanation — the "Confirm does nothing" report. Ask for the
            // faces instead of silently exiting.
            useCadCoreStore
              .getState()
              .addMessage(t("panels.extrude.selectFacesFirst"));
            return;
          }
          if (canRepairWithSelection) {
            await runAction(async () => {
              await updateExtrudeProfiles(
                activeExtrudeFeatureId,
                selectedProfileIds,
              );
            });
          }
          await confirmActiveExtrude({
            document,
            activeExtrudeFeatureId,
            setHiddenFeatureIds,
            setExtrudeAction,
            runAction,
            clearSelection,
            restoreTimelineCursorAfterEdit,
          });
        } catch (error) {
          reportUiError(
            "extrude confirm failed",
            error instanceof Error ? `${error.message}\n${error.stack ?? ""}` : String(error),
          );
          throw error;
        }
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
  // Guard against stale feature ids: after an undo race or a failed
  // cancel the panel may still hold ids that no longer exist in the
  // document.  Sending an update for one makes the core answer
  // "Feature not found: <id>" and, worse, the panel keeps issuing
  // more updates for the same ghost feature on every interaction.
  const liveIds = new Set(
    (useCadCoreStore.getState().document?.feature_history ?? []).map(
      (entry) => entry.feature_id,
    ),
  );
  for (const featureId of featureIds) {
    if (!liveIds.has(featureId)) {
      continue;
    }
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

