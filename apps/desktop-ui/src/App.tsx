import { useEffect, useMemo, useRef, useState } from "react";
import { open, save } from "@tauri-apps/plugin-dialog";
import { openUrl } from "@tauri-apps/plugin-opener";
import { getCurrentWindow } from "@tauri-apps/api/window";
import { convertFileSrc } from "@tauri-apps/api/core";
import { useTranslation } from "react-i18next";
import {
  awaitDocumentChange,
  awaitDocumentExport,
  awaitDocumentSaved,
  useCadCoreStore,
} from "./state";
import { useCadCore } from "./hooks";
import {
  createProjectFolder,
  deleteProjectFolder,
  deleteProjectFile,
  findDependents,
  loadRecentProjects,
  matchesHotkey,
  moveProjectToFolder,
  projectFileExists,
  projectNameFromPath,
  readProjectThumbnail,
  removeProjectFromRecentProjects,
  renameProjectFolder,
  renameRecentProject,
  saveRecentProjects,
  upsertRecentProject,
  useAppConfig,
  writeProjectThumbnail,
  Checkbox,
  Dropdown,
  applyHoleStandard,
  findHoleStandard,
  holeStandardsForMode,
} from "./lib";
import {
  embedOrcaWindow,
  hideOrcaWindow,
  prepareOrcaExportPath,
  resizeOrcaWindow,
  setOrcaMapped,
  type SlicerViewportBounds,
} from "./lib";
import {
  AppHeader,
  AiAssistantPanel,
  BoxFeatureForm,
  CylinderFeatureForm,
  DocumentHierarchyPanel,
  EdgeOpPreviewPanel,
  ShellPreviewPanel,
  ExtrudePreviewPanel,
  AnglePlanePanel,
  LoftPreviewPanel,
  RevolvePreviewPanel,
  SweepPreviewPanel,
  OffsetPlanePanel,
  SketchFilletPanel,
  MirrorToolPanel,
  FeatureTimeline,
  LogsWindow,
  MaterialsPanel,
  MessageLog,
  MovePreviewPanel,
  ImportPreviewPanel,
  SettingsModal,
  ViewportPanel,
  ProjectsPanel,
} from "./layout";
import type { CategoryId } from "./layout";
import { ArmedSketchConstraint } from "./types";
import type {
  ExtrudeAdvancedParameters,
  ExtrudeFeatureParameters,
  ExtrudeMode,
  FastenerFeatureParameters,
  HelixFeatureParameters,
  HoleFit,
  HoleFeatureParameters,
  HoleStandard,
  MoveFeatureParameters,
  ImportPlacementPayload,
  PlaneFrame,
  ThreadFeatureParameters,
} from "./types";
import type { DocumentState } from "./types/ipc";
import type { RecentProject, RecentProjectsDocument } from "./lib";

const DEFAULT_EXTRUDE_DEPTH = 20;
const DEFAULT_FILLET_RADIUS = 1;
const DEFAULT_CHAMFER_DISTANCE = 1;
const DEFAULT_SHELL_THICKNESS = 2;
const DEFAULT_HOLE_DIAMETER = 5;
const DEFAULT_HOLE_DEPTH = 10;
const DEFAULT_HELIX_RADIUS = 2.5;
const DEFAULT_HELIX_PITCH = 1;
const DEFAULT_HELIX_HEIGHT = 10;
const DEFAULT_THREAD_MAJOR_DIAMETER = 5;
const DEFAULT_THREAD_MINOR_DIAMETER = 4;
const DEFAULT_THREAD_PITCH = 0.8;
const DEFAULT_THREAD_LENGTH = 10;
const DEFAULT_FASTENER_SIZE = "M5";
const DEFAULT_FASTENER_DIAMETER = 5;
const DEFAULT_FASTENER_LENGTH = 20;
const DEFAULT_FASTENER_THREAD_LENGTH = 16;
// Default seed for the Offset Plane panel. Zero would be a valid
// frame (sitting on top of the source) but gives no visible preview;
// 10 mm matches common CAD workflow's "show me something" default.
const DEFAULT_OFFSET_PLANE_DISTANCE = 10;
const DEFAULT_ANGLE_PLANE_DEGREES = 45;

