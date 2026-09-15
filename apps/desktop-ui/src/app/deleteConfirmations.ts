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
  // Number of sketch entities in the selection — used to confirm
  // LARGE deletes (a lag-invisible marquee once deleted 24 spokes in
  // one hotkey press with no dialog at all).
  entityCount: number;
  // Hotkey deletes carry no trustworthy snapshot (the UI state can lag
  // the last marquee) — on confirm the core resolves the CURRENT
  // selection instead of deleting the snapshot ids.
  deleteCurrentOnConfirm: boolean;
}

// Deleting many entities at once is the dangerous case: with a laggy
// UI the marquee rectangle may never have rendered, so the user never
// saw WHAT was selected. Force a look at the count before it goes.
export const LARGE_SKETCH_DELETE_THRESHOLD = 5;

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
  // null = delete the core's CURRENT selection (hotkey flow): the
  // core resolves it at command time, immune to UI state lagging the
  // last marquee. A snapshot is still passed when it carries user
  // intent (context menu right-click).
  selection: SketchDeleteSelection | null;
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
    if (selection) {
      await deleteSketchSelection(
        selection.entityIds,
        selection.vertexIds,
        selection.profileIds,
      );
      return;
    }
    // Empty ids: the core resolves the live selection at command time.
    await deleteSketchSelection([], [], []);
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

  // No snapshot on the hotkey path — resolve from the freshest UI
  // state for the dependents check only; the deletion itself goes to
  // the core with empty ids so it deletes the live selection.
  const deleteCurrent = !selection;
  const deleteSelection = selection ?? currentSketchDeleteSelection(document);
  if (isEmptySketchDeleteSelection(deleteSelection)) {
    return;
  }

  const dependents = extrudesAffectedBySketchSelection({
    document,
    activeSketchFeature,
    selection: deleteSelection,
  });
  const entityCount =
    deleteSelection.entityIds.length + deleteSelection.profileIds.length;
  if (dependents.length > 0 ||
      entityCount >= LARGE_SKETCH_DELETE_THRESHOLD) {
    setPendingSketchDeleteConfirmation({
      selection: deleteSelection,
      affectedFeatureNames: dependents.map((entry) => entry.name || entry.kind),
      entityCount,
      deleteCurrentOnConfirm: deleteCurrent,
    });
    return;
  }

  deleteSketchSelectionFromContext({
    selection: deleteCurrent ? null : deleteSelection,
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
