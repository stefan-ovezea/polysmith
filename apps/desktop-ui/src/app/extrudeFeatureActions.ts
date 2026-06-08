import { useEffect } from "react";
import type { Dispatch, MutableRefObject, SetStateAction } from "react";

import type {
  ExtrudeAdvancedParameters,
  ExtrudeMode,
  ViewportState,
} from "../types";
import type { DocumentState } from "../types/ipc";
import { awaitDocumentChange, useCadCoreStore } from "../state";
import {
  activeExtrudeActionFromCreatedFeature,
  canCombineExtrudeWithExistingBody,
  type ActiveExtrudeAction,
} from "./appState";
import { isToolStartBlocked, type AppToolState } from "./actionAvailability";
import { awaitCreatedFeatureOfKind } from "./featureCreation";
import {
  createdProfileExtrudeAction,
  pendingExtrudeActionFromSelection,
} from "./extrudeCreationState";
import { resolveDefaultExtrudeSettings } from "./extrudeDefaults";
import { syncExtrudeProfileSelection } from "./extrudeProfileSync";

type RunAction = (action: () => Promise<void>) => Promise<void>;

interface ExtrudeFeatureActionsContext {
  document: DocumentState | null;
  viewport: ViewportState | null;
  activeToolState: AppToolState;
  extrudeAction: ActiveExtrudeAction | null;
  selectedSketchProfileIds: readonly string[];
  selectedSketchProfileIdsKey: string;
  selectedExtrudableFaceId: string | null;
  selectedSketchEntityIds: readonly string[];
  extrudeCreateInFlightRef: MutableRefObject<boolean>;
  lastExtrudeProfileUpdateRef: MutableRefObject<string>;
  setExtrudeAction: Dispatch<SetStateAction<ActiveExtrudeAction | null>>;
  extrudeProfile: (
    profileIds: readonly string[],
    depth: number,
    mode: ExtrudeMode | null,
    targetBodyId: string | null,
    parameters?: ExtrudeAdvancedParameters | null,
  ) => Promise<void>;
  extrudeFace: (
    faceId: string,
    depth: number,
    mode: ExtrudeMode | null,
    targetBodyId: string | null,
    parameters?: ExtrudeAdvancedParameters | null,
  ) => Promise<void>;
  extrudeOpenEntities: (
    entityIds: readonly string[],
    depth: number,
    mode: ExtrudeMode | null,
    targetBodyId: string | null,
    parameters: ExtrudeAdvancedParameters,
  ) => Promise<void>;
  updateExtrudeProfiles: (
    featureId: string,
    profileIds: readonly string[],
  ) => Promise<void>;
  undo: () => Promise<void>;
  runAction: RunAction;
  addMessage: (message: string) => void;
}

