import type { MutableRefObject } from "react";
import type {
  ExtrudeFeatureParameters,
  ExtrudeMode,
  FastenerFeatureParameters,
  MoveFeatureParameters,
  ThreadFeatureParameters,
} from "../types";
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

type AsyncVoid = () => Promise<void>;
type RunAction = (action: AsyncVoid) => Promise<void>;
type Setter<T> = (value: T | null) => void;

export interface CancelActiveToolContext {
  actions: {
    extrudeAction: ActiveExtrudeAction | null;
    loftAction: ActiveLoftAction | null;
    revolveAction: ActiveRevolveAction | null;
    sweepAction: ActiveSweepAction | null;
    moveAction: ActiveMoveAction | null;
    edgeOpAction: ActiveEdgeOpAction | null;
    shellAction: ShellAction | null;
    holeAction: HoleAction | null;
    offsetPlaneAction: OffsetPlaneAction | null;
    anglePlaneAction: AnglePlaneAction | null;
    midplaneAction: MidplaneAction | null;
    tangentPlaneAction: PendingReferenceAction | null;
    constructionAxisAction: PendingReferenceAction | null;
    constructionPointAction: PendingReferenceAction | null;
    threadAction: ThreadAction | null;
    fastenerAction: FastenerAction | null;
    helixAction: HelixAction | null;
    editingFeatureId: string | null;
    materialsPanelOpen: boolean;
  };
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
  updateExtrudeDepth: (featureId: string, depth: number) => Promise<void>;
  updateExtrudeMode: (
    featureId: string,
    mode: ExtrudeMode,
  ) => Promise<void>;
  updateExtrudeTargetBody: (
    featureId: string,
    targetBodyId: string | null,
  ) => Promise<void>;
  updateExtrudeParameters: (
    featureId: string,
    parameters: ExtrudeFeatureParameters,
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
  actions,
  setters,
  activeEdgeIdsRef,
  runAction,
  restoreTimelineCursorAfterEdit,
  undo,
  undoUntilExtrudePreviewRemoved,
  updateExtrudeDepth,
  updateExtrudeMode,
  updateExtrudeTargetBody,
  updateExtrudeParameters,
  updateLoftProfiles,
  updateLoftRuled,
  updateRevolveProfile,
  updateRevolveAxis,
  updateRevolveAngle,
  updateSweepProfile,
  updateSweepPath,
  updateMoveParameters,
  updateThreadParameters,
  updateFastenerParameters,
}: CancelActiveToolContext) {
  const {
    extrudeAction,
    loftAction,
    revolveAction,
    sweepAction,
    moveAction,
    edgeOpAction,
    shellAction,
    holeAction,
    offsetPlaneAction,
    anglePlaneAction,
    midplaneAction,
    tangentPlaneAction,
    constructionAxisAction,
    constructionPointAction,
    threadAction,
    fastenerAction,
    helixAction,
    editingFeatureId,
    materialsPanelOpen,
  } = actions;

  if (extrudeAction) {
    if (extrudeAction.phase === "active" && extrudeAction.featureId) {
      const snapshot = extrudeAction.originalSnapshot;
      if (snapshot) {
        await runAction(async () => {
          await updateExtrudeDepth(extrudeAction.featureId!, snapshot.depth);
          await updateExtrudeMode(extrudeAction.featureId!, snapshot.mode);
          await updateExtrudeTargetBody(
            extrudeAction.featureId!,
            snapshot.targetBodyId,
          );
          await updateExtrudeParameters(
            extrudeAction.featureId!,
            snapshot.parameters,
          );
        });
      } else {
        await undoUntilExtrudePreviewRemoved(
          extrudeAction.featureIds.length > 0
            ? extrudeAction.featureIds
            : [extrudeAction.featureId],
        );
      }
    }
    setters.setExtrudeAction(null);
    await restoreTimelineCursorAfterEdit();
    return true;
  }

  if (loftAction) {
    if (loftAction.originalSnapshot && loftAction.featureId) {
      const snapshot = loftAction.originalSnapshot;
      await runAction(async () => {
        await updateLoftProfiles(loftAction.featureId!, snapshot.profileIds);
        await updateLoftRuled(loftAction.featureId!, snapshot.ruled);
      });
    } else if (loftAction.phase === "active") {
      await runAction(async () => {
        await undo();
      });
    }
    setters.setLoftAction(null);
    await restoreTimelineCursorAfterEdit();
    return true;
  }

  if (revolveAction) {
    if (revolveAction.originalSnapshot && revolveAction.featureId) {
      const snapshot = revolveAction.originalSnapshot;
      await runAction(async () => {
        await updateRevolveProfile(revolveAction.featureId!, snapshot.profileId);
        await updateRevolveAxis(revolveAction.featureId!, snapshot.axisEntityId);
        await updateRevolveAngle(
          revolveAction.featureId!,
          snapshot.angleDegrees,
        );
      });
    } else if (revolveAction.phase === "active") {
      await runAction(async () => {
        await undo();
      });
    }
    setters.setRevolveAction(null);
    await restoreTimelineCursorAfterEdit();
    return true;
  }

  if (sweepAction) {
    if (sweepAction.originalSnapshot && sweepAction.featureId) {
      const snapshot = sweepAction.originalSnapshot;
      await runAction(async () => {
        await updateSweepProfile(sweepAction.featureId!, snapshot.profileId);
        await updateSweepPath(sweepAction.featureId!, snapshot.pathEntityId);
      });
    } else if (sweepAction.phase === "active") {
      await runAction(async () => {
        await undo();
      });
    }
    setters.setSweepAction(null);
    await restoreTimelineCursorAfterEdit();
    return true;
  }

  if (moveAction) {
    if (moveAction.phase === "active") {
      if (moveAction.originalSnapshot) {
        await runAction(async () => {
          await updateMoveParameters(
            moveAction.featureId,
            moveAction.originalSnapshot!,
          );
        });
      } else {
        await runAction(async () => {
          await undo();
          if (moveAction.createdCopyFeatureId) {
            await undo();
          }
        });
      }
    }
    setters.setMoveAction(null);
    await restoreTimelineCursorAfterEdit();
    return true;
  }

  if (edgeOpAction) {
    if (edgeOpAction.phase === "active") {
      await runAction(async () => {
        await undo();
      });
    }
    activeEdgeIdsRef.current = [];
    setters.setEdgeOpAction(null);
    return true;
  }

  if (shellAction) {
    if (shellAction.phase === "active") {
      await runAction(async () => {
        await undo();
      });
    }
    setters.setShellAction(null);
    return true;
  }

  if (holeAction) {
    if (holeAction.phase === "active") {
      await runAction(async () => {
        await undo();
      });
    }
    setters.setHoleAction(null);
    return true;
  }

  if (offsetPlaneAction) {
    if (offsetPlaneAction.phase === "active") {
      await runAction(async () => {
        await undo();
      });
    }
    setters.setOffsetPlaneAction(null);
    return true;
  }

  if (anglePlaneAction) {
    if (anglePlaneAction.phase === "active") {
      await runAction(async () => {
        await undo();
      });
    }
    setters.setAnglePlaneAction(null);
    return true;
  }

  if (midplaneAction) {
    setters.setMidplaneAction(null);
    return true;
  }

  if (tangentPlaneAction) {
    setters.setTangentPlaneAction(null);
    return true;
  }

  if (constructionAxisAction) {
    setters.setConstructionAxisAction(null);
    return true;
  }

  if (constructionPointAction) {
    setters.setConstructionPointAction(null);
    return true;
  }

  if (threadAction) {
    if (threadAction.phase === "active") {
      await runAction(async () => {
        if (threadAction.originalParameters) {
          await updateThreadParameters(
            threadAction.featureId,
            threadAction.originalParameters,
          );
        } else {
          await undo();
        }
      });
      await restoreTimelineCursorAfterEdit();
    }
    setters.setThreadAction(null);
    return true;
  }

  if (fastenerAction) {
    await runAction(async () => {
      if (fastenerAction.originalParameters) {
        await updateFastenerParameters(
          fastenerAction.featureId,
          fastenerAction.originalParameters,
        );
      } else {
        await undo();
      }
    });
    setters.setFastenerAction(null);
    await restoreTimelineCursorAfterEdit();
    return true;
  }

  if (helixAction) {
    if (helixAction.phase === "active") {
      await runAction(async () => {
        await undo();
      });
    }
    setters.setHelixAction(null);
    return true;
  }

  if (editingFeatureId) {
    setters.setEditingFeatureId(null);
    await restoreTimelineCursorAfterEdit();
    return true;
  }

  if (materialsPanelOpen) {
    setters.setMaterialsPanelOpen(false);
    return true;
  }

  return false;
}
