import type {
  ActiveExtrudeAction,
  ActiveLoftAction,
  ActiveMoveAction,
  ActiveRevolveAction,
  ActiveSweepAction,
  FastenerAction,
  ThreadAction,
} from "./appState";
import type { DocumentState, ViewportState } from "../types/ipc";

type FeatureEntry = DocumentState["feature_history"][number];

interface MutableRef<T> {
  current: T;
}

type AsyncVoid = () => Promise<void>;
type RunAction = (action: AsyncVoid) => Promise<void>;

interface TimelineFeatureEditContext {
  document: DocumentState | null;
  viewport: ViewportState | null;
  featureId: string;
  canStartTimelineFeatureEdit: boolean;
  beginTimelineEditSession: (featureId: string, featureKind: string) => void;
  runAction: RunAction;
  reenterSketch: (featureId: string) => Promise<void>;
  setEditingFeatureId: (featureId: string | null) => void;
  setExtrudeAction: (action: ActiveExtrudeAction | null) => void;
  setLoftAction: (action: ActiveLoftAction | null) => void;
  setRevolveAction: (action: ActiveRevolveAction | null) => void;
  setSweepAction: (action: ActiveSweepAction | null) => void;
  setThreadAction: (action: ThreadAction | null) => void;
  setFastenerAction: (action: FastenerAction | null) => void;
  setMoveAction: (action: ActiveMoveAction | null) => void;
  lastLoftProfileUpdateRef: MutableRef<string>;
  lastRevolveInputsRef: MutableRef<string>;
  lastSweepInputsRef: MutableRef<string>;
}

export function editTimelineFeature(context: TimelineFeatureEditContext) {
  const {
    document,
    featureId,
    canStartTimelineFeatureEdit,
    beginTimelineEditSession,
    runAction,
    reenterSketch,
    setEditingFeatureId,
  } = context;

  const feature = document?.feature_history.find(
    (entry) => entry.feature_id === featureId,
  );
  if (!feature) {
    return;
  }

  if (feature.kind === "box" || feature.kind === "cylinder") {
    beginTimelineEditSession(featureId, feature.kind);
    setEditingFeatureId(featureId);
    return;
  }

  if (feature.kind === "sketch") {
    beginTimelineEditSession(featureId, feature.kind);
    void runAction(async () => {
      await reenterSketch(featureId);
    });
    return;
  }

  if (!canStartTimelineFeatureEdit) {
    return;
  }

  switch (feature.kind) {
    case "extrude":
      editExtrudeFeature(context, feature);
      break;
    case "loft":
      editLoftFeature(context, feature);
      break;
    case "revolve":
      editRevolveFeature(context, feature);
      break;
    case "sweep":
      editSweepFeature(context, feature);
      break;
    case "thread":
      editThreadFeature(context, feature);
      break;
    case "fastener":
      editFastenerFeature(context, feature);
      break;
    case "move":
      editMoveFeature(context, feature);
      break;
  }
}

export function createTimelineFeatureEditHandler(
  context: Omit<TimelineFeatureEditContext, "featureId">,
) {
  return (featureId: string) => {
    editTimelineFeature({ ...context, featureId });
  };
}

function editExtrudeFeature(
  { viewport, featureId, beginTimelineEditSession, setExtrudeAction }: TimelineFeatureEditContext,
  feature: FeatureEntry,
) {
  const params = feature.extrude_parameters;
  if (!params) {
    return;
  }

  const otherBodies = (viewport?.bodies ?? []).filter(
    (body) => body.id !== featureId,
  );
  beginTimelineEditSession(featureId, feature.kind);
  setExtrudeAction({
    phase: "active",
    featureId,
    featureIds: [featureId],
    profileIds: [...(params.profile_ids ?? [])],
    automaticMode: false,
    initialDepth: params.depth,
    initialMode: params.mode,
    initialParameters: params,
    initialTargetBodyId: params.target_body_id ?? null,
    profileCount: params.profile_ids?.length || 1,
    canCombineWithExistingBody: otherBodies.length > 0,
    originalSnapshot: {
      depth: params.depth,
      mode: params.mode,
      targetBodyId: params.target_body_id ?? null,
      parameters: params,
    },
  });
}

function editLoftFeature(
  {
    featureId,
    beginTimelineEditSession,
    setLoftAction,
    lastLoftProfileUpdateRef,
  }: TimelineFeatureEditContext,
  feature: FeatureEntry,
) {
  const params = feature.loft_parameters;
  if (!params) {
    return;
  }

  const profileIds = params.sections.map((section) => section.profile_id);
  beginTimelineEditSession(featureId, feature.kind);
  lastLoftProfileUpdateRef.current = profileIds.join("|");
  setLoftAction({
    phase: "active",
    featureId,
    initialRuled: params.ruled,
    profileIds,
    originalSnapshot: {
      profileIds,
      ruled: params.ruled,
    },
  });
}

function editRevolveFeature(
  {
    featureId,
    beginTimelineEditSession,
    setRevolveAction,
    lastRevolveInputsRef,
  }: TimelineFeatureEditContext,
  feature: FeatureEntry,
) {
  const params = feature.revolve_parameters;
  if (!params) {
    return;
  }

  beginTimelineEditSession(featureId, feature.kind);
  lastRevolveInputsRef.current = `${params.profile_id}|${params.axis_entity_id}`;
  setRevolveAction({
    phase: "active",
    featureId,
    profileId: params.profile_id,
    axisEntityId: params.axis_entity_id,
    initialAngle: params.angle_degrees,
    originalSnapshot: {
      profileId: params.profile_id,
      axisEntityId: params.axis_entity_id,
      angleDegrees: params.angle_degrees,
    },
  });
}

function editSweepFeature(
  {
    featureId,
    beginTimelineEditSession,
    setSweepAction,
    lastSweepInputsRef,
  }: TimelineFeatureEditContext,
  feature: FeatureEntry,
) {
  const params = feature.sweep_parameters;
  if (!params) {
    return;
  }

  beginTimelineEditSession(featureId, feature.kind);
  lastSweepInputsRef.current = `${params.profile_id}|${params.path_entity_id}`;
  setSweepAction({
    phase: "active",
    featureId,
    profileId: params.profile_id,
    pathEntityId: params.path_entity_id,
    originalSnapshot: {
      profileId: params.profile_id,
      pathEntityId: params.path_entity_id,
    },
  });
}

function editThreadFeature(
  { featureId, beginTimelineEditSession, setThreadAction }: TimelineFeatureEditContext,
  feature: FeatureEntry,
) {
  if (!feature.thread_parameters) {
    return;
  }

  beginTimelineEditSession(featureId, feature.kind);
  setThreadAction({
    phase: "active",
    featureId,
    originalParameters: feature.thread_parameters,
  });
}

function editFastenerFeature(
  { featureId, beginTimelineEditSession, setFastenerAction }: TimelineFeatureEditContext,
  feature: FeatureEntry,
) {
  if (!feature.fastener_parameters) {
    return;
  }

  beginTimelineEditSession(featureId, feature.kind);
  setFastenerAction({
    featureId,
    originalParameters: feature.fastener_parameters,
  });
}

function editMoveFeature(
  { featureId, beginTimelineEditSession, setMoveAction }: TimelineFeatureEditContext,
  feature: FeatureEntry,
) {
  if (!feature.move_parameters) {
    return;
  }

  beginTimelineEditSession(featureId, feature.kind);
  setMoveAction({
    phase: "active",
    featureId,
    targetBodyId: feature.move_parameters.target_body_id,
    parameters: feature.move_parameters,
    originalSnapshot: feature.move_parameters,
  });
}
