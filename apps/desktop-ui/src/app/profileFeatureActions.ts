import { useEffect } from "react";
import type { Dispatch, MutableRefObject, SetStateAction } from "react";

import { awaitCreatedFeatureOfKind } from "./featureCreation";
import type {
  ActiveLoftAction,
  ActiveRevolveAction,
  ActiveSweepAction,
} from "./appState";
import {
  syncRevolveActionInputs,
  syncSweepActionInputs,
} from "./profileFeatureSync";

type RunAction = (action: () => Promise<void>) => Promise<void>;

interface ProfileFeatureActionsContext {
  selectedSketchProfileIds: readonly string[];
  selectedSweepPathEntityId: string | null;
  loftAction: ActiveLoftAction | null;
  revolveAction: ActiveRevolveAction | null;
  sweepAction: ActiveSweepAction | null;
  lastLoftProfileUpdateRef: MutableRefObject<string>;
  lastRevolveInputsRef: MutableRefObject<string>;
  lastSweepInputsRef: MutableRefObject<string>;
  loftCreateInFlightRef: MutableRefObject<boolean>;
  revolveCreateInFlightRef: MutableRefObject<boolean>;
  sweepCreateInFlightRef: MutableRefObject<boolean>;
  setLoftAction: Dispatch<SetStateAction<ActiveLoftAction | null>>;
  setRevolveAction: Dispatch<SetStateAction<ActiveRevolveAction | null>>;
  setSweepAction: Dispatch<SetStateAction<ActiveSweepAction | null>>;
  loftProfiles: (
    profileIds: readonly string[],
    ruled: boolean,
  ) => Promise<void>;
  updateLoftProfiles: (
    featureId: string,
    profileIds: readonly string[],
  ) => Promise<void>;
  revolveProfile: (
    profileId: string,
    axisEntityId: string,
    angleDegrees?: number,
  ) => Promise<void>;
  updateRevolveProfile: (
    featureId: string,
    profileId: string,
  ) => Promise<void>;
  updateRevolveAxis: (
    featureId: string,
    axisEntityId: string,
  ) => Promise<void>;
  sweepProfile: (profileId: string, pathEntityId: string) => Promise<void>;
  updateSweepProfile: (featureId: string, profileId: string) => Promise<void>;
  updateSweepPath: (featureId: string, pathEntityId: string) => Promise<void>;
  runAction: RunAction;
  addMessage: (message: string) => void;
}