function defaultMoveParameters(targetBodyId = ""): MoveFeatureParameters {
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
const SHOW_DEBUG_MESSAGE_LOG =
  import.meta.env.VITE_SHOW_DEBUG_MESSAGE_LOG === "true";

type WorkspaceView = "cad" | "slicer";
type SidebarTab = "hierarchy" | "projects";
type PendingUnsavedAction =
  | { kind: "quit" }
  | { kind: "new" }
  | { kind: "newProject"; parentFolderId: string | null }
  | { kind: "load"; filePath: string };
interface SavedDocumentBaseline {
  documentId: string;
  revision: number;
}
const EMPTY_RECENT_PROJECTS_DOCUMENT: RecentProjectsDocument = {
  version: 3,
  rootFolderIds: [],
  rootProjectPaths: [],
  folders: [],
  projects: [],
};
const IS_MACOS =
  typeof navigator !== "undefined" &&
  navigator.platform.toLowerCase().includes("mac");
const STANDALONE_SLICER_BOUNDS: SlicerViewportBounds = {
  x: 0,
  y: 0,
  width: 1,
  height: 1,
  scaleFactor: 1,
};
const BODY_KINDS = new Set([
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

function documentHasSolidBody(documentState: DocumentState | null) {
  return (documentState?.feature_history ?? []).some(
    (feature) =>
      BODY_KINDS.has(feature.kind) &&
      feature.suppressed !== true &&
      feature.status !== "warning" &&
      feature.dependency_broken !== true,
  );
}

function defaultHiddenSketchIdsForLoadedDocument(documentState: DocumentState) {
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

interface ActiveExtrudeAction {
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
  // user invoked this extrude?" — drives whether Join/Cut are offered.
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

interface ActiveLoftAction {
  phase: "pending" | "active";
  featureId: string | null;
  initialRuled: boolean;
  profileIds: string[];
  originalSnapshot: {
    profileIds: string[];
    ruled: boolean;
  } | null;
}

interface ActiveRevolveAction {
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

interface ActiveSweepAction {
  phase: "pending" | "active";
  featureId: string | null;
  profileId: string | null;
  pathEntityId: string | null;
  originalSnapshot: {
    profileId: string;
    pathEntityId: string;
  } | null;
}

type ActiveMoveAction =
  | { phase: "pending"; parameters: MoveFeatureParameters }
  | {
      phase: "active";
      featureId: string;
      targetBodyId: string;
      parameters: MoveFeatureParameters;
      originalSnapshot: MoveFeatureParameters | null;
      createdCopyFeatureId?: string | null;
    };

type ActiveImportAction = {
  kind: "image" | "svg";
  featureId: string | null;
  sourcePath: string | null;
  fileName: string | null;
  parameters: ImportPlacementPayload | null;
};

interface SketchDeleteSelection {
  entityIds: string[];
  pointIds: string[];
  profileIds: string[];
}

function bodyIdFromFaceId(faceId: string | null | undefined) {
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

async function readImageSize(filePath: string): Promise<{ width: number; height: number }> {
  return new Promise((resolve) => {
    const image = new Image();
    image.onload = () => {
      resolve({
        width: image.naturalWidth || 1,
        height: image.naturalHeight || 1,
      });
    };
    image.onerror = () => resolve({ width: 1, height: 1 });
    image.src = convertFileSrc(filePath);
  });
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
//     authoritative source while editing — relying on the document
//     round-trip for it caused dropped edges under rapid clicking,
//     because each click read a stale snapshot of `selected_edge_ids`.
type ActiveEdgeOpAction =
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

function App() {
  const { t } = useTranslation();
  const { config } = useAppConfig();
  const [armedSketchConstraint, setArmedSketchConstraint] =
    useState<ArmedSketchConstraint>(null);
  // Which input slot in the floating Mirror panel is currently
  // focused (and therefore captures viewport entity clicks).
  // `null` means the panel is closed; the *open / closed* state is
  // mirrored in the document's `pending_mirror` so the UI flag
  // and the core stay in sync via the document round-trip.
  const [mirrorFocusedSlot, setMirrorFocusedSlot] = useState<
    "objects" | "axis" | null
  >(null);
  const [extrudeAction, setExtrudeAction] =
    useState<ActiveExtrudeAction | null>(null);
  const extrudeCreateInFlightRef = useRef(false);
  const lastExtrudeProfileUpdateRef = useRef("");
  const [loftAction, setLoftAction] = useState<ActiveLoftAction | null>(null);
  const loftCreateInFlightRef = useRef(false);
  const lastLoftProfileUpdateRef = useRef("");
  const [revolveAction, setRevolveAction] =
    useState<ActiveRevolveAction | null>(null);
  const revolveCreateInFlightRef = useRef(false);
  const lastRevolveInputsRef = useRef("");
  const [sweepAction, setSweepAction] = useState<ActiveSweepAction | null>(null);
  const sweepCreateInFlightRef = useRef(false);
  const lastSweepInputsRef = useRef("");
  const [moveAction, setMoveAction] = useState<ActiveMoveAction | null>(null);
  const [importAction, setImportAction] = useState<ActiveImportAction | null>(null);
  // Arc tool creation mode. Defaults to three-point (common CAD workflow's default
  // and the most ergonomic for shaping curves on the fly). The
  // SketchToolbar exposes a segmented control to toggle to
  // center+start+end without leaving the tool.
  const [arcToolMode, setArcToolMode] = useState<
    "three_point" | "center_start_end"
  >("three_point");
  // Rectangle creation mode. Defaults to corner-to-corner (2-point).
  // The SketchToolbar shows a split button with a variant dropdown
  // to switch between corner-corner, center-point, and 3-point.
  const [rectangleToolMode, setRectangleToolMode] = useState<
    "corner_corner" | "center_point" | "three_point"
  >("corner_corner");
  // Circle creation mode. Defaults to center+radius.
  // The SketchToolbar shows a split button with variants for
  // 2-point, 3-point, and tangent circles (reserved for core support).
  const [circleToolMode, setCircleToolMode] = useState<
    "center_radius" | "two_point" | "three_point" | "tangent_two_lines" | "tangent_three_lines"
  >("center_radius");
  // Polygon creation mode. Defaults to inscribed.
  const [polygonToolMode, setPolygonToolMode] = useState<
    "circumscribed" | "inscribed" | "edge"
  >("inscribed");

  // Sketch fillet panel session. Mirrors `ActiveEdgeOpAction` (the
  // 3D fillet/chamfer flow) shape-for-shape: it opens the moment
  // the user activates the Fillet tool (`pending` phase, no
  // fillets yet) and transitions to `active` as soon as they
  // click their first eligible corner. The panel's `radius`
  // applies to every fillet created in the session and is fanned
  // out across all of them on every debounced numeric change so
  // the user gets the same "select N corners, dial in one
  // radius" experience as 3D fillets give for edges.
  type SketchFilletAction =
    | { phase: "pending"; radius: number }
    | { phase: "active"; radius: number; filletIds: string[] };
  const [sketchFilletAction, setSketchFilletAction] =
    useState<SketchFilletAction | null>(null);
  const [pendingSketchDeleteConfirmation, setPendingSketchDeleteConfirmation] =
    useState<{
      selection: SketchDeleteSelection;
      affectedFeatureNames: string[];
    } | null>(null);
  // Mirror of `sketchFilletAction.filletIds` for the inline
  // viewport callback. Same trick as `activeEdgeIdsRef` in the 3D
  // edge-op flow: the click handler runs inside a closure that
  // captures the value at panel-open time, so we need a ref to
  // see the live list when each subsequent click lands.
  const sketchFilletIdsRef = useRef<string[]>([]);
  const [edgeOpAction, setEdgeOpAction] = useState<ActiveEdgeOpAction | null>(
    null,
  );
  type ShellAction =
    | { phase: "pending"; initialThickness: number }
    | {
        phase: "active";
        featureId: string;
        faceId: string;
        faceSummary: string;
        initialThickness: number;
      };
  const [shellAction, setShellAction] = useState<ShellAction | null>(null);
  const pendingShellThicknessRef = useRef<number>(DEFAULT_SHELL_THICKNESS);
  // In-progress offset plane session. Mirrors the fillet/chamfer
  // two-phase pattern:
  //   - "pending": panel is open but no construction_plane feature
  //     exists yet. The user must click a plane / planar face in the
  //     viewport. The next valid click promotes the session to
  //     "active". `pendingOffsetRef` holds the latest typed offset
  //     so the create call uses whatever the user dialed in before
  //     clicking.
  //   - "active": the core created the feature; typing here drives
  //     `update_offset_plane` for live preview.
  type OffsetPlaneAction =
    | { phase: "pending"; initialOffset: number }
    | {
        phase: "active";
        featureId: string;
        initialOffset: number;
        sourceSummary: string;
      };
  const [offsetPlaneAction, setOffsetPlaneAction] =
    useState<OffsetPlaneAction | null>(null);
  const pendingOffsetRef = useRef<number>(DEFAULT_OFFSET_PLANE_DISTANCE);
  const [midplaneAction, setMidplaneAction] = useState<{
    sourceIds: string[];
  } | null>(null);
  const [tangentPlaneAction, setTangentPlaneAction] = useState<{
    isPending: true;
  } | null>(null);
  type AnglePlaneAction =
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
  const [anglePlaneAction, setAnglePlaneAction] =
    useState<AnglePlaneAction | null>(null);
  const pendingAngleRef = useRef<number>(DEFAULT_ANGLE_PLANE_DEGREES);
  const [constructionAxisAction, setConstructionAxisAction] =
    useState<{ isPending: true } | null>(null);
  const [constructionPointAction, setConstructionPointAction] =
    useState<{ isPending: true } | null>(null);
  type HelixAction =
    | { phase: "pending" }
    | { phase: "active"; featureId: string };
  const [helixAction, setHelixAction] = useState<HelixAction | null>(null);
  type ThreadAction =
    | { phase: "pick_target"; axisSourceId: string | null }
    | { phase: "pick_axis"; targetBodyId: string; targetSummary: string }
    | {
        phase: "active";
        featureId: string;
        originalParameters: ThreadFeatureParameters | null;
      };
  const [threadAction, setThreadAction] = useState<ThreadAction | null>(null);
  type FastenerAction = {
    featureId: string;
    originalParameters: FastenerFeatureParameters | null;
  };
  const [fastenerAction, setFastenerAction] = useState<FastenerAction | null>(
    null,
  );
  type HoleAction =
    | { phase: "pending" }
    | { phase: "active"; featureId: string };
  const [holeAction, setHoleAction] = useState<HoleAction | null>(null);
  // Identifies which feature (if any) is being edited via the floating
  // edit panel. The panel itself reads the feature's parameters
  // directly from `document.feature_history`, so we only need the id
  // here. `null` means the panel is closed. Triggered by a
  // double-click in the timeline (see `onEditFeature` below).
  const [editingFeatureId, setEditingFeatureId] = useState<string | null>(null);
  const restoreTimelineCursorAfterEditRef = useRef(false);
  const [timelineEditVisibleFeatureIds, setTimelineEditVisibleFeatureIds] =
    useState<Set<string>>(() => new Set<string>());
  const [hiddenFeatureIds, setHiddenFeatureIds] = useState<Set<string>>(
    () => new Set<string>(),
  );
  const [hiddenCategories, setHiddenCategories] = useState<Set<CategoryId>>(
    () => new Set<CategoryId>(),
  );
  const [sidebarTab, setSidebarTab] = useState<SidebarTab>("projects");
  const [recentProjectsDocument, setRecentProjectsDocument] =
    useState<RecentProjectsDocument>(EMPTY_RECENT_PROJECTS_DOCUMENT);
  const recentProjectsDocumentRef = useRef<RecentProjectsDocument>(
    EMPTY_RECENT_PROJECTS_DOCUMENT,
  );
  recentProjectsDocumentRef.current = recentProjectsDocument;
  const [currentProjectPath, setCurrentProjectPath] = useState<string | null>(
    null,
  );
  const [savedDocumentBaseline, setSavedDocumentBaseline] =
    useState<SavedDocumentBaseline | null>(null);
  const [pendingUnsavedAction, setPendingUnsavedAction] =
    useState<PendingUnsavedAction | null>(null);
  const originVisibilityManuallyChangedRef = useRef(false);
  const previousDocumentIdRef = useRef<string | null>(null);
  const snapshotCaptureRef = useRef<(() => string | null) | null>(null);
  const allowAppCloseRef = useRef(false);
  const isDocumentDirtyRef = useRef(false);
  // Hierarchy sidebar layout. Collapsed: shown as a thin vertical bar
  // labelled "Hierarchy" on the left edge. Width is user-resizable
  // via a drag handle on the sidebar's right edge.
  const [isHierarchyCollapsed, setIsHierarchyCollapsed] =
    useState<boolean>(false);
  const [hierarchyWidth, setHierarchyWidth] = useState<number>(320);
  const status = useCadCoreStore((state) => state.status);
  const messages = useCadCoreStore((state) => state.messages);
  const logs = useCadCoreStore((state) => state.logs);
  const document = useCadCoreStore((state) => state.document);
  const session = useCadCoreStore((state) => state.session);
  const viewport = useCadCoreStore((state) => state.viewport);
  const addMessage = useCadCoreStore((state) => state.addMessage);
  const clearLogs = useCadCoreStore((state) => state.clearLogs);
  const [isLogsOpen, setIsLogsOpen] = useState(false);
  const [isSettingsOpen, setIsSettingsOpen] = useState(false);
  const [isAiPanelOpen, setIsAiPanelOpen] = useState(false);
  const [parametersPanelOpen, setParametersPanelOpen] = useState(false);
  const [filterPanelOpen, setFilterPanelOpen] = useState(false);
  const [materialsPanelOpen, setMaterialsPanelOpen] = useState(false);
  const [workspaceView, setWorkspaceView] = useState<WorkspaceView>("cad");
  const [slicerStatus, setSlicerStatus] = useState<string | null>(null);
  const [hasOrcaEmbedSession, setHasOrcaEmbedSession] = useState(false);
  const workspaceViewRef = useRef(workspaceView);
  workspaceViewRef.current = workspaceView;
  const slicerViewportRef = useRef<HTMLDivElement | null>(null);
  const errorLogCount = logs.filter((entry) => entry.level === "error").length;
  const isAiAssistantAvailable =
    config.ai.enabled &&
    config.ai.baseUrl.trim().length > 0 &&
    config.ai.model.trim().length > 0;
  const selectedReference =
    viewport?.reference_planes.find(
      (referencePlane) =>
        referencePlane.reference_id === document?.selected_reference_id,
    ) ??
    viewport?.reference_planes.find(
      (referencePlane) => referencePlane.is_selected,
    ) ??
    null;
  const selectedSketchableFace =
    document?.selected_face_id && viewport
      ? (viewport.solid_faces.find(
          (face) =>
            face.face_id === document.selected_face_id &&
            face.sketchability === "planar",
        ) ?? null)
      : null;
  const selectedMaterialFace =
    document?.selected_face_id && viewport
      ? (viewport.solid_faces.find(
          (face) => face.face_id === document.selected_face_id,
        ) ?? null)
      : null;
  const selectedMaterialBodyId =
    selectedMaterialFace?.owner_id ??
    (document?.selected_feature_id &&
    viewport?.bodies.some((body) => body.id === document.selected_feature_id)
      ? document.selected_feature_id
      : null);
  const selectedSketchProfile =
    viewport?.sketch_profiles.find((profile) => profile.is_selected) ?? null;
  const selectedSketchProfiles =
    viewport?.sketch_profiles.filter((profile) => profile.is_selected) ?? [];
  const selectedSketchProfileIds =
    document?.selected_sketch_profile_ids ?? selectedSketchProfiles.map(
      (profile) => profile.profile_id,
    );
  const selectedSketchEntityIds = document?.selected_sketch_entity_ids ?? [];
  const activeHoleFeature =
    holeAction?.phase === "active"
      ? (document?.feature_history.find(
          (feature) => feature.feature_id === holeAction.featureId,
        ) ?? null)
      : null;
  const activeHoleParameters = activeHoleFeature?.hole_parameters ?? null;
  const activeHoleStandards = activeHoleParameters
    ? holeStandardsForMode(activeHoleParameters.standard)
    : [];
  const activeHelixFeature =
    helixAction?.phase === "active"
      ? (document?.feature_history.find(
          (feature) => feature.feature_id === helixAction.featureId,
        ) ?? null)
      : null;
  const activeHelixParameters = activeHelixFeature?.helix_parameters ?? null;
  const activeThreadFeature =
    threadAction?.phase === "active"
      ? (document?.feature_history.find(
          (feature) => feature.feature_id === threadAction.featureId,
        ) ?? null)
      : null;
  const activeThreadParameters = activeThreadFeature?.thread_parameters ?? null;
  const activeThreadStandards = activeThreadParameters
    ? holeStandardsForMode(activeThreadParameters.standard)
    : [];
  const activeFastenerFeature = fastenerAction
    ? (document?.feature_history.find(
        (feature) => feature.feature_id === fastenerAction.featureId,
      ) ?? null)
    : null;
  const activeFastenerParameters =
    activeFastenerFeature?.fastener_parameters ?? null;
  const activeFastenerStandards = activeFastenerParameters
    ? holeStandardsForMode(activeFastenerParameters.standard)
    : [];
  const activeMoveFeature =
    moveAction?.phase === "active"
      ? (document?.feature_history.find(
          (feature) => feature.feature_id === moveAction.featureId,
        ) ?? null)
      : null;
  const activeMoveParameters =
    activeMoveFeature?.move_parameters ??
    (moveAction?.phase === "active" ? moveAction.parameters : null);
  const selectedMoveBodyId =
    selectedMaterialBodyId ??
    (document?.selected_feature_id &&
    viewport?.bodies.some((body) => body.id === document.selected_feature_id)
      ? document.selected_feature_id
      : null);
  const selectedSketchProfileIdsKey = selectedSketchProfileIds.join("|");
  const sketchProfileLabelById = new Map<string, string>();
  const sketchLineLabelById = new Map<string, string>();
  const sketchPathEntityLabelById = new Map<string, string>();
  for (const feature of document?.feature_history ?? []) {
    if (feature.kind !== "sketch" || !feature.sketch_parameters) {
      continue;
    }
    feature.sketch_parameters.profiles.forEach((profile, index) => {
      sketchProfileLabelById.set(
        profile.profile_id,
        `${feature.name || "Sketch"} · Profile ${index + 1}`,
      );
    });
    feature.sketch_parameters.lines.forEach((line, index) => {
      const label = `${feature.name || "Sketch"} · Line ${index + 1}`;
      sketchLineLabelById.set(line.line_id, label);
      sketchPathEntityLabelById.set(line.line_id, label);
    });
    feature.sketch_parameters.arcs.forEach((arc, index) => {
      sketchPathEntityLabelById.set(
        arc.arc_id,
        `${feature.name || "Sketch"} · Arc ${index + 1}`,
      );
    });
  }
  const selectedExtrudableFaceId =
    selectedSketchProfileIds.length === 0
      ? selectedSketchableFace?.face_id ?? null
      : null;
  const activeSketchPlaneId = document?.active_sketch_plane_id ?? null;
  const activeSketchTool = document?.active_sketch_tool ?? null;
  // The active sketch's pending mirror state lives in the document.
  // The UI presents the floating panel whenever this is non-null;
  // local React state only tracks which slot has keyboard / pick
  // focus.
  const activeSketchFeature =
    document?.feature_history.find(
      (entry) => entry.feature_id === document?.active_sketch_feature_id,
    ) ?? null;
  const hasDocumentContent = useMemo(() => {
    if (!document) {
      return false;
    }
    return (
      document.feature_history.some(
        (feature) => feature.kind !== "root_part",
      ) || document.parameters.length > 0
    );
  }, [document]);
  const isDocumentDirty =
    document !== null &&
    (hasDocumentContent || currentProjectPath !== null) &&
    (savedDocumentBaseline?.documentId !== document.document_id ||
      savedDocumentBaseline.revision !== document.revision);
  isDocumentDirtyRef.current = isDocumentDirty;
  const currentDocumentName = currentProjectPath
    ? projectNameFromPath(currentProjectPath)
    : document?.name || t("documentStatus.untitled");
  const windowDocumentTitle = `${currentDocumentName}${isDocumentDirty ? "*" : ""} - Polysmith`;
  const pendingMirror =
    activeSketchFeature?.sketch_parameters?.pending_mirror ?? null;
  const isMirrorToolOpen = pendingMirror !== null;
  function toCorePlaneFrame(planeFrame: {
    origin: [number, number, number];
    xAxis: [number, number, number];
    yAxis: [number, number, number];
    normal: [number, number, number];
  }) {
    return {
      origin: {
        x: planeFrame.origin[0],
        y: planeFrame.origin[1],
        z: planeFrame.origin[2],
      },
      x_axis: {
        x: planeFrame.xAxis[0],
        y: planeFrame.xAxis[1],
        z: planeFrame.xAxis[2],
      },
      y_axis: {
        x: planeFrame.yAxis[0],
        y: planeFrame.yAxis[1],
        z: planeFrame.yAxis[2],
      },
      normal: {
        x: planeFrame.normal[0],
        y: planeFrame.normal[1],
        z: planeFrame.normal[2],
      },
    };
  }

  function referencePlaneFrame(reference: NonNullable<typeof selectedReference>): PlaneFrame {
    if (reference.plane_frame) {
      return reference.plane_frame;
    }
    if (reference.orientation === "yz") {
      return {
        origin: { x: 0, y: 0, z: 0 },
        x_axis: { x: 0, y: 1, z: 0 },
        y_axis: { x: 0, y: 0, z: 1 },
        normal: { x: 1, y: 0, z: 0 },
      };
    }
    if (reference.orientation === "xz") {
      return {
        origin: { x: 0, y: 0, z: 0 },
        x_axis: { x: 1, y: 0, z: 0 },
        y_axis: { x: 0, y: 0, z: 1 },
        normal: { x: 0, y: 1, z: 0 },
      };
    }
    return {
      origin: { x: 0, y: 0, z: 0 },
      x_axis: { x: 1, y: 0, z: 0 },
      y_axis: { x: 0, y: 0, z: 1 },
      normal: { x: 0, y: 1, z: 0 },
    };
  }

  function currentImportPlane():
    | { planeId: string; planeFrame: PlaneFrame }
    | null {
    if (selectedReference) {
      return {
        planeId: selectedReference.reference_id,
        planeFrame: referencePlaneFrame(selectedReference),
      };
    }
    if (selectedSketchableFace) {
      return {
        planeId: selectedSketchableFace.face_id,
        planeFrame: selectedSketchableFace.plane_frame,
      };
    }
    return null;
  }

  function defaultImportPlacement(
    planeId: string,
    planeFrame: PlaneFrame,
    width = 100,
    height = 100,
  ): ImportPlacementPayload {
    return {
      plane_id: planeId,
      plane_frame: planeFrame,
      offset_u_mm: 0,
      offset_v_mm: 0,
      rotation_degrees: 0,
      width_mm: width,
      height_mm: height,
      lock_aspect: true,
      is_pending: true,
    };
  }

  function triggerImportAction(kind: "image" | "svg") {
    if (
      extrudeAction ||
      loftAction ||
      revolveAction ||
      sweepAction ||
      edgeOpAction ||
      shellAction ||
      holeAction ||
      offsetPlaneAction ||
      midplaneAction ||
      tangentPlaneAction ||
      anglePlaneAction ||
      constructionAxisAction ||
      constructionPointAction ||
      helixAction ||
      threadAction ||
      fastenerAction ||
      moveAction ||
      importAction
    ) {
      return;
    }
    const plane = currentImportPlane();
    setImportAction({
      kind,
      featureId: null,
      sourcePath: null,
      fileName: null,
      parameters: plane
        ? defaultImportPlacement(plane.planeId, plane.planeFrame)
        : null,
    });
  }

  async function pickImportFile() {
    if (!importAction) {
      return;
    }
    const plane = importAction.parameters
      ? {
          planeId: importAction.parameters.plane_id,
          planeFrame: importAction.parameters.plane_frame,
        }
      : currentImportPlane();
    if (!plane?.planeFrame) {
      addMessage("Select a plane before importing");
      return;
    }
    const result = await open({
      multiple: false,
      filters:
        importAction.kind === "image"
          ? [
              {
                name: "Images",
                extensions: ["png", "jpg", "jpeg", "webp", "gif"],
              },
            ]
          : [{ name: "SVG", extensions: ["svg"] }],
    });
    if (typeof result !== "string") {
      return;
    }
    const size =
      importAction.kind === "image"
        ? await readImageSize(result)
        : { width: 100, height: 100 };
    const width = 100;
    const height = Math.max((size.height / Math.max(size.width, 1)) * width, 0.001);
    const placement = defaultImportPlacement(
      plane.planeId,
      plane.planeFrame,
      width,
      height,
    );
    placement.source_width = size.width;
    placement.source_height = size.height;
    const expectedFeatureKind =
      importAction.kind === "image" ? "image_import" : "sketch";
    const documentPromise = awaitDocumentChange((next) => {
      if (!next.selected_feature_id) {
        return false;
      }
      const feature = next.feature_history.find(
        (entry) => entry.feature_id === next.selected_feature_id,
      );
      return feature?.kind === expectedFeatureKind;
    }, 8000);
    try {
      if (importAction.kind === "image") {
        await createImageImport(result, placement);
        const nextDocument = await documentPromise;
        const featureId = nextDocument.selected_feature_id;
        if (!featureId) {
          throw new Error("Imported feature was not selected by the core");
        }
        setImportAction({
          ...importAction,
          featureId,
          sourcePath: result,
          fileName: result.split(/[\\/]/).pop() ?? result,
          parameters: placement,
        });
      } else {
        await createSvgImport(result, placement);
        await documentPromise;
        setImportAction(null);
        await clearSelection();
      }
    } catch (error) {
      void documentPromise.catch(() => undefined);
      addMessage(`import error: ${String(error)}`);
      setImportAction((current) =>
        current
          ? {
              ...current,
              parameters: placement,
            }
          : current,
      );
    }
  }

  async function triggerCreateSketchAction() {
    if (activeSketchPlaneId) {
      return;
    }

    if (selectedReference) {
      await runAction(async () => {
        await startSketchOnPlane(selectedReference.reference_id);
      });
      return;
    }

    if (selectedSketchableFace) {
      await runAction(async () => {
        await startSketchOnFace(
          selectedSketchableFace.face_id,
          toCorePlaneFrame({
            origin: [
              selectedSketchableFace.plane_frame.origin.x,
              selectedSketchableFace.plane_frame.origin.y,
              selectedSketchableFace.plane_frame.origin.z,
            ],
            xAxis: [
              selectedSketchableFace.plane_frame.x_axis.x,
              selectedSketchableFace.plane_frame.x_axis.y,
              selectedSketchableFace.plane_frame.x_axis.z,
            ],
            yAxis: [
              selectedSketchableFace.plane_frame.y_axis.x,
              selectedSketchableFace.plane_frame.y_axis.y,
              selectedSketchableFace.plane_frame.y_axis.z,
            ],
            normal: [
              selectedSketchableFace.plane_frame.normal.x,
              selectedSketchableFace.plane_frame.normal.y,
              selectedSketchableFace.plane_frame.normal.z,
            ],
          }),
        );
      });
    }
  }
  const {
    start,
    createDocument,
    exportDocument,
    exportDocumentStl,
    exportBodyStl,
    saveDocument,
    loadDocument,
    projectFaceIntoSketch,
    projectProfileIntoSketch,
    projectEdgeIntoSketch,
    projectVertexIntoSketch,
    addBoxFeature,
    addCylinderFeature,
    updateBoxFeature,
    updateCylinderFeature,
    updateExtrudeDepth,
    renameFeature,
    setFeatureSuppressed,
    deleteFeature,
    undo,
    redo,
    setTimelineCursor,
    selectFeature,
    selectReference,
    selectFace,
    selectEdge,
    selectVertex,
    setBodyColor,
    setFaceColor,
    clearBodyColor,
    clearFaceColor,
    clearAppearanceOverrides,
    createFillet,
    updateFilletRadius,
    updateFilletEdges,
    createChamfer,
    updateChamferDistance,
    updateChamferEdges,
    confirmFillet,
    confirmChamfer,
    createShell,
    updateShellThickness,
    confirmShell,
    createOffsetPlane,
    createMidplane,
    createTangentPlane,
    createAnglePlane,
    createConstructionAxis,
    createConstructionPoint,
    createHole,
    updateHoleParameters,
    confirmHole,
    createHelix,
    updateHelixParameters,
    createThread,
    updateThreadParameters,
    confirmThread,
    createFastener,
    updateFastenerParameters,
    createMove,
    createBodyCopy,
    unlinkBodyCopy,
    updateMoveParameters,
    confirmMove,
    createImageImport,
    updateImageImport,
    confirmImageImport,
    cancelImageImport,
    createSvgImport,
    updateSvgImport,
    confirmSvgImport,
    cancelSvgImport,
    updateOffsetPlane,
    updateAnglePlane,
    startSketchOnPlane,
    startSketchOnFace,
    setSketchTool,
    setSketchLineConstraint,
    clearSketchLineConstraints,
    resolveDraftSnap,
    setSketchEqualLengthConstraint,
    setSketchCoincidentConstraint,
    setSketchParallelConstraint,
    setSketchPerpendicularConstraint,
    setSketchTangentConstraint,
    startMirrorPreview,
    updateMirrorPreviewAxis,
    updateMirrorPreviewObjects,
    commitMirrorPreview,
    cancelMirrorPreview,
    setSketchPointFixed,
    updateSketchPoint,
    updateSketchDimension,
    updateSketchDimensionLabelPosition,
    selectSketchProfile,
    extrudeProfile,
    extrudeOpenEntities,
    extrudeFace,
    updateExtrudeMode,
    updateExtrudeParameters,
    updateExtrudeProfiles,
    updateExtrudeTargetBody,
    loftProfiles,
    updateLoftProfiles,
    updateLoftRuled,
    revolveProfile,
    updateRevolveProfile,
    updateRevolveAxis,
    updateRevolveAngle,
    sweepProfile,
    updateSweepProfile,
    updateSweepPath,
    addSketchLine,
    setSketchMidpointAnchor,
    setSketchPointLineAnchor,
    addSketchAngleDimension,
    addSketchDistanceDimension,
    addSketchLineLengthDimension,
    addSketchCircleRadiusDimension,
    addSketchPolygonRadiusDimension,
    addSketchRectangle,
    addSketchCircle,
    addSketchPolygon,
    addSketchArc,
    addSketchFillet,
    updateSketchFilletRadius,
    deleteSketchFillet,
    deleteSketchDimension,
    addSketchPointDistanceDimension,
    updateSketchDimensionDisplay,
    deleteSketchSelection,
    trimSketchEntity,
    selectSketchPoint,
    selectSketchEntity,
    selectSketchDimension,
    finishSketch,
    reenterSketch,
    clearSelection,
    batchSelectSketchEntities,
    updateSelectionFilter,
  } = useCadCore();

  function clearTimelineEditVisibility() {
    setTimelineEditVisibleFeatureIds((current) =>
      current.size === 0 ? current : new Set<string>(),
    );
  }

  function beginTimelineEditSession(featureId: string, featureKind: string) {
    restoreTimelineCursorAfterEditRef.current = document?.timeline_cursor === null;
    if (featureKind === "sketch") {
      setTimelineEditVisibleFeatureIds(new Set([featureId]));
      return;
    }
    clearTimelineEditVisibility();
  }

  async function restoreTimelineCursorAfterEdit() {
    clearTimelineEditVisibility();
    if (!restoreTimelineCursorAfterEditRef.current) {
      return;
    }
    restoreTimelineCursorAfterEditRef.current = false;
    const latestDocument = useCadCoreStore.getState().document ?? document;
    const actionCount =
      latestDocument?.feature_history.filter(
        (feature) => feature.kind !== "root_part",
      ).length ?? 0;
    await setTimelineCursor(actionCount);
  }

  useEffect(() => {
    if (!activeSketchPlaneId) {
      clearTimelineEditVisibility();
      setArmedSketchConstraint(null);
      // Mirror tool is sketch-scoped: if the user finishes the
      // sketch (or the active sketch otherwise becomes null) we
      // drop the focus state. The core's pending_mirror is
      // already gone with the sketch, so there's nothing else to
      // clean up here.
      setMirrorFocusedSlot(null);
      // Same reasoning for the in-progress sketch fillet: the
      // fillet record lives on the sketch, so once the sketch
      // closes the panel has nothing to drive. We don't try to
      // commit or cancel implicitly; the user's `finishSketch`
      // already left the fillet in the sketch in its current
      // committed state.
      setSketchFilletAction(null);
      sketchFilletIdsRef.current = [];
    }
  }, [activeSketchPlaneId]);

  useEffect(() => {
    if (!importAction || importAction.featureId) {
      return;
    }
    const plane = currentImportPlane();
    if (!plane) {
      return;
    }
    setImportAction((current) =>
      current &&
      !current.featureId &&
      current.parameters?.plane_id !== plane.planeId
        ? {
            ...current,
            parameters: defaultImportPlacement(plane.planeId, plane.planeFrame),
          }
        : current,
    );
  }, [document?.selected_reference_id, document?.selected_face_id, importAction]);

  // Open / close the Sketch Fillet panel in lockstep with the
  // active sketch tool. Activating the Fillet tool opens the panel
  // in `pending` phase; deactivating it (switching to any other
  // tool) implicitly *confirms* — we keep whatever fillets the
  // user already created and drop the session. Cancel-with-undo
  // remains explicit via the panel's Cancel button. This keeps
  // the door open for casual exploration: drop in a fillet, switch
  // to Line, draw something, come back later — without losing
  // work.
  useEffect(() => {
    if (
      activeSketchTool === "fillet" &&
      activeSketchPlaneId &&
      !sketchFilletAction
    ) {
      setSketchFilletAction({ phase: "pending", radius: 5 });
      sketchFilletIdsRef.current = [];
      return;
    }
    if (activeSketchTool !== "fillet" && sketchFilletAction) {
      setSketchFilletAction(null);
      sketchFilletIdsRef.current = [];
    }
  }, [activeSketchTool, activeSketchPlaneId, sketchFilletAction]);

  // Auto-startup and crash recovery: launch the native core as soon as
  // the UI mounts, and relaunch it after an unexpected process exit.
  // `handleCoreStopped` clears the stale document snapshot, so a
  // successfully restarted core lands on a fresh untitled document.
  useEffect(() => {
    if (status === "idle" || status === "stopped") {
      void start();
      return;
    }
    if (status === "connected" && document === null) {
      void createDocument();
    }
  }, [status, document, start, createDocument]);

  useEffect(() => {
    if (status !== "stopped") {
      return;
    }
    setCurrentProjectPath(null);
    setSavedDocumentBaseline(null);
    setPendingUnsavedAction(null);
    setHiddenFeatureIds(new Set<string>());
    setHiddenCategories(new Set<CategoryId>());
    originVisibilityManuallyChangedRef.current = false;
  }, [status]);

  useEffect(() => {
    let canceled = false;
    void loadRecentProjects()
      .then((projectsDocument) => {
        if (!canceled) {
          recentProjectsDocumentRef.current = projectsDocument;
          setRecentProjectsDocument(projectsDocument);
        }
      })
      .catch((error) => {
        addMessage(`recent projects load error: ${String(error)}`);
      });
    return () => {
      canceled = true;
    };
  }, [addMessage]);

  useEffect(() => {
    let unlisten: (() => void) | null = null;
    let disposed = false;
    void getCurrentWindow()
      .onCloseRequested((event) => {
        if (allowAppCloseRef.current || !isDocumentDirtyRef.current) {
          return;
        }
        event.preventDefault();
        setPendingUnsavedAction({ kind: "quit" });
      })
      .then((nextUnlisten) => {
        if (disposed) {
          nextUnlisten();
          return;
        }
        unlisten = nextUnlisten;
      });
    return () => {
      disposed = true;
      unlisten?.();
    };
  }, []);

  useEffect(() => {
    void getCurrentWindow()
      .setTitle(windowDocumentTitle)
      .catch((error) => {
        addMessage(`window title error: ${String(error)}`);
      });
  }, [windowDocumentTitle, addMessage]);

  useEffect(() => {
    function handleBeforeUnload(event: BeforeUnloadEvent) {
      if (!isDocumentDirty) {
        return;
      }
      event.preventDefault();
      event.returnValue = "";
    }

    window.addEventListener("beforeunload", handleBeforeUnload);
    return () => {
      window.removeEventListener("beforeunload", handleBeforeUnload);
    };
  }, [isDocumentDirty]);

  // UI-only visibility: combine per-feature hides with category hides into
  // sets the viewport can use to filter primitives, sketch entities, and
  // reference geometry. Timeline sketch edits can temporarily force their
  // sketch visible without changing the user's saved visibility choices.
  // Sketch entities are filtered by plane id since the viewport snapshot
  // does not carry the owning sketch feature id on each sketch primitive.
  const hasSolidBody = useMemo(
    () => documentHasSolidBody(document),
    [document],
  );
  const hasExportableBody = (viewport?.bodies.length ?? 0) > 0;
  const isSlicerConfigured =
    config.orcaSlicer.enabled &&
    (config.orcaSlicer.integrationMode === "web"
      ? config.orcaSlicer.webUrl.trim().length > 0
      : config.orcaSlicer.binaryPath.trim().length > 0);
  const canExportToSlicer = hasExportableBody && isSlicerConfigured;
  useEffect(() => {
    const documentId = document?.document_id ?? null;
    if (previousDocumentIdRef.current !== documentId) {
      previousDocumentIdRef.current = documentId;
      originVisibilityManuallyChangedRef.current = false;
      setHiddenCategories((current) => {
        const next = new Set(current);
        if (documentId && hasSolidBody) {
          next.add("origin");
        } else {
          next.delete("origin");
        }
        return next;
      });
      return;
    }

    if (
      !documentId ||
      !hasSolidBody ||
      originVisibilityManuallyChangedRef.current
    ) {
      return;
    }

    setHiddenCategories((current) => {
      if (current.has("origin")) {
        return current;
      }
      const next = new Set(current);
      next.add("origin");
      return next;
    });
  }, [document?.document_id, hasSolidBody]);

  const effectiveHiddenFeatureIds = useMemo(() => {
    const set = new Set<string>(hiddenFeatureIds);
    if (!document) {
      return set;
    }
    for (const feature of document.feature_history) {
      if (hiddenCategories.has("sketches") && feature.kind === "sketch") {
        set.add(feature.feature_id);
      }
      if (hiddenCategories.has("bodies") && BODY_KINDS.has(feature.kind)) {
        set.add(feature.feature_id);
      }
      if (
        hiddenCategories.has("assets") &&
        (feature.kind === "image_import" || feature.kind === "svg_import")
      ) {
        set.add(feature.feature_id);
      }
      // Hiding the Construction category hides every parametric
      // construction reference (and indirectly suppresses any sketch
      // anchored on one, since `hiddenSketchPlaneIds` follows from
      // these ids via the per-plane sketch grouping below).
      if (
        hiddenCategories.has("construction") &&
        (feature.kind === "construction_plane" ||
          feature.kind === "construction_axis" ||
          feature.kind === "construction_point")
      ) {
        set.add(feature.feature_id);
      }
    }
    for (const featureId of timelineEditVisibleFeatureIds) {
      set.delete(featureId);
    }
    return set;
  }, [document, hiddenFeatureIds, hiddenCategories, timelineEditVisibleFeatureIds]);

  const hiddenSketchPlaneIds = useMemo(() => {
    const result = new Set<string>();
    if (!document) {
      return result;
    }
    // Group sketch features by plane id so we only hide a plane when every
    // sketch attached to it is hidden.
    const planeToSketches = new Map<string, string[]>();
    for (const feature of document.feature_history) {
      if (feature.kind !== "sketch" || !feature.sketch_parameters) {
        continue;
      }
      const planeId = feature.sketch_parameters.plane_id;
      const list = planeToSketches.get(planeId) ?? [];
      list.push(feature.feature_id);
      planeToSketches.set(planeId, list);
    }
    for (const [planeId, sketchIds] of planeToSketches) {
      if (sketchIds.every((id) => effectiveHiddenFeatureIds.has(id))) {
        result.add(planeId);
      }
    }
    return result;
  }, [document, effectiveHiddenFeatureIds]);

  async function triggerExtrudeAction() {
    if (extrudeAction?.phase === "active") {
      return;
    }
    if (
      loftAction ||
      revolveAction ||
      sweepAction ||
      edgeOpAction ||
      shellAction ||
      holeAction ||
      offsetPlaneAction ||
      midplaneAction ||
      tangentPlaneAction ||
      anglePlaneAction ||
      constructionAxisAction ||
      constructionPointAction ||
      helixAction ||
      threadAction ||
      fastenerAction ||
      moveAction
    ) {
      return;
    }

    const hasExistingBody =
      (document?.feature_history ?? []).some(
        (entry) =>
          entry.kind === "box" ||
          entry.kind === "cylinder" ||
          entry.kind === "extrude",
      ) ?? false;
    const defaultSettings = getDefaultExtrudeSettings(selectedSketchProfileIds);

    setExtrudeAction({
      phase: "pending",
      featureId: null,
      featureIds: [],
      profileIds: [...selectedSketchProfileIds],
      automaticMode: true,
      initialDepth: DEFAULT_EXTRUDE_DEPTH,
      initialMode: defaultSettings.mode,
      initialParameters: null,
      initialTargetBodyId: defaultSettings.targetBodyId,
      profileCount:
        selectedSketchProfileIds.length ||
        (selectedExtrudableFaceId ? 1 : 0) ||
        selectedSketchEntityIds.length,
      originalSnapshot: null,
      canCombineWithExistingBody: hasExistingBody,
    });
  }

  function getDefaultExtrudeSettings(
    profileIds: readonly string[],
    faceIdOverride: string | null = null,
  ): {
    mode: ExtrudeMode;
    targetBodyId: string | null;
  } {
    const bodyIds = new Set((viewport?.bodies ?? []).map((body) => body.id));

    if (profileIds.length === 0) {
      const selectedFaceBodyId = bodyIdFromFaceId(
        faceIdOverride ?? document?.selected_face_id,
      );
      if (selectedFaceBodyId && bodyIds.has(selectedFaceBodyId)) {
        return {mode: "join", targetBodyId: selectedFaceBodyId};
      }
      return {mode: "new_body", targetBodyId: null};
    }

    let sourceBodyId: string | null = null;
    for (const profileId of profileIds) {
      const sketchFeature = document?.feature_history.find((feature) => {
        if (feature.kind !== "sketch" || !feature.sketch_parameters) {
          return false;
        }
        return feature.sketch_parameters.profiles.some(
          (profile) => profile.profile_id === profileId,
        );
      });
      const nextBodyId = bodyIdFromFaceId(
        sketchFeature?.sketch_parameters?.plane_id,
      );
      if (!nextBodyId || !bodyIds.has(nextBodyId)) {
        return {mode: "new_body", targetBodyId: null};
      }
      if (sourceBodyId && sourceBodyId !== nextBodyId) {
        return {mode: "new_body", targetBodyId: null};
      }
      sourceBodyId = nextBodyId;
    }

    return sourceBodyId
      ? {mode: "join", targetBodyId: sourceBodyId}
      : {mode: "new_body", targetBodyId: null};
  }

  async function createExtrudeFromSelectedProfiles(
    depth: number,
    mode: ExtrudeMode | null,
    targetBodyId: string | null,
    parameters: ExtrudeAdvancedParameters | null = null,
  ) {
    await createExtrudeFromProfiles(
      [...selectedSketchProfileIds],
      depth,
      mode,
      targetBodyId,
      parameters,
    );
  }

  async function createExtrudeFromProfiles(
    profileIds: string[],
    depth: number,
    mode: ExtrudeMode | null,
    targetBodyId: string | null,
    parameters: ExtrudeAdvancedParameters | null = null,
    canCombineWithExistingBodyOverride: boolean | null = null,
  ) {
    if (profileIds.length === 0) {
      return;
    }
    if (extrudeCreateInFlightRef.current) {
      return;
    }
    extrudeCreateInFlightRef.current = true;

    // The IPC bridge is fire-and-forget: `extrudeProfile` returns as soon as
    // the command is written to cad_core stdin, before the core has emitted
    // the `document_state` event with the new feature. To capture the real
    // new feature id we subscribe to the next document update that contains
    // a freshly created extrude feature.
    let createdFeatureIds: string[] = [];
    const documentPromise = awaitDocumentChange((next, previous) => {
      if (!next.selected_feature_id) {
        return false;
      }
      const previousLength = previous?.feature_history.length ?? 0;
      if (next.feature_history.length <= previousLength) {
        return false;
      }
      const createdFeatures = next.feature_history
        .slice(previousLength)
        .filter((feature) => feature.kind === "extrude");
      if (createdFeatures.length === 0) {
        return false;
      }
      const lastFeature = createdFeatures[createdFeatures.length - 1];
      if (lastFeature.feature_id !== next.selected_feature_id) {
        return false;
      }
      createdFeatureIds = createdFeatures.map((feature) => feature.feature_id);
      return true;
    });

    await runAction(async () => {
      try {
        await extrudeProfile(profileIds, depth, mode, targetBodyId, parameters);
        const nextDocument = await documentPromise;
        const newFeatureId = nextDocument.selected_feature_id ?? null;
        if (!newFeatureId) {
          return;
        }
        lastExtrudeProfileUpdateRef.current = profileIds.join("|");
        const createdFeature = nextDocument.feature_history.find(
          (entry) => entry.feature_id === newFeatureId,
        );
        const createdParams = createdFeature?.extrude_parameters;
        setExtrudeAction({
          phase: "active",
          featureId: newFeatureId,
          featureIds: createdFeatureIds.length > 0 ? createdFeatureIds : [newFeatureId],
          profileIds,
          automaticMode: false,
          initialDepth: depth,
          initialMode: createdParams?.mode ?? mode ?? "new_body",
          initialParameters: createdParams ?? null,
          initialTargetBodyId:
            createdParams?.target_body_id ?? targetBodyId ?? null,
          profileCount: profileIds.length,
          // Newly-created extrude: cancel = undo (handled below).
          originalSnapshot: null,
          canCombineWithExistingBody:
            canCombineWithExistingBodyOverride ??
            (document?.feature_history ?? []).some(
              (entry) =>
                entry.kind === "box" ||
                entry.kind === "cylinder" ||
                entry.kind === "extrude",
            ),
        });
      } catch (error) {
        addMessage(`extrude action error: ${String(error)}`);
      } finally {
        extrudeCreateInFlightRef.current = false;
      }
    });
  }

  function extrudeFeatureIsPresent(featureIds: Set<string>) {
    const currentDocument = useCadCoreStore.getState().document;
    return (
      currentDocument?.feature_history.some((feature) =>
        featureIds.has(feature.feature_id),
      ) ?? false
    );
  }

  async function undoUntilExtrudePreviewRemoved(featureIds: readonly string[]) {
    const pendingFeatureIds = new Set(featureIds);
    if (pendingFeatureIds.size === 0) {
      return;
    }

    for (let attempt = 0; attempt < 20; attempt += 1) {
      if (!extrudeFeatureIsPresent(pendingFeatureIds)) {
        return;
      }
      const documentPromise = awaitDocumentChange(() => true);
      await runAction(async () => {
        await undo();
      });
      await documentPromise;
    }
  }

  function activeExtrudeAdvancedParameters(
    params: ExtrudeFeatureParameters | null | undefined,
    operation: ExtrudeMode,
  ): ExtrudeAdvancedParameters | null {
    if (!params) {
      return null;
    }
    return {
      extent_mode: params.extent_mode,
      side1: params.side1,
      side2: params.side2,
      thin: params.thin,
      operation,
      intersect_result: params.intersect_result,
    };
  }

  async function recreateNewProfileExtrudePreview(
    action: ActiveExtrudeAction,
    depth: number,
    mode: ExtrudeMode,
    targetBodyId: string | null,
    parameters: ExtrudeAdvancedParameters | null,
  ) {
    if (
      action.originalSnapshot ||
      action.profileIds.length === 0 ||
      action.featureIds.length === 0
    ) {
      return false;
    }

    await undoUntilExtrudePreviewRemoved(action.featureIds);
    await createExtrudeFromProfiles(
      [...action.profileIds],
      depth,
      mode,
      targetBodyId,
      parameters,
      action.canCombineWithExistingBody,
    );
    return true;
  }

  async function createExtrudeFromSelectedFace(
    faceId: string,
    depth: number,
    mode: ExtrudeMode | null,
    targetBodyId: string | null,
    parameters: ExtrudeAdvancedParameters | null = null,
  ) {
    if (extrudeCreateInFlightRef.current) {
      return;
    }
    extrudeCreateInFlightRef.current = true;

    const documentPromise = awaitDocumentChange((next, previous) => {
      if (!next.selected_feature_id) {
        return false;
      }
      const previousLength = previous?.feature_history.length ?? 0;
      if (next.feature_history.length <= previousLength) {
        return false;
      }
      const lastFeature = next.feature_history[next.feature_history.length - 1];
      return (
        lastFeature.feature_id === next.selected_feature_id &&
        lastFeature.kind === "extrude"
      );
    });

    await runAction(async () => {
      try {
        await extrudeFace(faceId, depth, mode, targetBodyId, parameters);
        const nextDocument = await documentPromise;
        const newFeatureId = nextDocument.selected_feature_id ?? null;
        if (!newFeatureId) {
          return;
        }
        const createdFeature = nextDocument.feature_history.find(
          (entry) => entry.feature_id === newFeatureId,
        );
        const createdParams = createdFeature?.extrude_parameters;
        setExtrudeAction({
          phase: "active",
          featureId: newFeatureId,
          featureIds: [newFeatureId],
          profileIds: [],
          automaticMode: false,
          initialDepth: depth,
          initialMode: createdParams?.mode ?? mode ?? "new_body",
          initialParameters: createdParams ?? null,
          initialTargetBodyId:
            createdParams?.target_body_id ?? targetBodyId ?? null,
          profileCount: 1,
          originalSnapshot: null,
          canCombineWithExistingBody:
            (document?.feature_history ?? []).some(
              (entry) =>
                entry.kind === "box" ||
                entry.kind === "cylinder" ||
                entry.kind === "extrude",
            ) ?? false,
        });
      } catch (error) {
        addMessage(`extrude face action error: ${String(error)}`);
      } finally {
        extrudeCreateInFlightRef.current = false;
      }
    });
  }

  async function createThinExtrudeFromSelectedEntities(
    depth: number,
    mode: ExtrudeMode | null,
    targetBodyId: string | null,
    parameters: ExtrudeAdvancedParameters,
  ) {
    const entityIds = [...selectedSketchEntityIds];
    if (entityIds.length === 0 || extrudeCreateInFlightRef.current) {
      return;
    }
    extrudeCreateInFlightRef.current = true;

    const documentPromise = awaitDocumentChange((next, previous) => {
      if (!next.selected_feature_id) {
        return false;
      }
      const previousLength = previous?.feature_history.length ?? 0;
      if (next.feature_history.length <= previousLength) {
        return false;
      }
      const lastFeature = next.feature_history[next.feature_history.length - 1];
      return (
        lastFeature.feature_id === next.selected_feature_id &&
        lastFeature.kind === "extrude"
      );
    });

    await runAction(async () => {
      try {
        await extrudeOpenEntities(entityIds, depth, mode, targetBodyId, parameters);
        const nextDocument = await documentPromise;
        const newFeatureId = nextDocument.selected_feature_id ?? null;
        if (!newFeatureId) {
          return;
        }
        const createdFeature = nextDocument.feature_history.find(
          (entry) => entry.feature_id === newFeatureId,
        );
        const createdParams = createdFeature?.extrude_parameters;
        setExtrudeAction({
          phase: "active",
          featureId: newFeatureId,
          featureIds: [newFeatureId],
          profileIds: [],
          automaticMode: false,
          initialDepth: depth,
          initialMode: createdParams?.mode ?? mode ?? "new_body",
          initialParameters: createdParams ?? null,
          initialTargetBodyId:
            createdParams?.target_body_id ?? targetBodyId ?? null,
          profileCount: entityIds.length,
          originalSnapshot: null,
          canCombineWithExistingBody:
            (document?.feature_history ?? []).some(
              (entry) =>
                entry.kind === "box" ||
                entry.kind === "cylinder" ||
                entry.kind === "extrude",
            ) ?? false,
        });
      } catch (error) {
        addMessage(`thin extrude action error: ${String(error)}`);
      } finally {
        extrudeCreateInFlightRef.current = false;
      }
    });
  }

  useEffect(() => {
    if (!extrudeAction) {
      return;
    }

    if (selectedSketchProfileIds.length === 0) {
      if (extrudeAction.phase === "pending" && selectedExtrudableFaceId) {
        const defaultSettings = getDefaultExtrudeSettings(
          [],
          selectedExtrudableFaceId,
        );
        const mode =
          extrudeAction.initialMode === "new_body"
            ? defaultSettings.mode
            : extrudeAction.initialMode;
        const targetBodyId =
          mode === "new_body"
            ? null
            : extrudeAction.initialTargetBodyId ?? defaultSettings.targetBodyId;
        void createExtrudeFromSelectedFace(
          selectedExtrudableFaceId,
          extrudeAction.initialDepth,
          extrudeAction.automaticMode ? null : mode,
          extrudeAction.automaticMode ? null : targetBodyId,
        );
      }
      return;
    }

    if (extrudeAction.phase === "pending") {
      const defaultSettings = getDefaultExtrudeSettings(selectedSketchProfileIds);
      const mode =
        extrudeAction.initialMode === "new_body"
          ? defaultSettings.mode
          : extrudeAction.initialMode;
      const targetBodyId =
        mode === "new_body"
          ? null
          : extrudeAction.initialTargetBodyId ?? defaultSettings.targetBodyId;
      void createExtrudeFromSelectedProfiles(
        extrudeAction.initialDepth,
        extrudeAction.automaticMode ? null : mode,
        extrudeAction.automaticMode ? null : targetBodyId,
      );
      return;
    }

    if (!extrudeAction.featureId) {
      return;
    }

    if (lastExtrudeProfileUpdateRef.current === selectedSketchProfileIdsKey) {
      return;
    }

    lastExtrudeProfileUpdateRef.current = selectedSketchProfileIdsKey;
    const nextCount = selectedSketchProfileIds.length;
    void runAction(async () => {
      await updateExtrudeProfiles(extrudeAction.featureId!, selectedSketchProfileIds);
      setExtrudeAction((current) =>
        current?.phase === "active" && current.featureId === extrudeAction.featureId
          ? {
              ...current,
              profileIds: [...selectedSketchProfileIds],
              profileCount: nextCount,
            }
          : current,
      );
    });
  }, [extrudeAction, selectedSketchProfileIdsKey, selectedExtrudableFaceId]);

  async function createLoftFromProfiles(
    profileIds: readonly string[],
    ruled: boolean,
  ) {
    if (profileIds.length < 2) {
      return;
    }
    if (loftCreateInFlightRef.current) {
      return;
    }
    loftCreateInFlightRef.current = true;

    const documentPromise = awaitDocumentChange((next, previous) => {
      if (!next.selected_feature_id) {
        return false;
      }
      const previousLength = previous?.feature_history.length ?? 0;
      if (next.feature_history.length <= previousLength) {
        return false;
      }
      const lastFeature = next.feature_history[next.feature_history.length - 1];
      return (
        lastFeature.feature_id === next.selected_feature_id &&
        lastFeature.kind === "loft"
      );
    });

    await runAction(async () => {
      try {
        await loftProfiles(profileIds, ruled);
        const nextDocument = await documentPromise;
        const newFeatureId = nextDocument.selected_feature_id ?? null;
        if (!newFeatureId) {
          return;
        }
        lastLoftProfileUpdateRef.current = profileIds.join("|");
        const createdFeature = nextDocument.feature_history.find(
          (entry) => entry.feature_id === newFeatureId,
        );
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

    const documentPromise = awaitDocumentChange((next, previous) => {
      if (!next.selected_feature_id) {
        return false;
      }
      const previousLength = previous?.feature_history.length ?? 0;
      if (next.feature_history.length <= previousLength) {
        return false;
      }
      const lastFeature = next.feature_history[next.feature_history.length - 1];
      return (
        lastFeature.feature_id === next.selected_feature_id &&
        lastFeature.kind === "revolve"
      );
    });

    await runAction(async () => {
      try {
        await revolveProfile(profileId, axisEntityId, angleDegrees);
        const nextDocument = await documentPromise;
        const newFeatureId = nextDocument.selected_feature_id ?? null;
        if (!newFeatureId) {
          return;
        }
        lastRevolveInputsRef.current = `${profileId}|${axisEntityId}`;
        const createdFeature = nextDocument.feature_history.find(
          (entry) => entry.feature_id === newFeatureId,
        );
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
    if (!revolveAction?.profileId || !revolveAction.axisEntityId) {
      return;
    }
    const key = `${revolveAction.profileId}|${revolveAction.axisEntityId}`;
    if (lastRevolveInputsRef.current === key) {
      return;
    }
    lastRevolveInputsRef.current = key;
    if (revolveAction.phase === "pending") {
      void createRevolveFromInputs(
        revolveAction.profileId,
        revolveAction.axisEntityId,
        revolveAction.initialAngle,
      );
      return;
    }
    if (!revolveAction.featureId) {
      return;
    }
    void runAction(async () => {
      await updateRevolveProfile(revolveAction.featureId!, revolveAction.profileId!);
      await updateRevolveAxis(revolveAction.featureId!, revolveAction.axisEntityId!);
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

    const documentPromise = awaitDocumentChange((next, previous) => {
      if (!next.selected_feature_id) {
        return false;
      }
      const previousLength = previous?.feature_history.length ?? 0;
      if (next.feature_history.length <= previousLength) {
        return false;
      }
      const lastFeature = next.feature_history[next.feature_history.length - 1];
      return (
        lastFeature.feature_id === next.selected_feature_id &&
        lastFeature.kind === "sweep"
      );
    });

    await runAction(async () => {
      try {
        await sweepProfile(profileId, pathEntityId);
        const nextDocument = await documentPromise;
        const newFeatureId = nextDocument.selected_feature_id ?? null;
        if (!newFeatureId) {
          return;
        }
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
    const selectedPathEntityId =
      document?.selected_sketch_entity_id &&
      sketchPathEntityLabelById.has(document.selected_sketch_entity_id)
        ? document.selected_sketch_entity_id
        : null;
    lastSweepInputsRef.current = "";
    setSweepAction({
      phase: "pending",
      featureId: null,
      profileId,
      pathEntityId: selectedPathEntityId,
      originalSnapshot: null,
    });
  }

  useEffect(() => {
    if (!sweepAction?.profileId || !sweepAction.pathEntityId) {
      return;
    }
    const key = `${sweepAction.profileId}|${sweepAction.pathEntityId}`;
    if (lastSweepInputsRef.current === key) {
      return;
    }
    lastSweepInputsRef.current = key;
    if (sweepAction.phase === "pending") {
      void createSweepFromInputs(sweepAction.profileId, sweepAction.pathEntityId);
      return;
    }
    if (!sweepAction.featureId) {
      return;
    }
    void runAction(async () => {
      await updateSweepProfile(sweepAction.featureId!, sweepAction.profileId!);
      await updateSweepPath(sweepAction.featureId!, sweepAction.pathEntityId!);
    });
  }, [sweepAction, updateSweepPath, updateSweepProfile]);

  // Latest typed value while in the "pending" phase. The panel debounces
  // its onPreviewValue callback, so a click that lands mid-typing must
  // read the freshest value via this ref rather than from React state.
  const pendingValueRef = useRef<number>(DEFAULT_FILLET_RADIUS);

  // Live edge_ids for the in-progress fillet/chamfer feature. We mirror
  // the list in a ref (in addition to React state for the UI count) so
  // every viewport edge click can read the *current* set synchronously
  // and dispatch update_*_edges immediately. A purely state-based
  // approach can't do that — `setState` updaters run asynchronously, so
  // any IPC dispatch decided inside the updater fires after the click
  // handler has already returned, dropping the call entirely. The ref
  // sidesteps both that and the IPC-echo-lag race in one move.
  const activeEdgeIdsRef = useRef<string[]>([]);

  // contextual modeling flow for Fillet / Chamfer. The user invokes the action
  // first (button or hotkey), the panel opens in "pending" phase, and
  // the *first* edge click is what actually creates the feature in the
  // core via create_fillet / create_chamfer. If edges happen to be
  // pre-selected when the action is invoked, we honor that and create
  // immediately, jumping straight to the "active" phase. See the
  // ActiveEdgeOpAction comment above for the rationale.
  // Friendly label for a plane the user just clicked. Per AGENTS.md
  // UI Copy Rules we never expose internal ids — origin planes get
  // "XY plane" / "YZ plane" / "XZ plane", construction planes use
  // their feature name (e.g. "Offset Plane 2"), and faces use the
  // owning body name + the face's kind label.
  function describePlaneSource(referenceId: string): string {
    if (referenceId === "ref-plane-xy") return t("geometry.xyPlane");
    if (referenceId === "ref-plane-yz") return t("geometry.yzPlane");
    if (referenceId === "ref-plane-xz") return t("geometry.xzPlane");
    const feature = document?.feature_history.find(
      (entry) => entry.feature_id === referenceId,
    );
    if (feature) {
      return feature.name || feature.kind;
    }
    const profileLabel = sketchProfileLabelById.get(referenceId);
    if (profileLabel) {
      return profileLabel;
    }
    // Face id "<body_id>:face:<index>" — pull the face's label /
    // owning body label off the viewport snapshot if we can.
    const face = viewport?.solid_faces.find(
      (entry) => entry.face_id === referenceId,
    );
    if (face) {
      return face.label || t("geometry.ownerFace", { owner: face.owner_kind });
    }
    return t("geometry.selectedPlane");
  }

  // Start the contextual modeling Offset Plane flow. Opens the panel in
  // pending phase; the next viewport click on a plane / planar face
  // promotes the session to active by calling `create_offset_plane`.
  // If a plane / face is already selected we honor the
  // "select-then-invoke" shortcut and create the feature immediately.
  async function triggerOffsetPlaneAction() {
    if (
      extrudeAction ||
      loftAction ||
      revolveAction ||
      sweepAction ||
      edgeOpAction ||
      shellAction ||
      offsetPlaneAction ||
      midplaneAction ||
      tangentPlaneAction ||
      anglePlaneAction ||
      constructionAxisAction ||
      constructionPointAction ||
      activeSketchPlaneId
    ) {
      return;
    }
    pendingOffsetRef.current = DEFAULT_OFFSET_PLANE_DISTANCE;

    // Already-selected plane? Use it immediately.
    const preselectedReference = document?.selected_reference_id ?? null;
    const preselectedFaceId = document?.selected_face_id ?? null;
    const preselectedProfileId =
      document?.selected_sketch_profile_id ??
      selectedSketchProfileIds[selectedSketchProfileIds.length - 1] ??
      null;
    const preselectedFace = preselectedFaceId
      ? (viewport?.solid_faces.find(
          (entry) => entry.face_id === preselectedFaceId,
        ) ?? null)
      : null;
    const sourceId =
      preselectedReference ??
      (preselectedFace && preselectedFace.sketchability === "planar"
        ? preselectedFaceId
        : null) ??
      preselectedProfileId;
    if (sourceId) {
      await createOffsetPlaneFeature(sourceId, DEFAULT_OFFSET_PLANE_DISTANCE);
      return;
    }

    setOffsetPlaneAction({
      phase: "pending",
      initialOffset: DEFAULT_OFFSET_PLANE_DISTANCE,
    });
  }

  // Dispatch `create_offset_plane` and wait for the new feature to
  // come back over IPC, mirroring the extrude / fillet flows. Promotes
  // the panel from pending to active once the feature id is known.
  async function createOffsetPlaneFeature(sourceId: string, offset: number) {
    const documentPromise = awaitDocumentChange((next, previous) => {
      if (!next.selected_feature_id) {
        return false;
      }
      const previousLength = previous?.feature_history.length ?? 0;
      if (next.feature_history.length <= previousLength) {
        return false;
      }
      const lastFeature = next.feature_history[next.feature_history.length - 1];
      return (
        lastFeature.feature_id === next.selected_feature_id &&
        lastFeature.kind === "construction_plane"
      );
    });

    await runAction(async () => {
      await createOffsetPlane(sourceId, offset);
      try {
        const nextDocument = await documentPromise;
        const newFeatureId = nextDocument.selected_feature_id ?? null;
        if (!newFeatureId) {
          return;
        }
        setOffsetPlaneAction({
          phase: "active",
          featureId: newFeatureId,
          initialOffset: offset,
          sourceSummary: describePlaneSource(sourceId),
        });
      } catch (error) {
        addMessage(`offset plane error: ${String(error)}`);
      }
    });
  }

  function currentPlaneLikeSourceId(): string | null {
    const preselectedReference = document?.selected_reference_id ?? null;
    const preselectedFaceId = document?.selected_face_id ?? null;
    const preselectedFace = preselectedFaceId
      ? (viewport?.solid_faces.find(
          (entry) => entry.face_id === preselectedFaceId,
        ) ?? null)
      : null;
    const preselectedProfileId =
      document?.selected_sketch_profile_id ??
      selectedSketchProfileIds[selectedSketchProfileIds.length - 1] ??
      null;
    return (
      preselectedReference ??
      (preselectedFace && preselectedFace.sketchability === "planar"
        ? preselectedFaceId
        : null) ??
      preselectedProfileId
    );
  }

  function currentFaceSourceId(): string | null {
    return document?.selected_face_id ?? null;
  }

  function currentAxisSourceId(): string | null {
    const selectedEdgeId = document?.selected_edge_ids[0] ?? null;
    if (selectedEdgeId) {
      return selectedEdgeId;
    }
    const selectedSketchEntityId = document?.selected_sketch_entity_id ?? null;
    if (
      selectedSketchEntityId &&
      sketchLineLabelById.has(selectedSketchEntityId)
    ) {
      return selectedSketchEntityId;
    }
    const selectedFeatureId = document?.selected_feature_id ?? null;
    const selectedFeature = selectedFeatureId
      ? document?.feature_history.find(
          (feature) => feature.feature_id === selectedFeatureId,
        )
      : null;
    if (selectedFeature?.kind === "construction_axis") {
      return selectedFeature.feature_id;
    }
    return null;
  }

  function describeAxisSource(axisId: string): string {
    const feature = document?.feature_history.find(
      (entry) => entry.feature_id === axisId,
    );
    return (
      sketchLineLabelById.get(axisId) ??
      feature?.name ??
      t("geometry.selectedAxis")
    );
  }

  function currentThreadTargetBody(): {
    bodyId: string;
    summary: string;
  } | null {
    const selectedFaceId = document?.selected_face_id ?? null;
    if (selectedFaceId) {
      const face = viewport?.solid_faces.find(
        (entry) => entry.face_id === selectedFaceId,
      );
      if (face) {
        const bodyLabel =
          viewport?.bodies.find((body) => body.id === face.owner_id)?.label ??
          face.owner_id;
        return {
          bodyId: face.owner_id,
          summary: `${bodyLabel} · ${face.label}`,
        };
      }
    }
    const selectedFeatureId = document?.selected_feature_id ?? null;
    if (selectedFeatureId) {
      const body = viewport?.bodies.find((entry) => entry.id === selectedFeatureId);
      if (body) {
        return { bodyId: body.id, summary: body.label };
      }
    }
    return null;
  }

  function describeThreadTarget(bodyId: string): string {
    return (
      viewport?.bodies.find((body) => body.id === bodyId)?.label ??
      document?.feature_history.find((feature) => feature.feature_id === bodyId)
        ?.name ??
      t("geometry.selectedBody")
    );
  }

  function currentPointSourceId(): string | null {
    const selectedVertexId = document?.selected_vertex_ids[0] ?? null;
    if (selectedVertexId) {
      return selectedVertexId;
    }
    return document?.selected_sketch_point_id ?? null;
  }

  async function createMidplaneFeature(sourceIds: [string, string]) {
    const documentPromise = awaitDocumentChange((next, previous) => {
      if (!next.selected_feature_id) {
        return false;
      }
      const previousLength = previous?.feature_history.length ?? 0;
      if (next.feature_history.length <= previousLength) {
        return false;
      }
      const lastFeature = next.feature_history[next.feature_history.length - 1];
      return (
        lastFeature.feature_id === next.selected_feature_id &&
        lastFeature.kind === "construction_plane"
      );
    });

    await runAction(async () => {
      await createMidplane(sourceIds);
      try {
        await documentPromise;
      } catch (error) {
        addMessage(`midplane error: ${String(error)}`);
      }
    });
  }

  async function createTangentPlaneFeature(sourceFaceId: string) {
    const documentPromise = awaitDocumentChange((next, previous) => {
      if (!next.selected_feature_id) {
        return false;
      }
      const previousLength = previous?.feature_history.length ?? 0;
      if (next.feature_history.length <= previousLength) {
        return false;
      }
      const lastFeature = next.feature_history[next.feature_history.length - 1];
      return (
        lastFeature.feature_id === next.selected_feature_id &&
        lastFeature.kind === "construction_plane"
      );
    });

    await runAction(async () => {
      await createTangentPlane(sourceFaceId);
      try {
        await documentPromise;
      } catch (error) {
        addMessage(`tangent plane error: ${String(error)}`);
      }
    });
  }

  async function createAnglePlaneFeature(
    sourcePlaneId: string,
    sourceAxisId: string,
    angleDegrees: number,
  ) {
    const documentPromise = awaitDocumentChange((next, previous) => {
      if (!next.selected_feature_id) {
        return false;
      }
      const previousLength = previous?.feature_history.length ?? 0;
      if (next.feature_history.length <= previousLength) {
        return false;
      }
      const lastFeature = next.feature_history[next.feature_history.length - 1];
      return (
        lastFeature.feature_id === next.selected_feature_id &&
        lastFeature.kind === "construction_plane"
      );
    });

    await runAction(async () => {
      await createAnglePlane(sourcePlaneId, sourceAxisId, angleDegrees);
      try {
        const nextDocument = await documentPromise;
        const newFeatureId = nextDocument.selected_feature_id ?? null;
        if (!newFeatureId) {
          return;
        }
        setAnglePlaneAction({
          phase: "active",
          featureId: newFeatureId,
          sourcePlaneId,
          sourceSummary: describePlaneSource(sourcePlaneId),
          axisId: sourceAxisId,
          axisSummary: describeAxisSource(sourceAxisId),
          initialAngle: angleDegrees,
        });
      } catch (error) {
        addMessage(`angle plane error: ${String(error)}`);
      }
    });
  }

  async function addMidplaneSource(sourceId: string) {
    if (!midplaneAction) {
      return;
    }
    if (midplaneAction.sourceIds.includes(sourceId)) {
      setMidplaneAction({
        sourceIds: midplaneAction.sourceIds.filter((id) => id !== sourceId),
      });
      return;
    }
    const next = [...midplaneAction.sourceIds, sourceId];
    if (next.length >= 2) {
      setMidplaneAction(null);
      await createMidplaneFeature([next[0], next[1]]);
      return;
    }
    setMidplaneAction({ sourceIds: next });
  }

  async function triggerMidplaneAction() {
    if (
      extrudeAction ||
      loftAction ||
      revolveAction ||
      sweepAction ||
      edgeOpAction ||
      shellAction ||
      offsetPlaneAction ||
      midplaneAction ||
      tangentPlaneAction ||
      anglePlaneAction ||
      activeSketchPlaneId
    ) {
      return;
    }
    const firstSourceId = currentPlaneLikeSourceId();
    setMidplaneAction({ sourceIds: firstSourceId ? [firstSourceId] : [] });
  }

  async function triggerTangentPlaneAction() {
    if (
      extrudeAction ||
      loftAction ||
      revolveAction ||
      sweepAction ||
      edgeOpAction ||
      shellAction ||
      offsetPlaneAction ||
      midplaneAction ||
      tangentPlaneAction ||
      anglePlaneAction ||
      activeSketchPlaneId
    ) {
      return;
    }
    const sourceFaceId = currentFaceSourceId();
    if (sourceFaceId) {
      await createTangentPlaneFeature(sourceFaceId);
      return;
    }
    setTangentPlaneAction({ isPending: true });
  }

  async function triggerAnglePlaneAction() {
    if (
      extrudeAction ||
      loftAction ||
      revolveAction ||
      sweepAction ||
      edgeOpAction ||
      shellAction ||
      offsetPlaneAction ||
      midplaneAction ||
      tangentPlaneAction ||
      anglePlaneAction ||
      activeSketchPlaneId
    ) {
      return;
    }
    pendingAngleRef.current = DEFAULT_ANGLE_PLANE_DEGREES;
    const sourcePlaneId = currentPlaneLikeSourceId();
    const sourceAxisId = currentAxisSourceId();
    if (sourcePlaneId && sourceAxisId) {
      await createAnglePlaneFeature(
        sourcePlaneId,
        sourceAxisId,
        DEFAULT_ANGLE_PLANE_DEGREES,
      );
      return;
    }
    if (sourcePlaneId) {
      setAnglePlaneAction({
        phase: "pick_axis",
        sourcePlaneId,
        sourceSummary: describePlaneSource(sourcePlaneId),
        initialAngle: DEFAULT_ANGLE_PLANE_DEGREES,
      });
      return;
    }
    setAnglePlaneAction({
      phase: "pick_plane",
      initialAngle: DEFAULT_ANGLE_PLANE_DEGREES,
    });
  }

  async function createConstructionAxisFeature(sourceId: string) {
    await runAction(async () => {
      try {
        await createConstructionAxis(sourceId);
        setConstructionAxisAction(null);
      } catch (error) {
        addMessage(`axis error: ${String(error)}`);
      }
    });
  }

  async function createConstructionPointFeature(sourceId: string) {
    await runAction(async () => {
      try {
        await createConstructionPoint(sourceId);
        setConstructionPointAction(null);
      } catch (error) {
        addMessage(`point error: ${String(error)}`);
      }
    });
  }

  async function triggerConstructionAxisAction() {
    if (
      extrudeAction ||
      loftAction ||
      revolveAction ||
      sweepAction ||
      edgeOpAction ||
      shellAction ||
      holeAction ||
      offsetPlaneAction ||
      midplaneAction ||
      tangentPlaneAction ||
      anglePlaneAction ||
      constructionAxisAction ||
      constructionPointAction ||
      helixAction ||
      threadAction ||
      fastenerAction ||
      moveAction
    ) {
      return;
    }
    const sourceId = currentAxisSourceId();
    if (sourceId) {
      await createConstructionAxisFeature(sourceId);
      return;
    }
    setConstructionAxisAction({ isPending: true });
  }

  async function triggerConstructionPointAction() {
    if (
      extrudeAction ||
      loftAction ||
      revolveAction ||
      sweepAction ||
      edgeOpAction ||
      shellAction ||
      holeAction ||
      offsetPlaneAction ||
      midplaneAction ||
      tangentPlaneAction ||
      anglePlaneAction ||
      constructionAxisAction ||
      constructionPointAction ||
      helixAction ||
      threadAction ||
      fastenerAction ||
      moveAction
    ) {
      return;
    }
    const sourceId = currentPointSourceId();
    if (sourceId) {
      await createConstructionPointFeature(sourceId);
      return;
    }
    setConstructionPointAction({ isPending: true });
  }

  async function createHelixFeature(
    axisSourceId: string,
    parameters: Partial<HelixFeatureParameters> = {},
  ) {
    const documentPromise = awaitDocumentChange((next, previous) => {
      if (!next.selected_feature_id) {
        return false;
      }
      const previousLength = previous?.feature_history.length ?? 0;
      if (next.feature_history.length <= previousLength) {
        return false;
      }
      const lastFeature = next.feature_history[next.feature_history.length - 1];
      return (
        lastFeature.feature_id === next.selected_feature_id &&
        lastFeature.kind === "helix"
      );
    });

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
        const nextDocument = await documentPromise;
        const newFeatureId = nextDocument.selected_feature_id ?? null;
        if (newFeatureId) {
          setHelixAction({ phase: "active", featureId: newFeatureId });
        }
      } catch (error) {
        addMessage(`helix error: ${String(error)}`);
      }
    });
  }

  async function triggerHelixAction() {
    if (
      extrudeAction ||
      loftAction ||
      revolveAction ||
      sweepAction ||
      edgeOpAction ||
      shellAction ||
      holeAction ||
      offsetPlaneAction ||
      midplaneAction ||
      tangentPlaneAction ||
      anglePlaneAction ||
      constructionAxisAction ||
      constructionPointAction ||
      helixAction ||
      threadAction ||
      fastenerAction ||
      moveAction ||
      activeSketchPlaneId
    ) {
      return;
    }
    const sourceId = currentAxisSourceId();
    if (sourceId) {
      await createHelixFeature(sourceId);
      return;
    }
    setHelixAction({ phase: "pending" });
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

  async function createThreadFeature(
    targetBodyId: string,
    axisSourceId: string,
    parameters: Partial<ThreadFeatureParameters> = {},
  ) {
    const documentPromise = awaitDocumentChange((next, previous) => {
      if (!next.selected_feature_id) {
        return false;
      }
      const previousLength = previous?.feature_history.length ?? 0;
      if (next.feature_history.length <= previousLength) {
        return false;
      }
      const lastFeature = next.feature_history[next.feature_history.length - 1];
      return (
        lastFeature.feature_id === next.selected_feature_id &&
        lastFeature.kind === "thread"
      );
    });

    await runAction(async () => {
      try {
        await createThread({
          ...defaultThreadParameters(targetBodyId, axisSourceId),
          ...parameters,
        });
        const nextDocument = await documentPromise;
        const newFeatureId = nextDocument.selected_feature_id ?? null;
        if (newFeatureId) {
          setThreadAction({
            phase: "active",
            featureId: newFeatureId,
            originalParameters: null,
          });
        }
      } catch (error) {
        addMessage(`thread error: ${String(error)}`);
      }
    });
  }

  async function triggerThreadAction() {
    if (
      extrudeAction ||
      loftAction ||
      revolveAction ||
      sweepAction ||
      edgeOpAction ||
      shellAction ||
      holeAction ||
      offsetPlaneAction ||
      midplaneAction ||
      tangentPlaneAction ||
      anglePlaneAction ||
      constructionAxisAction ||
      constructionPointAction ||
      helixAction ||
      threadAction ||
      fastenerAction ||
      moveAction ||
      activeSketchPlaneId
    ) {
      return;
    }
    const target = currentThreadTargetBody();
    const axisSourceId = currentAxisSourceId();
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

  function defaultFastenerParameters(): FastenerFeatureParameters {
    const standard = findHoleStandard("metric", DEFAULT_FASTENER_SIZE);
    return {
      standard: "metric",
      size: standard?.id ?? DEFAULT_FASTENER_SIZE,
      diameter: standard?.majorDiameter ?? DEFAULT_FASTENER_DIAMETER,
      minor_diameter: standard?.minorDiameter ?? DEFAULT_FASTENER_DIAMETER * 0.84,
      pitch: standard?.pitch ?? 0.8,
      length: DEFAULT_FASTENER_LENGTH,
      thread_length: DEFAULT_FASTENER_THREAD_LENGTH,
      head_type: "socket_head",
      drive_type: "hex_socket",
      thread_representation: "cosmetic",
    };
  }

  async function triggerFastenerAction() {
    if (
      extrudeAction ||
      loftAction ||
      revolveAction ||
      sweepAction ||
      edgeOpAction ||
      shellAction ||
      holeAction ||
      offsetPlaneAction ||
      midplaneAction ||
      tangentPlaneAction ||
      anglePlaneAction ||
      constructionAxisAction ||
      constructionPointAction ||
      helixAction ||
      threadAction ||
      fastenerAction ||
      moveAction ||
      activeSketchPlaneId
    ) {
      return;
    }

    const documentPromise = awaitDocumentChange((next, previous) => {
      if (!next.selected_feature_id) {
        return false;
      }
      const previousLength = previous?.feature_history.length ?? 0;
      if (next.feature_history.length <= previousLength) {
        return false;
      }
      const lastFeature = next.feature_history[next.feature_history.length - 1];
      return (
        lastFeature.feature_id === next.selected_feature_id &&
        lastFeature.kind === "fastener"
      );
    });

    await runAction(async () => {
      try {
        await createFastener(defaultFastenerParameters());
        const nextDocument = await documentPromise;
        const newFeatureId = nextDocument.selected_feature_id ?? null;
        if (newFeatureId) {
          setFastenerAction({
            featureId: newFeatureId,
            originalParameters: null,
          });
        }
      } catch (error) {
        addMessage(`fastener error: ${String(error)}`);
      }
    });
  }

  async function createMoveFeature(
    targetBodyId: string,
    parameters: MoveFeatureParameters = defaultMoveParameters(targetBodyId),
    options: { createdCopyFeatureId?: string | null } = {},
  ) {
    const documentPromise = awaitDocumentChange((next, previous) => {
      if (!next.selected_feature_id) {
        return false;
      }
      const previousLength = previous?.feature_history.length ?? 0;
      if (next.feature_history.length <= previousLength) {
        return false;
      }
      const lastFeature = next.feature_history[next.feature_history.length - 1];
      return (
        lastFeature.feature_id === next.selected_feature_id &&
        lastFeature.kind === "move"
      );
    });

    await runAction(async () => {
      try {
        const seeded = {
          ...parameters,
          target_body_id: targetBodyId,
          is_pending: true,
        };
        await createMove(targetBodyId, seeded);
        const nextDocument = await documentPromise;
        const newFeatureId = nextDocument.selected_feature_id ?? null;
        const created = nextDocument.feature_history.find(
          (feature) => feature.feature_id === newFeatureId,
        );
        if (newFeatureId && created?.move_parameters) {
          setMoveAction({
            phase: "active",
            featureId: newFeatureId,
            targetBodyId,
            parameters: created.move_parameters,
            originalSnapshot: null,
            createdCopyFeatureId: options.createdCopyFeatureId ?? null,
          });
        }
      } catch (error) {
        addMessage(`move error: ${String(error)}`);
        setMoveAction(null);
      }
    });
  }

  function isBodyPlacementActionBlocked() {
    return (
      activeSketchPlaneId ||
      extrudeAction ||
      loftAction ||
      revolveAction ||
      sweepAction ||
      edgeOpAction ||
      shellAction ||
      holeAction ||
      offsetPlaneAction ||
      midplaneAction ||
      tangentPlaneAction ||
      anglePlaneAction ||
      constructionAxisAction ||
      constructionPointAction ||
      helixAction ||
      threadAction ||
      fastenerAction ||
      moveAction
    );
  }

  async function moveBodyFromContext(bodyId: string) {
    if (isBodyPlacementActionBlocked()) {
      return;
    }
    await createMoveFeature(bodyId, defaultMoveParameters(bodyId));
  }

  async function exportBodyAsMesh(bodyId: string) {
    const bodyName =
      document?.feature_history.find((feature) => feature.feature_id === bodyId)
        ?.name ?? document?.name;
    const filePath = await pickExportStlPath(bodyName);
    if (!filePath) {
      return;
    }

    await runAction(async () => {
      await exportBodyStl(filePath, bodyId);
      addMessage(`mesh export requested: ${filePath}`);
    });
  }

  async function copyBodyAndMove(
    sourceBodyId: string,
    copyMode: "linked" | "standalone",
  ) {
    if (isBodyPlacementActionBlocked()) {
      return;
    }

    const documentPromise = awaitDocumentChange((next, previous) => {
      if (!next.selected_feature_id) {
        return false;
      }
      const previousLength = previous?.feature_history.length ?? 0;
      if (next.feature_history.length <= previousLength) {
        return false;
      }
      const lastFeature = next.feature_history[next.feature_history.length - 1];
      return (
        lastFeature.feature_id === next.selected_feature_id &&
        lastFeature.kind === "body_copy" &&
        lastFeature.body_copy_parameters?.source_body_id === sourceBodyId &&
        lastFeature.body_copy_parameters.copy_mode === copyMode
      );
    });

    try {
      await runAction(async () => {
        await createBodyCopy(sourceBodyId, copyMode);
      });
      const nextDocument = await documentPromise;
      const copyBodyId = nextDocument.selected_feature_id ?? null;
      if (copyBodyId) {
        await createMoveFeature(copyBodyId, defaultMoveParameters(copyBodyId), {
          createdCopyFeatureId: copyBodyId,
        });
      }
    } catch (error) {
      addMessage(`copy body error: ${String(error)}`);
    }
  }

  async function triggerMoveAction() {
    if (
      activeSketchPlaneId ||
      extrudeAction ||
      loftAction ||
      revolveAction ||
      sweepAction ||
      edgeOpAction ||
      shellAction ||
      holeAction ||
      offsetPlaneAction ||
      midplaneAction ||
      tangentPlaneAction ||
      anglePlaneAction ||
      constructionAxisAction ||
      constructionPointAction ||
      helixAction ||
      threadAction ||
      fastenerAction ||
      moveAction
    ) {
      return;
    }

    if (selectedMoveBodyId) {
      await createMoveFeature(selectedMoveBodyId, defaultMoveParameters(selectedMoveBodyId));
      return;
    }

    setMoveAction({
      phase: "pending",
      parameters: defaultMoveParameters(),
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

  async function triggerEdgeOpAction(kind: "fillet" | "chamfer") {
    if (
      extrudeAction ||
      loftAction ||
      revolveAction ||
      sweepAction ||
      edgeOpAction ||
      shellAction ||
      holeAction ||
      offsetPlaneAction ||
      midplaneAction ||
      tangentPlaneAction ||
      anglePlaneAction ||
      constructionAxisAction ||
      constructionPointAction ||
      helixAction ||
      threadAction ||
      fastenerAction ||
      moveAction ||
      activeSketchPlaneId
    ) {
      return;
    }
    const initialValue =
      kind === "fillet" ? DEFAULT_FILLET_RADIUS : DEFAULT_CHAMFER_DISTANCE;
    pendingValueRef.current = initialValue;

    const preSelectedEdgeIds = document?.selected_edge_ids ?? [];
    if (preSelectedEdgeIds.length === 0) {
      // No edges yet — open the panel in "pending" mode and let the
      // user pick edges in the viewport.
      setEdgeOpAction({ phase: "pending", kind, initialValue });
      return;
    }

    // Pre-selected edges: behave like common CAD workflow's "select-then-invoke"
    // shortcut and create the feature immediately.
    await createEdgeOpFeature(kind, preSelectedEdgeIds, initialValue);
  }

  // Creates the fillet/chamfer feature in the core and transitions the
  // panel into the "active" phase once the freshly-created feature id
  // has come back over IPC.
  async function createEdgeOpFeature(
    kind: "fillet" | "chamfer",
    edgeIds: string[],
    value: number,
  ) {
    if (edgeIds.length === 0) {
      return;
    }
    // Same fire-and-forget IPC trick as triggerExtrudeAction: subscribe to
    // the next document update that contains a freshly-created feature of
    // the requested kind so we can pick up its real id.
    const documentPromise = awaitDocumentChange((next, previous) => {
      if (!next.selected_feature_id) {
        return false;
      }
      const previousLength = previous?.feature_history.length ?? 0;
      if (next.feature_history.length <= previousLength) {
        return false;
      }
      const lastFeature = next.feature_history[next.feature_history.length - 1];
      return (
        lastFeature.feature_id === next.selected_feature_id &&
        lastFeature.kind === kind
      );
    });

    await runAction(async () => {
      if (kind === "fillet") {
        await createFillet(edgeIds, value);
      } else {
        await createChamfer(edgeIds, value);
      }
      try {
        const nextDocument = await documentPromise;
        const newFeatureId = nextDocument.selected_feature_id ?? null;
        if (!newFeatureId) {
          return;
        }
        activeEdgeIdsRef.current = [...edgeIds];
        setEdgeOpAction({
          phase: "active",
          kind,
          featureId: newFeatureId,
          initialValue: value,
          edgeIds: [...edgeIds],
        });
      } catch (error) {
        addMessage(`${kind} action error: ${String(error)}`);
      }
    });
  }

  async function createShellFeature(faceId: string, thickness: number) {
    const documentPromise = awaitDocumentChange((next, previous) => {
      if (!next.selected_feature_id) {
        return false;
      }
      const previousLength = previous?.feature_history.length ?? 0;
      if (next.feature_history.length <= previousLength) {
        return false;
      }
      const lastFeature = next.feature_history[next.feature_history.length - 1];
      return (
        lastFeature.feature_id === next.selected_feature_id &&
        lastFeature.kind === "shell"
      );
    });

    await runAction(async () => {
      await createShell(faceId, thickness);
      try {
        const nextDocument = await documentPromise;
        const newFeatureId = nextDocument.selected_feature_id ?? null;
        if (!newFeatureId) {
          return;
        }
        setShellAction({
          phase: "active",
          featureId: newFeatureId,
          faceId,
          faceSummary: describePlaneSource(faceId),
          initialThickness: thickness,
        });
      } catch (error) {
        addMessage(`shell action error: ${String(error)}`);
      }
    });
  }

  async function triggerShellAction() {
    if (
      extrudeAction ||
      loftAction ||
      revolveAction ||
      sweepAction ||
      edgeOpAction ||
      shellAction ||
      holeAction ||
      offsetPlaneAction ||
      midplaneAction ||
      tangentPlaneAction ||
      anglePlaneAction ||
      constructionAxisAction ||
      constructionPointAction ||
      helixAction ||
      threadAction ||
      activeSketchPlaneId
    ) {
      return;
    }
    pendingShellThicknessRef.current = DEFAULT_SHELL_THICKNESS;
    const selectedFaceId = document?.selected_face_id ?? null;
    if (selectedFaceId) {
      await createShellFeature(selectedFaceId, DEFAULT_SHELL_THICKNESS);
      return;
    }
    setShellAction({
      phase: "pending",
      initialThickness: DEFAULT_SHELL_THICKNESS,
    });
  }

  async function createHoleFeature(
    faceId: string,
    parameters: Partial<HoleFeatureParameters> = {},
  ) {
    const face = viewport?.solid_faces.find((entry) => entry.face_id === faceId);
    if (!face) {
      addMessage("hole action error: selected face is no longer available");
      return;
    }
    const documentPromise = awaitDocumentChange((next, previous) => {
      if (!next.selected_feature_id) {
        return false;
      }
      const previousLength = previous?.feature_history.length ?? 0;
      if (next.feature_history.length <= previousLength) {
        return false;
      }
      const lastFeature = next.feature_history[next.feature_history.length - 1];
      return (
        lastFeature.feature_id === next.selected_feature_id &&
        lastFeature.kind === "hole"
      );
    });

    await runAction(async () => {
      await createHole(faceId, face.center, {
        hole_type: "simple",
        extent_type: "blind",
        diameter: DEFAULT_HOLE_DIAMETER,
        depth: DEFAULT_HOLE_DEPTH,
        counterbore_diameter: DEFAULT_HOLE_DIAMETER * 1.6,
        counterbore_depth: 2,
        countersink_diameter: DEFAULT_HOLE_DIAMETER * 1.6,
        countersink_angle_degrees: 82,
        standard: "custom",
        standard_size: "",
        hole_fit: "clearance",
        thread_enabled: false,
        thread_spec: "",
        thread_pitch: 0,
        major_diameter: 0,
        minor_diameter: 0,
        thread_depth: DEFAULT_HOLE_DEPTH,
        thread_representation: "cosmetic",
        ...parameters,
      });
      try {
        const nextDocument = await documentPromise;
        const newFeatureId = nextDocument.selected_feature_id ?? null;
        if (newFeatureId) {
          setHoleAction({ phase: "active", featureId: newFeatureId });
        }
      } catch (error) {
        addMessage(`hole action error: ${String(error)}`);
      }
    });
  }

  async function triggerHoleAction() {
    if (
      extrudeAction ||
      loftAction ||
      revolveAction ||
      sweepAction ||
      edgeOpAction ||
      shellAction ||
      holeAction ||
      offsetPlaneAction ||
      midplaneAction ||
      tangentPlaneAction ||
      anglePlaneAction ||
      constructionAxisAction ||
      constructionPointAction ||
      helixAction ||
      activeSketchPlaneId
    ) {
      return;
    }
    const selectedFaceId = document?.selected_face_id ?? null;
    if (selectedFaceId) {
      await createHoleFeature(selectedFaceId);
      return;
    }
    setHoleAction({ phase: "pending" });
  }

  async function cancelActiveTool() {
    // Central Escape/Cancel path for app-level tools. Sketch mode is
    // deliberately excluded here; sketch drafting Escape stays owned by
    // ViewportPanel so Esc never exits an active sketch.
    if (extrudeAction) {
      if (extrudeAction.phase === "active" && extrudeAction.featureId) {
        const snapshot = extrudeAction.originalSnapshot;
        if (snapshot) {
          await runAction(async () => {
            await updateExtrudeDepth(extrudeAction.featureId!, snapshot.depth);
            await updateExtrudeMode(extrudeAction.featureId!, snapshot.mode);
            await updateExtrudeTargetBody(
              extrudeAction.featureId!,
              snapshot.targetBodyId,
            );
            await updateExtrudeParameters(
              extrudeAction.featureId!,
              snapshot.parameters,
            );
          });
        } else {
          await undoUntilExtrudePreviewRemoved(
            extrudeAction.featureIds.length > 0
              ? extrudeAction.featureIds
              : [extrudeAction.featureId],
          );
        }
      }
      setExtrudeAction(null);
      await restoreTimelineCursorAfterEdit();
      return true;
    }

    if (loftAction) {
      if (loftAction.originalSnapshot && loftAction.featureId) {
        const snapshot = loftAction.originalSnapshot;
        await runAction(async () => {
          await updateLoftProfiles(loftAction.featureId!, snapshot.profileIds);
          await updateLoftRuled(loftAction.featureId!, snapshot.ruled);
        });
      } else if (loftAction.phase === "active") {
        await runAction(async () => {
          await undo();
        });
      }
      setLoftAction(null);
      await restoreTimelineCursorAfterEdit();
      return true;
    }

    if (revolveAction) {
      if (revolveAction.originalSnapshot && revolveAction.featureId) {
        const snapshot = revolveAction.originalSnapshot;
        await runAction(async () => {
          await updateRevolveProfile(revolveAction.featureId!, snapshot.profileId);
          await updateRevolveAxis(revolveAction.featureId!, snapshot.axisEntityId);
          await updateRevolveAngle(
            revolveAction.featureId!,
            snapshot.angleDegrees,
          );
        });
      } else if (revolveAction.phase === "active") {
        await runAction(async () => {
          await undo();
        });
      }
      setRevolveAction(null);
      await restoreTimelineCursorAfterEdit();
      return true;
    }

    if (sweepAction) {
      if (sweepAction.originalSnapshot && sweepAction.featureId) {
        const snapshot = sweepAction.originalSnapshot;
        await runAction(async () => {
          await updateSweepProfile(sweepAction.featureId!, snapshot.profileId);
          await updateSweepPath(sweepAction.featureId!, snapshot.pathEntityId);
        });
      } else if (sweepAction.phase === "active") {
        await runAction(async () => {
          await undo();
        });
      }
      setSweepAction(null);
      await restoreTimelineCursorAfterEdit();
      return true;
    }

    if (moveAction) {
      if (moveAction.phase === "active") {
        if (moveAction.originalSnapshot) {
          await runAction(async () => {
            await updateMoveParameters(
              moveAction.featureId,
              moveAction.originalSnapshot!,
            );
          });
        } else {
          await runAction(async () => {
            await undo();
            if (moveAction.createdCopyFeatureId) {
              await undo();
            }
          });
        }
      }
      setMoveAction(null);
      await restoreTimelineCursorAfterEdit();
      return true;
    }

    if (importAction) {
      if (importAction.featureId) {
        await runAction(async () => {
          if (importAction.kind === "image") {
            await cancelImageImport(importAction.featureId!);
          } else {
            await cancelSvgImport(importAction.featureId!);
          }
        });
      }
      setImportAction(null);
      return true;
    }

    if (edgeOpAction) {
      if (edgeOpAction.phase === "active") {
        await runAction(async () => {
          await undo();
        });
      }
      activeEdgeIdsRef.current = [];
      setEdgeOpAction(null);
      return true;
    }

    if (shellAction) {
      if (shellAction.phase === "active") {
        await runAction(async () => {
          await undo();
        });
      }
      setShellAction(null);
      return true;
    }

    if (holeAction) {
      if (holeAction.phase === "active") {
        await runAction(async () => {
          await undo();
        });
      }
      setHoleAction(null);
      return true;
    }

    if (offsetPlaneAction) {
      if (offsetPlaneAction.phase === "active") {
        await runAction(async () => {
          await undo();
        });
      }
      setOffsetPlaneAction(null);
      return true;
    }

    if (anglePlaneAction) {
      if (anglePlaneAction.phase === "active") {
        await runAction(async () => {
          await undo();
        });
      }
      setAnglePlaneAction(null);
      return true;
    }

    if (midplaneAction) {
      setMidplaneAction(null);
      return true;
    }

    if (tangentPlaneAction) {
      setTangentPlaneAction(null);
      return true;
    }

    if (constructionAxisAction) {
      setConstructionAxisAction(null);
      return true;
    }

    if (constructionPointAction) {
      setConstructionPointAction(null);
      return true;
    }

    if (threadAction) {
      if (threadAction.phase === "active") {
        await runAction(async () => {
          if (threadAction.originalParameters) {
            await updateThreadParameters(
              threadAction.featureId,
              threadAction.originalParameters,
            );
          } else {
            await undo();
          }
        });
        await restoreTimelineCursorAfterEdit();
      }
      setThreadAction(null);
      return true;
    }

    if (fastenerAction) {
      await runAction(async () => {
        if (fastenerAction.originalParameters) {
          await updateFastenerParameters(
            fastenerAction.featureId,
            fastenerAction.originalParameters,
          );
        } else {
          await undo();
        }
      });
      setFastenerAction(null);
      await restoreTimelineCursorAfterEdit();
      return true;
    }

    if (helixAction) {
      if (helixAction.phase === "active") {
        await runAction(async () => {
          await undo();
        });
      }
      setHelixAction(null);
      return true;
    }

    if (editingFeatureId) {
      setEditingFeatureId(null);
      await restoreTimelineCursorAfterEdit();
      return true;
    }

    if (materialsPanelOpen) {
      setMaterialsPanelOpen(false);
      return true;
    }

    return false;
  }

  useEffect(() => {
    function isTypingTarget(target: EventTarget | null) {
      return (
        target instanceof HTMLInputElement ||
        target instanceof HTMLTextAreaElement ||
        target instanceof HTMLSelectElement ||
        (target instanceof HTMLElement && target.isContentEditable)
      );
    }

    function handleKeyDown(event: KeyboardEvent) {
      if (event.defaultPrevented) {
        return;
      }

      const target = event.target;
      const hasCancelableTool =
        Boolean(
          extrudeAction ||
            loftAction ||
            revolveAction ||
            sweepAction ||
            moveAction ||
            edgeOpAction ||
            shellAction ||
            holeAction ||
            offsetPlaneAction ||
            anglePlaneAction ||
            midplaneAction ||
            tangentPlaneAction ||
            constructionAxisAction ||
            constructionPointAction ||
            threadAction ||
            fastenerAction ||
            helixAction ||
            editingFeatureId,
        ) || materialsPanelOpen;

      if (event.code === "Escape" && hasCancelableTool) {
        event.preventDefault();
        event.stopPropagation();
        event.stopImmediatePropagation();
        void cancelActiveTool();
        return;
      }

      if (isTypingTarget(target)) {
        return;
      }

      if (
        event.code === "KeyS" &&
        (IS_MACOS ? event.metaKey : event.ctrlKey) &&
        !event.altKey &&
        !event.shiftKey
      ) {
        event.preventDefault();
        void runAction(async () => {
          await saveCurrentDocument();
        });
        return;
      }

      if (
        event.code === "Escape" &&
        !activeSketchPlaneId &&
        !extrudeAction &&
        !loftAction &&
        !revolveAction &&
        !sweepAction &&
        !edgeOpAction &&
        !shellAction &&
        !threadAction &&
        !fastenerAction &&
        !moveAction &&
        !materialsPanelOpen &&
        document &&
        (document.selected_feature_id ||
          document.selected_reference_id ||
          document.selected_face_id ||
          document.selected_edge_ids.length > 0 ||
          document.selected_vertex_ids.length > 0)
      ) {
        event.preventDefault();
        void runAction(clearSelection);
        return;
      }

      if (matchesHotkey(event, config.hotkeys.global.undo)) {
        event.preventDefault();
        if (session?.can_undo) {
          void runAction(undo);
        }
        return;
      }

      if (matchesHotkey(event, config.hotkeys.global.redo)) {
        event.preventDefault();
        if (session?.can_redo) {
          void runAction(redo);
        }
        return;
      }

      // Trigger configured toolbar actions.
      // Chamfer intentionally has no hotkey — it's invoked from the
      // Modify ribbon button only.
      if (matchesHotkey(event, config.hotkeys.toolbar.extrude)) {
        event.preventDefault();
        void triggerExtrudeAction();
        return;
      }

      // Fillet is gated to non-sketch mode — body edges aren't
      // selectable inside an active sketch anyway, but the explicit
      // guard keeps the hotkey from surprising the user during
      // sketching. No edge-selection requirement: pressing F with
      // nothing selected opens the panel in "pending" mode and the
      // user picks edges in the viewport, mirroring the toolbar
      // button. (`triggerEdgeOpAction` already handles both phases.)
      if (matchesHotkey(event, config.hotkeys.toolbar.fillet)) {
        if (activeSketchPlaneId) {
          return;
        }
        event.preventDefault();
        void triggerEdgeOpAction("fillet");
        return;
      }

      if (matchesHotkey(event, config.hotkeys.sketchToolbar.createSketch)) {
        if (
          activeSketchPlaneId ||
          (!selectedReference && !selectedSketchableFace)
        ) {
          return;
        }
        event.preventDefault();
        void triggerCreateSketchAction();
        return;
      }

      // P: toggle the modal Project tool inside an active sketch.
      // While the tool is active, viewport face / edge / vertex
      // clicks are routed to `project_*_into_sketch` instead of the
      // normal selection (see App.tsx click intercepts). Pressing P
      // again (or Esc, or picking another tool) switches back to
      // Select. No-op outside sketch mode.
      if (matchesHotkey(event, config.hotkeys.toolbar.project)) {
        if (!activeSketchPlaneId) {
          return;
        }
        event.preventDefault();
        const nextTool = activeSketchTool === "project" ? "select" : "project";
        void runAction(async () => {
          await setSketchTool(nextTool);
        });
        return;
      }
    }

    window.addEventListener("keydown", handleKeyDown);
    return () => {
      window.removeEventListener("keydown", handleKeyDown);
    };
  }, [
    selectedSketchProfile,
    extrudeAction,
    loftAction,
    revolveAction,
    sweepAction,
    moveAction,
    edgeOpAction,
    shellAction,
    holeAction,
    offsetPlaneAction,
    anglePlaneAction,
    midplaneAction,
    tangentPlaneAction,
    constructionAxisAction,
    constructionPointAction,
    threadAction,
    fastenerAction,
    helixAction,
    editingFeatureId,
    materialsPanelOpen,
    activeSketchPlaneId,
    activeSketchTool,
    document?.selected_edge_ids,
    document?.selected_face_id,
    document?.selected_feature_id,
    document?.selected_reference_id,
    document?.selected_vertex_ids,
    viewport?.solid_faces,
    session?.can_undo,
    session?.can_redo,
    config.hotkeys.global,
    config.hotkeys.toolbar,
    config.hotkeys.sketchToolbar.createSketch,
    document,
    currentProjectPath,
  ]);

  function clearArmedSketchConstraint() {
    setArmedSketchConstraint(null);
  }

  async function handleSketchConstraintLinePick(lineId: string, additive = false) {
    if (!armedSketchConstraint) {
      await selectSketchEntity(lineId, additive);
      return;
    }

    if (armedSketchConstraint.kind === "coincident") {
      await selectSketchEntity(lineId);
      return;
    }

    if (armedSketchConstraint.kind === "horizontal") {
      await setSketchLineConstraint(lineId, "horizontal");
      clearArmedSketchConstraint();
      return;
    }

    if (armedSketchConstraint.kind === "vertical") {
      await setSketchLineConstraint(lineId, "vertical");
      clearArmedSketchConstraint();
      return;
    }

    if (armedSketchConstraint.kind === "clear") {
      await clearSketchLineConstraints(lineId);
      // Stay armed so the user can clear multiple lines without
      // re‑clicking the toolbar button. Escape or another button
      // will un‑arm it.
      return;
    }

    const firstLineId =
      "firstLineId" in armedSketchConstraint
        ? armedSketchConstraint.firstLineId
        : null;

    if (!firstLineId) {
      await selectSketchEntity(lineId);
      setArmedSketchConstraint({
        kind: armedSketchConstraint.kind,
        firstLineId: lineId,
      });
      return;
    }

    if (firstLineId === lineId) {
      return;
    }

    if (armedSketchConstraint.kind === "equal_length") {
      await setSketchEqualLengthConstraint(lineId, firstLineId);
    } else if (armedSketchConstraint.kind === "parallel") {
      await setSketchParallelConstraint(lineId, firstLineId);
    } else {
      await setSketchPerpendicularConstraint(lineId, firstLineId);
    }
    // Keep the constraint armed so the user can immediately pick
    // another pair without re‑clicking the toolbar button.
    // Escape, another button, or a tool switch will un‑arm it.
    setArmedSketchConstraint({
      kind: armedSketchConstraint.kind,
      firstLineId: null,
    });
  }

  async function handleSketchConstraintPointPick(
    pointId: string,
    kind: "endpoint" | "center" | "quadrant",
    additive = false,
  ) {
    if (!armedSketchConstraint || armedSketchConstraint.kind !== "coincident") {
      await selectSketchPoint(pointId, additive);
      return;
    }

    if (!armedSketchConstraint.firstPointId) {
      await selectSketchPoint(pointId);
      setArmedSketchConstraint({
        kind: "coincident",
        firstPointId: pointId,
      });
      return;
    }

    if (armedSketchConstraint.firstPointId === pointId) {
      return;
    }

    await setSketchCoincidentConstraint(
      pointId,
      armedSketchConstraint.firstPointId,
    );
    // Stay armed so the user can pick more point pairs without
    // re‑clicking the toolbar button.
    setArmedSketchConstraint({
      kind: "coincident",
      firstPointId: null,
    });
  }

  function makeDefaultExportBaseName(name?: string | null) {
    return (
      (name ?? document?.name ?? "polysmith-part")
        .trim()
        .toLowerCase()
        .replace(/[^a-z0-9]+/g, "-")
        .replace(/^-+|-+$/g, "") || "polysmith-part"
    );
  }

  async function pickExportPath() {
    const filePath = await save({
      title: t("dialogs.exportStepTitle"),
      defaultPath: `${makeDefaultExportBaseName()}.step`,
      filters: [
        {
          name: t("dialogs.stepFileType"),
          extensions: ["step", "stp"],
        },
      ],
    });

    if (filePath === null) {
      addMessage("export canceled");
      return null;
    }

    return filePath;
  }

  async function pickExportStlPath(baseName?: string | null) {
    const filePath = await save({
      title: t("dialogs.exportMeshTitle"),
      defaultPath: `${makeDefaultExportBaseName(baseName)}.stl`,
      filters: [
        {
          name: t("dialogs.stlFileType"),
          extensions: ["stl"],
        },
      ],
    });

    if (filePath === null) {
      addMessage("export canceled");
      return null;
    }

    return filePath;
  }

  async function pickSaveDocumentPath() {
    const filePath = await save({
      title: t("dialogs.saveDocumentTitle"),
      defaultPath: `${makeDefaultExportBaseName()}.polysmith`,
      filters: [
        {
          name: t("dialogs.polysmithDocumentType"),
          extensions: ["polysmith", "json"],
        },
      ],
    });

    if (filePath === null) {
      addMessage("save canceled");
      return null;
    }
    return filePath;
  }

  async function pickLoadDocumentPath() {
    const result = await open({
      title: t("dialogs.openDocumentTitle"),
      multiple: false,
      directory: false,
      filters: [
        {
          name: t("dialogs.polysmithDocumentType"),
          extensions: ["polysmith", "json"],
        },
      ],
    });

    if (result === null || Array.isArray(result)) {
      addMessage("open canceled");
      return null;
    }
    return result;
  }

  async function recordRecentProject(
    filePath: string,
    thumbnailDataUrl: string | null,
    parentFolderId?: string | null,
  ) {
    const baseDocument = recentProjectsDocumentRef.current;
    const existing = baseDocument.projects.find(
      (project) => project.path === filePath,
    );
    const nextProjectsDocument = upsertRecentProject(baseDocument, {
      path: filePath,
      name: existing?.name,
      thumbnailDataUrl: thumbnailDataUrl ?? existing?.thumbnailDataUrl ?? null,
      parentFolderId,
    });
    recentProjectsDocumentRef.current = nextProjectsDocument;
    setRecentProjectsDocument(nextProjectsDocument);
    await saveRecentProjects(nextProjectsDocument);
  }

  async function updateRecentProjectsDocument(
    nextProjectsDocument: RecentProjectsDocument,
  ) {
    recentProjectsDocumentRef.current = nextProjectsDocument;
    setRecentProjectsDocument(nextProjectsDocument);
    await saveRecentProjects(nextProjectsDocument);
  }

  async function createRecentProjectFolder(
    name: string,
    parentFolderId: string | null,
  ) {
    const baseDocument = recentProjectsDocumentRef.current;
    await updateRecentProjectsDocument(
      createProjectFolder(baseDocument, name, parentFolderId),
    );
  }

  async function moveRecentProject(projectPath: string, folderId: string | null) {
    const baseDocument = recentProjectsDocumentRef.current;
    await updateRecentProjectsDocument(
      moveProjectToFolder(baseDocument, projectPath, folderId),
    );
  }

  async function renameRecentProjectEntry(project: RecentProject, name: string) {
    const baseDocument = recentProjectsDocumentRef.current;
    await updateRecentProjectsDocument(
      renameRecentProject(baseDocument, project.path, name),
    );
  }

  async function deleteRecentProject(
    project: RecentProject,
    shouldDeleteFile: boolean,
  ) {
    const wasActiveProject = project.path === currentProjectPath;
    if (shouldDeleteFile) {
      await deleteProjectFile(project.path);
    }
    const baseDocument = recentProjectsDocumentRef.current;
    await updateRecentProjectsDocument(
      removeProjectFromRecentProjects(baseDocument, project.path),
    );
    if (wasActiveProject) {
      await performCreateDocument();
      setSidebarTab("projects");
    }
  }

  async function deleteRecentProjectFolder(folderId: string) {
    const baseDocument = recentProjectsDocumentRef.current;
    await updateRecentProjectsDocument(
      deleteProjectFolder(baseDocument, folderId),
    );
  }

  async function renameRecentProjectFolder(folderId: string, name: string) {
    const baseDocument = recentProjectsDocumentRef.current;
    await updateRecentProjectsDocument(
      renameProjectFolder(baseDocument, folderId, name),
    );
  }

  async function requestOpenRecentProject(project: RecentProject) {
    if (project.path === currentProjectPath) {
      return;
    }
    let exists = false;
    try {
      exists = await projectFileExists(project.path);
    } catch (error) {
      addMessage(`project file check error: ${String(error)}`);
      return;
    }
    if (!exists) {
      addMessage(t("projects.openMissingFile", { name: project.name }));
      await deleteRecentProject(project, false);
      return;
    }
    requestUnsavedGate({
      kind: "load",
      filePath: project.path,
    });
  }

  function captureProjectThumbnail() {
    return snapshotCaptureRef.current?.() ?? null;
  }

  async function saveCurrentDocument(parentFolderId?: string | null) {
    if (!document) {
      return false;
    }
    const filePath = currentProjectPath ?? (await pickSaveDocumentPath());
    if (!filePath) {
      return false;
    }

    const savedPromise = awaitDocumentSaved((savedPath) => savedPath === filePath);
    try {
      await saveDocument(filePath);
      await savedPromise;
    } catch (error) {
      void savedPromise.catch(() => {});
      throw error;
    }
    const thumbnailDataUrl = captureProjectThumbnail();
    await writeProjectThumbnail(filePath, thumbnailDataUrl);
    const savedDocument = useCadCoreStore.getState().document ?? document;
    setCurrentProjectPath(filePath);
    setSavedDocumentBaseline({
      documentId: savedDocument.document_id,
      revision: savedDocument.revision,
    });
    try {
      await recordRecentProject(filePath, thumbnailDataUrl, parentFolderId);
    } catch (error) {
      addMessage(`recent projects save error: ${String(error)}`);
    }
    addMessage(`saved: ${filePath}`);
    return true;
  }

  async function performCreateDocument() {
    const documentPromise = awaitDocumentChange(
      (next, previous) => next.document_id !== previous?.document_id,
    );
    await createDocument();
    const nextDocument = await documentPromise;
    setCurrentProjectPath(null);
    setSavedDocumentBaseline(null);
    setHiddenFeatureIds(new Set<string>());
    setHiddenCategories(new Set<CategoryId>());
    originVisibilityManuallyChangedRef.current = false;
    addMessage(`created: ${nextDocument.name}`);
  }

  async function performCreateAndSaveProject(parentFolderId: string | null) {
    await performCreateDocument();
    await saveCurrentDocument(parentFolderId);
  }

  async function performLoadDocument(filePath: string) {
    const documentPromise = awaitDocumentChange(() => true);
    await loadDocument(filePath);
    const loadedDocument = await documentPromise;
    setCurrentProjectPath(filePath);
    setSavedDocumentBaseline({
      documentId: loadedDocument.document_id,
      revision: loadedDocument.revision,
    });
    setSidebarTab("hierarchy");
    const loadedDocumentHasSolidBody = documentHasSolidBody(loadedDocument);
    setHiddenFeatureIds(defaultHiddenSketchIdsForLoadedDocument(loadedDocument));
    setHiddenCategories(
      loadedDocumentHasSolidBody
        ? new Set<CategoryId>(["origin"])
        : new Set<CategoryId>(),
    );
    originVisibilityManuallyChangedRef.current = false;
    const thumbnailDataUrl = await readProjectThumbnail(filePath);
    try {
      await recordRecentProject(filePath, thumbnailDataUrl);
    } catch (error) {
      addMessage(`recent projects save error: ${String(error)}`);
    }
    addMessage(`loaded: ${filePath}`);
  }

  async function executePendingAction(action: PendingUnsavedAction) {
    if (action.kind === "quit") {
      allowAppCloseRef.current = true;
      await getCurrentWindow().destroy();
      return;
    }
    if (action.kind === "new") {
      await runAction(performCreateDocument);
      return;
    }
    if (action.kind === "newProject") {
      await runAction(async () => {
        await performCreateAndSaveProject(action.parentFolderId);
      });
      return;
    }
    await runAction(async () => {
      await performLoadDocument(action.filePath);
    });
  }

  function requestUnsavedGate(action: PendingUnsavedAction) {
    if (isDocumentDirty) {
      setPendingUnsavedAction(action);
      return;
    }
    void executePendingAction(action);
  }

  async function saveThenContinuePendingAction() {
    if (!pendingUnsavedAction) {
      return;
    }
    const action = pendingUnsavedAction;
    await runAction(async () => {
      const didSave = await saveCurrentDocument();
      if (!didSave) {
        return;
      }
      setPendingUnsavedAction(null);
      await executePendingAction(action);
    });
  }

  function discardThenContinuePendingAction() {
    if (!pendingUnsavedAction) {
      return;
    }
    const action = pendingUnsavedAction;
    setPendingUnsavedAction(null);
    void executePendingAction(action);
  }

  async function runAction(action: () => Promise<void>) {
    try {
      await action();
    } catch (error) {
      addMessage(`action error: ${String(error)}`);
    }
  }

  function readSlicerViewportBounds(): SlicerViewportBounds | null {
    const container = slicerViewportRef.current;
    if (!container) {
      return null;
    }

    const rect = container.getBoundingClientRect();
    if (rect.width < 1 || rect.height < 1) {
      return null;
    }

    return {
      x: rect.left,
      y: rect.top,
      width: rect.width,
      height: rect.height,
      scaleFactor: window.devicePixelRatio || 1,
    };
  }

  function waitForNextFrame() {
    return new Promise<void>((resolve) => {
      window.requestAnimationFrame(() => resolve());
    });
  }

  async function showCadView() {
    setWorkspaceView("cad");
    setSlicerStatus(null);
    if (!hasOrcaEmbedSession) {
      return;
    }
    try {
      const result = await hideOrcaWindow();
      addMessage(`slicer: ${result.message}`);
    } catch (error) {
      addMessage(`slicer hide error: ${String(error)}`);
    } finally {
      setHasOrcaEmbedSession(false);
    }
  }

  async function showSlicerView() {
    setWorkspaceView("slicer");

    if (!config.orcaSlicer.enabled) {
      setSlicerStatus(t("workspace.slicerDisabled"));
      return;
    }

    // Web mode — the iframe handles everything, no native embed needed.
    if (config.orcaSlicer.integrationMode === "web") {
      setSlicerStatus(null);
      return;
    }

    setSlicerStatus(t("workspace.openingSlicer"));
    const binaryPath = config.orcaSlicer.binaryPath.trim();
    if (!binaryPath) {
      setSlicerStatus(t("workspace.slicerBinaryMissing"));
      return;
    }

    try {
      await waitForNextFrame();
      const bounds = readSlicerViewportBounds();
      if (!bounds) {
        setSlicerStatus(t("workspace.slicerContainerUnavailable"));
        return;
      }

      const result = await embedOrcaWindow({
        binaryPath,
        modelFilePath: null,
        bounds,
      });
      setSlicerStatus(result.message);
      setHasOrcaEmbedSession(result.status === "embedded");
      addMessage(`slicer: ${result.message}`);
    } catch (error) {
      const message = String(error);
      setSlicerStatus(t("workspace.slicerEmbedFailed", { error: message }));
      addMessage(`slicer error: ${message}`);
    }
  }

  async function exportToSlicer() {
    if (!hasExportableBody) {
      setSlicerStatus(t("workspace.slicerNoExportableBody"));
      return;
    }
    if (!config.orcaSlicer.enabled) {
      setSlicerStatus(t("workspace.slicerDisabled"));
      return;
    }

    // Web mode — export STL, upload to Docker OrcaSlicer, then show iframe.
    if (config.orcaSlicer.integrationMode === "web") {
      try {
        setSlicerStatus(t("workspace.exportingToSlicer"));
        const exportPath = await prepareOrcaExportPath();
        await exportDocumentStl(exportPath);
        await awaitDocumentExport(
          (result) =>
            result.format === "stl" && result.file_path === exportPath,
        );
        await uploadStlToOrcaWeb(exportPath);
        setWorkspaceView("slicer");
        setSlicerStatus(null);
        addMessage("slicer: exported STL to OrcaSlicer web.");
      } catch (error) {
        const message = String(error);
        setSlicerStatus(
          t("workspace.slicerEmbedFailed", { error: message }),
        );
        addMessage(`slicer web export error: ${message}`);
      }
      return;
    }

    const binaryPath = config.orcaSlicer.binaryPath.trim();
    if (!binaryPath) {
      setSlicerStatus(t("workspace.slicerBinaryMissing"));
      return;
    }

    try {
      if (IS_MACOS) {
        setSlicerStatus(t("workspace.exportingToSlicer"));
        const exportPath = await prepareOrcaExportPath();
        await exportDocumentStl(exportPath);
        await awaitDocumentExport(
          (result) => result.format === "stl" && result.file_path === exportPath,
        );

        const result = await embedOrcaWindow({
          binaryPath,
          modelFilePath: exportPath,
          bounds: STANDALONE_SLICER_BOUNDS,
        });
        setSlicerStatus(result.message);
        addMessage(`slicer export: ${result.message}`);
        return;
      }

      setWorkspaceView("slicer");
      setSlicerStatus(t("workspace.exportingToSlicer"));
      await waitForNextFrame();
      const bounds = readSlicerViewportBounds();
      if (!bounds) {
        setSlicerStatus(t("workspace.slicerContainerUnavailable"));
        return;
      }

      const exportPath = await prepareOrcaExportPath();
      await exportDocumentStl(exportPath);
      await awaitDocumentExport(
        (result) => result.format === "stl" && result.file_path === exportPath,
      );

      const result = await embedOrcaWindow({
        binaryPath,
        modelFilePath: exportPath,
        bounds,
      });
      setSlicerStatus(result.message);
      setHasOrcaEmbedSession(result.status === "embedded");
      addMessage(`slicer export: ${result.message}`);
    } catch (error) {
      const message = String(error);
      setSlicerStatus(t("workspace.slicerEmbedFailed", { error: message }));
      addMessage(`slicer export error: ${message}`);
    }
  }

  // When the workspace view dropdown opens, the HTML popup extends below
  // the header into the content area where the native Orca X11 window sits.
  // Native windows always render above the WebView, so we temporarily hide
  // the Orca window to keep the dropdown visible. The ref guards against a
  // race where workspaceView changes before the close event fires.
  function handleWorkspaceDropdownOpenChange(isOpen: boolean) {
    if (!hasOrcaEmbedSession || workspaceViewRef.current !== "slicer") {
      return;
    }
    void setOrcaMapped(!isOpen).catch((error) => {
      addMessage(`slicer map error: ${String(error)}`);
    });
  }

  useEffect(() => {
    if (workspaceView !== "slicer" || !hasOrcaEmbedSession) {
      return;
    }

    let frameId = 0;
    const syncBounds = () => {
      window.cancelAnimationFrame(frameId);
      frameId = window.requestAnimationFrame(() => {
        const bounds = readSlicerViewportBounds();
        if (!bounds) {
          return;
        }
        void resizeOrcaWindow(bounds).catch((error) => {
          addMessage(`slicer resize error: ${String(error)}`);
        });
      });
    };

    const observer = new ResizeObserver(syncBounds);
    if (slicerViewportRef.current) {
      observer.observe(slicerViewportRef.current);
    }
    window.addEventListener("resize", syncBounds);
    syncBounds();

    return () => {
      window.cancelAnimationFrame(frameId);
      window.removeEventListener("resize", syncBounds);
      observer.disconnect();
    };
  }, [workspaceView, hasOrcaEmbedSession, addMessage]);

  // Shared delete handler used by both the timeline and hierarchy
  // context menus. Walks the dependency graph and prompts the user
  // when downstream features would be broken; silently deletes when
  // the feature is a leaf. Also closes the edit panel if the
  // deleted feature was being edited.
  function confirmAndDeleteFeature(featureId: string) {
    if (!document) {
      return;
    }
    const feature = document.feature_history.find(
      (entry) => entry.feature_id === featureId,
    );
    if (
      feature?.kind === "sketch" &&
      document.active_sketch_feature_id === featureId
    ) {
      addMessage(t("timeline.activeSketchDeleteBlocked"));
      return;
    }
    const dependents = findDependents(document, featureId);
    if (dependents.length > 0) {
      const names = dependents
        .map((entry) => entry.name || entry.kind)
        .join(", ");
      const confirmed = window.confirm(
        `Deleting this feature will break ${dependents.length} downstream feature(s): ${names}. Delete anyway?`,
      );
      if (!confirmed) {
        return;
      }
    }
    void runAction(async () => {
      await deleteFeature(featureId);
      setEditingFeatureId((current) =>
        current === featureId ? null : current,
      );
    });
  }

  function confirmAndUnlinkBodyCopy(featureId: string) {
    const confirmed = window.confirm(t("dialogs.unlinkLinkedCopyMessage"));
    if (!confirmed) {
      return;
    }
    void runAction(async () => {
      await unlinkBodyCopy(featureId);
    });
  }

  function currentSketchSelection() {
    const entityIds = [
      ...(document?.selected_sketch_entity_ids ?? []),
      ...(document?.selected_sketch_entity_id
        ? [document.selected_sketch_entity_id]
        : []),
    ];
    const pointIds = [
      ...(document?.selected_sketch_point_ids ?? []),
      ...(document?.selected_sketch_point_id
        ? [document.selected_sketch_point_id]
        : []),
    ];
    const profileIds = [
      ...(document?.selected_sketch_profile_ids ?? []),
      ...(document?.selected_sketch_profile_id
        ? [document.selected_sketch_profile_id]
        : []),
    ];
    return {
      entityIds: [...new Set(entityIds)],
      pointIds: [...new Set(pointIds)],
      profileIds: [...new Set(profileIds)],
    };
  }

  function sketchSelectionAffectsExtrude(selection: SketchDeleteSelection) {
    if (!document?.active_sketch_feature_id || !activeSketchFeature) {
      return [];
    }
    const sketch = activeSketchFeature.sketch_parameters;
    if (!sketch) {
      return [];
    }

    const entityIds = new Set(selection.entityIds);
    for (const pointId of selection.pointIds) {
      for (const line of sketch.lines) {
        if (
          line.start_point_id === pointId ||
          line.end_point_id === pointId
        ) {
          entityIds.add(line.line_id);
        }
      }
      for (const arc of sketch.arcs ?? []) {
        if (arc.start_point_id === pointId || arc.end_point_id === pointId) {
          entityIds.add(arc.arc_id);
        }
      }
      for (const circle of sketch.circles) {
        if (`point-circle-${circle.circle_id}-center` === pointId) {
          entityIds.add(circle.circle_id);
        }
      }
    }

    const affectedProfileIds = new Set(selection.profileIds);
    for (const profile of sketch.profiles) {
      const usesSelectedLine = profile.line_ids.some((id) =>
        entityIds.has(id),
      );
      const usesSelectedCircle =
        profile.source_circle_id !== null &&
        entityIds.has(profile.source_circle_id);
      if (usesSelectedLine || usesSelectedCircle) {
        affectedProfileIds.add(profile.profile_id);
      }
    }

    if (affectedProfileIds.size === 0) {
      return [];
    }

    return document.feature_history.filter((feature) => {
      if (
        feature.kind !== "extrude" ||
        !feature.extrude_parameters ||
        feature.extrude_parameters.sketch_feature_id !==
          document.active_sketch_feature_id
      ) {
        return false;
      }
      const sourceProfileIds =
        feature.extrude_parameters.profile_ids.length > 0
          ? feature.extrude_parameters.profile_ids
          : [feature.extrude_parameters.profile_id];
      return sourceProfileIds.some((profileId) =>
        affectedProfileIds.has(profileId),
      );
    });
  }

  function deleteSketchSelectionNow(selection: SketchDeleteSelection) {
    void runAction(async () => {
      await deleteSketchSelection(
        selection.entityIds,
        selection.pointIds,
        selection.profileIds,
      );
    });
  }

  function confirmAndDeleteSketchSelection(selection?: SketchDeleteSelection) {
    if (!document?.active_sketch_feature_id) {
      return;
    }
    const deleteSelection = selection ?? currentSketchSelection();
    const { entityIds, pointIds, profileIds } = deleteSelection;
    if (
      entityIds.length === 0 &&
      pointIds.length === 0 &&
      profileIds.length === 0
    ) {
      return;
    }

    const dependents = sketchSelectionAffectsExtrude({
      entityIds,
      pointIds,
      profileIds,
    });
    if (dependents.length > 0) {
      setPendingSketchDeleteConfirmation({
        selection: deleteSelection,
        affectedFeatureNames: dependents.map(
          (entry) => entry.name || entry.kind,
        ),
      });
      return;
    }

    deleteSketchSelectionNow(deleteSelection);
  }

  async function uploadStlToOrcaWeb(stlPath: string): Promise<void> {
    const webUrl = config.orcaSlicer.webUrl.trim();
    // Read the STL file and upload it to the OrcaSlicer Docker instance.
    const response = await fetch(stlPath);
    if (!response.ok) {
      throw new Error(`Failed to read STL file: ${response.statusText}`);
    }
    const blob = await response.blob();

    const formData = new FormData();
    formData.append("file", blob, "model.stl");

    const uploadResponse = await fetch(`${webUrl}/api/upload`, {
      method: "POST",
      body: formData,
    });
    if (!uploadResponse.ok) {
      throw new Error(
        `OrcaSlicer web upload failed (${uploadResponse.status}): ${uploadResponse.statusText}`,
      );
    }
  }

  function renderSlicerWorkspace() {
    if (
      config.orcaSlicer.enabled &&
      config.orcaSlicer.integrationMode === "web"
    ) {
      return (
        <section className="relative flex min-h-0 min-w-0 flex-1 flex-col">
          <div className="flex items-center gap-2 border-b border-white/10 px-3 py-1.5">
            <span className="text-xs text-on-surface-muted">
              OrcaSlicer Web
            </span>
            <div className="ml-auto">
              <button
                type="button"
                className="rounded-md px-2 py-0.5 text-xs text-on-surface-muted transition-colors hover:bg-white/10 hover:text-on-surface"
                onClick={() => {
                  void openUrl(config.orcaSlicer.webUrl).catch(
                    (error) => {
                      addMessage(
                        `slicer: failed to open browser: ${String(error)}`,
                      );
                    },
                  );
                }}
              >
                {t("workspace.openInBrowser")}
              </button>
            </div>
          </div>
          <iframe
            src={config.orcaSlicer.webUrl}
            className="flex-1 w-full border-0"
            title="OrcaSlicer"
            allow="autoplay;camera;microphone;fullscreen"
          />
        </section>
      );
    }

    return (
      <section className="relative flex min-h-0 min-w-0 flex-1 flex-col">
        <div
          id="slicer-viewport-container"
          ref={slicerViewportRef}
          className="flex min-h-0 w-full min-w-0 flex-1 items-center justify-center bg-surface-lowest"
        >
          {hasOrcaEmbedSession ? null : (
            <span className="max-w-xl px-6 text-center text-sm text-on-surface-muted">
              {slicerStatus ?? t("workspace.slicerWaiting")}
            </span>
          )}
        </div>
      </section>
    );
  }

  return (
    <main className="cad-shell h-screen">
      <div className="grid h-full min-h-0 grid-rows-[auto_minmax(0,1fr)_auto]">
          <AppHeader
          workspaceView={workspaceView}
          canOpenSlicerView={
            isSlicerConfigured &&
            (config.orcaSlicer.integrationMode === "web" || !IS_MACOS)
          }
          canExportToSlicer={canExportToSlicer}
          onSetWorkspaceView={(view) => {
            if (view === "cad") {
              void showCadView();
              return;
            }
            void showSlicerView();
          }}
          onExportToSlicer={() => void exportToSlicer()}
          status={status}
          disabled={status !== "connected"}
          canUndo={session?.can_undo ?? false}
          canRedo={session?.can_redo ?? false}
          activeSketchPlaneId={activeSketchPlaneId}
          activeSketchTool={activeSketchTool}
          selectedReferenceId={selectedReference?.reference_id ?? null}
          selectedFaceId={selectedSketchableFace?.face_id ?? null}
          armedSketchConstraint={armedSketchConstraint}
          isMirrorToolOpen={isMirrorToolOpen}
          arcToolMode={arcToolMode}
          onSetArcToolMode={setArcToolMode}
          rectangleToolMode={rectangleToolMode}
          onSetRectangleToolMode={setRectangleToolMode}
          circleToolMode={circleToolMode}
          onSetCircleToolMode={setCircleToolMode}
          polygonToolMode={polygonToolMode}
          onSetPolygonToolMode={setPolygonToolMode}

          onStart={async () => {
            await runAction(start);
          }}
          onStartMirrorTool={async () => {
            // Idempotent: clicking Mirror while it's already open
            // re-focuses the Objects slot (a contextual modeling "I'd
            // like to redo my selection from scratch" gesture
            // would be Cancel + reopen, but we keep this lighter).
            await runAction(async () => {
              await startMirrorPreview();
              setMirrorFocusedSlot("objects");
              clearArmedSketchConstraint();
            });
          }}
          onCreateDocument={async () => {
            requestUnsavedGate({ kind: "new" });
          }}
          onExportDocument={async () => {
            const filePath = await pickExportPath();
            if (!filePath) {
              return;
            }

            await runAction(async () => {
              await exportDocument(filePath);
              addMessage(`export requested: ${filePath}`);
            });
          }}
          onSaveDocument={async () => {
            await runAction(async () => {
              await saveCurrentDocument();
            });
          }}
          onLoadDocument={async () => {
            const filePath = await pickLoadDocumentPath();
            if (!filePath) {
              return;
            }

            requestUnsavedGate({ kind: "load", filePath });
          }}
          onImportImage={() => triggerImportAction("image")}
          onImportSvg={() => triggerImportAction("svg")}
          onUndo={async () => {
            await runAction(undo);
          }}
          onRedo={async () => {
            await runAction(redo);
          }}
          logCount={logs.length}
          errorLogCount={errorLogCount}
          onOpenLogs={() => {
            setIsLogsOpen(true);
          }}
          onOpenSettings={() => {
            setIsSettingsOpen(true);
          }}
          showAiAssistant={isAiAssistantAvailable}
          isAiPanelOpen={isAiPanelOpen}
          onToggleAiPanel={() => {
            setIsAiPanelOpen((current) => !current);
          }}
          onAddBoxFeature={async (width, height, depth) => {
            await runAction(async () => {
              await addBoxFeature(width, height, depth);
            });
          }}
          onAddCylinderFeature={async (radius, height) => {
            await runAction(async () => {
              await addCylinderFeature(radius, height);
            });
          }}
          canExtrude={
            !loftAction &&
            !revolveAction &&
            !sweepAction &&
            !shellAction &&
            !holeAction &&
            !constructionAxisAction &&
            !constructionPointAction &&
            !helixAction &&
            !threadAction &&
            !fastenerAction &&
            !moveAction &&
            (!extrudeAction || extrudeAction.phase === "pending")
          }
          onExtrude={triggerExtrudeAction}
          canLoft={
            !activeSketchPlaneId &&
            !extrudeAction &&
            !loftAction &&
            !revolveAction &&
            !sweepAction &&
            !edgeOpAction &&
            !shellAction &&
            !holeAction &&
            !offsetPlaneAction &&
            !midplaneAction &&
            !tangentPlaneAction &&
            !anglePlaneAction &&
            !constructionAxisAction &&
            !constructionPointAction &&
            !helixAction &&
            !threadAction &&
            !fastenerAction &&
            !moveAction
          }
          onLoft={triggerLoftAction}
          canRevolve={
            !activeSketchPlaneId &&
            !extrudeAction &&
            !loftAction &&
            !revolveAction &&
            !sweepAction &&
            !edgeOpAction &&
            !shellAction &&
            !holeAction &&
            !offsetPlaneAction &&
            !midplaneAction &&
            !tangentPlaneAction &&
            !anglePlaneAction &&
            !constructionAxisAction &&
            !constructionPointAction &&
            !helixAction &&
            !threadAction &&
            !fastenerAction &&
            !moveAction
          }
          onRevolve={triggerRevolveAction}
          canSweep={
            !activeSketchPlaneId &&
            !extrudeAction &&
            !loftAction &&
            !revolveAction &&
            !sweepAction &&
            !edgeOpAction &&
            !shellAction &&
            !holeAction &&
            !offsetPlaneAction &&
            !midplaneAction &&
            !tangentPlaneAction &&
            !anglePlaneAction &&
            !constructionAxisAction &&
            !constructionPointAction &&
            !helixAction &&
            !threadAction &&
            !fastenerAction &&
            !moveAction
          }
          onSweep={triggerSweepAction}
          canHole={
            !activeSketchPlaneId &&
            !extrudeAction &&
            !loftAction &&
            !revolveAction &&
            !sweepAction &&
            !edgeOpAction &&
            !shellAction &&
            !holeAction &&
            !offsetPlaneAction &&
            !midplaneAction &&
            !tangentPlaneAction &&
            !anglePlaneAction &&
            !constructionAxisAction &&
            !constructionPointAction &&
            !helixAction &&
            !threadAction &&
            !fastenerAction &&
            !moveAction
          }
          onHole={triggerHoleAction}
          canThread={
            !activeSketchPlaneId &&
            !extrudeAction &&
            !loftAction &&
            !revolveAction &&
            !sweepAction &&
            !edgeOpAction &&
            !shellAction &&
            !holeAction &&
            !offsetPlaneAction &&
            !midplaneAction &&
            !tangentPlaneAction &&
            !anglePlaneAction &&
            !constructionAxisAction &&
            !constructionPointAction &&
            !helixAction &&
            !threadAction &&
            !fastenerAction &&
            !moveAction
          }
          onThread={triggerThreadAction}
          canFastener={
            !activeSketchPlaneId &&
            !extrudeAction &&
            !loftAction &&
            !revolveAction &&
            !sweepAction &&
            !edgeOpAction &&
            !shellAction &&
            !holeAction &&
            !offsetPlaneAction &&
            !midplaneAction &&
            !tangentPlaneAction &&
            !anglePlaneAction &&
            !constructionAxisAction &&
            !constructionPointAction &&
            !helixAction &&
            !threadAction &&
            !fastenerAction &&
            !moveAction
          }
          onFastener={triggerFastenerAction}
          // Modify ribbon: Fillet / Chamfer can be invoked at any
          // time outside a sketch / other floating action. Edge
          // selection is *not* required — the panel opens in
          // "pending" mode and waits for the user to click edges in
          // the viewport.
          canEdgeOp={
            !activeSketchPlaneId &&
            !extrudeAction &&
            !loftAction &&
            !revolveAction &&
            !sweepAction &&
            !edgeOpAction &&
            !shellAction &&
            !holeAction &&
            !offsetPlaneAction &&
            !midplaneAction &&
            !tangentPlaneAction &&
            !anglePlaneAction &&
            !constructionAxisAction &&
            !constructionPointAction &&
            !helixAction &&
            !threadAction &&
            !fastenerAction &&
            !moveAction
          }
          onFillet={async () => {
            await triggerEdgeOpAction("fillet");
          }}
          onChamfer={async () => {
            await triggerEdgeOpAction("chamfer");
          }}
          canMove={
            !activeSketchPlaneId &&
            !extrudeAction &&
            !loftAction &&
            !revolveAction &&
            !sweepAction &&
            !edgeOpAction &&
            !shellAction &&
            !holeAction &&
            !offsetPlaneAction &&
            !midplaneAction &&
            !tangentPlaneAction &&
            !anglePlaneAction &&
            !constructionAxisAction &&
            !constructionPointAction &&
            !helixAction &&
            !threadAction &&
            !fastenerAction &&
            !moveAction
          }
          onMove={async () => {
            await triggerMoveAction();
          }}
          canShell={
            !activeSketchPlaneId &&
            !extrudeAction &&
            !loftAction &&
            !revolveAction &&
            !sweepAction &&
            !edgeOpAction &&
            !shellAction &&
            !holeAction &&
            !offsetPlaneAction &&
            !midplaneAction &&
            !tangentPlaneAction &&
            !anglePlaneAction &&
            !constructionAxisAction &&
            !constructionPointAction &&
            !helixAction &&
            !threadAction &&
            !fastenerAction &&
            !moveAction
          }
          onShell={async () => {
            await triggerShellAction();
          }}
          canOffsetPlane={
            !activeSketchPlaneId &&
            !extrudeAction &&
            !loftAction &&
            !revolveAction &&
            !sweepAction &&
            !edgeOpAction &&
            !shellAction &&
            !offsetPlaneAction &&
            !midplaneAction &&
            !tangentPlaneAction &&
            !anglePlaneAction &&
            !constructionAxisAction &&
            !constructionPointAction &&
            !helixAction &&
            !threadAction &&
            !fastenerAction &&
            !moveAction
          }
          onOffsetPlane={() => {
            void triggerOffsetPlaneAction();
          }}
          canMidplane={
            !activeSketchPlaneId &&
            !extrudeAction &&
            !loftAction &&
            !revolveAction &&
            !sweepAction &&
            !edgeOpAction &&
            !shellAction &&
            !offsetPlaneAction &&
            !midplaneAction &&
            !tangentPlaneAction &&
            !anglePlaneAction &&
            !constructionAxisAction &&
            !constructionPointAction &&
            !helixAction &&
            !threadAction &&
            !fastenerAction &&
            !moveAction
          }
          canTangentPlane={
            !activeSketchPlaneId &&
            !extrudeAction &&
            !loftAction &&
            !revolveAction &&
            !sweepAction &&
            !edgeOpAction &&
            !shellAction &&
            !offsetPlaneAction &&
            !midplaneAction &&
            !tangentPlaneAction &&
            !anglePlaneAction &&
            !constructionAxisAction &&
            !constructionPointAction &&
            !helixAction &&
            !threadAction &&
            !fastenerAction &&
            !moveAction
          }
          canAnglePlane={
            !activeSketchPlaneId &&
            !extrudeAction &&
            !loftAction &&
            !revolveAction &&
            !sweepAction &&
            !edgeOpAction &&
            !shellAction &&
            !offsetPlaneAction &&
            !midplaneAction &&
            !tangentPlaneAction &&
            !anglePlaneAction &&
            !constructionAxisAction &&
            !constructionPointAction &&
            !helixAction &&
            !threadAction &&
            !fastenerAction &&
            !moveAction
          }
          canConstructionAxis={
            !extrudeAction &&
            !loftAction &&
            !revolveAction &&
            !sweepAction &&
            !edgeOpAction &&
            !shellAction &&
            !offsetPlaneAction &&
            !midplaneAction &&
            !tangentPlaneAction &&
            !anglePlaneAction &&
            !constructionAxisAction &&
            !constructionPointAction &&
            !helixAction &&
            !threadAction &&
            !fastenerAction &&
            !moveAction
          }
          canConstructionPoint={
            !extrudeAction &&
            !loftAction &&
            !revolveAction &&
            !sweepAction &&
            !edgeOpAction &&
            !shellAction &&
            !offsetPlaneAction &&
            !midplaneAction &&
            !tangentPlaneAction &&
            !anglePlaneAction &&
            !constructionAxisAction &&
            !constructionPointAction &&
            !helixAction &&
            !threadAction &&
            !fastenerAction &&
            !moveAction
          }
          canHelix={
            !activeSketchPlaneId &&
            !extrudeAction &&
            !loftAction &&
            !revolveAction &&
            !sweepAction &&
            !edgeOpAction &&
            !shellAction &&
            !holeAction &&
            !offsetPlaneAction &&
            !midplaneAction &&
            !tangentPlaneAction &&
            !anglePlaneAction &&
            !constructionAxisAction &&
            !constructionPointAction &&
            !helixAction &&
            !threadAction &&
            !fastenerAction
          }
          onMidplane={() => {
            void triggerMidplaneAction();
          }}
          onTangentPlane={() => {
            void triggerTangentPlaneAction();
          }}
          onAnglePlane={() => {
            void triggerAnglePlaneAction();
          }}
          onConstructionAxis={() => {
            void triggerConstructionAxisAction();
          }}
          onConstructionPoint={() => {
            void triggerConstructionPointAction();
          }}
          onHelix={() => {
            void triggerHelixAction();
          }}
          onStartSketch={triggerCreateSketchAction}
          onFinishSketch={async () => {
            await runAction(async () => {
              clearArmedSketchConstraint();
              await finishSketch();
              await restoreTimelineCursorAfterEdit();
            });
          }}
          onSetSketchTool={async (tool) => {
            await runAction(async () => {
              clearArmedSketchConstraint();
              await setSketchTool(tool);
            });
          }}
          onArmSketchConstraint={async (constraint) => {
            let shouldArm = true;

            setArmedSketchConstraint((current) => {
              const isSameConstraint =
                current &&
                current.kind === constraint &&
                (constraint !== "equal_length" &&
                constraint !== "coincident" &&
                constraint !== "perpendicular" &&
                constraint !== "parallel"
                  ? true
                  : current.kind === constraint);

              if (isSameConstraint) {
                shouldArm = false;
                return null;
              }

              // Mirror is no longer an armed constraint — the
              // toolbar's Mirror button calls
              // `onStartMirrorTool` directly. Defensively handle
              // a stray "mirror" arming request as a no-op so we
              // don't desync the toolbar's button state.
              if (constraint === "mirror") {
                shouldArm = false;
                return current;
              }
              return constraint === "equal_length" ||
                constraint === "coincident" ||
                constraint === "perpendicular" ||
                constraint === "parallel"
                ? constraint === "coincident"
                  ? { kind: constraint, firstPointId: null }
                  : { kind: constraint, firstLineId: null }
                : ({ kind: constraint } as ArmedSketchConstraint);
            });

            if (shouldArm && activeSketchTool !== "select") {
              await runAction(async () => {
                await setSketchTool("select");
              });
            }
          }}
          onCancelSketchConstraint={clearArmedSketchConstraint}
          onWorkspaceDropdownOpenChange={handleWorkspaceDropdownOpenChange}
          parametersPanelOpen={parametersPanelOpen}
          onToggleParametersPanel={() => {
            setParametersPanelOpen((current) => !current);
          }}
          filterPanelOpen={filterPanelOpen}
          onToggleFilterPanel={() => {
            setFilterPanelOpen((current) => !current);
          }}
          materialsPanelOpen={materialsPanelOpen}
          onToggleMaterialsPanel={() => {
            setMaterialsPanelOpen((current) => !current);
          }}
          onUpdateSelectionFilter={updateSelectionFilter}
        />

        <div className="flex min-h-0 min-w-0">
          {isLogsOpen ? (
            <LogsWindow
              logs={logs}
              onClose={() => {
                setIsLogsOpen(false);
              }}
              onClear={clearLogs}
            />
          ) : null}
          {isSettingsOpen ? (
            <SettingsModal
              onClose={() => {
                setIsSettingsOpen(false);
              }}
            />
          ) : null}
          {workspaceView === "slicer" ? (
            renderSlicerWorkspace()
          ) : (
            <>
          {isHierarchyCollapsed ? (
            <button
              type="button"
              className="cad-sidebar-collapsed"
              onClick={() => setIsHierarchyCollapsed(false)}
              aria-label={t("document.expandHierarchyPanel")}
              title={t("document.expandHierarchy")}
            >
              <span className="cad-sidebar-collapsed-label">
                {t("document.hierarchyProjects")}
              </span>
            </button>
          ) : (
            <aside
              className="cad-sidebar relative min-h-0 flex-shrink-0"
              style={{ width: hierarchyWidth }}
            >
              <div className="flex h-full min-h-0 flex-col">
                <div className="flex items-center justify-between gap-2 px-3 pt-2">
                  <div className="cad-sidebar-tabs" role="tablist">
                    <button
                      type="button"
                      className={
                        sidebarTab === "projects"
                          ? "cad-sidebar-tab cad-sidebar-tab-active"
                          : "cad-sidebar-tab"
                      }
                      onClick={() => setSidebarTab("projects")}
                      role="tab"
                      aria-selected={sidebarTab === "projects"}
                    >
                      {t("projects.title")}
                    </button>
                    <button
                      type="button"
                      className={
                        sidebarTab === "hierarchy"
                          ? "cad-sidebar-tab cad-sidebar-tab-active"
                          : "cad-sidebar-tab"
                      }
                      onClick={() => setSidebarTab("hierarchy")}
                      role="tab"
                      aria-selected={sidebarTab === "hierarchy"}
                    >
                      {t("document.hierarchy")}
                    </button>
                  </div>
                  <button
                    type="button"
                    className="cad-sidebar-collapse-button"
                    onClick={() => setIsHierarchyCollapsed(true)}
                    aria-label={t("document.collapseHierarchyPanel")}
                    title={t("document.collapse")}
                  >
                    ◀
                  </button>
                </div>
                {sidebarTab === "hierarchy" ? (
                  <DocumentHierarchyPanel
                    document={document}
                    hiddenFeatureIds={hiddenFeatureIds}
                    hiddenCategories={hiddenCategories}
                    onToggleFeatureVisibility={(featureId) => {
                      setHiddenFeatureIds((current) => {
                        const next = new Set(current);
                        if (next.has(featureId)) {
                          next.delete(featureId);
                        } else {
                          next.add(featureId);
                        }
                        return next;
                      });
                    }}
                    onToggleCategoryVisibility={(category) => {
                      if (category === "origin") {
                        originVisibilityManuallyChangedRef.current = true;
                      }
                      setHiddenCategories((current) => {
                        const next = new Set(current);
                        if (next.has(category)) {
                          next.delete(category);
                        } else {
                          next.add(category);
                        }
                        return next;
                      });
                    }}
                    onSelectFeature={async (featureId) => {
                      await runAction(async () => {
                        await selectFeature(featureId);
                      });
                    }}
                    onSelectReference={async (referenceId) => {
                      await runAction(async () => {
                        await selectReference(referenceId);
                      });
                    }}
                    onReenterSketch={async (featureId) => {
                      await runAction(async () => {
                        await reenterSketch(featureId);
                      });
                    }}
                    onRenameFeature={async (featureId, name) => {
                      await runAction(async () => {
                        await renameFeature(featureId, name);
                      });
                    }}
                    onDeleteFeature={async (featureId) => {
                      confirmAndDeleteFeature(featureId);
                    }}
                    onMoveBody={async (bodyId) => {
                      await moveBodyFromContext(bodyId);
                    }}
                    onCopyBody={async (bodyId, copyMode) => {
                      await copyBodyAndMove(bodyId, copyMode);
                    }}
                    onExportBodyMesh={async (bodyId) => {
                      await exportBodyAsMesh(bodyId);
                    }}
                    onUnlinkBodyCopy={(featureId) => {
                      confirmAndUnlinkBodyCopy(featureId);
                    }}
                    onSetFeatureSuppressed={async (featureId, suppressed) => {
                      await runAction(async () => {
                        await setFeatureSuppressed(featureId, suppressed);
                      });
                    }}
                  />
                ) : (
                  <ProjectsPanel
                    document={recentProjectsDocument}
                    activeProjectPath={currentProjectPath}
                    onOpenProject={(project) => {
                      void runAction(async () => {
                        await requestOpenRecentProject(project);
                      });
                    }}
                    onCreateFolder={(name, parentFolderId) => {
                      void runAction(async () => {
                        await createRecentProjectFolder(name, parentFolderId);
                      });
                    }}
                    onMoveProject={(projectPath, folderId) => {
                      void runAction(async () => {
                        await moveRecentProject(projectPath, folderId);
                      });
                    }}
                    onDeleteProject={(project, shouldDeleteFile) => {
                      void runAction(async () => {
                        await deleteRecentProject(project, shouldDeleteFile);
                      });
                    }}
                    onDeleteFolder={(folderId) => {
                      void runAction(async () => {
                        await deleteRecentProjectFolder(folderId);
                      });
                    }}
                    onRenameProject={(project, name) => {
                      void runAction(async () => {
                        await renameRecentProjectEntry(project, name);
                      });
                    }}
                    onRenameFolder={(folderId, name) => {
                      void runAction(async () => {
                        await renameRecentProjectFolder(folderId, name);
                      });
                    }}
                    onCreateProject={(parentFolderId) => {
                      requestUnsavedGate({
                        kind: "newProject",
                        parentFolderId,
                      });
                    }}
                  />
                )}
              </div>
              <div
                className="cad-sidebar-resizer"
                onPointerDown={(event) => {
                  // Pointer-driven drag: capture the start position
                  // and width, then update on every move until the
                  // user releases. Width is clamped to keep the
                  // panel usable.
                  event.preventDefault();
                  const startX = event.clientX;
                  const startWidth = hierarchyWidth;
                  const onMove = (moveEvent: PointerEvent) => {
                    const next = Math.max(
                      220,
                      Math.min(640, startWidth + (moveEvent.clientX - startX)),
                    );
                    setHierarchyWidth(next);
                  };
                  const onUp = () => {
                    window.removeEventListener("pointermove", onMove);
                    window.removeEventListener("pointerup", onUp);
                  };
                  window.addEventListener("pointermove", onMove);
                  window.addEventListener("pointerup", onUp);
                }}
              />
            </aside>
          )}

          <section className="relative min-h-0 min-w-0 flex-1">
            <ViewportPanel
              status={status}
              document={document}
              viewport={viewport}
              moveGizmo={
                moveAction?.phase === "active" && activeMoveParameters
                  ? (() => {
                      const body = viewport?.bodies.find(
                        (entry) => entry.id === moveAction.targetBodyId,
                      );
                      return body
                        ? {
                            bodyId: body.id,
                            center: body.center,
                            size: body.size,
                            localFrame: body.local_frame,
                            parameters: activeMoveParameters,
                            disabled: status !== "connected",
                          }
                        : null;
                    })()
                  : null
              }
              onMoveGizmoChange={async (parameters) => {
                if (moveAction?.phase !== "active") {
                  return;
                }
                await runAction(async () => {
                  await updateMoveParameters(moveAction.featureId, parameters);
                });
                setMoveAction((current) =>
                  current?.phase === "active" &&
                  current.featureId === moveAction.featureId
                    ? { ...current, parameters }
                    : current,
                );
              }}
              onMoveBody={async (bodyId) => {
                await moveBodyFromContext(bodyId);
              }}
              onCopyBody={async (bodyId, copyMode) => {
                await copyBodyAndMove(bodyId, copyMode);
              }}
              onExportBodyMesh={async (bodyId) => {
                await exportBodyAsMesh(bodyId);
              }}
              onUnlinkBodyCopy={(featureId) => {
                confirmAndUnlinkBodyCopy(featureId);
              }}
              inactiveSketchEntityPickEnabled={
                revolveAction !== null ||
                sweepAction !== null ||
                constructionAxisAction !== null ||
                helixAction !== null ||
                threadAction !== null
              }
              onPickInactiveSketchLine={async (lineId) => {
                if (threadAction?.phase === "pick_axis") {
                  await createThreadFeature(threadAction.targetBodyId, lineId);
                  return;
                }
                if (threadAction?.phase === "pick_target") {
                  setThreadAction({ ...threadAction, axisSourceId: lineId });
                  return;
                }
                if (helixAction?.phase === "pending") {
                  await createHelixFeature(lineId);
                  return;
                }
                if (constructionAxisAction) {
                  await createConstructionAxisFeature(lineId);
                  return;
                }
                if (revolveAction) {
                  setRevolveAction((current) =>
                    current ? {...current, axisEntityId: lineId} : current,
                  );
                  return;
                }
                if (sweepAction) {
                  setSweepAction((current) =>
                    current ? {...current, pathEntityId: lineId} : current,
                  );
                }
              }}
              onSnapshotCaptureReady={(capture) => {
                snapshotCaptureRef.current = capture;
              }}
              onSelectPrimitive={async (primitiveId) => {
                if (moveAction?.phase === "pending") {
                  const body = viewport?.bodies.find(
                    (entry) => entry.id === primitiveId,
                  );
                  if (body) {
                    await createMoveFeature(body.id, moveAction.parameters);
                  }
                  return;
                }
                if (
                  threadAction?.phase === "pick_target" ||
                  threadAction?.phase === "pick_axis"
                ) {
                  const body = viewport?.bodies.find(
                    (entry) => entry.id === primitiveId,
                  );
                  if (!body) {
                    return;
                  }
                  if (threadAction.phase === "pick_target") {
                    if (threadAction.axisSourceId) {
                      await createThreadFeature(body.id, threadAction.axisSourceId);
                    } else {
                      setThreadAction({
                        phase: "pick_axis",
                        targetBodyId: body.id,
                        targetSummary: body.label,
                      });
                    }
                    return;
                  }
                  setThreadAction({
                    ...threadAction,
                    targetBodyId: body.id,
                    targetSummary: body.label,
                  });
                  return;
                }
                await runAction(async () => {
                  await selectFeature(primitiveId);
                });
              }}
              onSelectReference={async (referenceId) => {
                // Offset Plane pending phase: the next plane click is
                // the source pick. Create the feature with the
                // currently-typed offset and let the panel transition
                // to its active phase. We deliberately do *not* also
                // call selectReference here — the core's
                // create_offset_plane already routes selection state.
                if (
                  offsetPlaneAction &&
                  offsetPlaneAction.phase === "pending"
                ) {
                  await createOffsetPlaneFeature(
                    referenceId,
                    pendingOffsetRef.current,
                  );
                  return;
                }
                if (midplaneAction) {
                  await addMidplaneSource(referenceId);
                  return;
                }
                if (anglePlaneAction?.phase === "pick_plane") {
                  setAnglePlaneAction({
                    phase: "pick_axis",
                    sourcePlaneId: referenceId,
                    sourceSummary: describePlaneSource(referenceId),
                    initialAngle: pendingAngleRef.current,
                  });
                  return;
                }
                await runAction(async () => {
                  await selectReference(referenceId);
                });
              }}
              onSelectFace={async (faceId) => {
                if (moveAction?.phase === "pending") {
                  const face = viewport?.solid_faces.find(
                    (entry) => entry.face_id === faceId,
                  );
                  if (face) {
                    await createMoveFeature(face.owner_id, moveAction.parameters);
                  }
                  return;
                }
                if (
                  threadAction?.phase === "pick_target" ||
                  threadAction?.phase === "pick_axis"
                ) {
                  const face = viewport?.solid_faces.find(
                    (entry) => entry.face_id === faceId,
                  );
                  if (!face) {
                    return;
                  }
                  const bodyLabel =
                    viewport?.bodies.find((body) => body.id === face.owner_id)
                      ?.label ?? face.owner_id;
                  const summary = `${bodyLabel} · ${face.label}`;
                  if (threadAction.phase === "pick_target") {
                    if (threadAction.axisSourceId) {
                      await createThreadFeature(
                        face.owner_id,
                        threadAction.axisSourceId,
                      );
                    } else {
                      setThreadAction({
                        phase: "pick_axis",
                        targetBodyId: face.owner_id,
                        targetSummary: summary,
                      });
                    }
                    return;
                  }
                  setThreadAction({
                    ...threadAction,
                    targetBodyId: face.owner_id,
                    targetSummary: summary,
                  });
                  return;
                }
                if (holeAction?.phase === "pending") {
                  const face = viewport?.solid_faces.find(
                    (entry) => entry.face_id === faceId,
                  );
                  if (face && face.sketchability === "planar") {
                    await createHoleFeature(faceId);
                  }
                  return;
                }
                if (shellAction?.phase === "pending") {
                  await createShellFeature(
                    faceId,
                    pendingShellThicknessRef.current,
                  );
                  return;
                }
                // Same intercept as onSelectReference: a face click
                // during the pending phase is a valid offset-plane
                // source as long as the face is planar.
                if (
                  offsetPlaneAction &&
                  offsetPlaneAction.phase === "pending"
                ) {
                  const face = viewport?.solid_faces.find(
                    (entry) => entry.face_id === faceId,
                  );
                  if (face && face.sketchability === "planar") {
                    await createOffsetPlaneFeature(
                      faceId,
                      pendingOffsetRef.current,
                    );
                    return;
                  }
                  // Non-planar face: ignore the click, leave the
                  // panel in pending phase so the user can pick
                  // somewhere else.
                  return;
                }
                if (midplaneAction) {
                  const face = viewport?.solid_faces.find(
                    (entry) => entry.face_id === faceId,
                  );
                  if (face && face.sketchability === "planar") {
                    await addMidplaneSource(faceId);
                  }
                  return;
                }
                if (anglePlaneAction?.phase === "pick_plane") {
                  const face = viewport?.solid_faces.find(
                    (entry) => entry.face_id === faceId,
                  );
                  if (face && face.sketchability === "planar") {
                    setAnglePlaneAction({
                      phase: "pick_axis",
                      sourcePlaneId: faceId,
                      sourceSummary: describePlaneSource(faceId),
                      initialAngle: pendingAngleRef.current,
                    });
                  }
                  return;
                }
                if (tangentPlaneAction) {
                  setTangentPlaneAction(null);
                  await createTangentPlaneFeature(faceId);
                  return;
                }
                // Modal Project tool: a face click while it's active
                // projects the face's outline onto the active sketch
                // plane via `project_face_into_sketch`. The tool
                // stays armed so the user can keep clicking more
                // faces / edges / vertices without re-toggling the
                // button.
                if (activeSketchPlaneId && activeSketchTool === "project") {
                  await runAction(async () => {
                    try {
                      await projectFaceIntoSketch(faceId);
                    } catch (error) {
                      addMessage(
                        `Project face: ${error instanceof Error ? error.message : String(error)}`,
                      );
                    }
                  });
                  return;
                }
                if (
                  extrudeAction &&
                  extrudeAction.phase === "pending" &&
                  selectedSketchProfileIds.length === 0
                ) {
                  const face = viewport?.solid_faces.find(
                    (entry) => entry.face_id === faceId,
                  );
                  if (face && face.sketchability === "planar") {
                    const defaultSettings = getDefaultExtrudeSettings(
                      [],
                      faceId,
                    );
                    const mode =
                      extrudeAction.initialMode === "new_body"
                        ? defaultSettings.mode
                        : extrudeAction.initialMode;
                    const targetBodyId =
                      mode === "new_body"
                        ? null
                        : extrudeAction.initialTargetBodyId ??
                          defaultSettings.targetBodyId ??
                          bodyIdFromFaceId(faceId);
                    await createExtrudeFromSelectedFace(
                      faceId,
                      extrudeAction.initialDepth,
                      mode,
                      targetBodyId,
                    );
                    return;
                  }
                  return;
                }
                await runAction(async () => {
                  await selectFace(faceId);
                });
              }}
              onSelectEdge={async (edgeId, additive) => {
                if (threadAction?.phase === "pick_axis") {
                  await createThreadFeature(threadAction.targetBodyId, edgeId);
                  return;
                }
                if (threadAction?.phase === "pick_target") {
                  setThreadAction({ ...threadAction, axisSourceId: edgeId });
                  return;
                }
                if (helixAction?.phase === "pending") {
                  await createHelixFeature(edgeId);
                  return;
                }
                if (constructionAxisAction) {
                  await createConstructionAxisFeature(edgeId);
                  return;
                }
                // Modal Project tool wins over the normal edge-pick
                // path. Same shape as the face intercept above.
                if (activeSketchPlaneId && activeSketchTool === "project") {
                  await runAction(async () => {
                    try {
                      await projectEdgeIntoSketch(edgeId);
                    } catch (error) {
                      addMessage(
                        `Project edge: ${error instanceof Error ? error.message : String(error)}`,
                      );
                    }
                  });
                  return;
                }
                if (anglePlaneAction?.phase === "pick_axis") {
                  await createAnglePlaneFeature(
                    anglePlaneAction.sourcePlaneId,
                    edgeId,
                    pendingAngleRef.current,
                  );
                  return;
                }
                // While a fillet / chamfer floating panel is open the
                // user is in "pick edges" mode: every edge click
                // toggles that edge in the feature's edge_ids set
                // rather than the document selection. The body
                // recompiles live so the user sees the fillet grow /
                // shrink as they pick. We ignore `additive` here —
                // toggle is the only meaningful gesture during edit.
                if (edgeOpAction) {
                  // Pending phase: this is the first edge — create the
                  // feature with the latest typed value (which may have
                  // been edited before any edge was clicked).
                  if (edgeOpAction.phase === "pending") {
                    await createEdgeOpFeature(
                      edgeOpAction.kind,
                      [edgeId],
                      pendingValueRef.current,
                    );
                    return;
                  }

                  // Active phase: toggle membership. We compute the
                  // next list against the ref (not against React
                  // state, whose updater runs asynchronously, and not
                  // against the document, whose echo for the previous
                  // click may not have arrived yet) so rapid
                  // successive clicks compound correctly.
                  const current = activeEdgeIdsRef.current;
                  const isMember = current.includes(edgeId);
                  const updated = isMember
                    ? current.filter((id) => id !== edgeId)
                    : [...current, edgeId];
                  if (updated.length === 0) {
                    // Last edge: refusing the toggle keeps the
                    // feature valid (the core requires at least one
                    // edge). The user can Cancel the panel to undo
                    // entirely.
                    return;
                  }
                  activeEdgeIdsRef.current = updated;
                  setEdgeOpAction((prev) =>
                    prev && prev.phase === "active"
                      ? { ...prev, edgeIds: updated }
                      : prev,
                  );
                  await runAction(async () => {
                    if (edgeOpAction.kind === "fillet") {
                      await updateFilletEdges(edgeOpAction.featureId, updated);
                    } else {
                      await updateChamferEdges(edgeOpAction.featureId, updated);
                    }
                  });
                  return;
                }
                await runAction(async () => {
                  await selectEdge(edgeId, additive);
                });
              }}
              onSelectVertex={async (vertexId, additive) => {
                if (constructionPointAction) {
                  await createConstructionPointFeature(vertexId);
                  return;
                }
                // Modal Project tool: vertex click projects a fixed
                // standalone sketch point onto the active plane.
                if (activeSketchPlaneId && activeSketchTool === "project") {
                  await runAction(async () => {
                    try {
                      await projectVertexIntoSketch(vertexId);
                    } catch (error) {
                      addMessage(
                        `Project vertex: ${error instanceof Error ? error.message : String(error)}`,
                      );
                    }
                  });
                  return;
                }
                await runAction(async () => {
                  await selectVertex(vertexId, additive);
                });
              }}
              onStartSketch={async (referenceId) => {
                await runAction(async () => {
                  await startSketchOnPlane(referenceId);
                });
              }}
              onStartSketchOnFace={async (faceId, planeFrame) => {
                await runAction(async () => {
                  await startSketchOnFace(faceId, toCorePlaneFrame(planeFrame));
                });
              }}
              onAddSketchLine={async (
                startX,
                startY,
                endX,
                endY,
                isConstruction,
              ) => {
                await runAction(async () => {
                  await addSketchLine(
                    startX,
                    startY,
                    endX,
                    endY,
                    isConstruction,
                  );
                });
              }}
              onSetSketchMidpointAnchor={async (pointId, hostLineId) => {
                await runAction(async () => {
                  await setSketchMidpointAnchor(pointId, hostLineId);
                });
              }}
              onSetSketchPointLineAnchor={async (pointId, hostLineId, t) => {
                await runAction(async () => {
                  await setSketchPointLineAnchor(pointId, hostLineId, t);
                });
              }}
              onResolveDraftSnap={async (cx, cy, sx, sy) => {
                await resolveDraftSnap(cx, cy, sx, sy);
              }}
              onAddSketchAngleDimension={async (firstLineId, secondLineId) => {
                await runAction(async () => {
                  await addSketchAngleDimension(firstLineId, secondLineId);
                });
              }}
              onAddSketchDistanceDimension={async (firstEntityId, secondEntityId) => {
                await runAction(async () => {
                  await addSketchDistanceDimension(firstEntityId, secondEntityId);
                });
              }}
              onAddSketchLineLengthDimension={async (lineId) => {
                await runAction(async () => {
                  await addSketchLineLengthDimension(lineId);
                });
              }}
              onAddSketchCircleRadiusDimension={async (circleId) => {
                await runAction(async () => {
                  await addSketchCircleRadiusDimension(circleId);
                });
              }}
              onAddSketchPolygonRadiusDimension={async (polygonId) => {
                await runAction(async () => {
                  await addSketchPolygonRadiusDimension(polygonId);
                });
              }}
              onSetSketchLineConstraint={async (lineId, constraint) => {
                await runAction(async () => {
                  await setSketchLineConstraint(lineId, constraint);
                });
              }}
              onSetSketchPerpendicularConstraint={async (
                lineId,
                otherLineId,
              ) => {
                await runAction(async () => {
                  await setSketchPerpendicularConstraint(lineId, otherLineId);
                });
              }}
              onSetSketchTangentConstraint={async (lineId, circleId) => {
                await runAction(async () => {
                  await setSketchTangentConstraint(lineId, circleId);
                });
              }}
              onAddSketchRectangle={async (
                startX,
                startY,
                endX,
                endY,
                isConstruction,
              ) => {
                await runAction(async () => {
                  await addSketchRectangle(
                    startX,
                    startY,
                    endX,
                    endY,
                    isConstruction,
                  );
                });
              }}
              onAddSketchCircle={async (
                centerX,
                centerY,
                radius,
                isConstruction,
              ) => {
                await runAction(async () => {
                  await addSketchCircle(
                    centerX,
                    centerY,
                    radius,
                    isConstruction,
                  );
                });
              }}
              onAddSketchArc={async (
                startX,
                startY,
                endX,
                endY,
                anchorX,
                anchorY,
                mode,
                isConstruction,
              ) => {
                await runAction(async () => {
                  await addSketchArc(
                    startX,
                    startY,
                    endX,
                    endY,
                    anchorX,
                    anchorY,
                    mode,
                    isConstruction,
                  );
                });
              }}
              arcToolMode={arcToolMode}
              onSetArcToolMode={setArcToolMode}
              rectangleToolMode={rectangleToolMode}
              onSetRectangleToolMode={setRectangleToolMode}
              circleToolMode={circleToolMode}
              onSetCircleToolMode={setCircleToolMode}
              polygonToolMode={polygonToolMode}
              onSetPolygonToolMode={setPolygonToolMode}
              onAddSketchPolygon={async (
                sides,
                mode,
                startX,
                startY,
                endX,
                endY,
                isConstruction,
              ) => {
                await runAction(async () => {
                  await addSketchPolygon(
                    sides,
                    mode,
                    startX,
                    startY,
                    endX,
                    endY,
                    isConstruction,
                  );
                });
              }}
              onAddSketchFillet={async (cornerPointId, lineAId, lineBId) => {
                // Panel must be open in either phase for adds to be
                // accepted. The viewport's eligibility filter is the
                // primary guard; this is just a defence against a
                // race where the user drops the panel mid-click.
                if (!sketchFilletAction) {
                  return;
                }
                const sessionRadius = sketchFilletAction.radius;
                // Same fire-and-forget IPC trick as the extrude /
                // edge-op flows: subscribe to the next document
                // update that adds a new fillet on the active
                // sketch so we can pick up the real fillet id and
                // append it to the session list.
                const documentPromise = awaitDocumentChange(
                  (next, previous) => {
                    if (!next.active_sketch_feature_id) {
                      return false;
                    }
                    const nextSketch = next.feature_history.find(
                      (entry) =>
                        entry.feature_id === next.active_sketch_feature_id,
                    );
                    const prevSketch = previous?.feature_history.find(
                      (entry) =>
                        entry.feature_id === next.active_sketch_feature_id,
                    );
                    const nextFillets =
                      nextSketch?.sketch_parameters?.fillets ?? [];
                    const prevFillets =
                      prevSketch?.sketch_parameters?.fillets ?? [];
                    return nextFillets.length > prevFillets.length;
                  },
                );

                await runAction(async () => {
                  await addSketchFillet(
                    cornerPointId,
                    lineAId,
                    lineBId,
                    sessionRadius,
                  );
                });

                try {
                  const nextDocument = await documentPromise;
                  const nextSketch = nextDocument.feature_history.find(
                    (entry) =>
                      entry.feature_id ===
                      nextDocument.active_sketch_feature_id,
                  );
                  const fillets = nextSketch?.sketch_parameters?.fillets ?? [];
                  const newFillet = fillets[fillets.length - 1];
                  if (!newFillet) {
                    return;
                  }
                  // Append the new fillet to the session and flip
                  // pending → active on the first click.
                  const updatedIds = [
                    ...sketchFilletIdsRef.current,
                    newFillet.fillet_id,
                  ];
                  sketchFilletIdsRef.current = updatedIds;
                  setSketchFilletAction({
                    phase: "active",
                    radius: sessionRadius,
                    filletIds: updatedIds,
                  });
                } catch {
                  // Document watcher timed out — leave the session
                  // state alone. The next click can recover.
                }
              }}
              onSelectSketchEntity={async (entityId, additive) => {
                if (constructionAxisAction && sketchLineLabelById.has(entityId)) {
                  await createConstructionAxisFeature(entityId);
                  return;
                }
                if (revolveAction && sketchLineLabelById.has(entityId)) {
                  setRevolveAction((current) =>
                    current ? {...current, axisEntityId: entityId} : current,
                  );
                  return;
                }
                if (sweepAction) {
                  setSweepAction((current) =>
                    current ? {...current, pathEntityId: entityId} : current,
                  );
                  return;
                }
                if (
                  anglePlaneAction?.phase === "pick_axis" &&
                  sketchLineLabelById.has(entityId)
                ) {
                  await createAnglePlaneFeature(
                    anglePlaneAction.sourcePlaneId,
                    entityId,
                    pendingAngleRef.current,
                  );
                  return;
                }
                await runAction(async () => {
                  await handleSketchConstraintLinePick(entityId, additive);
                });
              }}
              onBatchSelectEntities={async (entityIds, additive) => {
                await runAction(async () => {
                  await batchSelectSketchEntities(entityIds, additive);
                });
              }}
              onPickSketchPoint={async (pointId, kind, additive) => {
                if (constructionPointAction) {
                  await createConstructionPointFeature(pointId);
                  return;
                }
                await runAction(async () => {
                  await handleSketchConstraintPointPick(
                    pointId,
                    kind,
                    additive,
                  );
                });
              }}
              armedSketchConstraint={armedSketchConstraint}
              mirrorFocusedSlot={mirrorFocusedSlot}
              onMirrorEntityPick={async (entityId, entityKind) => {
                if (!pendingMirror) {
                  return;
                }
                await runAction(async () => {
                  if (mirrorFocusedSlot === "axis") {
                    // Only lines can be mirror axes. Silently
                    // ignore circles to avoid bouncing the user
                    // out of the slot.
                    if (entityKind !== "line") {
                      return;
                    }
                    await updateMirrorPreviewAxis(entityId);
                    // Auto-advance to the Objects slot if it's
                    // empty — common CAD workflow's small UX touch that saves
                    // a click on the typical "axis first, then
                    // objects" flow. If the user explicitly
                    // re-focused Axis with objects already
                    // selected, leave focus on Axis (they're
                    // probably re-picking).
                    if (pendingMirror.object_ids.length === 0) {
                      setMirrorFocusedSlot("objects");
                    }
                    return;
                  }

                  // Objects slot. Toggle membership.
                  const current = pendingMirror.object_ids;
                  const next = current.includes(entityId)
                    ? current.filter((id) => id !== entityId)
                    : [...current, entityId];
                  await updateMirrorPreviewObjects(next);
                });
              }}
              onCancelSketchConstraint={clearArmedSketchConstraint}
              onClearSketchConstraint={async (
                kind,
                entityId,
                _relatedEntityId,
              ) => {
                await runAction(async () => {
                  if (kind === "fixed") {
                    await setSketchPointFixed(entityId, false);
                    return;
                  }

                  if (kind === "equal_length") {
                    await setSketchEqualLengthConstraint(entityId, null);
                    return;
                  }

                  if (kind === "perpendicular") {
                    await setSketchPerpendicularConstraint(entityId, null);
                    return;
                  }

                  if (kind === "parallel") {
                    await setSketchParallelConstraint(entityId, null);
                    return;
                  }

                  await clearSketchLineConstraints(entityId);
                });
              }}
              onSelectSketchDimension={async (dimensionId) => {
                await runAction(async () => {
                  await selectSketchDimension(dimensionId);
                });
              }}
              onUpdateSketchDimension={async (dimensionId, value) => {
                await runAction(async () => {
                  await updateSketchDimension(dimensionId, value);
                });
              }}
              onUpdateSketchDimensionLabelPosition={async (
                dimensionId,
                labelX,
                labelY,
              ) => {
                await runAction(async () => {
                  await updateSketchDimensionLabelPosition(
                    dimensionId,
                    labelX,
                    labelY,
                  );
                });
              }}
              onSelectSketchProfile={async (profileId, additive) => {
                if (
                  offsetPlaneAction &&
                  offsetPlaneAction.phase === "pending"
                ) {
                  await createOffsetPlaneFeature(
                    profileId,
                    pendingOffsetRef.current,
                  );
                  return;
                }
                if (midplaneAction) {
                  await addMidplaneSource(profileId);
                  return;
                }
                if (anglePlaneAction?.phase === "pick_plane") {
                  setAnglePlaneAction({
                    phase: "pick_axis",
                    sourcePlaneId: profileId,
                    sourceSummary: describePlaneSource(profileId),
                    initialAngle: pendingAngleRef.current,
                  });
                  return;
                }
                if (activeSketchPlaneId && activeSketchTool === "project") {
                  await runAction(async () => {
                    try {
                      await projectProfileIntoSketch(profileId);
                    } catch (error) {
                      addMessage(
                        `Project profile: ${error instanceof Error ? error.message : String(error)}`,
                      );
                    }
                  });
                  return;
                }
                if (loftAction) {
                  const alreadySelected = loftAction.profileIds.includes(profileId);
                  if (!alreadySelected) {
                    await runAction(async () => {
                      await selectSketchProfile(profileId, true);
                    });
                    setLoftAction((current) =>
                      current
                        ? {
                            ...current,
                            profileIds: [...current.profileIds, profileId],
                          }
                        : current,
                    );
                  }
                  return;
                }
                if (revolveAction) {
                  await runAction(async () => {
                    await selectSketchProfile(profileId, false);
                  });
                  setRevolveAction((current) =>
                    current ? {...current, profileId} : current,
                  );
                  return;
                }
                if (sweepAction) {
                  await runAction(async () => {
                    await selectSketchProfile(profileId, false);
                  });
                  setSweepAction((current) =>
                    current ? {...current, profileId} : current,
                  );
                  return;
                }
                await runAction(async () => {
                  await selectSketchProfile(
                    profileId,
                    extrudeAction ? true : additive,
                  );
                });
              }}
              onDeleteSketchSelection={async (selection) => {
                confirmAndDeleteSketchSelection(selection);
              }}
              onTrimSketchEntity={async (entityId, clickX, clickY) => {
                await runAction(async () => {
                  await trimSketchEntity(entityId, clickX, clickY);
                });
              }}
              onDeleteSketchDimension={async (dimensionId) => {
                await runAction(async () => {
                  await deleteSketchDimension(dimensionId);
                });
              }}
              onAddSketchPointDistanceDimension={async (
                pointAId,
                pointBId,
              ) => {
                await runAction(async () => {
                  await addSketchPointDistanceDimension(
                    pointAId,
                    pointBId,
                  );
                });
              }}
              onUpdateSketchDimensionDisplay={async (
                dimensionId,
                displayAs,
              ) => {
                await runAction(async () => {
                  await updateSketchDimensionDisplay(
                    dimensionId,
                    displayAs,
                  );
                });
              }}
              onFinishSketch={async () => {
                await runAction(async () => {
                  clearArmedSketchConstraint();
                  await finishSketch();
                  await restoreTimelineCursorAfterEdit();
                });
              }}
              onSetSketchTool={async (tool) => {
                await runAction(async () => {
                  clearArmedSketchConstraint();
                  await setSketchTool(tool);
                });
              }}
              onUpdateSketchPoint={async (pointId, x, y) => {
                await runAction(async () => {
                  await updateSketchPoint(pointId, x, y);
                });
              }}
              hiddenFeatureIds={effectiveHiddenFeatureIds}
              hiddenSketchPlaneIds={hiddenSketchPlaneIds}
              hideReferences={hiddenCategories.has("origin")}
            />

            <div className="pointer-events-none absolute bottom-4 right-4 top-4 z-10 flex min-h-0 w-[340px] flex-col gap-3">
              {materialsPanelOpen ? (
                <div className="pointer-events-auto">
                  <MaterialsPanel
                    selectedBodyId={selectedMaterialBodyId}
                    selectedFaceId={selectedMaterialFace?.face_id ?? null}
                    onApplyBodyColor={async (bodyId, color) => {
                      await runAction(async () => {
                        await setBodyColor(bodyId, color);
                      });
                    }}
                    onApplyFaceColor={async (faceId, color) => {
                      await runAction(async () => {
                        await setFaceColor(faceId, color);
                      });
                    }}
                    onClearBodyColor={async (bodyId) => {
                      await runAction(async () => {
                        await clearBodyColor(bodyId);
                      });
                    }}
                    onClearFaceColor={async (faceId) => {
                      await runAction(async () => {
                        await clearFaceColor(faceId);
                      });
                    }}
                    onClearAll={async () => {
                      await runAction(async () => {
                        await clearAppearanceOverrides();
                      });
                    }}
                  />
                </div>
              ) : null}
              {extrudeAction?.phase === "pending" ? (
                <ExtrudePreviewPanel
                  phase="pending"
                  initialDepth={extrudeAction.initialDepth}
                  initialMode={extrudeAction.initialMode}
                  initialParameters={extrudeAction.initialParameters}
                  selectedProfileCount={extrudeAction.profileCount}
                  canCombineWithExistingBody={
                    extrudeAction.canCombineWithExistingBody
                  }
                  availableTargetBodies={viewport?.bodies ?? []}
                  selectedFaceTargetId={document?.selected_face_id ?? null}
                  initialTargetBodyId={extrudeAction.initialTargetBodyId}
                  disabled={status !== "connected"}
                  onPreviewDepth={async (depth) => {
                    setExtrudeAction((current) =>
                      current?.phase === "pending"
                        ? {...current, initialDepth: depth}
                        : current,
                    );
                  }}
                  onPreviewMode={async (mode) => {
                    setExtrudeAction((current) =>
                      current?.phase === "pending"
                        ? {
                            ...current,
                            automaticMode: false,
                            initialMode: mode,
                            initialTargetBodyId:
                              mode === "new_body"
                                ? null
                                : current.initialTargetBodyId ??
                                  getDefaultExtrudeSettings(
                                    selectedSketchProfileIds,
                                  ).targetBodyId,
                          }
                        : current,
                    );
                  }}
                  onPreviewTargetBody={async (targetBodyId) => {
                    setExtrudeAction((current) =>
                      current?.phase === "pending"
                        ? {...current, initialTargetBodyId: targetBodyId}
                        : current,
                    );
                  }}
                  onPreviewParameters={async (parameters) => {
                    if ("sketch_feature_id" in parameters) {
                      return;
                    }
                    setExtrudeAction((current) =>
                      current?.phase === "pending"
                        ? {
                            ...current,
                            automaticMode: false,
                            initialParameters: null,
                          }
                        : current,
                    );
                  }}
                  onConfirm={async (depth, mode, targetBodyId, parameters) => {
                    if (
                      parameters.thin.enabled &&
                      selectedSketchProfileIds.length === 0 &&
                      selectedExtrudableFaceId === null &&
                      selectedSketchEntityIds.length > 0
                    ) {
                      await createThinExtrudeFromSelectedEntities(
                        depth,
                        mode,
                        targetBodyId,
                        parameters,
                      );
                      return;
                    }
                    if (selectedSketchProfileIds.length > 0) {
                      await createExtrudeFromSelectedProfiles(
                        depth,
                        mode,
                        targetBodyId,
                        parameters,
                      );
                      return;
                    }
                    if (selectedExtrudableFaceId) {
                      await createExtrudeFromSelectedFace(
                        selectedExtrudableFaceId,
                        depth,
                        mode,
                        targetBodyId,
                        parameters,
                      );
                    }
                  }}
                  onCancel={async () => {
                    await cancelActiveTool();
                  }}
                />
              ) : null}
              {extrudeAction?.phase === "active" && extrudeAction.featureId
                ? (() => {
                    const activeExtrudeFeatureId = extrudeAction.featureId;
                    if (!activeExtrudeFeatureId) {
                      return null;
                    }
                    const activeExtrudeFeatureIds =
                      extrudeAction.featureIds.length > 0
                        ? extrudeAction.featureIds
                        : [activeExtrudeFeatureId];
                    // Bodies that the in-progress extrude can target. We
                    // exclude the extrude itself: at this point the core
                    // already created the feature in `new_body` mode and
                    // it appears as its own body in `viewport.bodies`,
                    // but targeting it would be a no-op (or nonsensical
                    // for cut). Filtering it out keeps the picker honest.
                    const availableTargetBodies = (
                      viewport?.bodies ?? []
                    ).filter((body) => !activeExtrudeFeatureIds.includes(body.id));
                    const activeExtrudeFeature =
                      document?.feature_history.find(
                        (entry) =>
                          entry.feature_id === activeExtrudeFeatureId,
                      ) ?? null;
                    const extrudePreviewError =
                      activeExtrudeFeature?.dependency_broken ||
                      activeExtrudeFeature?.status === "warning"
                        ? activeExtrudeFeature.dependency_warning ?? null
                        : null;
                    return (
                      <ExtrudePreviewPanel
                        initialDepth={extrudeAction.initialDepth}
                        initialMode={extrudeAction.initialMode}
                        initialParameters={extrudeAction.initialParameters}
                        selectedProfileCount={extrudeAction.profileCount}
                        canCombineWithExistingBody={
                          extrudeAction.canCombineWithExistingBody
                        }
                        availableTargetBodies={availableTargetBodies}
                        selectedFaceTargetId={document?.selected_face_id ?? null}
                        initialTargetBodyId={extrudeAction.initialTargetBodyId}
                        previewError={extrudePreviewError}
                        disabled={status !== "connected"}
                        onPreviewDepth={async (depth) => {
                          await runAction(async () => {
                            for (const featureId of activeExtrudeFeatureIds) {
                              await updateExtrudeDepth(featureId, depth);
                            }
                          });
                          setExtrudeAction((current) =>
                            current?.phase === "active" &&
                            current.featureId === activeExtrudeFeatureId
                              ? {...current, initialDepth: depth}
                              : current,
                          );
                        }}
                        onPreviewMode={async (mode) => {
                          const recreated =
                            await recreateNewProfileExtrudePreview(
                              extrudeAction,
                              activeExtrudeFeature?.extrude_parameters?.depth ??
                                extrudeAction.initialDepth,
                              mode,
                              mode === "new_body"
                                ? null
                                : activeExtrudeFeature?.extrude_parameters
                                    ?.target_body_id ??
                                  extrudeAction.initialTargetBodyId,
                              activeExtrudeAdvancedParameters(
                                activeExtrudeFeature?.extrude_parameters,
                                mode,
                              ),
                            );
                          if (recreated) {
                            return;
                          }
                          await runAction(async () => {
                            for (const featureId of activeExtrudeFeatureIds) {
                              await updateExtrudeMode(featureId, mode);
                            }
                          });
                        }}
                        onPreviewTargetBody={async (targetBodyId) => {
                          await runAction(async () => {
                            for (const featureId of activeExtrudeFeatureIds) {
                              await updateExtrudeTargetBody(featureId, targetBodyId);
                            }
                          });
                        }}
                        onPreviewParameters={async (parameters) => {
                          if (!("sketch_feature_id" in parameters)) {
                            return;
                          }
                          await runAction(async () => {
                            for (const featureId of activeExtrudeFeatureIds) {
                              await updateExtrudeParameters(featureId, parameters);
                            }
                          });
                        }}
                        onConfirm={async () => {
                          // Look up the just-confirmed extrude in the
                          // current document so we can apply CAD-
                          // style post-confirm UX without round-
                          // tripping new state through the core:
                          //   - hide the source sketch (the user is
                          //     done with it; leaving it visible
                          //     clutters the body they just created)
                          //   - if the extrude was a cut, drop the
                          //     selection so the floating panel
                          //     doesn't reopen and the user is left
                          //     looking at the resulting body, not
                          //     the cutter feature itself
                          const confirmedFeature =
                            document?.feature_history.find(
                              (entry) =>
                                entry.feature_id === activeExtrudeFeatureId,
                            ) ?? null;
                          const sketchFeatureId =
                            confirmedFeature?.extrude_parameters
                              ?.sketch_feature_id ?? null;
                          const confirmedMode =
                            confirmedFeature?.extrude_parameters?.mode ?? null;
                          if (sketchFeatureId) {
                            setHiddenFeatureIds((current) => {
                              if (current.has(sketchFeatureId)) {
                                return current;
                              }
                              const next = new Set(current);
                              next.add(sketchFeatureId);
                              return next;
                            });
                          }
                          setExtrudeAction(null);
                          if (confirmedMode === "cut") {
                            await runAction(async () => {
                              await clearSelection();
                            });
                          }
                          await restoreTimelineCursorAfterEdit();
                        }}
                        onCancel={async () => {
                          await cancelActiveTool();
                        }}
                      />
                    );
                  })()
                : null}
              {loftAction ? (
                <LoftPreviewPanel
                  initialRuled={loftAction.initialRuled}
                  profiles={loftAction.profileIds.map((profileId, index) => ({
                    profileId,
                    label:
                      sketchProfileLabelById.get(profileId) ??
                      `Profile ${index + 1}`,
                  }))}
                  disabled={status !== "connected"}
                  canConfirm={
                    loftAction.phase === "active" &&
                    loftAction.profileIds.length >= 2
                  }
                  onPreviewRuled={async (ruled) => {
                    if (loftAction.phase === "active" && loftAction.featureId) {
                      await runAction(async () => {
                        await updateLoftRuled(loftAction.featureId!, ruled);
                      });
                    }
                    setLoftAction((current) =>
                      current?.featureId === loftAction.featureId
                        ? {...current, initialRuled: ruled}
                        : current,
                    );
                  }}
                  onMoveProfile={(profileId, direction) => {
                    setLoftAction((current) => {
                      if (!current) {
                        return current;
                      }
                      const fromIndex = current.profileIds.indexOf(profileId);
                      const toIndex = fromIndex + direction;
                      if (
                        fromIndex < 0 ||
                        toIndex < 0 ||
                        toIndex >= current.profileIds.length
                      ) {
                        return current;
                      }
                      const nextProfileIds = [...current.profileIds];
                      const [moved] = nextProfileIds.splice(fromIndex, 1);
                      nextProfileIds.splice(toIndex, 0, moved);
                      return {...current, profileIds: nextProfileIds};
                    });
                  }}
                  onRemoveProfile={async (profileId) => {
                    setLoftAction((current) =>
                      current
                        ? {
                            ...current,
                            profileIds: current.profileIds.filter(
                              (id) => id !== profileId,
                            ),
                          }
                        : current,
                    );
                    if (selectedSketchProfileIds.includes(profileId)) {
                      await runAction(async () => {
                        await selectSketchProfile(profileId, true);
                      });
                    }
                  }}
                  onConfirm={async () => {
                    if (loftAction.phase !== "active" || !loftAction.featureId) {
                      return;
                    }
                    const confirmedFeature =
                      document?.feature_history.find(
                        (entry) => entry.feature_id === loftAction.featureId,
                      ) ?? null;
                    const sketchFeatureIds = new Set(
                      confirmedFeature?.loft_parameters?.sections.map(
                        (section) => section.sketch_feature_id,
                      ) ?? [],
                    );
                    if (sketchFeatureIds.size > 0) {
                      setHiddenFeatureIds((current) => {
                        const next = new Set(current);
                        for (const featureId of sketchFeatureIds) {
                          next.add(featureId);
                        }
                        return next;
                      });
                    }
                    setLoftAction(null);
                    await restoreTimelineCursorAfterEdit();
                  }}
                  onCancel={async () => {
                    await cancelActiveTool();
                  }}
                />
              ) : null}
              {revolveAction ? (
                <RevolvePreviewPanel
                  phase={revolveAction.phase}
                  initialAngle={revolveAction.initialAngle}
                  profileLabel={
                    revolveAction.profileId
                      ? sketchProfileLabelById.get(revolveAction.profileId) ??
                        "Profile"
                      : null
                  }
                  axisLabel={
                    revolveAction.axisEntityId
                      ? sketchLineLabelById.get(revolveAction.axisEntityId) ??
                        "Line"
                      : null
                  }
                  disabled={status !== "connected"}
                  canConfirm={
                    revolveAction.phase === "active" &&
                    Boolean(revolveAction.profileId) &&
                    Boolean(revolveAction.axisEntityId)
                  }
                  onPreviewAngle={async (angleDegrees) => {
                    if (
                      revolveAction.phase === "active" &&
                      revolveAction.featureId
                    ) {
                      await runAction(async () => {
                        await updateRevolveAngle(
                          revolveAction.featureId!,
                          angleDegrees,
                        );
                      });
                    }
                    setRevolveAction((current) =>
                      current?.featureId === revolveAction.featureId
                        ? {...current, initialAngle: angleDegrees}
                        : current,
                    );
                  }}
                  onConfirm={async (angleDegrees) => {
                    if (
                      revolveAction.phase !== "active" ||
                      !revolveAction.featureId
                    ) {
                      return;
                    }
                    await runAction(async () => {
                      await updateRevolveAngle(
                        revolveAction.featureId!,
                        angleDegrees,
                      );
                    });
                    const confirmedFeature =
                      document?.feature_history.find(
                        (entry) => entry.feature_id === revolveAction.featureId,
                      ) ?? null;
                    const sourceSketchIds = [
                      confirmedFeature?.revolve_parameters?.sketch_feature_id,
                      confirmedFeature?.revolve_parameters?.axis_sketch_feature_id,
                    ].filter((id): id is string => Boolean(id));
                    if (sourceSketchIds.length > 0) {
                      setHiddenFeatureIds((current) => {
                        const next = new Set(current);
                        for (const featureId of sourceSketchIds) {
                          next.add(featureId);
                        }
                        return next;
                      });
                    }
                    setRevolveAction(null);
                    await restoreTimelineCursorAfterEdit();
                  }}
                  onCancel={async () => {
                    await cancelActiveTool();
                  }}
                />
              ) : null}
              {sweepAction ? (
                <SweepPreviewPanel
                  phase={sweepAction.phase}
                  profileLabel={
                    sweepAction.profileId
                      ? sketchProfileLabelById.get(sweepAction.profileId) ??
                        "Profile"
                      : null
                  }
                  pathLabel={
                    sweepAction.pathEntityId
                      ? sketchPathEntityLabelById.get(sweepAction.pathEntityId) ??
                        "Line"
                      : null
                  }
                  disabled={status !== "connected"}
                  canConfirm={
                    sweepAction.phase === "active" &&
                    Boolean(sweepAction.profileId) &&
                    Boolean(sweepAction.pathEntityId)
                  }
                  onConfirm={async () => {
                    if (
                      sweepAction.phase !== "active" ||
                      !sweepAction.featureId
                    ) {
                      return;
                    }
                    const confirmedFeature =
                      document?.feature_history.find(
                        (entry) => entry.feature_id === sweepAction.featureId,
                      ) ?? null;
                    const sourceSketchIds = [
                      confirmedFeature?.sweep_parameters?.sketch_feature_id,
                      confirmedFeature?.sweep_parameters?.path_sketch_feature_id,
                    ].filter((id): id is string => Boolean(id));
                    if (sourceSketchIds.length > 0) {
                      setHiddenFeatureIds((current) => {
                        const next = new Set(current);
                        for (const featureId of sourceSketchIds) {
                          next.add(featureId);
                        }
                        return next;
                      });
                    }
                    setSweepAction(null);
                    await restoreTimelineCursorAfterEdit();
                  }}
                  onCancel={async () => {
                    await cancelActiveTool();
                  }}
                />
              ) : null}
              {editingFeatureId
                ? (() => {
                    // Resolve the feature being edited from the live
                    // document so the form always reflects the latest
                    // server-confirmed values (relevant if the user
                    // undid a change while the panel was open).
                    const editing = document?.feature_history.find(
                      (entry) => entry.feature_id === editingFeatureId,
                    );
                    if (!editing) {
                      // Feature was deleted (e.g. via the hierarchy
                      // panel). Close the editor on next render — we
                      // can't call setState during render, so we use a
                      // microtask. Returning null keeps the previous
                      // panel chrome from flashing.
                      queueMicrotask(() => setEditingFeatureId(null));
                      return null;
                    }
                    if (editing.kind === "box" && editing.box_parameters) {
                      return (
                        <div className="cad-toolbar-popover pointer-events-auto">
                          <BoxFeatureForm
                            disabled={status !== "connected"}
                            mode="edit"
                            initialValues={{
                              width: editing.box_parameters.width,
                              height: editing.box_parameters.height,
                              depth: editing.box_parameters.depth,
                            }}
                            variant="toolbar"
                            onSubmit={async (width, height, depth) => {
                              await runAction(async () => {
                                await updateBoxFeature(
                                  editingFeatureId,
                                  width,
                                  height,
                                  depth,
                                );
                              });
                              setEditingFeatureId(null);
                              await restoreTimelineCursorAfterEdit();
                            }}
                          />
                        </div>
                      );
                    }
                    if (
                      editing.kind === "cylinder" &&
                      editing.cylinder_parameters
                    ) {
                      return (
                        <div className="cad-toolbar-popover pointer-events-auto">
                          <CylinderFeatureForm
                            disabled={status !== "connected"}
                            mode="edit"
                            initialValues={{
                              radius: editing.cylinder_parameters.radius,
                              height: editing.cylinder_parameters.height,
                            }}
                            variant="toolbar"
                            onSubmit={async (radius, height) => {
                              await runAction(async () => {
                                await updateCylinderFeature(
                                  editingFeatureId,
                                  radius,
                                  height,
                                );
                              });
                              setEditingFeatureId(null);
                              await restoreTimelineCursorAfterEdit();
                            }}
                          />
                        </div>
                      );
                    }
                    return null;
                  })()
                : null}
              {pendingMirror ? (
                <MirrorToolPanel
                  axisLineId={pendingMirror.axis_line_id}
                  objectIds={pendingMirror.object_ids}
                  generatedLineCount={pendingMirror.generated_lines.length}
                  generatedCircleCount={pendingMirror.generated_circles.length}
                  focusedSlot={mirrorFocusedSlot}
                  disabled={status !== "connected"}
                  onFocusObjects={() => setMirrorFocusedSlot("objects")}
                  onFocusAxis={() => setMirrorFocusedSlot("axis")}
                  onClearObjects={async () => {
                    await runAction(async () => {
                      await updateMirrorPreviewObjects([]);
                    });
                  }}
                  onClearAxis={async () => {
                    await runAction(async () => {
                      await updateMirrorPreviewAxis(null);
                    });
                  }}
                  onConfirm={async () => {
                    await runAction(async () => {
                      await commitMirrorPreview();
                    });
                    setMirrorFocusedSlot(null);
                  }}
                  onCancel={async () => {
                    await runAction(async () => {
                      await cancelMirrorPreview();
                    });
                    setMirrorFocusedSlot(null);
                  }}
                />
              ) : null}
              {moveAction ? (
                <MovePreviewPanel
                  phase={moveAction.phase}
                  bodyLabel={
                    moveAction.phase === "active"
                      ? (viewport?.bodies.find(
                          (body) => body.id === moveAction.targetBodyId,
                        )?.label ?? null)
                      : null
                  }
                  parameters={
                    moveAction.phase === "active"
                      ? (activeMoveParameters ?? moveAction.parameters)
                      : moveAction.parameters
                  }
                  disabled={status !== "connected"}
                  onPreviewParameters={async (parameters) => {
                    if (moveAction.phase !== "active") {
                      setMoveAction((current) =>
                        current?.phase === "pending"
                          ? { ...current, parameters }
                          : current,
                      );
                      return;
                    }
                    await runAction(async () => {
                      await updateMoveParameters(moveAction.featureId, parameters);
                    });
                    setMoveAction((current) =>
                      current?.phase === "active" &&
                      current.featureId === moveAction.featureId
                        ? { ...current, parameters }
                        : current,
                    );
                  }}
                  onConfirm={async () => {
                    if (moveAction.phase === "active") {
                      await runAction(async () => {
                        await confirmMove(moveAction.featureId);
                        await clearSelection();
                      });
                    }
                    setMoveAction(null);
                    await restoreTimelineCursorAfterEdit();
                  }}
                  onCancel={async () => {
                    await cancelActiveTool();
                  }}
                />
              ) : null}
              {importAction ? (
                <ImportPreviewPanel
                  kind={importAction.kind}
                  planeSelected={importAction.parameters !== null}
                  fileSelected={importAction.featureId !== null}
                  fileName={importAction.fileName}
                  parameters={importAction.parameters}
                  disabled={status !== "connected"}
                  onPickFile={pickImportFile}
                  onPreviewParameters={async (parameters) => {
                    setImportAction((current) =>
                      current ? { ...current, parameters } : current,
                    );
                    if (!importAction.featureId) {
                      return;
                    }
                    await runAction(async () => {
                      if (importAction.kind === "image") {
                        await updateImageImport(importAction.featureId!, parameters);
                      } else {
                        await updateSvgImport(importAction.featureId!, parameters);
                      }
                    });
                  }}
                  onConfirm={async () => {
                    if (!importAction.featureId) {
                      return;
                    }
                    try {
                      if (importAction.kind === "image") {
                        await confirmImageImport(importAction.featureId!);
                      } else {
                        await confirmSvgImport(importAction.featureId!);
                      }
                      await clearSelection();
                      setImportAction(null);
                    } catch (error) {
                      addMessage(`import confirm error: ${String(error)}`);
                    }
                  }}
                  onCancel={async () => {
                    await cancelActiveTool();
                  }}
                />
              ) : null}
              {edgeOpAction ? (
                <EdgeOpPreviewPanel
                  title={
                    edgeOpAction.kind === "fillet"
                      ? t("toolbar.fillet")
                      : t("toolbar.chamfer")
                  }
                  valueLabel={
                    edgeOpAction.kind === "fillet"
                      ? t("forms.radiusMm")
                      : t("forms.distanceMm")
                  }
                  initialValue={edgeOpAction.initialValue}
                  disabled={status !== "connected"}
                  edgeCount={
                    edgeOpAction.phase === "active"
                      ? edgeOpAction.edgeIds.length
                      : 0
                  }
                  onPreviewValue={async (value) => {
                    if (edgeOpAction.phase === "pending") {
                      // No feature exists yet; remember the value so
                      // the next edge click creates the feature with
                      // it.
                      pendingValueRef.current = value;
                      return;
                    }
                    await runAction(async () => {
                      if (edgeOpAction.kind === "fillet") {
                        await updateFilletRadius(edgeOpAction.featureId, value);
                      } else {
                        await updateChamferDistance(
                          edgeOpAction.featureId,
                          value,
                        );
                      }
                    });
                  }}
                  onConfirm={async () => {
                    // Pending phase: nothing to commit — just close.
                    if (edgeOpAction.phase === "active") {
                      // Active: tell the core the panel session is
                      // over so the body's edge identity stops
                      // shadowing through the pre-fillet pick_shape
                      // and follows the post-fillet topology like
                      // any other confirmed feature. Then clear the
                      // edge selection (the core kept it in sync
                      // with the live edge_ids during the session
                      // for the highlight) so the user looks at a
                      // clean body.
                      const featureId = edgeOpAction.featureId;
                      const kind = edgeOpAction.kind;
                      await runAction(async () => {
                        if (kind === "fillet") {
                          await confirmFillet(featureId);
                        } else {
                          await confirmChamfer(featureId);
                        }
                        await clearSelection();
                      });
                    }
                    activeEdgeIdsRef.current = [];
                    setEdgeOpAction(null);
                  }}
                  onCancel={async () => {
                    await cancelActiveTool();
                  }}
                />
              ) : null}
              {shellAction ? (
                <ShellPreviewPanel
                  isPending={shellAction.phase === "pending"}
                  initialThickness={shellAction.initialThickness}
                  faceSummary={
                    shellAction.phase === "active"
                      ? shellAction.faceSummary
                      : ""
                  }
                  disabled={status !== "connected"}
                  onPreviewThickness={async (thickness) => {
                    if (shellAction.phase === "pending") {
                      pendingShellThicknessRef.current = thickness;
                      return;
                    }
                    await runAction(async () => {
                      await updateShellThickness(
                        shellAction.featureId,
                        thickness,
                      );
                    });
                  }}
                  onConfirm={async () => {
                    if (shellAction.phase === "active") {
                      await runAction(async () => {
                        await confirmShell(shellAction.featureId);
                        await clearSelection();
                      });
                    }
                    setShellAction(null);
                  }}
                  onCancel={async () => {
                    await cancelActiveTool();
                  }}
                />
              ) : null}
              {holeAction ? (
                <section className="pointer-events-auto cad-floating-panel px-5 py-5">
                  <div className="space-y-4">
                    <div>
                      <p className="cad-kicker">{t("panels.hole.title")}</p>
                      <p className="mt-3 text-sm tracking-[0.18em] text-[color:var(--cad-muted)] uppercase">
                        {holeAction.phase === "pending"
                          ? t("panels.hole.pickFace")
                          : t("panels.hole.faceSelected")}
                      </p>
                    </div>
                    {activeHoleParameters ? (
                      <>
                        <Dropdown
                          value={activeHoleParameters.standard}
                          label={t("panels.hole.standard")}
                          options={[
                            {
                              value: "custom",
                              label: t("panels.hole.custom"),
                            },
                            {
                              value: "metric",
                              label: t("panels.hole.metric"),
                            },
                            {
                              value: "imperial",
                              label: t("panels.hole.imperial"),
                            },
                          ]}
                          disabled={status !== "connected"}
                          onChange={(standard: HoleStandard) => {
                            void runAction(async () => {
                              if (holeAction.phase !== "active") {
                                return;
                              }
                              const standards = holeStandardsForMode(standard);
                              if (standard === "custom" || standards.length === 0) {
                                await updateHoleParameters(holeAction.featureId, {
                                  ...activeHoleParameters,
                                  standard: "custom",
                                  standard_size: "",
                                  hole_fit: "clearance",
                                });
                                return;
                              }
                              await updateHoleParameters(
                                holeAction.featureId,
                                applyHoleStandard(
                                  {
                                    ...activeHoleParameters,
                                    standard,
                                    standard_size: standards[0].id,
                                  },
                                  standards[0],
                                  activeHoleParameters.hole_fit,
                                ),
                              );
                            });
                          }}
                        />
                        {activeHoleParameters.standard !== "custom" ? (
                          <div className="grid grid-cols-2 gap-3">
                            <Dropdown
                              value={
                                activeHoleParameters.standard_size ||
                                activeHoleStandards[0]?.id ||
                                ""
                              }
                              label={t("panels.hole.size")}
                              options={activeHoleStandards.map((entry) => ({
                                value: entry.id,
                                label: entry.label,
                              }))}
                              disabled={
                                status !== "connected" ||
                                activeHoleStandards.length === 0
                              }
                              onChange={(standardSize) => {
                                void runAction(async () => {
                                  if (holeAction.phase !== "active") {
                                    return;
                                  }
                                  const entry = findHoleStandard(
                                    activeHoleParameters.standard,
                                    standardSize,
                                  );
                                  if (!entry) {
                                    return;
                                  }
                                  await updateHoleParameters(
                                    holeAction.featureId,
                                    applyHoleStandard(
                                      activeHoleParameters,
                                      entry,
                                      activeHoleParameters.hole_fit,
                                    ),
                                  );
                                });
                              }}
                            />
                            <Dropdown
                              value={activeHoleParameters.hole_fit}
                              label={t("panels.hole.fit")}
                              options={[
                                {
                                  value: "clearance",
                                  label: t("panels.hole.clearance"),
                                },
                                {
                                  value: "tap_drill",
                                  label: t("panels.hole.tapDrill"),
                                },
                                {
                                  value: "threaded",
                                  label: t("panels.hole.threaded"),
                                },
                              ]}
                              disabled={status !== "connected"}
                              onChange={(fit: HoleFit) => {
                                void runAction(async () => {
                                  if (holeAction.phase !== "active") {
                                    return;
                                  }
                                  const entry = findHoleStandard(
                                    activeHoleParameters.standard,
                                    activeHoleParameters.standard_size,
                                  );
                                  if (!entry) {
                                    return;
                                  }
                                  await updateHoleParameters(
                                    holeAction.featureId,
                                    applyHoleStandard(
                                      activeHoleParameters,
                                      entry,
                                      fit,
                                    ),
                                  );
                                });
                              }}
                            />
                          </div>
                        ) : null}
                        <Dropdown
                          value={activeHoleParameters.hole_type}
                          label={t("panels.hole.type")}
                          options={[
                            {
                              value: "simple",
                              label: t("panels.hole.simple"),
                            },
                            {
                              value: "counterbore",
                              label: t("panels.hole.counterbore"),
                            },
                            {
                              value: "countersink",
                              label: t("panels.hole.countersink"),
                            },
                            {
                              value: "spotface",
                              label: t("panels.hole.spotface"),
                            },
                          ]}
                          disabled={status !== "connected"}
                          onChange={(holeType) => {
                            void runAction(async () => {
                              await updateHoleParameters(
                                holeAction.phase === "active"
                                  ? holeAction.featureId
                                  : "",
                                {
                                  ...activeHoleParameters,
                                  hole_type: holeType,
                                },
                              );
                            });
                          }}
                        />
                        <Dropdown
                          value={activeHoleParameters.extent_type}
                          label={t("panels.hole.extent")}
                          options={[
                            {
                              value: "blind",
                              label: t("panels.hole.blind"),
                            },
                            {
                              value: "through_all",
                              label: t("panels.hole.throughAll"),
                            },
                          ]}
                          disabled={status !== "connected"}
                          onChange={(extentType) => {
                            void runAction(async () => {
                              await updateHoleParameters(
                                holeAction.phase === "active"
                                  ? holeAction.featureId
                                  : "",
                                {
                                  ...activeHoleParameters,
                                  extent_type: extentType,
                                },
                              );
                            });
                          }}
                        />
                        <label className="block text-xs uppercase tracking-[0.18em] text-on-surface-muted">
                          <span>{t("panels.hole.diameter")}</span>
                          <input
                            className="cad-input mt-2"
                            type="number"
                            min="0.01"
                            step="any"
                            value={activeHoleParameters.diameter}
                            disabled={status !== "connected"}
                            onChange={(event) => {
                              const diameter = Number(event.target.value);
                              if (!Number.isFinite(diameter) || diameter <= 0) {
                                return;
                              }
                              void runAction(async () => {
                                await updateHoleParameters(
                                  holeAction.phase === "active"
                                    ? holeAction.featureId
                                    : "",
                                  {
                                    ...activeHoleParameters,
                                    diameter,
                                  },
                                );
                              });
                            }}
                          />
                        </label>
                        {activeHoleParameters.extent_type === "blind" ? (
                          <label className="block text-xs uppercase tracking-[0.18em] text-on-surface-muted">
                            <span>{t("panels.hole.depth")}</span>
                            <input
                              className="cad-input mt-2"
                              type="number"
                              min="0.01"
                              step="any"
                              value={activeHoleParameters.depth}
                              disabled={status !== "connected"}
                              onChange={(event) => {
                                const depth = Number(event.target.value);
                                if (!Number.isFinite(depth) || depth <= 0) {
                                  return;
                                }
                                void runAction(async () => {
                                  await updateHoleParameters(
                                    holeAction.phase === "active"
                                      ? holeAction.featureId
                                      : "",
                                    {
                                      ...activeHoleParameters,
                                      depth,
                                      thread_depth:
                                        activeHoleParameters.thread_enabled
                                          ? Math.min(
                                              activeHoleParameters.thread_depth,
                                              depth,
                                            )
                                          : activeHoleParameters.thread_depth,
                                    },
                                  );
                                });
                              }}
                            />
                          </label>
                        ) : null}
                        {activeHoleParameters.hole_type === "counterbore" ||
                        activeHoleParameters.hole_type === "spotface" ? (
                          <div className="grid grid-cols-2 gap-3">
                            <label className="block text-xs uppercase tracking-[0.18em] text-on-surface-muted">
                              <span>{t("panels.hole.counterboreDiameter")}</span>
                              <input
                                className="cad-input mt-2"
                                type="number"
                                min="0.01"
                                step="any"
                                value={activeHoleParameters.counterbore_diameter}
                                disabled={status !== "connected"}
                                onChange={(event) => {
                                  const value = Number(event.target.value);
                                  if (!Number.isFinite(value) || value <= 0) {
                                    return;
                                  }
                                  void runAction(async () => {
                                    await updateHoleParameters(
                                      holeAction.phase === "active"
                                        ? holeAction.featureId
                                        : "",
                                      {
                                        ...activeHoleParameters,
                                        counterbore_diameter: value,
                                      },
                                    );
                                  });
                                }}
                              />
                            </label>
                            <label className="block text-xs uppercase tracking-[0.18em] text-on-surface-muted">
                              <span>{t("panels.hole.counterboreDepth")}</span>
                              <input
                                className="cad-input mt-2"
                                type="number"
                                min="0.01"
                                step="any"
                                value={activeHoleParameters.counterbore_depth}
                                disabled={status !== "connected"}
                                onChange={(event) => {
                                  const value = Number(event.target.value);
                                  if (!Number.isFinite(value) || value <= 0) {
                                    return;
                                  }
                                  void runAction(async () => {
                                    await updateHoleParameters(
                                      holeAction.phase === "active"
                                        ? holeAction.featureId
                                        : "",
                                      {
                                        ...activeHoleParameters,
                                        counterbore_depth: value,
                                      },
                                    );
                                  });
                                }}
                              />
                            </label>
                          </div>
                        ) : null}
                        {activeHoleParameters.hole_type === "countersink" ? (
                          <div className="grid grid-cols-2 gap-3">
                            <label className="block text-xs uppercase tracking-[0.18em] text-on-surface-muted">
                              <span>{t("panels.hole.countersinkDiameter")}</span>
                              <input
                                className="cad-input mt-2"
                                type="number"
                                min="0.01"
                                step="any"
                                value={activeHoleParameters.countersink_diameter}
                                disabled={status !== "connected"}
                                onChange={(event) => {
                                  const value = Number(event.target.value);
                                  if (!Number.isFinite(value) || value <= 0) {
                                    return;
                                  }
                                  void runAction(async () => {
                                    await updateHoleParameters(
                                      holeAction.phase === "active"
                                        ? holeAction.featureId
                                        : "",
                                      {
                                        ...activeHoleParameters,
                                        countersink_diameter: value,
                                      },
                                    );
                                  });
                                }}
                              />
                            </label>
                            <label className="block text-xs uppercase tracking-[0.18em] text-on-surface-muted">
                              <span>{t("panels.hole.countersinkAngle")}</span>
                              <input
                                className="cad-input mt-2"
                                type="number"
                                min="1"
                                max="179"
                                step="any"
                                value={
                                  activeHoleParameters.countersink_angle_degrees
                                }
                                disabled={status !== "connected"}
                                onChange={(event) => {
                                  const value = Number(event.target.value);
                                  if (!Number.isFinite(value) || value <= 0) {
                                    return;
                                  }
                                  void runAction(async () => {
                                    await updateHoleParameters(
                                      holeAction.phase === "active"
                                        ? holeAction.featureId
                                        : "",
                                      {
                                        ...activeHoleParameters,
                                        countersink_angle_degrees: value,
                                      },
                                    );
                                  });
                                }}
                              />
                            </label>
                          </div>
                        ) : null}
                        <label className="flex items-center justify-between gap-3 text-xs uppercase tracking-[0.18em] text-on-surface-muted">
                          <span>{t("panels.hole.threaded")}</span>
                          <Checkbox
                            checked={activeHoleParameters.thread_enabled}
                            ariaLabel={t("panels.hole.threaded")}
                            disabled={status !== "connected"}
                            onCheckedChange={(checked) => {
                              void runAction(async () => {
                                await updateHoleParameters(
                                  holeAction.phase === "active"
                                    ? holeAction.featureId
                                    : "",
                                  {
                                    ...activeHoleParameters,
                                    thread_enabled: checked,
                                    thread_representation: checked
                                      ? activeHoleParameters.thread_representation
                                      : "cosmetic",
                                  },
                                );
                              });
                            }}
                          />
                        </label>
                        {activeHoleParameters.thread_enabled ? (
                          <div>
                            <span className="cad-field-label">
                              {t("panels.hole.representation")}
                            </span>
                            <Dropdown
                              label={t("panels.hole.representation")}
                              className="mt-2"
                              value={activeHoleParameters.thread_representation}
                              disabled={status !== "connected"}
                              options={[
                                {
                                  value: "cosmetic",
                                  label: t("panels.hole.cosmetic"),
                                },
                                {
                                  value: "modeled",
                                  label: t("panels.hole.modeled"),
                                },
                              ]}
                              onChange={(value) => {
                                void runAction(async () => {
                                  await updateHoleParameters(
                                    holeAction.phase === "active"
                                      ? holeAction.featureId
                                      : "",
                                    {
                                      ...activeHoleParameters,
                                      thread_representation: value,
                                    },
                                  );
                                });
                              }}
                            />
                            <p className="mt-1 text-xs text-[color:var(--cad-muted)]">
                              {t("panels.hole.cosmeticThreadOnly")}
                            </p>
                          </div>
                        ) : null}
                      </>
                    ) : null}
                    <div className="flex gap-3 pt-1">
                      <button
                        type="button"
                        className="cad-ribbon-action cad-ribbon-action-primary flex-1"
                        disabled={
                          status !== "connected" ||
                          holeAction.phase !== "active"
                        }
                        onClick={() => {
                          void runAction(async () => {
                            if (holeAction.phase === "active") {
                              await confirmHole(holeAction.featureId);
                              await clearSelection();
                            }
                            setHoleAction(null);
                          });
                        }}
                      >
                        {t("common.confirm")}
                      </button>
                      <button
                        type="button"
                        className="cad-ribbon-action flex-1"
                        onClick={() => {
                          void cancelActiveTool();
                        }}
                      >
                        {t("common.cancel")}
                      </button>
                    </div>
                  </div>
                </section>
              ) : null}
              {offsetPlaneAction ? (
                <OffsetPlanePanel
                  isPending={offsetPlaneAction.phase === "pending"}
                  initialOffset={offsetPlaneAction.initialOffset}
                  sourceSummary={
                    offsetPlaneAction.phase === "active"
                      ? offsetPlaneAction.sourceSummary
                      : ""
                  }
                  disabled={status !== "connected"}
                  onPreviewOffset={async (offset) => {
                    if (offsetPlaneAction.phase === "pending") {
                      // No feature exists yet; remember the value so
                      // the next plane click creates the feature
                      // with it.
                      pendingOffsetRef.current = offset;
                      return;
                    }
                    await runAction(async () => {
                      await updateOffsetPlane(
                        offsetPlaneAction.featureId,
                        offset,
                      );
                    });
                  }}
                  onConfirm={async () => {
                    // Active phase only: feature stays in the
                    // document with whatever offset is currently in
                    // the panel. Selection is left as-is so the user
                    // can immediately turn around and Sketch on it.
                    setOffsetPlaneAction(null);
                  }}
                  onCancel={async () => {
                    await cancelActiveTool();
                  }}
                />
              ) : null}
              {anglePlaneAction ? (
                <AnglePlanePanel
                  phase={anglePlaneAction.phase}
                  initialAngle={anglePlaneAction.initialAngle}
                  sourceSummary={
                    anglePlaneAction.phase === "pick_plane"
                      ? ""
                      : anglePlaneAction.sourceSummary
                  }
                  axisSummary={
                    anglePlaneAction.phase === "active"
                      ? anglePlaneAction.axisSummary
                      : ""
                  }
                  disabled={status !== "connected"}
                  onPreviewAngle={async (angleDegrees) => {
                    if (anglePlaneAction.phase !== "active") {
                      pendingAngleRef.current = angleDegrees;
                      return;
                    }
                    await runAction(async () => {
                      await updateAnglePlane(
                        anglePlaneAction.featureId,
                        angleDegrees,
                      );
                    });
                  }}
                  onConfirm={async () => {
                    setAnglePlaneAction(null);
                  }}
                  onCancel={async () => {
                    await cancelActiveTool();
                  }}
                />
              ) : null}
              {midplaneAction ? (
                <section className="pointer-events-auto cad-floating-panel px-5 py-5">
                  <p className="cad-kicker">{t("panels.midplane.title")}</p>
                  <p className="mt-3 text-xs text-on-surface-muted">
                    {midplaneAction.sourceIds.length === 0
                      ? t("panels.midplane.pickFirst")
                      : t("panels.midplane.pickSecond")}
                  </p>
                  <p className="mt-3 text-xs uppercase tracking-[0.16em] text-on-surface-dim">
                    {t("panels.midplane.selected", {
                      count: midplaneAction.sourceIds.length,
                    })}
                  </p>
                  <button
                    type="button"
                    className="cad-action-ghost mt-4 w-full"
                    disabled={status !== "connected"}
                    onClick={() => {
                      void cancelActiveTool();
                    }}
                  >
                    {t("common.cancel")}
                  </button>
                </section>
              ) : null}
              {tangentPlaneAction ? (
                <section className="pointer-events-auto cad-floating-panel px-5 py-5">
                  <p className="cad-kicker">{t("panels.tangentPlane.title")}</p>
                  <p className="mt-3 text-xs text-on-surface-muted">
                    {t("panels.tangentPlane.pickSource")}
                  </p>
                  <button
                    type="button"
                    className="cad-action-ghost mt-4 w-full"
                    disabled={status !== "connected"}
                    onClick={() => {
                      void cancelActiveTool();
                    }}
                  >
                    {t("common.cancel")}
                  </button>
                </section>
              ) : null}
              {constructionAxisAction ? (
                <section className="pointer-events-auto cad-floating-panel px-5 py-5">
                  <p className="cad-kicker">{t("panels.constructionAxis.title")}</p>
                  <p className="mt-3 text-xs text-on-surface-muted">
                    {t("panels.constructionAxis.pickSource")}
                  </p>
                  <button
                    type="button"
                    className="cad-action-ghost mt-4 w-full"
                    disabled={status !== "connected"}
                    onClick={() => {
                      void cancelActiveTool();
                    }}
                  >
                    {t("common.cancel")}
                  </button>
                </section>
              ) : null}
              {constructionPointAction ? (
                <section className="pointer-events-auto cad-floating-panel px-5 py-5">
                  <p className="cad-kicker">{t("panels.constructionPoint.title")}</p>
                  <p className="mt-3 text-xs text-on-surface-muted">
                    {t("panels.constructionPoint.pickSource")}
                  </p>
                  <button
                    type="button"
                    className="cad-action-ghost mt-4 w-full"
                    disabled={status !== "connected"}
                    onClick={() => {
                      void cancelActiveTool();
                    }}
                  >
                    {t("common.cancel")}
                  </button>
                </section>
              ) : null}
              {threadAction ? (
                <section className="pointer-events-auto cad-floating-panel cad-scrollbar max-h-[min(42rem,calc(100vh-12rem))] w-[21rem] overflow-y-auto px-5 py-5">
                  <p className="cad-kicker">{t("panels.thread.title")}</p>
                  {threadAction.phase !== "active" ? (
                    <>
                      {threadAction.phase === "pick_axis" ? (
                        <div className="mt-4 grid grid-cols-[auto_1fr] gap-x-4 gap-y-2 text-xs uppercase tracking-[0.18em] text-on-surface-dim">
                          <span>{t("panels.thread.target")}</span>
                          <span className="truncate text-right text-on-surface">
                            {threadAction.targetSummary}
                          </span>
                        </div>
                      ) : threadAction.axisSourceId ? (
                        <div className="mt-4 grid grid-cols-[auto_1fr] gap-x-4 gap-y-2 text-xs uppercase tracking-[0.18em] text-on-surface-dim">
                          <span>{t("panels.thread.axis")}</span>
                          <span className="truncate text-right text-on-surface">
                            {describeAxisSource(threadAction.axisSourceId)}
                          </span>
                        </div>
                      ) : null}
                      <p className="mt-3 text-xs text-on-surface-muted">
                        {threadAction.phase === "pick_axis"
                          ? t("panels.thread.pickAxis")
                          : t("panels.thread.pickTarget")}
                      </p>
                      <button
                        type="button"
                        className="cad-action-ghost mt-4 w-full"
                        disabled={status !== "connected"}
                        onClick={() => {
                          void cancelActiveTool();
                        }}
                      >
                        {t("common.cancel")}
                      </button>
                    </>
                  ) : activeThreadParameters ? (
                    <>
                      <div className="mt-4 grid grid-cols-[auto_1fr] gap-x-4 gap-y-2 text-xs uppercase tracking-[0.18em] text-on-surface-dim">
                        <span>{t("panels.thread.target")}</span>
                        <span className="truncate text-right text-on-surface">
                          {describeThreadTarget(activeThreadParameters.target_body_id)}
                        </span>
                        <span>{t("panels.thread.axis")}</span>
                        <span className="truncate text-right text-on-surface">
                          {describeAxisSource(activeThreadParameters.axis_source_id)}
                        </span>
                      </div>
                      <div className="mt-5 space-y-4">
                        <div>
                          <span className="cad-field-label">
                            {t("panels.thread.mode")}
                          </span>
                          <Dropdown
                            label={t("panels.thread.mode")}
                            className="mt-2"
                            value={activeThreadParameters.mode}
                            disabled={status !== "connected"}
                            options={[
                              {
                                value: "external",
                                label: t("panels.thread.external"),
                              },
                              {
                                value: "internal",
                                label: t("panels.thread.internal"),
                              },
                            ]}
                            onChange={(value) => {
                              void updateActiveThreadParameters({ mode: value });
                            }}
                          />
                        </div>
                        <div>
                          <span className="cad-field-label">
                            {t("panels.thread.standard")}
                          </span>
                          <Dropdown
                            label={t("panels.thread.standard")}
                            className="mt-2"
                            value={activeThreadParameters.standard}
                            disabled={status !== "connected"}
                            options={[
                              { value: "custom", label: t("panels.thread.custom") },
                              { value: "metric", label: t("panels.thread.metric") },
                              {
                                value: "imperial",
                                label: t("panels.thread.imperial"),
                              },
                            ]}
                            onChange={(value) => {
                              if (value === "custom") {
                                void updateActiveThreadParameters({
                                  standard: value,
                                  size: "",
                                });
                                return;
                              }
                              const entry = holeStandardsForMode(value)[0];
                              if (!entry) {
                                return;
                              }
                              void updateActiveThreadParameters({
                                standard: value,
                                size: entry.id,
                                major_diameter: entry.majorDiameter,
                                minor_diameter: entry.minorDiameter,
                                pitch: entry.pitch,
                              });
                            }}
                          />
                        </div>
                        {activeThreadParameters.standard !== "custom" ? (
                          <div>
                            <span className="cad-field-label">
                              {t("panels.thread.size")}
                            </span>
                            <Dropdown
                              label={t("panels.thread.size")}
                              className="mt-2"
                              value={
                                findHoleStandard(
                                  activeThreadParameters.standard,
                                  activeThreadParameters.size,
                                )?.id ??
                                activeThreadStandards[0]?.id ??
                                ""
                              }
                              disabled={
                                status !== "connected" ||
                                activeThreadStandards.length === 0
                              }
                              options={activeThreadStandards.map((entry) => ({
                                value: entry.id,
                                label: entry.label,
                              }))}
                              onChange={(value) => {
                                const entry = findHoleStandard(
                                  activeThreadParameters.standard,
                                  value,
                                );
                                if (!entry) {
                                  return;
                                }
                                void updateActiveThreadParameters({
                                  size: entry.id,
                                  major_diameter: entry.majorDiameter,
                                  minor_diameter: entry.minorDiameter,
                                  pitch: entry.pitch,
                                });
                              }}
                            />
                          </div>
                        ) : null}
                        <div className="grid grid-cols-2 gap-3">
                          <label className="block">
                            <span className="cad-field-label">
                              {t("panels.thread.majorDiameter")}
                            </span>
                            <input
                              type="number"
                              min={0}
                              step={0.1}
                              className="cad-input mt-2 w-full"
                              value={activeThreadParameters.major_diameter}
                              disabled={status !== "connected"}
                              onChange={(event) => {
                                const value = event.currentTarget.valueAsNumber;
                                if (!Number.isFinite(value)) {
                                  return;
                                }
                                void updateActiveThreadParameters({
                                  major_diameter: value,
                                });
                              }}
                            />
                          </label>
                          <label className="block">
                            <span className="cad-field-label">
                              {t("panels.thread.pitch")}
                            </span>
                            <input
                              type="number"
                              min={0}
                              step={0.1}
                              className="cad-input mt-2 w-full"
                              value={activeThreadParameters.pitch}
                              disabled={status !== "connected"}
                              onChange={(event) => {
                                const value = event.currentTarget.valueAsNumber;
                                if (!Number.isFinite(value)) {
                                  return;
                                }
                                void updateActiveThreadParameters({ pitch: value });
                              }}
                            />
                          </label>
                        </div>
                        <div className="grid grid-cols-2 gap-3">
                          <label className="block">
                            <span className="cad-field-label">
                              {t("panels.thread.length")}
                            </span>
                            <input
                              type="number"
                              min={0}
                              step={0.1}
                              className="cad-input mt-2 w-full"
                              value={activeThreadParameters.length}
                              disabled={status !== "connected"}
                              onChange={(event) => {
                                const value = event.currentTarget.valueAsNumber;
                                if (!Number.isFinite(value)) {
                                  return;
                                }
                                void updateActiveThreadParameters({ length: value });
                              }}
                            />
                          </label>
                          <label className="block">
                            <span className="cad-field-label">
                              {t("panels.thread.startOffset")}
                            </span>
                            <input
                              type="number"
                              step={0.1}
                              className="cad-input mt-2 w-full"
                              value={activeThreadParameters.start_offset}
                              disabled={status !== "connected"}
                              onChange={(event) => {
                                const value = event.currentTarget.valueAsNumber;
                                if (!Number.isFinite(value)) {
                                  return;
                                }
                                void updateActiveThreadParameters({
                                  start_offset: value,
                                });
                              }}
                            />
                          </label>
                        </div>
                        <div>
                          <span className="cad-field-label">
                            {t("panels.thread.handedness")}
                          </span>
                          <Dropdown
                            label={t("panels.thread.handedness")}
                            className="mt-2"
                            value={activeThreadParameters.handedness}
                            disabled={status !== "connected"}
                            options={[
                              {
                                value: "right",
                                label: t("panels.thread.rightHand"),
                              },
                              {
                                value: "left",
                                label: t("panels.thread.leftHand"),
                              },
                            ]}
                            onChange={(value) => {
                              void updateActiveThreadParameters({
                                handedness: value,
                              });
                            }}
                          />
                        </div>
                        <div className="rounded-md border border-outline/50 bg-surface-container-low px-3 py-2">
                          <span className="cad-field-label">
                            {t("panels.thread.representation")}
                          </span>
                          <Dropdown
                            label={t("panels.thread.representation")}
                            className="mt-2"
                            value={activeThreadParameters.representation}
                            disabled={status !== "connected"}
                            options={[
                              {
                                value: "cosmetic",
                                label: t("panels.thread.cosmetic"),
                              },
                              {
                                value: "modeled",
                                label: t("panels.thread.modeled"),
                              },
                            ]}
                            onChange={(value) => {
                              void updateActiveThreadParameters({
                                representation: value,
                              });
                            }}
                          />
                          <p className="mt-1 text-[11px] leading-4 text-on-surface-dim">
                            {t("panels.thread.modeledUnavailable")}
                          </p>
                        </div>
                      </div>
                      <div className="mt-5 flex gap-3">
                        <button
                          type="button"
                          className="cad-ribbon-action cad-ribbon-action-primary flex-1"
                          disabled={status !== "connected"}
                          onClick={() => {
                            void runAction(async () => {
                              await confirmThread(threadAction.featureId);
                              setThreadAction(null);
                              await restoreTimelineCursorAfterEdit();
                            });
                          }}
                        >
                          {t("common.confirm")}
                        </button>
                        <button
                          type="button"
                          className="cad-ribbon-action flex-1"
                          onClick={() => {
                            void cancelActiveTool();
                          }}
                        >
                          {t("common.cancel")}
                        </button>
                      </div>
                    </>
                  ) : null}
                </section>
              ) : null}
              {fastenerAction && activeFastenerParameters ? (
                <section className="pointer-events-auto cad-floating-panel cad-scrollbar max-h-[min(40rem,calc(100vh-12rem))] w-[21rem] overflow-y-auto px-5 py-5">
                  <p className="cad-kicker">{t("panels.fastener.title")}</p>
                  <div className="mt-5 space-y-4">
                    <div>
                      <span className="cad-field-label">
                        {t("panels.fastener.standard")}
                      </span>
                      <Dropdown
                        label={t("panels.fastener.standard")}
                        className="mt-2"
                        value={activeFastenerParameters.standard}
                        disabled={status !== "connected"}
                        options={[
                          { value: "custom", label: t("panels.fastener.custom") },
                          { value: "metric", label: t("panels.fastener.metric") },
                          {
                            value: "imperial",
                            label: t("panels.fastener.imperial"),
                          },
                        ]}
                        onChange={(value) => {
                          if (value === "custom") {
                            void updateActiveFastenerParameters({
                              standard: value,
                              size: "",
                            });
                            return;
                          }
                          const entry = holeStandardsForMode(value)[0];
                          if (!entry) {
                            return;
                          }
                          void updateActiveFastenerParameters({
                            standard: value,
                            size: entry.id,
                            diameter: entry.majorDiameter,
                            minor_diameter: entry.minorDiameter,
                            pitch: entry.pitch,
                          });
                        }}
                      />
                    </div>
                    {activeFastenerParameters.standard !== "custom" ? (
                      <div>
                        <span className="cad-field-label">
                          {t("panels.fastener.size")}
                        </span>
                        <Dropdown
                          label={t("panels.fastener.size")}
                          className="mt-2"
                          value={
                            findHoleStandard(
                              activeFastenerParameters.standard,
                              activeFastenerParameters.size,
                            )?.id ??
                            activeFastenerStandards[0]?.id ??
                            ""
                          }
                          disabled={
                            status !== "connected" ||
                            activeFastenerStandards.length === 0
                          }
                          options={activeFastenerStandards.map((entry) => ({
                            value: entry.id,
                            label: entry.label,
                          }))}
                          onChange={(value) => {
                            const entry = findHoleStandard(
                              activeFastenerParameters.standard,
                              value,
                            );
                            if (!entry) {
                              return;
                            }
                            void updateActiveFastenerParameters({
                              size: entry.id,
                              diameter: entry.majorDiameter,
                              minor_diameter: entry.minorDiameter,
                              pitch: entry.pitch,
                            });
                          }}
                        />
                      </div>
                    ) : null}
                    <div className="grid grid-cols-2 gap-3">
                      <label className="block">
                        <span className="cad-field-label">
                          {t("panels.fastener.diameter")}
                        </span>
                        <input
                          type="number"
                          min={0}
                          step={0.1}
                          className="cad-input mt-2 w-full"
                          value={activeFastenerParameters.diameter}
                          disabled={status !== "connected"}
                          onChange={(event) => {
                            const value = event.currentTarget.valueAsNumber;
                            if (!Number.isFinite(value)) {
                              return;
                            }
                            void updateActiveFastenerParameters({
                              diameter: value,
                            });
                          }}
                        />
                      </label>
                      <label className="block">
                        <span className="cad-field-label">
                          {t("panels.fastener.length")}
                        </span>
                        <input
                          type="number"
                          min={0}
                          step={0.1}
                          className="cad-input mt-2 w-full"
                          value={activeFastenerParameters.length}
                          disabled={status !== "connected"}
                          onChange={(event) => {
                            const value = event.currentTarget.valueAsNumber;
                            if (!Number.isFinite(value)) {
                              return;
                            }
                            void updateActiveFastenerParameters({ length: value });
                          }}
                        />
                      </label>
                    </div>
                    <label className="block">
                      <span className="cad-field-label">
                        {t("panels.fastener.threadLength")}
                      </span>
                      <input
                        type="number"
                        min={0}
                        step={0.1}
                        className="cad-input mt-2 w-full"
                        value={activeFastenerParameters.thread_length}
                        disabled={status !== "connected"}
                        onChange={(event) => {
                          const value = event.currentTarget.valueAsNumber;
                          if (!Number.isFinite(value)) {
                            return;
                          }
                          void updateActiveFastenerParameters({
                            thread_length: value,
                          });
                        }}
                      />
                    </label>
                    <div>
                      <span className="cad-field-label">
                        {t("panels.fastener.headType")}
                      </span>
                      <Dropdown
                        label={t("panels.fastener.headType")}
                        className="mt-2"
                        value={activeFastenerParameters.head_type}
                        disabled={status !== "connected"}
                        options={[
                          {
                            value: "socket_head",
                            label: t("panels.fastener.socketHead"),
                          },
                          {
                            value: "button_head",
                            label: t("panels.fastener.buttonHead"),
                          },
                          { value: "flat", label: t("panels.fastener.flat") },
                          {
                            value: "hex_bolt",
                            label: t("panels.fastener.hexBolt"),
                          },
                        ]}
                        onChange={(value) => {
                          void updateActiveFastenerParameters({ head_type: value });
                        }}
                      />
                    </div>
                    <div>
                      <span className="cad-field-label">
                        {t("panels.fastener.driveType")}
                      </span>
                      <Dropdown
                        label={t("panels.fastener.driveType")}
                        className="mt-2"
                        value={activeFastenerParameters.drive_type}
                        disabled={status !== "connected"}
                        options={[
                          { value: "none", label: t("panels.fastener.none") },
                          {
                            value: "hex_socket",
                            label: t("panels.fastener.hexSocket"),
                          },
                          {
                            value: "phillips",
                            label: t("panels.fastener.phillips"),
                          },
                        ]}
                        onChange={(value) => {
                          void updateActiveFastenerParameters({ drive_type: value });
                        }}
                      />
                    </div>
                    <div className="rounded-md border border-outline/50 bg-surface-container-low px-3 py-2">
                      <span className="cad-field-label">
                        {t("panels.fastener.threadRepresentation")}
                      </span>
                      <Dropdown
                        label={t("panels.fastener.threadRepresentation")}
                        className="mt-2"
                        value={activeFastenerParameters.thread_representation}
                        disabled={status !== "connected"}
                        options={[
                          {
                            value: "cosmetic",
                            label: t("panels.fastener.cosmetic"),
                          },
                          {
                            value: "modeled",
                            label: t("panels.fastener.modeled"),
                          },
                        ]}
                        onChange={(value) => {
                          void updateActiveFastenerParameters({
                            thread_representation: value,
                          });
                        }}
                      />
                      <p className="mt-1 text-[11px] leading-4 text-on-surface-dim">
                        {t("panels.fastener.modeledUnavailable")}
                      </p>
                    </div>
                  </div>
                  <div className="mt-5 flex gap-3">
                    <button
                      type="button"
                      className="cad-ribbon-action cad-ribbon-action-primary flex-1"
                      disabled={status !== "connected"}
                      onClick={() => {
                        void runAction(async () => {
                          setFastenerAction(null);
                          await restoreTimelineCursorAfterEdit();
                        });
                      }}
                    >
                      {t("common.confirm")}
                    </button>
                    <button
                      type="button"
                      className="cad-ribbon-action flex-1"
                      onClick={() => {
                        void cancelActiveTool();
                      }}
                    >
                      {t("common.cancel")}
                    </button>
                  </div>
                </section>
              ) : null}
              {helixAction ? (
                <section className="pointer-events-auto cad-floating-panel w-[20rem] px-5 py-5">
                  <p className="cad-kicker">{t("panels.helix.title")}</p>
                  {helixAction.phase === "pending" || !activeHelixParameters ? (
                    <>
                      <p className="mt-3 text-xs text-on-surface-muted">
                        {t("panels.helix.pickAxis")}
                      </p>
                      <button
                        type="button"
                        className="cad-action-ghost mt-4 w-full"
                        disabled={status !== "connected"}
                        onClick={() => {
                          void cancelActiveTool();
                        }}
                      >
                        {t("common.cancel")}
                      </button>
                    </>
                  ) : (
                    <>
                      <div className="mt-4 grid grid-cols-[auto_1fr] gap-x-4 gap-y-2 text-xs uppercase tracking-[0.18em] text-on-surface-dim">
                        <span>{t("panels.helix.axis")}</span>
                        <span className="truncate text-right text-on-surface">
                          {describeAxisSource(activeHelixParameters.axis_source_id)}
                        </span>
                      </div>
                      <div className="mt-5 space-y-4">
                        <label className="block">
                          <span className="cad-field-label">
                            {t("panels.helix.radius")}
                          </span>
                          <input
                            type="number"
                            min={0}
                            step={0.1}
                            className="cad-input mt-2 w-full"
                            value={activeHelixParameters.radius}
                            disabled={status !== "connected"}
                            onChange={(event) => {
                              const value = event.currentTarget.valueAsNumber;
                              if (!Number.isFinite(value)) {
                                return;
                              }
                              void updateActiveHelixParameters({ radius: value });
                            }}
                          />
                        </label>
                        <label className="block">
                          <span className="cad-field-label">
                            {t("panels.helix.pitch")}
                          </span>
                          <input
                            type="number"
                            min={0}
                            step={0.1}
                            className="cad-input mt-2 w-full"
                            value={activeHelixParameters.pitch}
                            disabled={status !== "connected"}
                            onChange={(event) => {
                              const value = event.currentTarget.valueAsNumber;
                              if (!Number.isFinite(value)) {
                                return;
                              }
                              void updateActiveHelixParameters({ pitch: value });
                            }}
                          />
                        </label>
                        <label className="block">
                          <span className="cad-field-label">
                            {t("panels.helix.height")}
                          </span>
                          <input
                            type="number"
                            min={0}
                            step={0.1}
                            className="cad-input mt-2 w-full"
                            value={activeHelixParameters.height}
                            disabled={status !== "connected"}
                            onChange={(event) => {
                              const value = event.currentTarget.valueAsNumber;
                              if (!Number.isFinite(value)) {
                                return;
                              }
                              void updateActiveHelixParameters({ height: value });
                            }}
                          />
                        </label>
                        <label className="block">
                          <span className="cad-field-label">
                            {t("panels.helix.startAngle")}
                          </span>
                          <input
                            type="number"
                            step={1}
                            className="cad-input mt-2 w-full"
                            value={activeHelixParameters.start_angle_degrees}
                            disabled={status !== "connected"}
                            onChange={(event) => {
                              const value = event.currentTarget.valueAsNumber;
                              if (!Number.isFinite(value)) {
                                return;
                              }
                              void updateActiveHelixParameters({
                                start_angle_degrees: value,
                              });
                            }}
                          />
                        </label>
                        <div>
                          <span className="cad-field-label">
                            {t("panels.helix.handedness")}
                          </span>
                          <Dropdown
                            label={t("panels.helix.handedness")}
                            className="mt-2"
                            value={activeHelixParameters.handedness}
                            disabled={status !== "connected"}
                            options={[
                              {
                                value: "right",
                                label: t("panels.helix.rightHand"),
                              },
                              {
                                value: "left",
                                label: t("panels.helix.leftHand"),
                              },
                            ]}
                            onChange={(value) => {
                              void updateActiveHelixParameters({
                                handedness: value,
                              });
                            }}
                          />
                        </div>
                      </div>
                      <div className="mt-5 flex gap-3">
                        <button
                          type="button"
                          className="cad-ribbon-action cad-ribbon-action-primary flex-1"
                          disabled={status !== "connected"}
                          onClick={() => setHelixAction(null)}
                        >
                          {t("common.confirm")}
                        </button>
                        <button
                          type="button"
                          className="cad-ribbon-action flex-1"
                          onClick={() => {
                            void cancelActiveTool();
                          }}
                        >
                          {t("common.cancel")}
                        </button>
                      </div>
                    </>
                  )}
                </section>
              ) : null}
              {sketchFilletAction ? (
                <SketchFilletPanel
                  initialValue={sketchFilletAction.radius}
                  disabled={status !== "connected"}
                  count={
                    sketchFilletAction.phase === "active"
                      ? sketchFilletAction.filletIds.length
                      : 0
                  }
                  onPreviewValue={async (value) => {
                    // Fan the new radius out across every fillet
                    // already in the session. Same model as the 3D
                    // EdgeOpPreviewPanel: one input drives every
                    // selected target. We track the panel's
                    // session radius in state so future adds use
                    // the latest value too.
                    setSketchFilletAction((prev) =>
                      prev ? { ...prev, radius: value } : prev,
                    );
                    if (sketchFilletAction.phase !== "active") {
                      return;
                    }
                    await runAction(async () => {
                      for (const filletId of sketchFilletAction.filletIds) {
                        await updateSketchFilletRadius(filletId, value);
                      }
                    });
                  }}
                  onConfirm={async () => {
                    // Confirm keeps every fillet in the session
                    // and exits the tool back to Select — same
                    // post-confirm UX as 3D fillet, which drops
                    // out of edge-pick mode after Confirm.
                    sketchFilletIdsRef.current = [];
                    setSketchFilletAction(null);
                    await runAction(async () => {
                      await setSketchTool("select");
                    });
                  }}
                  onCancel={async () => {
                    // Cancel = discard the whole session. Each
                    // fillet's `delete_sketch_fillet` restores its
                    // corner; the trim points fall off via the
                    // next `rebuild_sketch_points`. Mirrors the
                    // EdgeOpPreviewPanel cancel contract.
                    if (sketchFilletAction.phase === "active") {
                      await runAction(async () => {
                        for (const filletId of sketchFilletAction.filletIds) {
                          await deleteSketchFillet(filletId);
                        }
                      });
                    }
                    sketchFilletIdsRef.current = [];
                    setSketchFilletAction(null);
                    await runAction(async () => {
                      await setSketchTool("select");
                    });
                  }}
                />
              ) : null}
              {pendingSketchDeleteConfirmation ? (
                <section className="pointer-events-auto cad-floating-panel px-5 py-5">
                  <div className="flex items-start gap-3">
                    <span className="mt-1 flex h-9 w-9 shrink-0 items-center justify-center rounded-md bg-amber-400/15 text-amber-300 ring-1 ring-amber-300/35">
                      <svg
                        viewBox="0 0 16 16"
                        width="20"
                        height="20"
                        aria-hidden="true"
                        fill="none"
                        stroke="currentColor"
                        strokeWidth="1.5"
                        strokeLinecap="round"
                        strokeLinejoin="round"
                      >
                        <path d="M8 2 14 13H2Z" />
                        <path d="M8 6v3" />
                        <path d="M8 11.5h.01" />
                      </svg>
                    </span>
                    <div>
                      <p className="cad-kicker text-amber-300">
                        {t("sketchDelete.warning")}
                      </p>
                      <h2 className="mt-2 font-display text-lg text-on-surface">
                        {t("sketchDelete.title")}
                      </h2>
                      <p className="mt-3 text-sm leading-5 text-on-surface-muted">
                        {t("sketchDelete.body", {
                          count:
                            pendingSketchDeleteConfirmation.affectedFeatureNames
                              .length,
                          plural:
                            pendingSketchDeleteConfirmation.affectedFeatureNames
                              .length === 1
                              ? ""
                              : "s",
                          names:
                            pendingSketchDeleteConfirmation.affectedFeatureNames.join(
                              ", ",
                            ),
                        })}
                      </p>
                    </div>
                  </div>
                  <div className="mt-5 grid grid-cols-2 gap-3">
                    <button
                      type="button"
                      className="rounded-md bg-primary px-4 py-2 text-sm font-medium text-on-primary transition-opacity hover:opacity-90"
                      onClick={() => {
                        const { selection } = pendingSketchDeleteConfirmation;
                        setPendingSketchDeleteConfirmation(null);
                        deleteSketchSelectionNow(selection);
                      }}
                    >
                      {t("common.ok")}
                    </button>
                    <button
                      type="button"
                      className="rounded-md bg-white/8 px-4 py-2 text-sm text-on-surface transition-colors hover:bg-white/12"
                      onClick={() => {
                        setPendingSketchDeleteConfirmation(null);
                      }}
                    >
                      {t("common.cancel")}
                    </button>
                  </div>
                </section>
              ) : null}
            </div>
            {SHOW_DEBUG_MESSAGE_LOG ? (
              <MessageLog messages={messages} />
            ) : null}
          </section>
          {isAiPanelOpen && isAiAssistantAvailable ? (
            <AiAssistantPanel
              config={config.ai}
              status={status}
              document={document}
              viewport={viewport}
              onClose={() => setIsAiPanelOpen(false)}
              onStartCore={async () => {
                await runAction(start);
              }}
            />
          ) : null}
            </>
          )}
        </div>

        {workspaceView === "cad" ? (
          <FeatureTimeline
          document={document}
          onSelectFeature={async (featureId) => {
            await runAction(async () => {
              await selectFeature(featureId);
            });
          }}
          onSetTimelineCursor={(includedActionCount) => {
            void runAction(async () => {
              await setTimelineCursor(includedActionCount);
            });
          }}
          onEditFeature={(featureId) => {
            // Dispatch by feature kind: box/cylinder open the inline
            // parameter form; sketch re-enters the sketch so the user
            // can edit its lines (extrudes that depend on the sketch
            // re-evaluate automatically); extrude opens the same
            // floating preview panel that creation uses, but seeded
            // with the existing depth/mode/target so cancel can
            // restore instead of undoing the whole feature.
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
            if (feature.kind === "extrude" && feature.extrude_parameters) {
              if (
                extrudeAction ||
                loftAction ||
                revolveAction ||
                sweepAction ||
                edgeOpAction ||
                threadAction ||
                fastenerAction ||
                moveAction
              ) {
                return;
              }
              const params = feature.extrude_parameters;
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
            if (feature.kind === "loft" && feature.loft_parameters) {
              if (
                extrudeAction ||
                loftAction ||
                revolveAction ||
                sweepAction ||
                edgeOpAction ||
                threadAction ||
                fastenerAction ||
                moveAction
              ) {
                return;
              }
              const params = feature.loft_parameters;
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
            if (feature.kind === "revolve" && feature.revolve_parameters) {
              if (
                extrudeAction ||
                loftAction ||
                revolveAction ||
                sweepAction ||
                edgeOpAction ||
                threadAction ||
                fastenerAction ||
                moveAction
              ) {
                return;
              }
              const params = feature.revolve_parameters;
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
            if (feature.kind === "sweep" && feature.sweep_parameters) {
              if (
                extrudeAction ||
                loftAction ||
                revolveAction ||
                sweepAction ||
                edgeOpAction ||
                threadAction ||
                fastenerAction ||
                moveAction
              ) {
                return;
              }
              const params = feature.sweep_parameters;
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
            if (feature.kind === "thread" && feature.thread_parameters) {
              if (
                extrudeAction ||
                loftAction ||
                revolveAction ||
                sweepAction ||
                edgeOpAction ||
                threadAction ||
                fastenerAction ||
                moveAction
              ) {
                return;
              }
              beginTimelineEditSession(featureId, feature.kind);
              setThreadAction({
                phase: "active",
                featureId,
                originalParameters: feature.thread_parameters,
              });
            }
            if (feature.kind === "fastener" && feature.fastener_parameters) {
              if (
                extrudeAction ||
                loftAction ||
                revolveAction ||
                sweepAction ||
                edgeOpAction ||
                threadAction ||
                fastenerAction ||
                moveAction
              ) {
                return;
              }
              beginTimelineEditSession(featureId, feature.kind);
              setFastenerAction({
                featureId,
                originalParameters: feature.fastener_parameters,
              });
            }
            if (feature.kind === "move" && feature.move_parameters) {
              if (
                extrudeAction ||
                loftAction ||
                revolveAction ||
                sweepAction ||
                edgeOpAction ||
                threadAction ||
                fastenerAction ||
                moveAction
              ) {
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
          }}
          onSuppressFeature={(featureId, suppressed) => {
            void runAction(async () => {
              await setFeatureSuppressed(featureId, suppressed);
            });
          }}
          onDeleteFeature={(featureId) => {
            confirmAndDeleteFeature(featureId);
          }}
          />
        ) : null}
      </div>
      {pendingUnsavedAction ? (
        <div className="cad-modal-backdrop" role="presentation">
          <section
            className="cad-unsaved-dialog"
            role="dialog"
            aria-modal="true"
            aria-labelledby="cad-unsaved-dialog-title"
          >
            <div>
              <h2 id="cad-unsaved-dialog-title" className="text-base font-semibold text-on-surface">
                {t("unsavedDialog.title", { name: currentDocumentName })}
              </h2>
              <p className="mt-2 text-sm text-on-surface-muted">
                {t("unsavedDialog.body")}
              </p>
            </div>
            <div className="mt-5 flex flex-wrap justify-end gap-2">
              <button
                type="button"
                className="cad-ribbon-action"
                onClick={() => setPendingUnsavedAction(null)}
              >
                {t("common.cancel")}
              </button>
              <button
                type="button"
                className="cad-ribbon-action"
                onClick={discardThenContinuePendingAction}
              >
                {pendingUnsavedAction.kind === "quit"
                  ? t("unsavedDialog.quitWithoutSaving")
                  : t("unsavedDialog.continueWithoutSaving")}
              </button>
              <button
                type="button"
                className="cad-ribbon-action cad-ribbon-action-primary"
                onClick={() => void saveThenContinuePendingAction()}
              >
                {t("unsavedDialog.saveFirst")}
              </button>
            </div>
          </section>
        </div>
      ) : null}
    </main>
  );
}

export default App;