export function useExtrudeFeatureActions({
  document,
  viewport,
  activeToolState,
  extrudeAction,
  selectedSketchProfileIds,
  selectedSketchProfileIdsKey,
  selectedExtrudableFaceId,
  selectedSketchEntityIds,
  extrudeCreateInFlightRef,
  lastExtrudeProfileUpdateRef,
  setExtrudeAction,
  extrudeProfile,
  extrudeFace,
  extrudeOpenEntities,
  updateExtrudeProfiles,
  undo,
  runAction,
  addMessage,
}: ExtrudeFeatureActionsContext) {
  function getDefaultExtrudeSettings(
    profileIds: readonly string[],
    faceIdOverride: string | null = null,
  ) {
    return resolveDefaultExtrudeSettings({
      document,
      viewport,
      profileIds,
      faceIdOverride,
    });
  }

  async function triggerExtrudeAction() {
    if (extrudeAction?.phase === "active") {
      return;
    }
    if (
      isToolStartBlocked(activeToolState, {
        activeSketchPlane: false,
        extrude: false,
      })
    ) {
      return;
    }

    setExtrudeAction(
      pendingExtrudeActionFromSelection({
        document,
        selectedSketchProfileIds,
        selectedExtrudableFaceId,
        selectedSketchEntityIds,
        defaultSettings: getDefaultExtrudeSettings(selectedSketchProfileIds),
      }),
    );
  }

  async function createExtrudeFromSelectedProfiles(
    depth: number,
    mode: ExtrudeMode | null,
    targetBodyId: string | null,
    parameters: ExtrudeAdvancedParameters | null = null,
  ) {
    await createExtrudeFromProfiles(
      [...selectedSketchProfileIds],
      depth,
      mode,
      targetBodyId,
      parameters,
    );
  }

  async function createExtrudeFromProfiles(
    profileIds: string[],
    depth: number,
    mode: ExtrudeMode | null,
    targetBodyId: string | null,
    parameters: ExtrudeAdvancedParameters | null = null,
    canCombineWithExistingBodyOverride: boolean | null = null,
  ) {
    if (profileIds.length === 0 || extrudeCreateInFlightRef.current) {
      return;
    }
    extrudeCreateInFlightRef.current = true;

    const documentPromise = awaitCreatedFeatureOfKind("extrude");

    await runAction(async () => {
      try {
        await extrudeProfile(profileIds, depth, mode, targetBodyId, parameters);
        const {
          feature: createdFeature,
          featureId: newFeatureId,
          createdFeatures,
        } = await documentPromise;
        lastExtrudeProfileUpdateRef.current = profileIds.join("|");
        setExtrudeAction(
          createdProfileExtrudeAction({
            createdFeature,
            featureId: newFeatureId,
            createdFeatures,
            profileIds,
            depth,
            mode,
            targetBodyId,
            canCombineWithExistingBody:
              canCombineWithExistingBodyOverride ??
              canCombineExtrudeWithExistingBody(document),
          }),
        );
      } catch (error) {
        addMessage(`extrude action error: ${String(error)}`);
      } finally {
        extrudeCreateInFlightRef.current = false;
      }
    });
  }

  function extrudeFeatureIsPresent(featureIds: Set<string>) {
    const currentDocument = useCadCoreStore.getState().document;
    return (
      currentDocument?.feature_history.some((feature) =>
        featureIds.has(feature.feature_id),
      ) ?? false
    );
  }

  async function undoUntilExtrudePreviewRemoved(featureIds: readonly string[]) {
    const pendingFeatureIds = new Set(featureIds);
    if (pendingFeatureIds.size === 0) {
      return;
    }

    for (let attempt = 0; attempt < 20; attempt += 1) {
      if (!extrudeFeatureIsPresent(pendingFeatureIds)) {
        return;
      }
      const documentPromise = awaitDocumentChange(() => true);
      await runAction(async () => {
        await undo();
      });
      await documentPromise;
    }
  }

  async function recreateNewProfileExtrudePreview(
    action: ActiveExtrudeAction,
    depth: number,
    mode: ExtrudeMode,
    targetBodyId: string | null,
    parameters: ExtrudeAdvancedParameters | null,
  ) {
    if (
      action.originalSnapshot ||
      action.profileIds.length === 0 ||
      action.featureIds.length === 0
    ) {
      return false;
    }

    await undoUntilExtrudePreviewRemoved(action.featureIds);
    await createExtrudeFromProfiles(
      [...action.profileIds],
      depth,
      mode,
      targetBodyId,
      parameters,
      action.canCombineWithExistingBody,
    );
    return true;
  }

  async function createExtrudeFromSelectedFace(
    faceId: string,
    depth: number,
    mode: ExtrudeMode | null,
    targetBodyId: string | null,
    parameters: ExtrudeAdvancedParameters | null = null,
  ) {
    if (extrudeCreateInFlightRef.current) {
      return;
    }
    extrudeCreateInFlightRef.current = true;

    const documentPromise = awaitCreatedFeatureOfKind("extrude");

    await runAction(async () => {
      try {
        await extrudeFace(faceId, depth, mode, targetBodyId, parameters);
        const { feature: createdFeature, featureId: newFeatureId } =
          await documentPromise;
        setExtrudeAction(
          activeExtrudeActionFromCreatedFeature({
            createdFeature,
            featureId: newFeatureId,
            depth,
            mode,
            targetBodyId,
            profileCount: 1,
            canCombineWithExistingBody:
              canCombineExtrudeWithExistingBody(document),
          }),
        );
      } catch (error) {
        addMessage(`extrude face action error: ${String(error)}`);
      } finally {
        extrudeCreateInFlightRef.current = false;
      }
    });
  }

  async function createThinExtrudeFromSelectedEntities(
    depth: number,
    mode: ExtrudeMode | null,
    targetBodyId: string | null,
    parameters: ExtrudeAdvancedParameters,
  ) {
    const entityIds = [...selectedSketchEntityIds];
    if (entityIds.length === 0 || extrudeCreateInFlightRef.current) {
      return;
    }
    extrudeCreateInFlightRef.current = true;

    const documentPromise = awaitCreatedFeatureOfKind("extrude");

    await runAction(async () => {
      try {
        await extrudeOpenEntities(entityIds, depth, mode, targetBodyId, parameters);
        const { feature: createdFeature, featureId: newFeatureId } =
          await documentPromise;
        setExtrudeAction(
          activeExtrudeActionFromCreatedFeature({
            createdFeature,
            featureId: newFeatureId,
            depth,
            mode,
            targetBodyId,
            profileCount: entityIds.length,
            canCombineWithExistingBody:
              canCombineExtrudeWithExistingBody(document),
          }),
        );
      } catch (error) {
        addMessage(`thin extrude action error: ${String(error)}`);
      } finally {
        extrudeCreateInFlightRef.current = false;
      }
    });
  }

  useEffect(() => {
    syncExtrudeProfileSelection({
      extrudeAction,
      selectedSketchProfileIds,
      selectedSketchProfileIdsKey,
      selectedExtrudableFaceId,
      lastExtrudeProfileUpdateRef,
      getDefaultExtrudeSettings,
      createExtrudeFromSelectedFace,
      createExtrudeFromSelectedProfiles,
      updateExtrudeProfiles,
      setExtrudeAction,
      runAction,
    });
  }, [extrudeAction, selectedSketchProfileIdsKey, selectedExtrudableFaceId]);

  return {
    createExtrudeFromSelectedFace,
    createExtrudeFromSelectedProfiles,
    createThinExtrudeFromSelectedEntities,
    getDefaultExtrudeSettings,
    recreateNewProfileExtrudePreview,
    triggerExtrudeAction,
    undoUntilExtrudePreviewRemoved,
  };
}
