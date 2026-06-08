import type { MutableRefObject } from "react";

import type { DocumentState } from "../types/ipc";
import {
  DEFAULT_ANGLE_PLANE_DEGREES,
  DEFAULT_OFFSET_PLANE_DISTANCE,
  type AnglePlaneAction,
  type MidplaneAction,
  type OffsetPlaneAction,
  type PendingReferenceAction,
} from "./appState";
import {
  type AppToolState,
  isToolStartBlocked,
} from "./actionAvailability";
import { awaitCreatedFeatureOfKind } from "./featureCreation";
import * as selectionSources from "./selectionSources";

type PlaneSourceContext = Parameters<
  typeof selectionSources.currentPlaneLikeSourceId
>[0];
type AxisSourceContext = Parameters<
  typeof selectionSources.currentAxisSourceId
>[0];

interface ConstructionActionContext {
  activeToolState: AppToolState;
  document: DocumentState | null;
  planeSourceContext: PlaneSourceContext;
  axisSourceContext: AxisSourceContext;
  pendingOffsetRef: MutableRefObject<number>;
  pendingAngleRef: MutableRefObject<number>;
  midplaneAction: MidplaneAction | null;
  setOffsetPlaneAction: (action: OffsetPlaneAction | null) => void;
  setMidplaneAction: (action: MidplaneAction | null) => void;
  setTangentPlaneAction: (action: PendingReferenceAction | null) => void;
  setAnglePlaneAction: (action: AnglePlaneAction | null) => void;
  setConstructionAxisAction: (action: PendingReferenceAction | null) => void;
  setConstructionPointAction: (action: PendingReferenceAction | null) => void;
  createOffsetPlane: (sourcePlaneId: string, offset: number) => Promise<void>;
  createMidplane: (sourcePlaneIds: [string, string]) => Promise<void>;
  createTangentPlane: (sourceFaceId: string) => Promise<void>;
  createAnglePlane: (
    sourcePlaneId: string,
    sourceAxisId: string,
    angleDegrees: number,
  ) => Promise<void>;
  createConstructionAxis: (sourceId: string) => Promise<void>;
  createConstructionPoint: (sourceId: string) => Promise<void>;
  runAction: (action: () => Promise<void>) => Promise<void>;
  addMessage: (message: string) => void;
}

