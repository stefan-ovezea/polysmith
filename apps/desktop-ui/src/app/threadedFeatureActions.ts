import type {
  FastenerFeatureParameters,
  HelixFeatureParameters,
  ThreadFeatureParameters,
} from "../types";
import {
  DEFAULT_HELIX_HEIGHT,
  DEFAULT_HELIX_PITCH,
  DEFAULT_HELIX_RADIUS,
  DEFAULT_THREAD_LENGTH,
  DEFAULT_THREAD_MAJOR_DIAMETER,
  DEFAULT_THREAD_MINOR_DIAMETER,
  DEFAULT_THREAD_PITCH,
  type FastenerAction,
  type HelixAction,
  type ThreadAction,
} from "./appState";
import {
  type AppToolState,
  isToolStartBlocked,
} from "./actionAvailability";
import { awaitCreatedFeatureOfKind } from "./featureCreation";
import { defaultFastenerParameters } from "./fastenerDefaults";
import * as selectionSources from "./selectionSources";

type AxisSourceContext = Parameters<
  typeof selectionSources.currentAxisSourceId
>[0];
type ThreadTargetContext = Parameters<
  typeof selectionSources.currentThreadTargetBody
>[0];

interface ThreadedFeatureActionContext {
  activeToolState: AppToolState;
  axisSourceContext: AxisSourceContext;
  threadTargetContext: ThreadTargetContext;
  helixAction: HelixAction | null;
  threadAction: ThreadAction | null;
  fastenerAction: FastenerAction | null;
  activeHelixParameters: HelixFeatureParameters | null;
  activeThreadParameters: ThreadFeatureParameters | null;
  activeFastenerParameters: FastenerFeatureParameters | null;
  setHelixAction: (action: HelixAction | null) => void;
  setThreadAction: (action: ThreadAction | null) => void;
  setFastenerAction: (action: FastenerAction | null) => void;
  createHelix: (
    axisSourceId: string,
    parameters?: Partial<HelixFeatureParameters>,
  ) => Promise<void>;
  createThread: (parameters: ThreadFeatureParameters) => Promise<void>;
  createFastener: (parameters: FastenerFeatureParameters) => Promise<void>;
  updateHelixParameters: (
    featureId: string,
    parameters: HelixFeatureParameters,
  ) => Promise<void>;
  updateThreadParameters: (
    featureId: string,
    parameters: ThreadFeatureParameters,
  ) => Promise<void>;
  updateFastenerParameters: (
    featureId: string,
    parameters: FastenerFeatureParameters,
  ) => Promise<void>;
  runAction: (action: () => Promise<void>) => Promise<void>;
  addMessage: (message: string) => void;
}

function defaultThreadParameters(
  targetBodyId: string,
  axisSourceId: string,
): ThreadFeatureParameters {
  return {
    target_body_id: targetBodyId,
    axis_source_id: axisSourceId,
    mode: "external",
    standard: "custom",
    size: "",
    major_diameter: DEFAULT_THREAD_MAJOR_DIAMETER,
    minor_diameter: DEFAULT_THREAD_MINOR_DIAMETER,
    pitch: DEFAULT_THREAD_PITCH,
    length: DEFAULT_THREAD_LENGTH,
    thread_angle_degrees: 60,
    start_offset: 0,
    handedness: "right",
    representation: "cosmetic",
    is_pending: true,
  };
}

