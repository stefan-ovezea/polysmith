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

export interface AppToolState {
  activeSketchPlaneId: string | null;
  extrudeAction: ActiveExtrudeAction | null;
  loftAction: ActiveLoftAction | null;
  revolveAction: ActiveRevolveAction | null;
  sweepAction: ActiveSweepAction | null;
  edgeOpAction: ActiveEdgeOpAction | null;
  shellAction: ShellAction | null;
  holeAction: HoleAction | null;
  offsetPlaneAction: OffsetPlaneAction | null;
  midplaneAction: MidplaneAction | null;
  tangentPlaneAction: PendingReferenceAction | null;
  anglePlaneAction: AnglePlaneAction | null;
  constructionAxisAction: PendingReferenceAction | null;
  constructionPointAction: PendingReferenceAction | null;
  helixAction: HelixAction | null;
  threadAction: ThreadAction | null;
  fastenerAction: FastenerAction | null;
  moveAction: ActiveMoveAction | null;
}

export interface ToolBlockOptions {
  activeSketchPlane?: boolean;
  extrude?: boolean;
  loft?: boolean;
  revolve?: boolean;
  sweep?: boolean;
  edgeOp?: boolean;
  shell?: boolean;
  hole?: boolean;
  offsetPlane?: boolean;
  midplane?: boolean;
  tangentPlane?: boolean;
  anglePlane?: boolean;
  constructionAxis?: boolean;
  constructionPoint?: boolean;
  helix?: boolean;
  thread?: boolean;
  fastener?: boolean;
  move?: boolean;
}

const DEFAULT_TOOL_BLOCKS: Required<ToolBlockOptions> = {
  activeSketchPlane: true,
  extrude: true,
  loft: true,
  revolve: true,
  sweep: true,
  edgeOp: true,
  shell: true,
  hole: true,
  offsetPlane: true,
  midplane: true,
  tangentPlane: true,
  anglePlane: true,
  constructionAxis: true,
  constructionPoint: true,
  helix: true,
  thread: true,
  fastener: true,
  move: true,
};

type ToolBlockKey = keyof Required<ToolBlockOptions>;

const TOOL_BLOCK_CHECKS: Array<{
  key: ToolBlockKey;
  read: (state: AppToolState) => unknown;
}> = [
  { key: "activeSketchPlane", read: (state) => state.activeSketchPlaneId },
  { key: "extrude", read: (state) => state.extrudeAction },
  { key: "loft", read: (state) => state.loftAction },
  { key: "revolve", read: (state) => state.revolveAction },
  { key: "sweep", read: (state) => state.sweepAction },
  { key: "edgeOp", read: (state) => state.edgeOpAction },
  { key: "shell", read: (state) => state.shellAction },
  { key: "hole", read: (state) => state.holeAction },
  { key: "offsetPlane", read: (state) => state.offsetPlaneAction },
  { key: "midplane", read: (state) => state.midplaneAction },
  { key: "tangentPlane", read: (state) => state.tangentPlaneAction },
  { key: "anglePlane", read: (state) => state.anglePlaneAction },
  { key: "constructionAxis", read: (state) => state.constructionAxisAction },
  { key: "constructionPoint", read: (state) => state.constructionPointAction },
  { key: "helix", read: (state) => state.helixAction },
  { key: "thread", read: (state) => state.threadAction },
  { key: "fastener", read: (state) => state.fastenerAction },
  { key: "move", read: (state) => state.moveAction },
];

export function isToolStartBlocked(
  state: AppToolState,
  options: ToolBlockOptions = {},
) {
  const blocks = { ...DEFAULT_TOOL_BLOCKS, ...options };
  return TOOL_BLOCK_CHECKS.some(
    ({ key, read }) => blocks[key] && Boolean(read(state)),
  );
}