export function createConstructionActions({
  activeToolState,
  document,
  planeSourceContext,
  axisSourceContext,
  pendingOffsetRef,
  pendingAngleRef,
  midplaneAction,
  setOffsetPlaneAction,
  setMidplaneAction,
  setTangentPlaneAction,
  setAnglePlaneAction,
  setConstructionAxisAction,
  setConstructionPointAction,
  createOffsetPlane,
  createMidplane,
  createTangentPlane,
  createAnglePlane,
  createConstructionAxis,
  createConstructionPoint,
  runAction,
  addMessage,
}: ConstructionActionContext) {
  async function triggerOffsetPlaneAction() {
    if (
      isToolStartBlocked(activeToolState, {
        hole: false,
        helix: false,
        thread: false,
        fastener: false,
        move: false,
      })
    ) {
      return;
    }
    pendingOffsetRef.current = DEFAULT_OFFSET_PLANE_DISTANCE;

    const sourceId =
      selectionSources.currentPlaneLikeSourceId(planeSourceContext);
    if (sourceId) {
      await createOffsetPlaneFeature(sourceId, DEFAULT_OFFSET_PLANE_DISTANCE);
      return;
    }

    setOffsetPlaneAction({
      phase: "pending",
      initialOffset: DEFAULT_OFFSET_PLANE_DISTANCE,
    });
  }

  async function createOffsetPlaneFeature(sourceId: string, offset: number) {
    const documentPromise = awaitCreatedFeatureOfKind("construction_plane");

    await runAction(async () => {
      await createOffsetPlane(sourceId, offset);
      try {
        const { featureId: newFeatureId } = await documentPromise;
        setOffsetPlaneAction({
          phase: "active",
          featureId: newFeatureId,
          initialOffset: offset,
          sourceSummary: selectionSources.describePlaneSource(
            planeSourceContext,
            sourceId,
          ),
        });
      } catch (error) {
        addMessage(`offset plane error: ${String(error)}`);
      }
    });
  }

  async function createMidplaneFeature(sourceIds: [string, string]) {
    const documentPromise = awaitCreatedFeatureOfKind("construction_plane");

    await runAction(async () => {
      await createMidplane(sourceIds);
      try {
        await documentPromise;
      } catch (error) {
        addMessage(`midplane error: ${String(error)}`);
      }
    });
  }

  async function createTangentPlaneFeature(sourceFaceId: string) {
    const documentPromise = awaitCreatedFeatureOfKind("construction_plane");

    await runAction(async () => {
      await createTangentPlane(sourceFaceId);
      try {
        await documentPromise;
      } catch (error) {
        addMessage(`tangent plane error: ${String(error)}`);
      }
    });
  }

  async function createAnglePlaneFeature(
    sourcePlaneId: string,
    sourceAxisId: string,
    angleDegrees: number,
  ) {
    const documentPromise = awaitCreatedFeatureOfKind("construction_plane");

    await runAction(async () => {
      await createAnglePlane(sourcePlaneId, sourceAxisId, angleDegrees);
      try {
        const { featureId: newFeatureId } = await documentPromise;
        setAnglePlaneAction({
          phase: "active",
          featureId: newFeatureId,
          sourcePlaneId,
          sourceSummary: selectionSources.describePlaneSource(
            planeSourceContext,
            sourcePlaneId,
          ),
          axisId: sourceAxisId,
          axisSummary: selectionSources.describeAxisSource(
            axisSourceContext,
            sourceAxisId,
          ),
          initialAngle: angleDegrees,
        });
      } catch (error) {
        addMessage(`angle plane error: ${String(error)}`);
      }
    });
  }

  async function addMidplaneSource(sourceId: string) {
    if (!midplaneAction) {
      return;
    }
    if (midplaneAction.sourceIds.includes(sourceId)) {
      setMidplaneAction({
        sourceIds: midplaneAction.sourceIds.filter((id) => id !== sourceId),
      });
      return;
    }
    const next = [...midplaneAction.sourceIds, sourceId];
    if (next.length >= 2) {
      setMidplaneAction(null);
      await createMidplaneFeature([next[0], next[1]]);
      return;
    }
    setMidplaneAction({ sourceIds: next });
  }

  async function triggerMidplaneAction() {
    if (
      isToolStartBlocked(activeToolState, {
        hole: false,
        constructionAxis: false,
        constructionPoint: false,
        helix: false,
        thread: false,
        fastener: false,
        move: false,
      })
    ) {
      return;
    }
    const firstSourceId =
      selectionSources.currentPlaneLikeSourceId(planeSourceContext);
    setMidplaneAction({ sourceIds: firstSourceId ? [firstSourceId] : [] });
  }

  async function triggerTangentPlaneAction() {
    if (
      isToolStartBlocked(activeToolState, {
        hole: false,
        constructionAxis: false,
        constructionPoint: false,
        helix: false,
        thread: false,
        fastener: false,
        move: false,
      })
    ) {
      return;
    }
    const sourceFaceId = selectionSources.currentFaceSourceId(document);
    if (sourceFaceId) {
      await createTangentPlaneFeature(sourceFaceId);
      return;
    }
    setTangentPlaneAction({ isPending: true });
  }

  async function triggerAnglePlaneAction() {
    if (
      isToolStartBlocked(activeToolState, {
        hole: false,
        constructionAxis: false,
        constructionPoint: false,
        helix: false,
        thread: false,
        fastener: false,
        move: false,
      })
    ) {
      return;
    }
    pendingAngleRef.current = DEFAULT_ANGLE_PLANE_DEGREES;
    const sourcePlaneId =
      selectionSources.currentPlaneLikeSourceId(planeSourceContext);
    const sourceAxisId =
      selectionSources.currentAxisSourceId(axisSourceContext);
    if (sourcePlaneId && sourceAxisId) {
      await createAnglePlaneFeature(
        sourcePlaneId,
        sourceAxisId,
        DEFAULT_ANGLE_PLANE_DEGREES,
      );
      return;
    }
    if (sourcePlaneId) {
      setAnglePlaneAction({
        phase: "pick_axis",
        sourcePlaneId,
        sourceSummary: selectionSources.describePlaneSource(
          planeSourceContext,
          sourcePlaneId,
        ),
        initialAngle: DEFAULT_ANGLE_PLANE_DEGREES,
      });
      return;
    }
    setAnglePlaneAction({
      phase: "pick_plane",
      initialAngle: DEFAULT_ANGLE_PLANE_DEGREES,
    });
  }

  async function createConstructionAxisFeature(sourceId: string) {
    await runAction(async () => {
      try {
        await createConstructionAxis(sourceId);
        setConstructionAxisAction(null);
      } catch (error) {
        addMessage(`axis error: ${String(error)}`);
      }
    });
  }

  async function createConstructionPointFeature(sourceId: string) {
    await runAction(async () => {
      try {
        await createConstructionPoint(sourceId);
        setConstructionPointAction(null);
      } catch (error) {
        addMessage(`point error: ${String(error)}`);
      }
    });
  }

  async function triggerConstructionAxisAction() {
    if (
      isToolStartBlocked(activeToolState, {
        activeSketchPlane: false,
      })
    ) {
      return;
    }
    const sourceId = selectionSources.currentAxisSourceId(axisSourceContext);
    if (sourceId) {
      await createConstructionAxisFeature(sourceId);
      return;
    }
    setConstructionAxisAction({ isPending: true });
  }

  async function triggerConstructionPointAction() {
    if (
      isToolStartBlocked(activeToolState, {
        activeSketchPlane: false,
      })
    ) {
      return;
    }
    const sourceId = selectionSources.currentPointSourceId(document);
    if (sourceId) {
      await createConstructionPointFeature(sourceId);
      return;
    }
    setConstructionPointAction({ isPending: true });
  }

  return {
    addMidplaneSource,
    createAnglePlaneFeature,
    createConstructionAxisFeature,
    createConstructionPointFeature,
    createOffsetPlaneFeature,
    createTangentPlaneFeature,
    triggerAnglePlaneAction,
    triggerConstructionAxisAction,
    triggerConstructionPointAction,
    triggerMidplaneAction,
    triggerOffsetPlaneAction,
    triggerTangentPlaneAction,
  };
}