export function createThreadedFeatureActions({
  activeToolState,
  axisSourceContext,
  threadTargetContext,
  helixAction,
  threadAction,
  fastenerAction,
  activeHelixParameters,
  activeThreadParameters,
  activeFastenerParameters,
  setHelixAction,
  setThreadAction,
  setFastenerAction,
  createHelix,
  createThread,
  createFastener,
  updateHelixParameters,
  updateThreadParameters,
  updateFastenerParameters,
  runAction,
  addMessage,
}: ThreadedFeatureActionContext) {
  async function createHelixFeature(
    axisSourceId: string,
    parameters: Partial<HelixFeatureParameters> = {},
  ) {
    const documentPromise = awaitCreatedFeatureOfKind("helix");

    await runAction(async () => {
      try {
        await createHelix(axisSourceId, {
          radius: DEFAULT_HELIX_RADIUS,
          pitch: DEFAULT_HELIX_PITCH,
          height: DEFAULT_HELIX_HEIGHT,
          handedness: "right",
          start_angle_degrees: 0,
          ...parameters,
        });
        const { featureId: newFeatureId } = await documentPromise;
        setHelixAction({ phase: "active", featureId: newFeatureId });
      } catch (error) {
        addMessage(`helix error: ${String(error)}`);
      }
    });
  }

  async function triggerHelixAction() {
    if (isToolStartBlocked(activeToolState)) {
      return;
    }
    const sourceId = selectionSources.currentAxisSourceId(axisSourceContext);
    if (sourceId) {
      await createHelixFeature(sourceId);
      return;
    }
    setHelixAction({ phase: "pending" });
  }

  async function createThreadFeature(
    targetBodyId: string,
    axisSourceId: string,
    parameters: Partial<ThreadFeatureParameters> = {},
  ) {
    const documentPromise = awaitCreatedFeatureOfKind("thread");

    await runAction(async () => {
      try {
        await createThread({
          ...defaultThreadParameters(targetBodyId, axisSourceId),
          ...parameters,
        });
        const { featureId: newFeatureId } = await documentPromise;
        setThreadAction({
          phase: "active",
          featureId: newFeatureId,
          originalParameters: null,
        });
      } catch (error) {
        addMessage(`thread error: ${String(error)}`);
      }
    });
  }

  async function triggerThreadAction() {
    if (isToolStartBlocked(activeToolState)) {
      return;
    }
    const target = selectionSources.currentThreadTargetBody(threadTargetContext);
    const axisSourceId =
      selectionSources.currentAxisSourceId(axisSourceContext);
    if (target && axisSourceId) {
      await createThreadFeature(target.bodyId, axisSourceId);
      return;
    }
    if (target) {
      setThreadAction({
        phase: "pick_axis",
        targetBodyId: target.bodyId,
        targetSummary: target.summary,
      });
      return;
    }
    setThreadAction({ phase: "pick_target", axisSourceId });
  }

  async function triggerFastenerAction() {
    if (isToolStartBlocked(activeToolState)) {
      return;
    }

    const documentPromise = awaitCreatedFeatureOfKind("fastener");

    await runAction(async () => {
      try {
        await createFastener(defaultFastenerParameters());
        const { featureId: newFeatureId } = await documentPromise;
        setFastenerAction({
          featureId: newFeatureId,
          originalParameters: null,
        });
      } catch (error) {
        addMessage(`fastener error: ${String(error)}`);
      }
    });
  }

  async function updateActiveFastenerParameters(
    patch: Partial<FastenerFeatureParameters>,
  ) {
    if (!fastenerAction || !activeFastenerParameters) {
      return;
    }
    await runAction(async () => {
      await updateFastenerParameters(fastenerAction.featureId, {
        ...activeFastenerParameters,
        ...patch,
      });
    });
  }

  async function updateActiveThreadParameters(
    patch: Partial<ThreadFeatureParameters>,
  ) {
    if (threadAction?.phase !== "active" || !activeThreadParameters) {
      return;
    }
    await runAction(async () => {
      await updateThreadParameters(threadAction.featureId, {
        ...activeThreadParameters,
        ...patch,
      });
    });
  }

  async function updateActiveHelixParameters(
    patch: Partial<HelixFeatureParameters>,
  ) {
    if (helixAction?.phase !== "active" || !activeHelixParameters) {
      return;
    }
    await runAction(async () => {
      await updateHelixParameters(helixAction.featureId, {
        ...activeHelixParameters,
        ...patch,
      });
    });
  }

  return {
    createHelixFeature,
    createThreadFeature,
    triggerFastenerAction,
    triggerHelixAction,
    triggerThreadAction,
    updateActiveFastenerParameters,
    updateActiveHelixParameters,
    updateActiveThreadParameters,
  };
}
