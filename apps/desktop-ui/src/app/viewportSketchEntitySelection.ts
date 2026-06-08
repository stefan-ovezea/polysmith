import type { Dispatch, SetStateAction } from "react";
import type {
  ActiveRevolveAction,
  ActiveSweepAction,
  AnglePlaneAction,
  HelixAction,
  PendingReferenceAction,
  ThreadAction,
} from "./appState";

interface MutableRef<T> {
  current: T;
}

type AsyncVoid = () => Promise<void>;
type RunAction = (action: AsyncVoid) => Promise<void>;

interface SketchLineRoutingContext {
  threadAction: ThreadAction | null;
  helixAction: HelixAction | null;
  constructionAxisAction: PendingReferenceAction | null;
  revolveAction: ActiveRevolveAction | null;
  sweepAction: ActiveSweepAction | null;
  setThreadAction: (action: ThreadAction | null) => void;
  setRevolveAction: Dispatch<SetStateAction<ActiveRevolveAction | null>>;
  setSweepAction: Dispatch<SetStateAction<ActiveSweepAction | null>>;
  createThreadFeature: (targetBodyId: string, axisSourceId: string) => Promise<void>;
  createHelixFeature: (axisSourceId: string) => Promise<void>;
  createConstructionAxisFeature: (sourceId: string) => Promise<void>;
}

export interface InactiveSketchLineSelectionContext
  extends SketchLineRoutingContext {
  lineId: string;
}

export interface SketchEntitySelectionContext extends SketchLineRoutingContext {
  entityId: string;
  additive: boolean;
  anglePlaneAction: AnglePlaneAction | null;
  sketchLineLabelById: ReadonlyMap<string, string>;
  pendingAngleRef: MutableRef<number>;
  createAnglePlaneFeature: (
    sourcePlaneId: string,
    axisId: string,
    angleDegrees: number,
  ) => Promise<void>;
  handleSketchConstraintLinePick: (
    lineId: string,
    additive?: boolean,
  ) => Promise<void>;
  runAction: RunAction;
}

export async function handleInactiveSketchLineSelection(
  context: InactiveSketchLineSelectionContext,
) {
  const handled = await handleSketchLineRouting({
    ...context,
    lineId: context.lineId,
  });
  if (!handled && context.sweepAction) {
    context.setSweepAction((current) =>
      current ? { ...current, pathEntityId: context.lineId } : current,
    );
  }
}

export async function handleSketchEntitySelection(
  context: SketchEntitySelectionContext,
) {
  const isLine = context.sketchLineLabelById.has(context.entityId);
  if (
    isLine &&
    (await handleSketchLineRouting({ ...context, lineId: context.entityId }))
  ) {
    return;
  }

  if (context.sweepAction) {
    context.setSweepAction((current) =>
      current ? { ...current, pathEntityId: context.entityId } : current,
    );
    return;
  }

  if (isLine && context.anglePlaneAction?.phase === "pick_axis") {
    await context.createAnglePlaneFeature(
      context.anglePlaneAction.sourcePlaneId,
      context.entityId,
      context.pendingAngleRef.current,
    );
    return;
  }

  await context.runAction(async () => {
    await context.handleSketchConstraintLinePick(
      context.entityId,
      context.additive,
    );
  });
}

async function handleSketchLineRouting(
  context: SketchLineRoutingContext & { lineId: string },
) {
  const { threadAction, lineId } = context;
  if (threadAction?.phase === "pick_axis") {
    await context.createThreadFeature(threadAction.targetBodyId, lineId);
    return true;
  }
  if (threadAction?.phase === "pick_target") {
    context.setThreadAction({ ...threadAction, axisSourceId: lineId });
    return true;
  }
  if (context.helixAction?.phase === "pending") {
    await context.createHelixFeature(lineId);
    return true;
  }
  if (context.constructionAxisAction) {
    await context.createConstructionAxisFeature(lineId);
    return true;
  }
  if (context.revolveAction) {
    context.setRevolveAction((current) =>
      current ? { ...current, axisEntityId: lineId } : current,
    );
    return true;
  }
  return false;
}
