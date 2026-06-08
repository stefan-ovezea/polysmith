import { holeStandardsForMode } from "../lib";
import type { DocumentState, ViewportState } from "../types";
import type {
  ActiveMoveAction,
  FastenerAction,
  HelixAction,
  HoleAction,
  ThreadAction,
} from "./appState";
import { selectedDocumentBodyId } from "./selectedDocumentBody";

type FeatureHistoryEntry = DocumentState["feature_history"][number];

export interface ActiveFeatureParametersInput {
  document: DocumentState | null;
  viewport: ViewportState | null;
  selectedMaterialBodyId: string | null;
  holeAction: HoleAction | null;
  helixAction: HelixAction | null;
  threadAction: ThreadAction | null;
  fastenerAction: FastenerAction | null;
  moveAction: ActiveMoveAction | null;
}

export function computeActiveFeatureParameters({
  document,
  viewport,
  selectedMaterialBodyId,
  holeAction,
  helixAction,
  threadAction,
  fastenerAction,
  moveAction,
}: ActiveFeatureParametersInput) {
  const activeHoleFeature = findActiveActionFeature(document, holeAction);
  const activeHoleParameters = activeHoleFeature?.hole_parameters ?? null;
  const activeHelixFeature = findActiveActionFeature(document, helixAction);
  const activeThreadFeature = findActiveActionFeature(document, threadAction);
  const activeThreadParameters = activeThreadFeature?.thread_parameters ?? null;
  const activeFastenerFeature = findFastenerActionFeature(
    document,
    fastenerAction,
  );
  const activeFastenerParameters =
    activeFastenerFeature?.fastener_parameters ?? null;
  const activeMoveFeature = findActiveActionFeature(document, moveAction);

  return {
    activeHoleFeature,
    activeHoleParameters,
    activeHoleStandards: standardsForParameters(activeHoleParameters),
    activeHelixFeature,
    activeHelixParameters: activeHelixFeature?.helix_parameters ?? null,
    activeThreadFeature,
    activeThreadParameters,
    activeThreadStandards: standardsForParameters(activeThreadParameters),
    activeFastenerFeature,
    activeFastenerParameters,
    activeFastenerStandards: standardsForParameters(activeFastenerParameters),
    activeMoveFeature,
    activeMoveParameters: activeMoveParameters(activeMoveFeature, moveAction),
    selectedMoveBodyId:
      selectedMaterialBodyId ?? selectedDocumentBodyId(document, viewport),
  };
}

function findActiveFeature(
  document: DocumentState | null,
  featureId: string | null,
): FeatureHistoryEntry | null {
  if (!featureId) {
    return null;
  }
  return (
    document?.feature_history.find((feature) => feature.feature_id === featureId) ??
    null
  );
}

function findActiveActionFeature(
  document: DocumentState | null,
  action:
    | HoleAction
    | HelixAction
    | ThreadAction
    | ActiveMoveAction
    | null,
) {
  if (action?.phase !== "active") {
    return null;
  }
  return findActiveFeature(document, action.featureId);
}

function findFastenerActionFeature(
  document: DocumentState | null,
  action: FastenerAction | null,
) {
  return findActiveFeature(document, action?.featureId ?? null);
}

function standardsForParameters(
  parameters:
    | NonNullable<FeatureHistoryEntry["hole_parameters"]>
    | NonNullable<FeatureHistoryEntry["thread_parameters"]>
    | NonNullable<FeatureHistoryEntry["fastener_parameters"]>
    | null,
) {
  return parameters ? holeStandardsForMode(parameters.standard) : [];
}

function activeMoveParameters(
  activeMoveFeature: FeatureHistoryEntry | null,
  moveAction: ActiveMoveAction | null,
) {
  if (activeMoveFeature?.move_parameters) {
    return activeMoveFeature.move_parameters;
  }
  return moveAction?.phase === "active" ? moveAction.parameters : null;
}
