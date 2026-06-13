export interface FeatureActionAvailabilityInput {
  activeSketchPlaneId: string | null;
  extrudeAction: { phase: string } | null;
  loftAction: unknown | null;
  revolveAction: unknown | null;
  sweepAction: unknown | null;
  edgeOpAction: unknown | null;
  shellAction: unknown | null;
  holeAction: unknown | null;
  offsetPlaneAction: unknown | null;
  midplaneAction: unknown | null;
  tangentPlaneAction: unknown | null;
  anglePlaneAction: unknown | null;
  constructionAxisAction: unknown | null;
  constructionPointAction: unknown | null;
  helixAction: unknown | null;
  threadAction: unknown | null;
  fastenerAction: unknown | null;
  moveAction: unknown | null;
  pluginAction: unknown | null;
}

export interface FeatureActionAvailability {
  canExtrudeFromSelection: boolean;
  canStartTimelineFeatureEdit: boolean;
  canStartReferencePlaneAction: boolean;
  canStartSolidFeatureAction: boolean;
  canStartConstructionReferenceAction: boolean;
  canStartHelixRibbonAction: boolean;
}

export function computeFeatureActionAvailability({
  activeSketchPlaneId,
  extrudeAction,
  loftAction,
  revolveAction,
  sweepAction,
  edgeOpAction,
  shellAction,
  holeAction,
  offsetPlaneAction,
  midplaneAction,
  tangentPlaneAction,
  anglePlaneAction,
  constructionAxisAction,
  constructionPointAction,
  helixAction,
  threadAction,
  fastenerAction,
  moveAction,
  pluginAction,
}: FeatureActionAvailabilityInput): FeatureActionAvailability {
  const blocksExtrudeFromSelection = hasAnyAction([
    loftAction,
    revolveAction,
    sweepAction,
    shellAction,
    holeAction,
    constructionAxisAction,
    constructionPointAction,
    helixAction,
    threadAction,
    fastenerAction,
    moveAction,
    pluginAction,
  ]);
  const canUseCurrentExtrudeAction =
    !extrudeAction || extrudeAction.phase === "pending";
  const canExtrudeFromSelection =
    !blocksExtrudeFromSelection && canUseCurrentExtrudeAction;
  const noTimelineFeatureActionExceptMove =
    !hasAnyAction([
      extrudeAction,
      loftAction,
      revolveAction,
      sweepAction,
      edgeOpAction,
      threadAction,
      fastenerAction,
      pluginAction,
    ]);
  const canStartTimelineFeatureEdit =
    noTimelineFeatureActionExceptMove && !moveAction;
  const noReferenceFeatureAction =
    !hasAnyAction([
      offsetPlaneAction,
      midplaneAction,
      tangentPlaneAction,
      anglePlaneAction,
      constructionAxisAction,
      constructionPointAction,
      helixAction,
    ]);
  const canStartReferencePlaneAction =
    !activeSketchPlaneId &&
    canStartTimelineFeatureEdit &&
    !shellAction &&
    noReferenceFeatureAction;
  const canStartSolidFeatureAction =
    canStartReferencePlaneAction && !holeAction;
  const canStartConstructionReferenceAction =
    canStartTimelineFeatureEdit &&
    !shellAction &&
    !holeAction &&
    noReferenceFeatureAction;
  const canStartHelixRibbonAction =
    !activeSketchPlaneId &&
    noTimelineFeatureActionExceptMove &&
    !shellAction &&
    !holeAction &&
    noReferenceFeatureAction;

  return {
    canExtrudeFromSelection,
    canStartTimelineFeatureEdit,
    canStartReferencePlaneAction,
    canStartSolidFeatureAction,
    canStartConstructionReferenceAction,
    canStartHelixRibbonAction,
  };
}

function hasAnyAction(actions: readonly unknown[]) {
  return actions.some(Boolean);
}
