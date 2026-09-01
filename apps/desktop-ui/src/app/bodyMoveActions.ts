import type { Dispatch, SetStateAction } from "react";

import type { MoveFeatureParameters } from "../types";
import type { DocumentState } from "../types/ipc";
import { pickExportPath, pickExportStlPath } from "./documentDialogs";
import {
  defaultMoveParameters,
  type ActiveMoveAction,
} from "./appState";
import {
  type AppToolState,
  isToolStartBlocked,
} from "./actionAvailability";
import {
  awaitCreatedFeature,
  awaitCreatedFeatureOfKind,
} from "./featureCreation";

type FeatureHistoryEntry = DocumentState["feature_history"][number];

interface BodyMoveActionsContext {
  activeToolState: AppToolState;
  document: DocumentState | null;
  moveAction: ActiveMoveAction | null;
  selectedMoveBodyId: string | null;
  setMoveAction: Dispatch<SetStateAction<ActiveMoveAction | null>>;
  setHiddenFeatureIds: Dispatch<SetStateAction<Set<string>>>;
  createMove: (
    targetBodyId: string,
    parameters: MoveFeatureParameters,
  ) => Promise<void>;
  createBodyCopy: (
    sourceBodyId: string,
    copyMode: "linked" | "standalone",
  ) => Promise<void>;
  unlinkBodyCopy: (featureId: string) => Promise<void>;
  exportBodyStl: (filePath: string, bodyId: string) => Promise<void>;
  exportBodyStep: (filePath: string, bodyId: string) => Promise<void>;
  convertMeshToBody: (bodyId: string) => Promise<void>;
  detachBodyProjections: (bodyId: string) => Promise<void>;
  updateMoveParameters: (
    featureId: string,
    parameters: MoveFeatureParameters,
  ) => Promise<void>;
  runAction: (action: () => Promise<void>) => Promise<void>;
  addMessage: (message: string) => void;
  translate: (key: string, options?: Record<string, unknown>) => string;
}

