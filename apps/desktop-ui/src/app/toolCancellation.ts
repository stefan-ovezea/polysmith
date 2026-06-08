import type { MutableRefObject } from "react";
import type {
  FastenerFeatureParameters,
  MoveFeatureParameters,
  ThreadFeatureParameters,
} from "../types";
import type { ActiveToolActions } from "./activeToolActions";
import type {
  ActiveEdgeOpAction,
  ActiveExtrudeAction,
  ActiveLoftAction,
  ActiveMoveAction,
  ActiveRevolveAction,
  ActiveSweepAction,
  AnglePlaneAction,
  FastenerAction,
  HelixAction,
  HoleAction,
  MidplaneAction,
  OffsetPlaneAction,
  PendingReferenceAction,
  ShellAction,
  ThreadAction,
} from "./appState";
import type { ExtrudeUpdateCallbacks } from "./extrudeUpdateCallbacks";

type AsyncVoid = () => Promise<void>;
type RunAction = (action: AsyncVoid) => Promise<void>;
type Setter<T> = (value: T | null) => void;

export interface CancelActiveToolContext extends ExtrudeUpdateCallbacks {
  actions: ActiveToolActions;
  setters: {
    setExtrudeAction: Setter<ActiveExtrudeAction>;
    setLoftAction: Setter<ActiveLoftAction>;
    setRevolveAction: Setter<ActiveRevolveAction>;
    setSweepAction: Setter<ActiveSweepAction>;
    setMoveAction: Setter<ActiveMoveAction>;
    setEdgeOpAction: Setter<ActiveEdgeOpAction>;
    setShellAction: Setter<ShellAction>;
    setHoleAction: Setter<HoleAction>;
    setOffsetPlaneAction: Setter<OffsetPlaneAction>;
    setAnglePlaneAction: Setter<AnglePlaneAction>;
    setMidplaneAction: Setter<MidplaneAction>;
    setTangentPlaneAction: Setter<PendingReferenceAction>;
    setConstructionAxisAction: Setter<PendingReferenceAction>;
    setConstructionPointAction: Setter<PendingReferenceAction>;
    setThreadAction: Setter<ThreadAction>;
    setFastenerAction: Setter<FastenerAction>;
    setHelixAction: Setter<HelixAction>;
    setEditingFeatureId: Setter<string>;
    setMaterialsPanelOpen: (open: boolean) => void;
  };
  activeEdgeIdsRef: MutableRefObject<string[]>;
  runAction: RunAction;
  restoreTimelineCursorAfterEdit: AsyncVoid;
  undo: AsyncVoid;
  undoUntilExtrudePreviewRemoved: (
    featureIds: readonly string[],
  ) => Promise<void>;
  updateLoftProfiles: (
    featureId: string,
    profileIds: readonly string[],
  ) => Promise<void>;
  updateLoftRuled: (featureId: string, ruled: boolean) => Promise<void>;
  updateRevolveProfile: (
    featureId: string,
    profileId: string,
  ) => Promise<void>;
  updateRevolveAxis: (featureId: string, axisId: string) => Promise<void>;
  updateRevolveAngle: (
    featureId: string,
    angleDegrees: number,
  ) => Promise<void>;
  updateSweepProfile: (featureId: string, profileId: string) => Promise<void>;
  updateSweepPath: (featureId: string, pathEntityId: string) => Promise<void>;
  updateMoveParameters: (
    featureId: string,
    parameters: MoveFeatureParameters,
  ) => Promise<void>;
  updateThreadParameters: (
    featureId: string,
    parameters: ThreadFeatureParameters,
  ) => Promise<void>;
  updateFastenerParameters: (
    featureId: string,
    parameters: FastenerFeatureParameters,
  ) => Promise<void>;
}

export async function cancelActiveToolFromContext({
  ...context
}: CancelActiveToolContext) {
  for (const handler of cancellationHandlers) {
    if (await handler(context)) {
      return true;
    }
  }
  return false;
}

type CancellationHandler = (
  context: CancelActiveToolContext,
) => Promise<boolean>;

const cancellationHandlers: readonly CancellationHandler[] = [
  cancelExtrudeTool,
  cancelLoftTool,
  cancelRevolveTool,
  cancelSweepTool,
  cancelMoveTool,
  cancelEdgeOpTool,
  cancelShellTool,
  cancelHoleTool,
  cancelOffsetPlaneTool,
  cancelAnglePlaneTool,
  cancelMidplaneTool,
  cancelTangentPlaneTool,
  cancelConstructionAxisTool,
  cancelConstructionPointTool,
  cancelThreadTool,
  cancelFastenerTool,
  cancelHelixTool,
  cancelTimelineEdit,
  closeMaterialsPanel,
];