export function useProfileFeatureActions({
  selectedSketchProfileIds,
  selectedSweepPathEntityId,
  loftAction,
  revolveAction,
  sweepAction,
  lastLoftProfileUpdateRef,
  lastRevolveInputsRef,
  lastSweepInputsRef,
  loftCreateInFlightRef,
  revolveCreateInFlightRef,
  sweepCreateInFlightRef,
  setLoftAction,
  setRevolveAction,
  setSweepAction,
  loftProfiles,
  updateLoftProfiles,
  revolveProfile,
  updateRevolveProfile,
  updateRevolveAxis,
  sweepProfile,
  updateSweepProfile,
  updateSweepPath,
  runAction,
  addMessage,
}: ProfileFeatureActionsContext) {
  async function createLoftFromProfiles(
    profileIds: readonly string[],
    ruled: boolean,
  ) {
    if (profileIds.length < 2 || loftCreateInFlightRef.current) {
      return;
    }
    loftCreateInFlightRef.current = true;

    const documentPromise = awaitCreatedFeatureOfKind("loft");

    await runAction(async () => {
      try {
        await loftProfiles(profileIds, ruled);
        const { feature: createdFeature, featureId: newFeatureId } =
          await documentPromise;
        lastLoftProfileUpdateRef.current = profileIds.join("|");
        setLoftAction({
          phase: "active",
          featureId: newFeatureId,
          initialRuled: createdFeature?.loft_parameters?.ruled ?? ruled,
          profileIds: [...profileIds],
          originalSnapshot: null,
        });
      } catch (error) {
        addMessage(`loft action error: ${String(error)}`);
      } finally {
        loftCreateInFlightRef.current = false;
      }
    });
  }

  async function triggerLoftAction() {
    if (loftAction) {
      return;
    }
    const profileIds = [...selectedSketchProfileIds];
    lastLoftProfileUpdateRef.current =
      profileIds.length >= 2 ? "" : profileIds.join("|");
    setLoftAction({
      phase: "pending",
      featureId: null,
      initialRuled: false,
      profileIds,
      originalSnapshot: null,
    });
  }

  useEffect(() => {
    if (!loftAction || loftAction.profileIds.length < 2) {
      return;
    }
    const profileKey = loftAction.profileIds.join("|");
    if (lastLoftProfileUpdateRef.current === profileKey) {
      return;
    }

    lastLoftProfileUpdateRef.current = profileKey;
    if (loftAction.phase === "pending") {
      void createLoftFromProfiles(loftAction.profileIds, loftAction.initialRuled);
      return;
    }
    if (!loftAction.featureId) {
      return;
    }
    void runAction(async () => {
      await updateLoftProfiles(loftAction.featureId!, loftAction.profileIds);
    });
  }, [loftAction, updateLoftProfiles]);

  async function createRevolveFromInputs(
    profileId: string,
    axisEntityId: string,
    angleDegrees: number,
  ) {
    if (revolveCreateInFlightRef.current) {
      return;
    }
    revolveCreateInFlightRef.current = true;

    const documentPromise = awaitCreatedFeatureOfKind("revolve");

    await runAction(async () => {
      try {
        await revolveProfile(profileId, axisEntityId, angleDegrees);
        const { feature: createdFeature, featureId: newFeatureId } =
          await documentPromise;
        lastRevolveInputsRef.current = `${profileId}|${axisEntityId}`;
        setRevolveAction({
          phase: "active",
          featureId: newFeatureId,
          profileId,
          axisEntityId,
          initialAngle:
            createdFeature?.revolve_parameters?.angle_degrees ?? angleDegrees,
          originalSnapshot: null,
        });
      } catch (error) {
        addMessage(`revolve action error: ${String(error)}`);
      } finally {
        revolveCreateInFlightRef.current = false;
      }
    });
  }

  async function triggerRevolveAction() {
    if (revolveAction) {
      return;
    }
    const profileId = selectedSketchProfileIds[0] ?? null;
    lastRevolveInputsRef.current = "";
    setRevolveAction({
      phase: "pending",
      featureId: null,
      profileId,
      axisEntityId: null,
      initialAngle: 360,
      originalSnapshot: null,
    });
  }

  useEffect(() => {
    syncRevolveActionInputs({
      revolveAction,
      lastRevolveInputsRef,
      createRevolveFromInputs,
      runAction,
      updateRevolveProfile,
      updateRevolveAxis,
    });
  }, [revolveAction, updateRevolveAxis, updateRevolveProfile]);

  async function createSweepFromInputs(
    profileId: string,
    pathEntityId: string,
  ) {
    if (sweepCreateInFlightRef.current) {
      return;
    }
    sweepCreateInFlightRef.current = true;

    const documentPromise = awaitCreatedFeatureOfKind("sweep");

    await runAction(async () => {
      try {
        await sweepProfile(profileId, pathEntityId);
        const { featureId: newFeatureId } = await documentPromise;
        lastSweepInputsRef.current = `${profileId}|${pathEntityId}`;
        setSweepAction({
          phase: "active",
          featureId: newFeatureId,
          profileId,
          pathEntityId,
          originalSnapshot: null,
        });
      } catch (error) {
        addMessage(`sweep action error: ${String(error)}`);
      } finally {
        sweepCreateInFlightRef.current = false;
      }
    });
  }

  async function triggerSweepAction() {
    if (sweepAction) {
      return;
    }
    const profileId = selectedSketchProfileIds[0] ?? null;
    lastSweepInputsRef.current = "";
    setSweepAction({
      phase: "pending",
      featureId: null,
      profileId,
      pathEntityId: selectedSweepPathEntityId,
      originalSnapshot: null,
    });
  }

  useEffect(() => {
    syncSweepActionInputs({
      sweepAction,
      lastSweepInputsRef,
      createSweepFromInputs,
      runAction,
      updateSweepProfile,
      updateSweepPath,
    });
  }, [sweepAction, updateSweepPath, updateSweepProfile]);

  return {
    triggerLoftAction,
    triggerRevolveAction,
    triggerSweepAction,
  };
}
