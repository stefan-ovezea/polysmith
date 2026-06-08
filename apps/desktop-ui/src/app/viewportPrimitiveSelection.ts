import type { ActiveMoveAction, ThreadAction } from "./appState";
import type { ViewportState } from "../types";

type AsyncVoid = () => Promise<void>;
type RunAction = (action: AsyncVoid) => Promise<void>;

export interface ViewportPrimitiveSelectionContext {
  primitiveId: string;
  viewport: ViewportState | null;
  moveAction: ActiveMoveAction | null;
  threadAction: ThreadAction | null;
  setThreadAction: (action: ThreadAction | null) => void;
  createMoveFeature: (
    bodyId: string,
    parameters: ActiveMoveAction["parameters"],
  ) => Promise<void>;
  createThreadFeature: (
    targetBodyId: string,
    axisSourceId: string,
  ) => Promise<void>;
  selectFeature: (featureId: string) => Promise<void>;
  runAction: RunAction;
}

export async function handleViewportPrimitiveSelection(
  context: ViewportPrimitiveSelectionContext,
) {
  if (await handlePendingMovePrimitivePick(context)) {
    return;
  }
  if (await handleThreadPrimitivePick(context)) {
    return;
  }
  await context.runAction(async () => {
    await context.selectFeature(context.primitiveId);
  });
}

async function handlePendingMovePrimitivePick(
  context: ViewportPrimitiveSelectionContext,
) {
  const { moveAction } = context;
  if (moveAction?.phase !== "pending") {
    return false;
  }
  const body = findBody(context);
  if (body) {
    await context.createMoveFeature(body.id, moveAction.parameters);
  }
  return true;
}

async function handleThreadPrimitivePick(
  context: ViewportPrimitiveSelectionContext,
) {
  const { threadAction } = context;
  if (!isThreadPrimitivePickActive(threadAction)) {
    return false;
  }
  const body = findBody(context);
  if (!body) {
    return true;
  }
  if (threadAction.phase === "pick_target") {
    await handleThreadTargetPick(context, body.id, body.label);
    return true;
  }
  setThreadTargetBody(context, body.id, body.label);
  return true;
}

function isThreadPrimitivePickActive(
  threadAction: ThreadAction | null,
): threadAction is Extract<
  ThreadAction,
  { phase: "pick_target" | "pick_axis" }
> {
  return (
    threadAction?.phase === "pick_target" ||
    threadAction?.phase === "pick_axis"
  );
}

async function handleThreadTargetPick(
  context: ViewportPrimitiveSelectionContext,
  bodyId: string,
  bodyLabel: string,
) {
  const { threadAction } = context;
  if (threadAction?.phase !== "pick_target") {
    return;
  }
  if (threadAction.axisSourceId) {
    await context.createThreadFeature(bodyId, threadAction.axisSourceId);
    return;
  }
  context.setThreadAction({
    phase: "pick_axis",
    targetBodyId: bodyId,
    targetSummary: bodyLabel,
  });
}

function setThreadTargetBody(
  context: ViewportPrimitiveSelectionContext,
  bodyId: string,
  bodyLabel: string,
) {
  const { threadAction } = context;
  if (threadAction?.phase !== "pick_axis") {
    return;
  }
  context.setThreadAction({
    ...threadAction,
    targetBodyId: bodyId,
    targetSummary: bodyLabel,
  });
}

function findBody({ viewport, primitiveId }: ViewportPrimitiveSelectionContext) {
  return viewport?.bodies.find((entry) => entry.id === primitiveId) ?? null;
}
