import { findDependents } from "../lib";
import type { DocumentState, FeatureEntry } from "../types";
import type { SketchDeleteSelection } from "./appState";
import {
  currentSketchDeleteSelection,
  extrudesAffectedBySketchSelection,
} from "./sketchSelectionDelete";

type RunAction = (action: () => Promise<void>) => void;

export interface PendingSketchDeleteConfirmation {
  selection: SketchDeleteSelection;
  affectedFeatureNames: string[];
}

interface FeatureDeleteContext {
  document: DocumentState | null;
  featureId: string;
  activeSketchDeleteBlockedMessage: string;
  addMessage: (message: string) => void;
  runAction: RunAction;
  deleteFeature: (featureId: string) => Promise<void>;
  clearEditingFeature: (featureId: string) => void;
}

export function confirmAndDeleteFeatureFromContext({
  document,
  featureId,
  activeSketchDeleteBlockedMessage,
  addMessage,
  runAction,
  deleteFeature,
  clearEditingFeature,
}: FeatureDeleteContext) {
  if (!document) {
    return;
  }

  const feature = document.feature_history.find(
    (entry) => entry.feature_id === featureId,
  );
  if (
    feature?.kind === "sketch" &&
    document.active_sketch_feature_id === featureId
  ) {
    addMessage(activeSketchDeleteBlockedMessage);
    return;
  }

  const dependents = findDependents(document, featureId);
  if (!confirmDependentFeatureDelete(dependents)) {
    return;
  }

  runAction(async () => {
    await deleteFeature(featureId);
    clearEditingFeature(featureId);
  });
}

function confirmDependentFeatureDelete(dependents: FeatureEntry[]) {
  if (dependents.length === 0) {
    return true;
  }

  const names = dependents.map((entry) => entry.name || entry.kind).join(", ");
  return window.confirm(
    `Deleting this feature will break ${dependents.length} downstream feature(s): ${names}. Delete anyway?`,
  );
}

interface SketchSelectionDeleteContext {
  selection: SketchDeleteSelection;
  runAction: RunAction;
  deleteSketchSelection: (
    entityIds: string[],
    vertexIds: string[],
    profileIds: string[],
  ) => Promise<void>;
}

export function deleteSketchSelectionFromContext({
  selection,
  runAction,
  deleteSketchSelection,
}: SketchSelectionDeleteContext) {
  runAction(async () => {
    await deleteSketchSelection(
      selection.entityIds,
      selection.vertexIds,
      selection.profileIds,
    );
  });
}

interface SketchSelectionDeleteConfirmationContext
  extends Omit<SketchSelectionDeleteContext, "selection"> {
  document: DocumentState | null;
  activeSketchFeature: FeatureEntry | null | undefined;
  selection?: SketchDeleteSelection;
  setPendingSketchDeleteConfirmation: (
    confirmation: PendingSketchDeleteConfirmation | null,
  ) => void;
}

export function confirmAndDeleteSketchSelectionFromContext({
  document,
  activeSketchFeature,
  selection,
  setPendingSketchDeleteConfirmation,
  runAction,
  deleteSketchSelection,
}: SketchSelectionDeleteConfirmationContext) {
  if (!document?.active_sketch_feature_id) {
    return;
  }

  const deleteSelection = selection ?? currentSketchDeleteSelection(document);
  if (isEmptySketchDeleteSelection(deleteSelection)) {
    return;
  }

  const dependents = extrudesAffectedBySketchSelection({
    document,
    activeSketchFeature,
    selection: deleteSelection,
  });
  if (dependents.length > 0) {
    setPendingSketchDeleteConfirmation({
      selection: deleteSelection,
      affectedFeatureNames: dependents.map((entry) => entry.name || entry.kind),
    });
    return;
  }

  deleteSketchSelectionFromContext({
    selection: deleteSelection,
    runAction,
    deleteSketchSelection,
  });
}

function isEmptySketchDeleteSelection(selection: SketchDeleteSelection) {
  return (
    selection.entityIds.length === 0 &&
    selection.vertexIds.length === 0 &&
    selection.profileIds.length === 0
  );
}