async function cancelExtrudeTool(context: CancelActiveToolContext) {
  const { extrudeAction } = context.actions;
  if (!extrudeAction) {
    return false;
  }
  if (extrudeAction.phase === "active" && extrudeAction.featureId) {
    await restoreOrUndoExtrude(context, extrudeAction);
  }
  context.setters.setExtrudeAction(null);
  await context.restoreTimelineCursorAfterEdit();
  return true;
}

async function restoreOrUndoExtrude(
  context: CancelActiveToolContext,
  extrudeAction: ActiveExtrudeAction,
) {
  const snapshot = extrudeAction.originalSnapshot;
  if (!snapshot) {
    await context.undoUntilExtrudePreviewRemoved(
      extrudeAction.featureIds.length > 0
        ? extrudeAction.featureIds
        : [extrudeAction.featureId],
    );
    return;
  }
  await context.runAction(async () => {
    await context.updateExtrudeDepth(extrudeAction.featureId!, snapshot.depth);
    await context.updateExtrudeMode(extrudeAction.featureId!, snapshot.mode);
    await context.updateExtrudeTargetBody(
      extrudeAction.featureId!,
      snapshot.targetBodyId,
    );
    await context.updateExtrudeParameters(
      extrudeAction.featureId!,
      snapshot.parameters,
    );
  });
}

async function cancelLoftTool(context: CancelActiveToolContext) {
  const { loftAction } = context.actions;
  if (!loftAction) {
    return false;
  }
  if (loftAction.originalSnapshot && loftAction.featureId) {
    const snapshot = loftAction.originalSnapshot;
    await context.runAction(async () => {
      await context.updateLoftProfiles(loftAction.featureId!, snapshot.profileIds);
      await context.updateLoftRuled(loftAction.featureId!, snapshot.ruled);
    });
  } else if (loftAction.phase === "active") {
    await undoLatest(context);
  }
  context.setters.setLoftAction(null);
  await context.restoreTimelineCursorAfterEdit();
  return true;
}

async function cancelRevolveTool(context: CancelActiveToolContext) {
  const { revolveAction } = context.actions;
  if (!revolveAction) {
    return false;
  }
  if (revolveAction.originalSnapshot && revolveAction.featureId) {
    const snapshot = revolveAction.originalSnapshot;
    await context.runAction(async () => {
      await context.updateRevolveProfile(revolveAction.featureId!, snapshot.profileId);
      await context.updateRevolveAxis(revolveAction.featureId!, snapshot.axisEntityId);
      await context.updateRevolveAngle(
        revolveAction.featureId!,
        snapshot.angleDegrees,
      );
    });
  } else if (revolveAction.phase === "active") {
    await undoLatest(context);
  }
  context.setters.setRevolveAction(null);
  await context.restoreTimelineCursorAfterEdit();
  return true;
}

async function cancelSweepTool(context: CancelActiveToolContext) {
  const { sweepAction } = context.actions;
  if (!sweepAction) {
    return false;
  }
  if (sweepAction.originalSnapshot && sweepAction.featureId) {
    const snapshot = sweepAction.originalSnapshot;
    await context.runAction(async () => {
      await context.updateSweepProfile(sweepAction.featureId!, snapshot.profileId);
      await context.updateSweepPath(sweepAction.featureId!, snapshot.pathEntityId);
    });
  } else if (sweepAction.phase === "active") {
    await undoLatest(context);
  }
  context.setters.setSweepAction(null);
  await context.restoreTimelineCursorAfterEdit();
  return true;
}

async function cancelMoveTool(context: CancelActiveToolContext) {
  const { moveAction } = context.actions;
  if (!moveAction) {
    return false;
  }
  if (moveAction.phase === "active") {
    await restoreOrUndoMove(context, moveAction);
  }
  context.setters.setMoveAction(null);
  await context.restoreTimelineCursorAfterEdit();
  return true;
}

async function restoreOrUndoMove(
  context: CancelActiveToolContext,
  moveAction: ActiveMoveAction & { phase: "active" },
) {
  if (moveAction.originalSnapshot) {
    await context.runAction(async () => {
      await context.updateMoveParameters(
        moveAction.featureId,
        moveAction.originalSnapshot!,
      );
    });
    return;
  }
  await context.runAction(async () => {
    await context.undo();
    if (moveAction.createdCopyFeatureId) {
      await context.undo();
    }
  });
}

