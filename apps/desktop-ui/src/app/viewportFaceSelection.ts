import type {
  ActiveExtrudeAction,
  ActiveMoveAction,
  AnglePlaneAction,
  HoleAction,
  MidplaneAction,
  OffsetPlaneAction,
  PendingReferenceAction,
  ShellAction,
  ThreadAction,
} from "./appState";
import { bodyIdFromFaceId } from "./appState";
import type {
  ExtrudeMode,
  MoveFeatureParameters,
  SketchTool,
  ViewportState,
} from "../types";

interface MutableRef<T> {
  current: T;
}

type AsyncVoid = () => Promise<void>;
type RunAction = (action: AsyncVoid) => Promise<void>;

interface DefaultExtrudeSettings {
  mode: ExtrudeMode;
  targetBodyId: string | null;
}

export interface ViewportFaceSelectionContext {
  faceId: string;
  viewport: ViewportState | null;
  moveAction: ActiveMoveAction | null;
  threadAction: ThreadAction | null;
  holeAction: HoleAction | null;
  shellAction: ShellAction | null;
  offsetPlaneAction: OffsetPlaneAction | null;
  midplaneAction: MidplaneAction | null;
  anglePlaneAction: AnglePlaneAction | null;
  tangentPlaneAction: PendingReferenceAction | null;
  activeSketchPlaneId: string | null;
  activeSketchTool: SketchTool | null;
  extrudeAction: ActiveExtrudeAction | null;
  selectedSketchProfileIds: readonly string[];
  pendingOffsetRef: MutableRef<number>;
  pendingShellThicknessRef: MutableRef<number>;
  pendingAngleRef: MutableRef<number>;
  setThreadAction: (action: ThreadAction | null) => void;
  setAnglePlaneAction: (action: AnglePlaneAction | null) => void;
  setTangentPlaneAction: (action: PendingReferenceAction | null) => void;
  createMoveFeature: (
    bodyId: string,
    parameters: MoveFeatureParameters,
  ) => Promise<void>;
  createThreadFeature: (
    targetBodyId: string,
    axisSourceId: string,
  ) => Promise<void>;
  createHoleFeature: (faceId: string) => Promise<void>;
  createShellFeature: (faceId: string, thickness: number) => Promise<void>;
  createOffsetPlaneFeature: (sourceId: string, offset: number) => Promise<void>;
  addMidplaneSource: (sourceId: string) => Promise<void>;
  createTangentPlaneFeature: (faceId: string) => Promise<void>;
  projectFaceIntoSketch: (faceId: string) => Promise<void>;
  createExtrudeFromSelectedFace: (
    faceId: string,
    depth: number,
    mode: ExtrudeMode,
    targetBodyId: string | null,
  ) => Promise<void>;
  selectFace: (faceId: string) => Promise<void>;
  getDefaultExtrudeSettings: (
    profileIds: readonly string[],
    faceId?: string | null,
  ) => DefaultExtrudeSettings;
  describePlaneSource: (sourceId: string) => string;
  addMessage: (message: string) => void;
  runAction: RunAction;
}

export async function handleViewportFaceSelection(
  context: ViewportFaceSelectionContext,
) {
  if (await handlePendingBodyMove(context)) {
    return;
  }
  if (await handleThreadFacePick(context)) {
    return;
  }
  if (await handlePendingHolePick(context)) {
    return;
  }
  if (await handlePendingShellPick(context)) {
    return;
  }
  if (await handlePendingPlaneSourcePick(context)) {
    return;
  }
  if (await handleTangentPlanePick(context)) {
    return;
  }
  if (await handleProjectFacePick(context)) {
    return;
  }
  if (await handlePendingExtrudeFacePick(context)) {
    return;
  }

  await context.runAction(async () => {
    await context.selectFace(context.faceId);
  });
}

function findFace({ viewport, faceId }: ViewportFaceSelectionContext) {
  return viewport?.solid_faces.find((entry) => entry.face_id === faceId) ?? null;
}

async function handlePendingBodyMove(context: ViewportFaceSelectionContext) {
  const { moveAction } = context;
  if (moveAction?.phase !== "pending") {
    return false;
  }

  const face = findFace(context);
  if (face) {
    await context.createMoveFeature(face.owner_id, moveAction.parameters);
  }
  return true;
}

