import type { ActiveRevolveAction, ActiveSweepAction } from "./appState";

type MutableRef<T> = { current: T };
type RunAction = (action: () => Promise<void>) => Promise<void> | void;

export function syncRevolveActionInputs({
  revolveAction,
  lastRevolveInputsRef,
  createRevolveFromInputs,
  runAction,
  updateRevolveProfile,
  updateRevolveAxis,
}: {
  revolveAction: ActiveRevolveAction | null;
  lastRevolveInputsRef: MutableRef<string>;
  createRevolveFromInputs: (
    profileId: string,
    axisEntityId: string,
    angleDegrees: number,
  ) => Promise<void>;
  runAction: RunAction;
  updateRevolveProfile: (
    featureId: string,
    profileId: string,
  ) => Promise<void>;
  updateRevolveAxis: (featureId: string, axisEntityId: string) => Promise<void>;
}) {
  const inputs = selectedRevolveInputs(revolveAction);
  if (!inputs || markSyncKeySeen(lastRevolveInputsRef, inputs.key)) {
    return;
  }

  if (revolveAction.phase === "pending") {
    void createRevolveFromInputs(
      inputs.profileId,
      inputs.axisEntityId,
      revolveAction.initialAngle,
    );
    return;
  }

  syncActiveRevolveInputs({
    featureId: revolveAction.featureId,
    inputs,
    runAction,
    updateRevolveProfile,
    updateRevolveAxis,
  });
}

export function syncSweepActionInputs({
  sweepAction,
  lastSweepInputsRef,
  createSweepFromInputs,
  runAction,
  updateSweepProfile,
  updateSweepPath,
}: {
  sweepAction: ActiveSweepAction | null;
  lastSweepInputsRef: MutableRef<string>;
  createSweepFromInputs: (
    profileId: string,
    pathEntityId: string,
  ) => Promise<void>;
  runAction: RunAction;
  updateSweepProfile: (featureId: string, profileId: string) => Promise<void>;
  updateSweepPath: (featureId: string, pathEntityId: string) => Promise<void>;
}) {
  const inputs = selectedSweepInputs(sweepAction);
  if (!inputs || markSyncKeySeen(lastSweepInputsRef, inputs.key)) {
    return;
  }

  if (sweepAction.phase === "pending") {
    void createSweepFromInputs(inputs.profileId, inputs.pathEntityId);
    return;
  }

  syncActiveSweepInputs({
    featureId: sweepAction.featureId,
    inputs,
    runAction,
    updateSweepProfile,
    updateSweepPath,
  });
}

function selectedRevolveInputs(revolveAction: ActiveRevolveAction | null) {
  if (!revolveAction?.profileId || !revolveAction.axisEntityId) {
    return null;
  }
  return {
    profileId: revolveAction.profileId,
    axisEntityId: revolveAction.axisEntityId,
    key: `${revolveAction.profileId}|${revolveAction.axisEntityId}`,
  };
}

function selectedSweepInputs(sweepAction: ActiveSweepAction | null) {
  if (!sweepAction?.profileId || !sweepAction.pathEntityId) {
    return null;
  }
  return {
    profileId: sweepAction.profileId,
    pathEntityId: sweepAction.pathEntityId,
    key: `${sweepAction.profileId}|${sweepAction.pathEntityId}`,
  };
}

function markSyncKeySeen(lastInputsRef: MutableRef<string>, key: string) {
  if (lastInputsRef.current === key) {
    return true;
  }
  lastInputsRef.current = key;
  return false;
}

function syncActiveRevolveInputs({
  featureId,
  inputs,
  runAction,
  updateRevolveProfile,
  updateRevolveAxis,
}: {
  featureId: string | null;
  inputs: NonNullable<ReturnType<typeof selectedRevolveInputs>>;
  runAction: RunAction;
  updateRevolveProfile: (
    featureId: string,
    profileId: string,
  ) => Promise<void>;
  updateRevolveAxis: (featureId: string, axisEntityId: string) => Promise<void>;
}) {
  if (!featureId) {
    return;
  }

  void runAction(async () => {
    await updateRevolveProfile(featureId, inputs.profileId);
    await updateRevolveAxis(featureId, inputs.axisEntityId);
  });
}

function syncActiveSweepInputs({
  featureId,
  inputs,
  runAction,
  updateSweepProfile,
  updateSweepPath,
}: {
  featureId: string | null;
  inputs: NonNullable<ReturnType<typeof selectedSweepInputs>>;
  runAction: RunAction;
  updateSweepProfile: (featureId: string, profileId: string) => Promise<void>;
  updateSweepPath: (featureId: string, pathEntityId: string) => Promise<void>;
}) {
  if (!featureId) {
    return;
  }

  void runAction(async () => {
    await updateSweepProfile(featureId, inputs.profileId);
    await updateSweepPath(featureId, inputs.pathEntityId);
  });
}
