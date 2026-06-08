import type { ExtrudeAdvancedParameters, ExtrudeMode } from "../types";
import type { DocumentState } from "../types/ipc";
import {
  canCombineExtrudeWithExistingBody,
  DEFAULT_EXTRUDE_DEPTH,
  type ActiveExtrudeAction,
} from "./appState";
import type { DefaultExtrudeSettings } from "./extrudeDefaults";

type FeatureHistoryEntry = DocumentState["feature_history"][number];

export function pendingExtrudeActionFromSelection({
  document,
  selectedSketchProfileIds,
  selectedExtrudableFaceId,
  selectedSketchEntityIds,
  defaultSettings,
}: {
  document: DocumentState | null;
  selectedSketchProfileIds: readonly string[];
  selectedExtrudableFaceId: string | null;
  selectedSketchEntityIds: readonly string[];
  defaultSettings: DefaultExtrudeSettings;
}): ActiveExtrudeAction {
  return {
    phase: "pending",
    featureId: null,
    featureIds: [],
    profileIds: [...selectedSketchProfileIds],
    automaticMode: true,
    initialDepth: DEFAULT_EXTRUDE_DEPTH,
    initialMode: defaultSettings.mode,
    initialParameters: null,
    initialTargetBodyId: defaultSettings.targetBodyId,
    profileCount: pendingExtrudeProfileCount({
      selectedSketchProfileIds,
      selectedExtrudableFaceId,
      selectedSketchEntityIds,
    }),
    originalSnapshot: null,
    canCombineWithExistingBody: canCombineExtrudeWithExistingBody(document),
  };
}

function pendingExtrudeProfileCount({
  selectedSketchProfileIds,
  selectedExtrudableFaceId,
  selectedSketchEntityIds,
}: {
  selectedSketchProfileIds: readonly string[];
  selectedExtrudableFaceId: string | null;
  selectedSketchEntityIds: readonly string[];
}) {
  return (
    selectedSketchProfileIds.length ||
    (selectedExtrudableFaceId ? 1 : 0) ||
    selectedSketchEntityIds.length
  );
}

export function createdProfileExtrudeAction({
  createdFeature,
  createdFeatures,
  featureId,
  profileIds,
  depth,
  mode,
  targetBodyId,
  canCombineWithExistingBody,
}: {
  createdFeature: FeatureHistoryEntry | null | undefined;
  createdFeatures: readonly FeatureHistoryEntry[];
  featureId: string;
  profileIds: string[];
  depth: number;
  mode: ExtrudeMode | null;
  targetBodyId: string | null;
  canCombineWithExistingBody: boolean;
}): ActiveExtrudeAction {
  const createdParams = createdFeature?.extrude_parameters;
  return {
    phase: "active",
    featureId,
    featureIds: createdFeatures.map((feature) => feature.feature_id),
    profileIds,
    automaticMode: false,
    initialDepth: depth,
    initialMode: initialModeForCreatedProfileExtrude(createdParams, mode),
    initialParameters: createdParams ?? null,
    initialTargetBodyId: initialTargetBodyForCreatedProfileExtrude(
      createdParams,
      targetBodyId,
    ),
    profileCount: profileIds.length,
    originalSnapshot: null,
    canCombineWithExistingBody,
  };
}

function initialModeForCreatedProfileExtrude(
  createdParams: FeatureHistoryEntry["extrude_parameters"] | null | undefined,
  requestedMode: ExtrudeMode | null,
) {
  return createdParams?.mode ?? requestedMode ?? "new_body";
}

function initialTargetBodyForCreatedProfileExtrude(
  createdParams: FeatureHistoryEntry["extrude_parameters"] | null | undefined,
  requestedTargetBodyId: string | null,
) {
  return createdParams?.target_body_id ?? requestedTargetBodyId ?? null;
}

export function activeExtrudeAdvancedParameters(
  params: FeatureHistoryEntry["extrude_parameters"] | null | undefined,
  operation: ExtrudeMode,
): ExtrudeAdvancedParameters | null {
  if (!params) {
    return null;
  }
  return {
    extent_mode: params.extent_mode,
    side1: params.side1,
    side2: params.side2,
    thin: params.thin,
    operation,
    intersect_result: params.intersect_result,
  };
}
