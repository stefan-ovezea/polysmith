import type { Dispatch, SetStateAction } from "react";
import type {
  ActiveEdgeOpAction,
  AnglePlaneAction,
  HelixAction,
  PendingReferenceAction,
  ThreadAction,
} from "./appState";
import type { SketchTool } from "../types";

interface MutableRef<T> {
  current: T;
}

type AsyncVoid = () => Promise<void>;
type RunAction = (action: AsyncVoid) => Promise<void>;

export interface ViewportEdgeSelectionContext {
  edgeId: string;
  additive: boolean;
  threadAction: ThreadAction | null;
  helixAction: HelixAction | null;
  constructionAxisAction: PendingReferenceAction | null;
  activeSketchPlaneId: string | null;
  activeSketchTool: SketchTool | null;
  anglePlaneAction: AnglePlaneAction | null;
  edgeOpAction: ActiveEdgeOpAction | null;
  pendingAngleRef: MutableRef<number>;
  pendingValueRef: MutableRef<number>;
  activeEdgeIdsRef: MutableRef<string[]>;
  setThreadAction: (action: ThreadAction | null) => void;
  setEdgeOpAction: Dispatch<SetStateAction<ActiveEdgeOpAction | null>>;
  createThreadFeature: (targetBodyId: string, axisSourceId: string) => Promise<void>;
  createHelixFeature: (axisSourceId: string) => Promise<void>;
  createConstructionAxisFeature: (sourceId: string) => Promise<void>;
  projectEdgeIntoSketch: (edgeId: string) => Promise<void>;
  createAnglePlaneFeature: (
    sourcePlaneId: string,
    axisId: string,
    angleDegrees: number,
  ) => Promise<void>;
  createEdgeOpFeature: (
    kind: "fillet" | "chamfer",
    edgeIds: string[],
    value: number,
  ) => Promise<void>;
  updateFilletEdges: (featureId: string, edgeIds: string[]) => Promise<void>;
  updateChamferEdges: (featureId: string, edgeIds: string[]) => Promise<void>;
  selectEdge: (edgeId: string, additive: boolean) => Promise<void>;
  addMessage: (message: string) => void;
  runAction: RunAction;
}

export async function handleViewportEdgeSelection(
  context: ViewportEdgeSelectionContext,
) {
  if (await handleThreadEdgePick(context)) {
    return;
  }
  if (await handlePendingHelixEdgePick(context)) {
    return;
  }
  if (await handleConstructionAxisEdgePick(context)) {
    return;
  }
  if (await handleProjectEdgePick(context)) {
    return;
  }
  if (await handleAnglePlaneAxisEdgePick(context)) {
    return;
  }
  if (await handleEdgeOpPick(context)) {
    return;
  }

  await context.runAction(async () => {
    await context.selectEdge(context.edgeId, context.additive);
  });
}

async function handleThreadEdgePick(context: ViewportEdgeSelectionContext) {
  const { threadAction } = context;
  if (threadAction?.phase === "pick_axis") {
    await context.createThreadFeature(threadAction.targetBodyId, context.edgeId);
    return true;
  }
  if (threadAction?.phase === "pick_target") {
    context.setThreadAction({ ...threadAction, axisSourceId: context.edgeId });
    return true;
  }
  return false;
}

async function handlePendingHelixEdgePick(
  context: ViewportEdgeSelectionContext,
) {
  if (context.helixAction?.phase !== "pending") {
    return false;
  }

  await context.createHelixFeature(context.edgeId);
  return true;
}

async function handleConstructionAxisEdgePick(
  context: ViewportEdgeSelectionContext,
) {
  if (!context.constructionAxisAction) {
    return false;
  }

  await context.createConstructionAxisFeature(context.edgeId);
  return true;
}

async function handleProjectEdgePick(context: ViewportEdgeSelectionContext) {
  if (!context.activeSketchPlaneId || context.activeSketchTool !== "project") {
    return false;
  }

  await context.runAction(async () => {
    try {
      await context.projectEdgeIntoSketch(context.edgeId);
    } catch (error) {
      context.addMessage(
        `Project edge: ${error instanceof Error ? error.message : String(error)}`,
      );
    }
  });
  return true;
}

async function handleAnglePlaneAxisEdgePick(
  context: ViewportEdgeSelectionContext,
) {
  if (context.anglePlaneAction?.phase !== "pick_axis") {
    return false;
  }

  await context.createAnglePlaneFeature(
    context.anglePlaneAction.sourcePlaneId,
    context.edgeId,
    context.pendingAngleRef.current,
  );
  return true;
}

async function handleEdgeOpPick(context: ViewportEdgeSelectionContext) {
  const { edgeOpAction } = context;
  if (!edgeOpAction) {
    return false;
  }

  if (edgeOpAction.phase === "pending") {
    await context.createEdgeOpFeature(
      edgeOpAction.kind,
      [context.edgeId],
      context.pendingValueRef.current,
    );
    return true;
  }

  const updated = nextEdgeOpEdgeIds(
    context.activeEdgeIdsRef.current,
    context.edgeId,
  );
  if (updated.length === 0) {
    return true;
  }

  context.activeEdgeIdsRef.current = updated;
  context.setEdgeOpAction((current) =>
    current?.phase === "active" ? { ...current, edgeIds: updated } : current,
  );
  await context.runAction(async () => {
    if (edgeOpAction.kind === "fillet") {
      await context.updateFilletEdges(edgeOpAction.featureId, updated);
    } else {
      await context.updateChamferEdges(edgeOpAction.featureId, updated);
    }
  });
  return true;
}

function nextEdgeOpEdgeIds(current: readonly string[], edgeId: string) {
  return current.includes(edgeId)
    ? current.filter((id) => id !== edgeId)
    : [...current, edgeId];
}
