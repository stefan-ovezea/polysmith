import type {
  ExtrudeFeatureParameters,
  ExtrudeMode,
  FastenerFeatureParameters,
  MoveFeatureParameters,
  ThreadFeatureParameters,
} from "../types";
import type { DocumentState } from "../types/ipc";
import type { RecentProjectsDocument, SlicerViewportBounds } from "../lib";

export const DEFAULT_EXTRUDE_DEPTH = 20;
export const DEFAULT_FILLET_RADIUS = 1;
export const DEFAULT_CHAMFER_DISTANCE = 1;
export const DEFAULT_SHELL_THICKNESS = 2;
export const DEFAULT_HOLE_DIAMETER = 5;
export const DEFAULT_HOLE_DEPTH = 10;
export const DEFAULT_HELIX_RADIUS = 2.5;
export const DEFAULT_HELIX_PITCH = 1;
export const DEFAULT_HELIX_HEIGHT = 10;
export const DEFAULT_THREAD_MAJOR_DIAMETER = 5;
export const DEFAULT_THREAD_MINOR_DIAMETER = 4;
export const DEFAULT_THREAD_PITCH = 0.8;
export const DEFAULT_THREAD_LENGTH = 10;
export const DEFAULT_FASTENER_SIZE = "M5";
export const DEFAULT_FASTENER_DIAMETER = 5;
export const DEFAULT_FASTENER_LENGTH = 20;
export const DEFAULT_FASTENER_THREAD_LENGTH = 16;

// Default seed for the Offset Plane panel. Zero would be a valid
// frame (sitting on top of the source) but gives no visible preview;
// 10 mm matches common CAD workflow's "show me something" default.
export const DEFAULT_OFFSET_PLANE_DISTANCE = 10;
export const DEFAULT_ANGLE_PLANE_DEGREES = 45;

export function defaultMoveParameters(
  targetBodyId = "",
): MoveFeatureParameters {
  return {
    target_body_id: targetBodyId,
    translation_x: 0,
    translation_y: 0,
    translation_z: 0,
    rotation_x_degrees: 0,
    rotation_y_degrees: 0,
    rotation_z_degrees: 0,
    is_pending: true,
  };
}

// The Core Messages debug panel is hidden by default. Set
// `VITE_SHOW_DEBUG_MESSAGE_LOG=true` in `.env.local` (or your shell when
// running `pnpm dev`) to surface it again while debugging the IPC bridge.
export const SHOW_DEBUG_MESSAGE_LOG =
  import.meta.env.VITE_SHOW_DEBUG_MESSAGE_LOG === "true";

export type WorkspaceView = "cad" | "slicer" | "cam" | "drawing";
export type SidebarTab = "hierarchy" | "projects";
export type PendingUnsavedAction =
  | { kind: "quit" }
  | { kind: "new" }
  | { kind: "newProject"; parentFolderId: string | null }
  | { kind: "load"; filePath: string };

export interface SavedDocumentBaseline {
  documentId: string;
  revision: number;
}

export const EMPTY_RECENT_PROJECTS_DOCUMENT: RecentProjectsDocument = {
  version: 3,
  rootFolderIds: [],
  rootProjectPaths: [],
  folders: [],
  projects: [],
};

export const IS_MACOS =
  typeof navigator !== "undefined" &&
  navigator.platform.toLowerCase().includes("mac");

export const STANDALONE_SLICER_BOUNDS: SlicerViewportBounds = {
  x: 0,
  y: 0,
  width: 1,
  height: 1,
  scaleFactor: 1,
};

export const BODY_KINDS = new Set([
  "box",
  "cylinder",
  "polygon_extrude",
  "extrude",
  "loft",
  "revolve",
  "sweep",
  "fastener",
  "body_copy",
]);

export function documentHasSolidBody(documentState: DocumentState | null) {
  return (documentState?.feature_history ?? []).some(
    (feature) =>
      BODY_KINDS.has(feature.kind) &&
      feature.suppressed !== true &&
      feature.status !== "warning" &&
      feature.dependency_broken !== true,
  );
}

export function defaultHiddenSketchIdsForLoadedDocument(
  documentState: DocumentState,
) {
  const next = new Set<string>();
  if (!documentHasSolidBody(documentState)) {
    return next;
  }
  for (const feature of documentState.feature_history) {
    if (feature.kind === "sketch") {
      next.add(feature.feature_id);
    }
  }
  return next;
}

type FeatureHistoryEntry = DocumentState["feature_history"][number];

export interface ActiveExtrudeAction {
  phase: "pending" | "active";
  featureId: string | null;
  featureIds: string[];
  profileIds: string[];
  automaticMode: boolean;
  initialDepth: number;
  initialMode: ExtrudeMode;
  initialParameters: ExtrudeFeatureParameters | null;
  initialTargetBodyId: string | null;
  profileCount: number;
  // Snapshot of "did the document have any other solid bodies before the
  // user invoked this extrude?" - drives whether Join/Cut are offered.
  canCombineWithExistingBody: boolean;
  // Set when the panel was opened to *edit* an existing extrude (via
  // double-click in the timeline) rather than to dial in a freshly-
  // created one. On cancel we restore these values instead of calling
  // `undo`, which would clobber whatever the user did *after* the
  // extrude was originally created.
  originalSnapshot: {
    depth: number;
    mode: ExtrudeMode;
    targetBodyId: string | null;
    parameters: ExtrudeFeatureParameters;
  } | null;
}