export function createBodyMoveActions({
  activeToolState,
  document,
  moveAction,
  selectedMoveBodyId,
  setMoveAction,
  setHiddenFeatureIds,
  createMove,
  createBodyCopy,
  unlinkBodyCopy,
  exportBodyStl,
  exportBodyStep,
  convertMeshToBody,
  detachBodyProjections,
  updateMoveParameters,
  runAction,
  addMessage,
  translate,
}: BodyMoveActionsContext) {
  async function createMoveFeature(
    targetBodyId: string,
    parameters: MoveFeatureParameters = defaultMoveParameters(targetBodyId),
    options: { createdCopyFeatureId?: string | null } = {},
  ) {
    const documentPromise = awaitCreatedFeatureOfKind("move");

    await runAction(async () => {
      try {
        const seeded = {
          ...parameters,
          target_body_id: targetBodyId,
          is_pending: true,
        };
        await createMove(targetBodyId, seeded);
        const { feature: created, featureId: newFeatureId } =
          await documentPromise;
        if (created.move_parameters) {
          setMoveAction({
            phase: "active",
            featureId: newFeatureId,
            targetBodyId,
            parameters: created.move_parameters,
            originalSnapshot: null,
            createdCopyFeatureId: options.createdCopyFeatureId ?? null,
          });
        }
      } catch (error) {
        addMessage(`move error: ${String(error)}`);
        setMoveAction(null);
      }
    });
  }

  function isBodyPlacementActionBlocked() {
    return isToolStartBlocked(activeToolState);
  }

  async function moveBodyFromContext(bodyId: string) {
    if (isBodyPlacementActionBlocked()) {
      return;
    }
    await createMoveFeature(bodyId, defaultMoveParameters(bodyId));
  }

  async function exportBodyAsMesh(bodyId: string) {
    const bodyName =
      document?.feature_history.find((feature) => feature.feature_id === bodyId)
        ?.name ?? document?.name;
    const filePath = await pickExportStlPath({
      translate,
      documentName: bodyName,
      addMessage,
    });
    if (!filePath) {
      return;
    }

    await runAction(async () => {
      await exportBodyStl(filePath, bodyId);
      addMessage(`mesh export requested: ${filePath}`);
    });
  }

  async function exportBodyAsStep(bodyId: string) {
    const bodyName =
      document?.feature_history.find((feature) => feature.feature_id === bodyId)
        ?.name ?? document?.name;
    const filePath = await pickExportPath({
      translate,
      documentName: bodyName,
      addMessage,
    });
    if (!filePath) {
      return;
    }

    await runAction(async () => {
      await exportBodyStep(filePath, bodyId);
      addMessage(`step export requested: ${filePath}`);
    });
  }

  async function copyBodyAndMove(
    sourceBodyId: string,
    copyMode: "linked" | "standalone",
  ) {
    if (isBodyPlacementActionBlocked()) {
      return;
    }

    const documentPromise = awaitCreatedFeature(
      (feature) =>
        feature.kind === "body_copy" &&
        feature.body_copy_parameters?.source_body_id === sourceBodyId &&
        feature.body_copy_parameters.copy_mode === copyMode,
    );

    try {
      await runAction(async () => {
        await createBodyCopy(sourceBodyId, copyMode);
      });
      const { featureId: copyBodyId } = await documentPromise;
      await createMoveFeature(copyBodyId, defaultMoveParameters(copyBodyId), {
        createdCopyFeatureId: copyBodyId,
      });
    } catch (error) {
      addMessage(`copy body error: ${String(error)}`);
    }
  }

  function confirmAndUnlinkBodyCopy(featureId: string) {
    const confirmed = window.confirm(translate("dialogs.unlinkLinkedCopyMessage"));
    if (!confirmed) {
      return;
    }
    void runAction(async () => {
      await unlinkBodyCopy(featureId);
    });
  }

  async function convertMeshBodyFromContext(bodyId: string) {
    await runAction(async () => {
      try {
        await convertMeshToBody(bodyId);
        addMessage("mesh converted to body");
      } catch (error) {
        addMessage(`convert mesh error: ${String(error)}`);
      }
    });
  }

  async function detachProjectionsFromContext(bodyId: string) {
    await runAction(async () => {
      try {
        await detachBodyProjections(bodyId);
        addMessage("projection links detached");
      } catch (error) {
        addMessage(`detach projections error: ${String(error)}`);
      }
    });
  }

  const bodyContextActions = {
    onMoveBody: moveBodyFromContext,
    onCopyBody: copyBodyAndMove,
    onExportBodyMesh: exportBodyAsMesh,
    onExportBodyStep: exportBodyAsStep,
    onConvertMeshToBody: convertMeshBodyFromContext,
    onDetachBodyProjections: detachProjectionsFromContext,
    onUnlinkBodyCopy: confirmAndUnlinkBodyCopy,
  };

  async function updateActiveMovePreviewParameters(
    parameters: MoveFeatureParameters,
  ) {
    if (moveAction?.phase !== "active") {
      return false;
    }
    await runAction(async () => {
      await updateMoveParameters(moveAction.featureId, parameters);
    });
    setMoveAction((current) =>
      current?.phase === "active" &&
      current.featureId === moveAction.featureId
        ? { ...current, parameters }
        : current,
    );
    return true;
  }

  function hideFeatureSourceSketches(
    featureId: string,
    readSourceSketchIds: (
      feature: FeatureHistoryEntry,
    ) => Array<string | null | undefined>,
  ) {
    const confirmedFeature =
      document?.feature_history.find((entry) => entry.feature_id === featureId) ??
      null;
    if (!confirmedFeature) {
      return;
    }

    const sourceSketchIds = readSourceSketchIds(confirmedFeature).filter(
      (id): id is string => Boolean(id),
    );
    if (sourceSketchIds.length === 0) {
      return;
    }

    setHiddenFeatureIds((current) => {
      const next = new Set(current);
      for (const sourceSketchId of sourceSketchIds) {
        next.add(sourceSketchId);
      }
      return next;
    });
  }

  async function triggerMoveAction() {
    if (isToolStartBlocked(activeToolState)) {
      return;
    }

    if (selectedMoveBodyId) {
      await createMoveFeature(
        selectedMoveBodyId,
        defaultMoveParameters(selectedMoveBodyId),
      );
      return;
    }

    setMoveAction({
      phase: "pending",
      parameters: defaultMoveParameters(),
    });
  }

  return {
    bodyContextActions,
    createMoveFeature,
    hideFeatureSourceSketches,
    triggerMoveAction,
    updateActiveMovePreviewParameters,
  };
}
