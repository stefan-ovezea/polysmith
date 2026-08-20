import type {
  ActiveEdgeOpAction,
  ActiveExtrudeAction,
  ActiveLoftAction,
  ActiveMoveAction,
  ActiveRevolveAction,
  ActiveSweepAction,
  AnglePlaneAction,
  FastenerAction,
  HelixAction,
  HoleAction,
  MidplaneAction,
  OffsetPlaneAction,
  PendingReferenceAction,
  ShellAction,
  ThreadAction,
} from "./appState";
import type { SketchTextAction } from "./sketchToolLifecycleEffects";

export interface ActiveToolActions {
  extrudeAction: ActiveExtrudeAction | null;
  loftAction: ActiveLoftAction | null;
  revolveAction: ActiveRevolveAction | null;
  sweepAction: ActiveSweepAction | null;
  moveAction: ActiveMoveAction | null;
  edgeOpAction: ActiveEdgeOpAction | null;
  shellAction: ShellAction | null;
  holeAction: HoleAction | null;
  offsetPlaneAction: OffsetPlaneAction | null;
  anglePlaneAction: AnglePlaneAction | null;
  midplaneAction: MidplaneAction | null;
  tangentPlaneAction: PendingReferenceAction | null;
  constructionAxisAction: PendingReferenceAction | null;
  constructionPointAction: PendingReferenceAction | null;
  threadAction: ThreadAction | null;
  fastenerAction: FastenerAction | null;
  helixAction: HelixAction | null;
  pluginAction: { featureId: string } | null;
  editingFeatureId: string | null;
  materialsPanelOpen: boolean;
  sketchTextAction: SketchTextAction | null;
}
