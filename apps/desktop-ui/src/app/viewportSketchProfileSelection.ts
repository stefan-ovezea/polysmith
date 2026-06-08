import type { Dispatch, SetStateAction } from "react";
import type {
  ActiveExtrudeAction,
  ActiveLoftAction,
  ActiveRevolveAction,
  ActiveSweepAction,
  AnglePlaneAction,
  MidplaneAction,
  OffsetPlaneAction,
} from "./appState";
import type { SketchTool } from "../types";

interface MutableRef<T> {
  current: T;
}

type AsyncVoid = () => Promise<void>;
type RunAction = (action: AsyncVoid) => Promise<void>;

export interface ViewportSketchProfileSelectionContext {
  profileId: string;
  additive: boolean;
  offsetPlaneAction: OffsetPlaneAction | null;
  midplaneAction: MidplaneAction | null;
  anglePlaneAction: AnglePlaneAction | null;
  activeSketchPlaneId: string | null;
  activeSketchTool: SketchTool | null;
  loftAction: ActiveLoftAction | null;
  revolveAction: ActiveRevolveAction | null;
  sweepAction: ActiveSweepAction | null;
  extrudeAction: ActiveExtrudeAction | null;
  pendingOffsetRef: MutableRef<number>;
  pendingAngleRef: MutableRef<number>;
  setAnglePlaneAction: (action: AnglePlaneAction | null) => void;
  setLoftAction: Dispatch<SetStateAction<ActiveLoftAction | null>>;
  setRevolveAction: Dispatch<SetStateAction<ActiveRevolveAction | null>>;
  setSweepAction: Dispatch<SetStateAction<ActiveSweepAction | null>>;
  createOffsetPlaneFeature: (sourceId: string, offset: number) => Promise<void>;
  addMidplaneSource: (sourceId: string) => Promise<void>;
  projectProfileIntoSketch: (profileId: string) => Promise<void>;
  selectSketchProfile: (profileId: string, additive: boolean) => Promise<void>;
  describePlaneSource: (sourceId: string) => string;
  addMessage: (message: string) => void;
  runAction: RunAction;
}

export async function handleViewportSketchProfileSelection(
  context: ViewportSketchProfileSelectionContext,
) {
  if (await handleProfilePlaneSourcePick(context)) {
    return;
  }
  if (await handleProjectProfilePick(context)) {
    return;
  }
  if (await handleLoftProfilePick(context)) {
    return;
  }
  if (await handleRevolveProfilePick(context)) {
    return;
  }
  if (await handleSweepProfilePick(context)) {
    return;
  }

  await context.runAction(async () => {
    await context.selectSketchProfile(
      context.profileId,
      context.extrudeAction ? true : context.additive,
    );
  });
}

async function handleProfilePlaneSourcePick(
  context: ViewportSketchProfileSelectionContext,
) {
  if (context.offsetPlaneAction?.phase === "pending") {
    await context.createOffsetPlaneFeature(
      context.profileId,
      context.pendingOffsetRef.current,
    );
    return true;
  }

  if (context.midplaneAction) {
    await context.addMidplaneSource(context.profileId);
    return true;
  }

  if (context.anglePlaneAction?.phase === "pick_plane") {
    context.setAnglePlaneAction({
      phase: "pick_axis",
      sourcePlaneId: context.profileId,
      sourceSummary: context.describePlaneSource(context.profileId),
      initialAngle: context.pendingAngleRef.current,
    });
    return true;
  }

  return false;
}

async function handleProjectProfilePick(
  context: ViewportSketchProfileSelectionContext,
) {
  if (!context.activeSketchPlaneId || context.activeSketchTool !== "project") {
    return false;
  }

  await context.runAction(async () => {
    try {
      await context.projectProfileIntoSketch(context.profileId);
    } catch (error) {
      context.addMessage(
        `Project profile: ${
          error instanceof Error ? error.message : String(error)
        }`,
      );
    }
  });
  return true;
}

async function handleLoftProfilePick(
  context: ViewportSketchProfileSelectionContext,
) {
  const { loftAction, profileId } = context;
  if (!loftAction) {
    return false;
  }

  if (!loftAction.profileIds.includes(profileId)) {
    await context.runAction(async () => {
      await context.selectSketchProfile(profileId, true);
    });
    context.setLoftAction((current) =>
      current
        ? {
            ...current,
            profileIds: [...current.profileIds, profileId],
          }
        : current,
    );
  }
  return true;
}

async function handleRevolveProfilePick(
  context: ViewportSketchProfileSelectionContext,
) {
  if (!context.revolveAction) {
    return false;
  }

  await context.runAction(async () => {
    await context.selectSketchProfile(context.profileId, false);
  });
  context.setRevolveAction((current) =>
    current ? { ...current, profileId: context.profileId } : current,
  );
  return true;
}

async function handleSweepProfilePick(
  context: ViewportSketchProfileSelectionContext,
) {
  if (!context.sweepAction) {
    return false;
  }

  await context.runAction(async () => {
    await context.selectSketchProfile(context.profileId, false);
  });
  context.setSweepAction((current) =>
    current ? { ...current, profileId: context.profileId } : current,
  );
  return true;
}