export function canCombineExtrudeWithExistingBody(
  documentState: DocumentState | null,
) {
  return (documentState?.feature_history ?? []).some(
    (entry) =>
      entry.kind === "box" ||
      entry.kind === "cylinder" ||
      entry.kind === "extrude",
  );
}

export function activeExtrudeActionFromCreatedFeature({
  createdFeature,
  featureId,
  depth,
  mode,
  targetBodyId,
  profileCount,
  canCombineWithExistingBody,
}: {
  createdFeature: FeatureHistoryEntry | null | undefined;
  featureId: string;
  depth: number;
  mode: ExtrudeMode | null;
  targetBodyId: string | null;
  profileCount: number;
  canCombineWithExistingBody: boolean;
}): ActiveExtrudeAction {
  const createdParams = createdFeature?.extrude_parameters;
  return {
    phase: "active",
    featureId,
    featureIds: [featureId],
    profileIds: [],
    automaticMode: false,
    initialDepth: depth,
    initialMode: createdParams?.mode ?? mode ?? "new_body",
    initialParameters: createdParams ?? null,
    initialTargetBodyId: createdParams?.target_body_id ?? targetBodyId ?? null,
    profileCount,
    originalSnapshot: null,
    canCombineWithExistingBody,
  };
}

export interface ActiveLoftAction {
  phase: "pending" | "active";
  featureId: string | null;
  initialRuled: boolean;
  profileIds: string[];
  originalSnapshot: {
    profileIds: string[];
    ruled: boolean;
  } | null;
}

export interface ActiveRevolveAction {
  phase: "pending" | "active";
  featureId: string | null;
  profileId: string | null;
  axisEntityId: string | null;
  initialAngle: number;
  originalSnapshot: {
    profileId: string;
    axisEntityId: string;
    angleDegrees: number;
  } | null;
}

export interface ActiveSweepAction {
  phase: "pending" | "active";
  featureId: string | null;
  profileId: string | null;
  pathEntityId: string | null;
  originalSnapshot: {
    profileId: string;
    pathEntityId: string;
  } | null;
}

export type ActiveMoveAction =
  | { phase: "pending"; parameters: MoveFeatureParameters }
  | {
      phase: "active";
      featureId: string;
      targetBodyId: string;
      parameters: MoveFeatureParameters;
      originalSnapshot: MoveFeatureParameters | null;
      createdCopyFeatureId?: string | null;
    };

export type ShellAction =
  | { phase: "pending"; initialThickness: number }
  | {
      phase: "active";
      featureId: string;
      faceId: string;
      faceSummary: string;
      initialThickness: number;
    };

export type OffsetPlaneAction =
  | { phase: "pending"; initialOffset: number }
  | {
      phase: "active";
      featureId: string;
      initialOffset: number;
      sourceSummary: string;
    };

export type MidplaneAction = {
  sourceIds: string[];
};

export type PendingReferenceAction = {
  isPending: true;
};

export type AnglePlaneAction =
  | {
      phase: "pick_plane";
      initialAngle: number;
    }
  | {
      phase: "pick_axis";
      sourcePlaneId: string;
      sourceSummary: string;
      initialAngle: number;
    }
  | {
      phase: "active";
      featureId: string;
      sourcePlaneId: string;
      sourceSummary: string;
      axisId: string;
      axisSummary: string;
      initialAngle: number;
    };

export type HelixAction =
  | { phase: "pending" }
  | { phase: "active"; featureId: string };

export type ThreadAction =
  | { phase: "pick_target"; axisSourceId: string | null }
  | { phase: "pick_axis"; targetBodyId: string; targetSummary: string }
  | {
      phase: "active";
      featureId: string;
      originalParameters: ThreadFeatureParameters | null;
    };

export type FastenerAction = {
  featureId: string;
  originalParameters: FastenerFeatureParameters | null;
};

export type HoleAction =
  | { phase: "pending" }
  | { phase: "active"; featureId: string };

export interface SketchDeleteSelection {
  entityIds: string[];
  vertexIds: string[];
  profileIds: string[];
}

export function bodyIdFromFaceId(faceId: string | null | undefined) {
  if (!faceId) {
    return null;
  }
  const marker = ":face:";
  const markerIndex = faceId.indexOf(marker);
  if (markerIndex <= 0) {
    return null;
  }
  return faceId.slice(0, markerIndex);
}

// In-progress fillet or chamfer feature. Two-phase contextual modeling flow:
//
//   - phase "pending": panel is open but no feature exists yet. The
//     user opens this by invoking Fillet / Chamfer with no edges
//     selected. They can either type a value first or click an edge
//     first; whichever comes first, the other is honored when the
//     feature is created on the first edge click.
//
//   - phase "active": the core created the feature on the first edge
//     pick. The panel now drives live `update_*_radius` /
//     `update_*_distance` and edge clicks toggle membership through
//     `update_*_edges`. We mirror the edge list locally as the
//     authoritative source while editing - relying on the document
//     round-trip for it caused dropped edges under rapid clicking,
//     because each click read a stale snapshot of `selected_edge_ids`.
export type ActiveEdgeOpAction =
  | {
      phase: "pending";
      kind: "fillet" | "chamfer";
      // Seed value displayed in the panel; the *current* typed value
      // lives in `pendingValueRef` so that an edge click placed
      // mid-typing still uses the latest input.
      initialValue: number;
    }
  | {
      phase: "active";
      kind: "fillet" | "chamfer";
      featureId: string;
      initialValue: number;
      edgeIds: string[];
    };