async function cancelEdgeOpTool(context: CancelActiveToolContext) {
  const { edgeOpAction } = context.actions;
  if (!edgeOpAction) {
    return false;
  }
  if (edgeOpAction.phase === "active") {
    await undoLatest(context);
  }
  context.activeEdgeIdsRef.current = [];
  context.setters.setEdgeOpAction(null);
  return true;
}

async function cancelShellTool(context: CancelActiveToolContext) {
  return cancelUndoableActiveAction({
    action: context.actions.shellAction,
    clear: context.setters.setShellAction,
    context,
  });
}

async function cancelHoleTool(context: CancelActiveToolContext) {
  return cancelUndoableActiveAction({
    action: context.actions.holeAction,
    clear: context.setters.setHoleAction,
    context,
  });
}

async function cancelOffsetPlaneTool(context: CancelActiveToolContext) {
  return cancelUndoableActiveAction({
    action: context.actions.offsetPlaneAction,
    clear: context.setters.setOffsetPlaneAction,
    context,
  });
}

async function cancelAnglePlaneTool(context: CancelActiveToolContext) {
  return cancelUndoableActiveAction({
    action: context.actions.anglePlaneAction,
    clear: context.setters.setAnglePlaneAction,
    context,
  });
}

async function cancelMidplaneTool(context: CancelActiveToolContext) {
  return clearPendingAction(
    context.actions.midplaneAction,
    context.setters.setMidplaneAction,
  );
}

async function cancelTangentPlaneTool(context: CancelActiveToolContext) {
  return clearPendingAction(
    context.actions.tangentPlaneAction,
    context.setters.setTangentPlaneAction,
  );
}

async function cancelConstructionAxisTool(context: CancelActiveToolContext) {
  return clearPendingAction(
    context.actions.constructionAxisAction,
    context.setters.setConstructionAxisAction,
  );
}

async function cancelConstructionPointTool(context: CancelActiveToolContext) {
  return clearPendingAction(
    context.actions.constructionPointAction,
    context.setters.setConstructionPointAction,
  );
}

async function cancelThreadTool(context: CancelActiveToolContext) {
  const { threadAction } = context.actions;
  if (!threadAction) {
    return false;
  }
  if (threadAction.phase === "active") {
    await context.runAction(async () => {
      if (threadAction.originalParameters) {
        await context.updateThreadParameters(
          threadAction.featureId,
          threadAction.originalParameters,
        );
      } else {
        await context.undo();
      }
    });
    await context.restoreTimelineCursorAfterEdit();
  }
  context.setters.setThreadAction(null);
  return true;
}

async function cancelFastenerTool(context: CancelActiveToolContext) {
  const { fastenerAction } = context.actions;
  if (!fastenerAction) {
    return false;
  }
  await context.runAction(async () => {
    if (fastenerAction.originalParameters) {
      await context.updateFastenerParameters(
        fastenerAction.featureId,
        fastenerAction.originalParameters,
      );
    } else {
      await context.undo();
    }
  });
  context.setters.setFastenerAction(null);
  await context.restoreTimelineCursorAfterEdit();
  return true;
}

async function cancelHelixTool(context: CancelActiveToolContext) {
  return cancelUndoableActiveAction({
    action: context.actions.helixAction,
    clear: context.setters.setHelixAction,
    context,
  });
}

async function cancelTimelineEdit(context: CancelActiveToolContext) {
  if (!context.actions.editingFeatureId) {
    return false;
  }
  context.setters.setEditingFeatureId(null);
  await context.restoreTimelineCursorAfterEdit();
  return true;
}

async function closeMaterialsPanel(context: CancelActiveToolContext) {
  if (!context.actions.materialsPanelOpen) {
    return false;
  }
  context.setters.setMaterialsPanelOpen(false);
  return true;
}

async function cancelUndoableActiveAction<T extends { phase?: string }>({
  action,
  clear,
  context,
}: {
  action: T | null;
  clear: Setter<T>;
  context: CancelActiveToolContext;
}) {
  if (!action) {
    return false;
  }
  if (action.phase === "active") {
    await undoLatest(context);
  }
  clear(null);
  return true;
}

async function clearPendingAction<T>(
  action: T | null,
  clear: Setter<T>,
) {
  if (!action) {
    return false;
  }
  clear(null);
  return true;
}

async function undoLatest(context: CancelActiveToolContext) {
  await context.runAction(async () => {
    await context.undo();
  });
}