async function handleThreadFacePick(context: ViewportFaceSelectionContext) {
  const { threadAction } = context;
  if (
    threadAction?.phase !== "pick_target" &&
    threadAction?.phase !== "pick_axis"
  ) {
    return false;
  }

  const face = findFace(context);
  if (!face) {
    return true;
  }

  const bodyLabel =
    context.viewport?.bodies.find((body) => body.id === face.owner_id)?.label ??
    face.owner_id;
  const targetSummary = `${bodyLabel} · ${face.label}`;
  if (threadAction.phase === "pick_target") {
    if (threadAction.axisSourceId) {
      await context.createThreadFeature(face.owner_id, threadAction.axisSourceId);
    } else {
      context.setThreadAction({
        phase: "pick_axis",
        targetBodyId: face.owner_id,
        targetSummary,
      });
    }
    return true;
  }

  context.setThreadAction({
    ...threadAction,
    targetBodyId: face.owner_id,
    targetSummary,
  });
  return true;
}

async function handlePendingHolePick(context: ViewportFaceSelectionContext) {
  if (context.holeAction?.phase !== "pending") {
    return false;
  }

  const face = findFace(context);
  if (face?.sketchability === "planar") {
    await context.createHoleFeature(context.faceId);
  }
  return true;
}

async function handlePendingShellPick(context: ViewportFaceSelectionContext) {
  if (context.shellAction?.phase !== "pending") {
    return false;
  }

  await context.createShellFeature(
    context.faceId,
    context.pendingShellThicknessRef.current,
  );
  return true;
}

async function handlePendingPlaneSourcePick(
  context: ViewportFaceSelectionContext,
) {
  if (await handleOffsetPlaneFacePick(context)) {
    return true;
  }
  if (await handleMidplaneFacePick(context)) {
    return true;
  }
  if (handleAnglePlaneFacePick(context)) {
    return true;
  }

  return false;
}

async function handleOffsetPlaneFacePick(context: ViewportFaceSelectionContext) {
  if (context.offsetPlaneAction?.phase !== "pending") {
    return false;
  }

  const face = findFace(context);
  if (face?.sketchability === "planar") {
    await context.createOffsetPlaneFeature(
      context.faceId,
      context.pendingOffsetRef.current,
    );
  }
  return true;
}

async function handleMidplaneFacePick(context: ViewportFaceSelectionContext) {
  if (!context.midplaneAction) {
    return false;
  }

  const face = findFace(context);
  if (face?.sketchability === "planar") {
    await context.addMidplaneSource(context.faceId);
  }
  return true;
}

function handleAnglePlaneFacePick(context: ViewportFaceSelectionContext) {
  if (context.anglePlaneAction?.phase !== "pick_plane") {
    return false;
  }

  const face = findFace(context);
  if (face?.sketchability === "planar") {
    context.setAnglePlaneAction({
      phase: "pick_axis",
      sourcePlaneId: context.faceId,
      sourceSummary: context.describePlaneSource(context.faceId),
      initialAngle: context.pendingAngleRef.current,
    });
  }
  return true;
}

async function handleTangentPlanePick(context: ViewportFaceSelectionContext) {
  if (!context.tangentPlaneAction) {
    return false;
  }

  context.setTangentPlaneAction(null);
  await context.createTangentPlaneFeature(context.faceId);
  return true;
}

async function handleProjectFacePick(context: ViewportFaceSelectionContext) {
  if (!context.activeSketchPlaneId || context.activeSketchTool !== "project") {
    return false;
  }

  await context.runAction(async () => {
    try {
      await context.projectFaceIntoSketch(context.faceId);
    } catch (error) {
      context.addMessage(
        `Project face: ${error instanceof Error ? error.message : String(error)}`,
      );
    }
  });
  return true;
}

async function handlePendingExtrudeFacePick(
  context: ViewportFaceSelectionContext,
) {
  const { extrudeAction } = context;
  if (
    !extrudeAction ||
    extrudeAction.phase !== "pending" ||
    context.selectedSketchProfileIds.length > 0
  ) {
    return false;
  }

  const face = findFace(context);
  if (face?.sketchability !== "planar") {
    return true;
  }

  const defaultSettings = context.getDefaultExtrudeSettings([], context.faceId);
  const mode =
    extrudeAction.initialMode === "new_body"
      ? defaultSettings.mode
      : extrudeAction.initialMode;
  const targetBodyId =
    mode === "new_body"
      ? null
      : extrudeAction.initialTargetBodyId ??
        defaultSettings.targetBodyId ??
        bodyIdFromFaceId(context.faceId);
  await context.createExtrudeFromSelectedFace(
    context.faceId,
    extrudeAction.initialDepth,
    mode,
    targetBodyId,
  );
  return true;
}
