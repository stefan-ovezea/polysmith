import {
  useEffect,
  useMemo,
  useRef,
  useState,
  type KeyboardEvent as ReactKeyboardEvent,
} from "react";
import { useTranslation } from "react-i18next";
import * as THREE from "three";
import { OrbitControls } from "three/examples/jsm/controls/OrbitControls.js";
import {
  applyTheme,
  formatHotkey,
  matchesHotkey,
  useAppConfig,
} from "@/config";
import { ToolbarTooltip } from "@/lib";
import type { CrosshairMode } from "@/config";
import { createViewportScene } from "@/lib";
import type {
  ArmedSketchConstraint,
  ConstraintType,
  DocumentState,
  SketchTool,
  ViewportState,
  SketchDimensionScene,
  SolidFacePlaneFrame,
  PrimitiveVisual,
  PrimitiveInteractionState,
  ReferencePlaneVisual,
  ReferencePlaneInteractionState,
  SolidFaceVisual,
  SolidFaceInteractionState,
  SketchProfileVisual,
  SketchProfileInteractionState,
  ViewportContextMenuState,
  SketchPreviewPoint,
  MoveFeatureParameters,
  SelectionFilter,
} from "@/types";
import {
  applyPrimitiveVisualState,
  applyReferencePlaneVisualState,
  applySketchProfileVisualState,
  applySolidFaceVisualState,
  buildSketchDimensionObject,
  applyEdgeVisualColor,
  applyVertexVisualColor,
  axisAlignedRectangleCorners2d,
  disposeGroup,
  disposeMaterial,
  distanceBetweenPoints,
  frameCameraToSketchPlane,
  projectWorldPointToViewport,
  resolveSketchPlanePoint,
  SKETCH_PLANE_OFFSET,
  SKETCH_SNAP_DISTANCE,
  themeColor,
  toWorldPoint,
  buildViewCubeGroup,
  createViewCubeScene,
  createViewCubeCamera,
  isPointerInCubeArea,
  clearCubeHover,
  disposeViewCubeGroup,
  createCubeRenderTarget,
  createCubeBlitScene,
  disposeCubeBlitScene,
} from "@/utils";
import type { ViewCubeHit, CubeBlitScene } from "@/utils";
import { makeGetViewportStateCommand } from "@/lib/ipcProtocol";
import { useCadCoreStore } from "@/state/cadCoreStore";
import { parseDimensionInput, mmToDisplay } from "@/utils/units";
import {
  disposeDynamicGrid,
  getOrthographicViewHeight,
  getSketchGridFrame,
  worldPointToSketchLocal,
  type ActiveSketchGridPlaneFrame,
  type DynamicGridRef,
} from "./viewport/grid";
import {
  beginMoveGizmoPointerDown,
  finishMoveGizmoPointerUp,
  moveGizmoParametersFromDrag,
  type MoveGizmoDescriptor,
  type MoveGizmoDragState,
} from "./viewport/moveGizmo";
import {
  applyPendingLineCommitRelations,
  draftStartRelations,
  lineCommitRelations,
} from "./viewport/lineCommitRelations";
import {
  collectRectangleSelectionIds,
  finishRectangleSelectionDrag,
  selectionRectOverlayFromDrag,
  type SelectionRectOverlay,
  type SelectionDrag,
} from "./viewport/selectionGeometry";
import { type EndpointDrag } from "./viewport/endpointDrag";
import { handleEndpointDragPointerMove } from "./viewport/endpointDragPointerMove";
import { finishEndpointDragPointerUp } from "./viewport/endpointDragPointerUp";
import { beginSelectPointerDown } from "./viewport/selectPointerDown";
import {
  handleViewCubeDragPointerMove,
  handleViewCubeHoverPointerMove,
} from "./viewport/viewCubePointerMove";
import { finishViewCubePointerUp } from "./viewport/viewCubePointerUp";
import {
  buildSketchSnapCandidates,
  closestStaticSnapCandidate,
  dynamicSnapCandidate,
  previewPointFromStaticCandidate,
  type SketchSnapCandidate,
} from "./viewport/snapResolution";
import {
  DRAFT_DIMENSION_OFFSET_PX,
  GridMiniIcon,
  applyDraftDimensionFieldValue,
  clampAngleRadius,
  draftSessionFields,
  draftSessionValues,
  formatDraftDimension,
  fuzzyParameterScore,
  isDraftDimensionTool,
  isDrawableSketchTool,
  parameterTokenAtCursor,
  updateDraftSessionCurrent,
  type DimensionLabelDragState,
  type DimensionRelationPreview,
  type DraftDimensionField,
  type DraftDimensionSession,
  type DraftDimensionTool,
  type ParameterSuggestion,
} from "./viewport/draftDimensions";
import { buildDraftDimensionPreview } from "./viewport/draftDimensionPreview";
import {
  beginDimensionLabelDragPointerDown,
  buildAngleDimensionFrame,
  buildDimensionPlacementStart,
  circleRadiusDimensionProjection,
  handleDimensionLabelDragPointerMove,
  type AngleDimensionFrame,
} from "./viewport/dimensionLabelDrag";
import {
  unaryDimensionIdForEntity,
} from "./viewport/dimensionToolPicking";
import { SketchToolPanel } from "./viewport/SketchToolPanel";
import { ViewportContextMenu } from "./viewport/ViewportContextMenu";
import { handleTrimPointerMove } from "./viewport/trimPointerMove";
import {
  buildDimensionRelationPreview,
  sketchLinesShareEndpoint as relationPreviewLinesShareEndpoint,
} from "./viewport/dimensionRelationPreview";
import {
  buildViewportContextMenuState,
  type SelectedConstraintState,
} from "./viewport/contextMenuState";
import {
  type ConstraintPreviewState,
} from "./viewport/constraintPreview";
import {
  beginDraftPointerDown,
  finishDraftStartedPointerUp,
  updateDraftChainBreakRequest,
} from "./viewport/draftPointerDown";
import { resolveDraftPointerMove } from "./viewport/draftPointerMove";
import { type ArcToolMode } from "./viewport/arcDraftPreview";
import { type RectangleToolMode } from "./viewport/rectangleDraftPreview";
import {
  commitDraftPointerUp,
} from "./viewport/draftCommit";
import { renderDraftPointerPreview } from "./viewport/draftPointerPreview";
import {
  type ActiveSketchSelectHit,
} from "./viewport/sketchClickSelection";
import {
  intersectViewportSceneTargets,
} from "./viewport/sceneTargetPicking";
import { handleActiveSketchPointerUpTool } from "./viewport/pointerUpActiveSketch";
import { handlePointerUpSceneSelection } from "./viewport/pointerUpSceneSelection";
import {
  applyProjectToolHover,
  applySceneHover,
  applySelectToolHover,
  clearSketchEntityHover,
} from "./viewport/pointerMoveHover";
import { disposeGeometryTreeResources } from "./viewport/threeDisposal";
import {
  invertSelectionFilter,
  readStoredFilter,
} from "./selectionFilterState";
import { updateScreenSpaceSketchSprites } from "./viewport/screenSpaceSketchSprites";
import { updateDynamicGrids } from "./viewport/dynamicGridUpdate";
import { bindSketchHotkeys } from "./viewport/sketchHotkeys";
import { syncViewportScene } from "./viewport/sceneSync";
import {
  renderViewCubeFrame,
  rotateCameraAroundCurrentView,
} from "./viewport/viewCubeRender";
import {
  configureViewportControls,
  handleViewportWheelZoom,
  resizeViewportRenderer,
  setupViewportSnapshotCapture,
} from "./viewport/viewportRenderer";

const ORTHO_FRUSTUM_HEIGHT = 220;
const ORTHO_MIN_ZOOM = 0.02;
const ORTHO_MAX_ZOOM = 500;
const WHEEL_ZOOM_SPEED = 0.0012;
const WHEEL_ZOOM_POINTER_PAN = 0.42;
const CROSSHAIR_SIZE_FACTORS: Partial<Record<CrosshairMode, number>> = {
  "viewport-25": 0.25,
  "viewport-50": 0.5,
  "viewport-75": 0.75,
};
const GRID_SNAP_SCREEN_DISTANCE_PX = 6;

interface ViewportPanelProps {
  status: "idle" | "starting" | "connected" | "error" | "stopped";
  document: DocumentState | null;
  viewport: ViewportState | null;
  showStock?: boolean;
  wcsOrientation?: string;
  onSnapshotCaptureReady?: (capture: (() => string | null) | null) => void;
  onSelectPrimitive: (primitiveId: string) => Promise<void>;
  onSelectReference: (referenceId: string) => Promise<void>;
  onSelectFace: (faceId: string) => Promise<void>;
  // `additive` is true when the user shift-clicked the edge: the
  // core toggles the edge into the existing selection rather than
  // replacing it. Other selection categories don't support multi
  // yet, so they keep their single-id callbacks.
  onSelectEdge: (edgeId: string, additive: boolean) => Promise<void>;
  // Same multi-select shape as `onSelectEdge`: shift-click toggles
  // the vertex into the existing vertex set (used by the bottom-right
  // Selection panel to show vertex-vertex distance), plain click
  // replaces.
  onSelectVertex: (vertexId: string, additive: boolean) => Promise<void>;
  onStartSketch: (referenceId: string) => Promise<void>;
  onStartSketchOnFace: (
    faceId: string,
    planeFrame: SolidFacePlaneFrame,
  ) => Promise<void>;
  onAddSketchLine: (
    startX: number,
    startY: number,
    endX: number,
    endY: number,
    isConstruction: boolean,
  ) => Promise<void>;
  onSetSketchMidpointAnchor: (
    pointId: string,
    hostLineId: string,
  ) => Promise<void>;
  onSetSketchPointLineAnchor: (
    pointId: string,
    hostLineId: string,
    t: number,
  ) => Promise<void>;
  onAddSketchAngleDimension: (
    firstLineId: string,
    secondLineId: string,
  ) => Promise<void>;
  onAddSketchDistanceDimension: (
    firstEntityId: string,
    secondEntityId: string,
  ) => Promise<void>;
  onAddSketchLineLengthDimension: (lineId: string) => Promise<void>;
  onAddSketchCircleRadiusDimension: (circleId: string, displayAs?: string) => Promise<void>;
  onAddSketchPolygonRadiusDimension: (polygonId: string) => Promise<void>;
  onSetSketchLineConstraint: (
    lineId: string,
    constraint: "none" | "horizontal" | "vertical",
  ) => Promise<void>;
  onSetSketchPerpendicularConstraint: (
    lineId: string,
    otherLineId: string | null,
  ) => Promise<void>;
  onSetSketchTangentConstraint: (
    lineId: string,
    circleId: string,
  ) => Promise<void>;
  onSetSketchParallelConstraint: (
    lineId: string,
    otherLineId: string | null,
  ) => Promise<void>;
  onAddSketchRectangle: (
    startX: number,
    startY: number,
    endX: number,
    endY: number,
    isConstruction: boolean,
  ) => Promise<void>;
  onAddSketchCircle: (
    centerX: number,
    centerY: number,
    radius: number,
    isConstruction: boolean,
  ) => Promise<void>;
  // Add an arc using one of two creation modes:
  //   - "three_point": (start, end, anchor) where anchor lies on the
  //     arc and fixes the bulge.
  //   - "center_start_end": anchor is the center; the end is snapped
  //     onto the resulting circle in the core.
  // Both modes accept the same three (x, y) pairs in sketch-local
  // 2D coordinates; the ViewportPanel resolves them from world clicks.
  onAddSketchArc: (
    startX: number,
    startY: number,
    endX: number,
    endY: number,
    anchorX: number,
    anchorY: number,
    mode: ArcToolMode,
    isConstruction: boolean,
  ) => Promise<void>;
  // Tool-level mode for the arc tool. Lifted out of ViewportPanel so
  // the SketchToolbar can render the segmented control. Defaults to
  // "three_point"; the toolbar updates it through `onSetArcToolMode`.
  arcToolMode: ArcToolMode;
  onSetArcToolMode: (mode: ArcToolMode) => void;
  // Rectangle creation mode — corner-to-corner, center-point, or 3-point.
  // Lifted from App.tsx so the viewport commit handler can compute
  // the rectangle corners differently per mode.
  rectangleToolMode: RectangleToolMode;
  // Circle creation mode — center+radius, 2-point, 3-point, or tangent.
  // Lifted from App.tsx so the viewport handler can compute the
  // circle geometry differently per mode.
  circleToolMode: "center_radius" | "two_point" | "three_point" | "tangent_two_lines" | "tangent_three_lines";
  onSetCircleToolMode: (mode: "center_radius" | "two_point" | "three_point" | "tangent_two_lines" | "tangent_three_lines") => void;
  onSetRectangleToolMode: (mode: "corner_corner" | "center_point" | "three_point") => void;
  polygonToolMode: "circumscribed" | "inscribed" | "edge";
  onSetPolygonToolMode: (mode: "circumscribed" | "inscribed" | "edge") => void;
  onAddSketchPolygon: (
    sides: number,
    mode: string,
    startX: number,
    startY: number,
    endX: number,
    endY: number,
    isConstruction: boolean,
  ) => Promise<void>;
  // Sketch fillet — fired when the user clicks an eligible corner
  // point under the Fillet tool. Eligible = sketch point shared by
  // exactly two non-construction sketch lines that are not already
  // filleted at this corner. The viewport only signals "user
  // clicked an eligible corner with these args"; App owns the
  // session radius (driven by the floating panel) and decides
  // what to do with the click. Mirrors the 3D edge-op flow where
  // the viewport reports edge picks and the panel owns the
  // numeric value being applied to all of them.
  onAddSketchFillet: (
    cornerPointId: string,
    lineAId: string,
    lineBId: string,
  ) => Promise<void>;
  onSelectSketchEntity: (entityId: string, additive: boolean) => Promise<void>;
  onBatchSelectEntities: (entityIds: string[], additive: boolean) => Promise<void>;
  onPickSketchPoint: (
    pointId: string,
    kind: "endpoint" | "center" | "quadrant",
    additive: boolean,
  ) => Promise<void>;
  armedSketchConstraint: ArmedSketchConstraint;
  // Which mirror tool slot is taking entity clicks. `null` when the
  // mirror tool isn't open. The mirror tool runs alongside the
  // armed-constraint flow but takes priority when active: a click
  // on a sketch entity is routed through `onMirrorEntityPick`
  // instead of the normal selection / armed-constraint paths.
  mirrorFocusedSlot: "objects" | "axis" | null;
  inactiveSketchEntityPickEnabled?: boolean;
  onPickInactiveSketchLine?: (lineId: string) => void | Promise<void>;
  onMirrorEntityPick: (
    entityId: string,
    entityKind: "line" | "circle",
  ) => Promise<void>;
  onCancelSketchConstraint: () => void;
  onClearSketchConstraint: (
    kind: ConstraintType,
    entityId: string,
    relatedEntityId: string | null,
  ) => Promise<void>;
  onSelectSketchDimension: (dimensionId: string) => Promise<void>;
  onUpdateSketchDimension: (
    dimensionId: string,
    value: number | string,
  ) => Promise<void>;
  onUpdateSketchDimensionLabelPosition: (
    dimensionId: string,
    labelX: number,
    labelY: number,
  ) => Promise<void>;
  onSelectSketchProfile: (profileId: string, additive: boolean) => Promise<void>;
  onTrimSketchEntity?: (
    entityId: string,
    clickX: number,
    clickY: number,
  ) => Promise<void>;
  onDeleteSketchSelection: (
    selection?: {
      entityIds: string[];
      pointIds: string[];
      profileIds: string[];
    },
  ) => Promise<void>;
  onDeleteSketchDimension: (dimensionId: string) => Promise<void>;
  onAddSketchPointDistanceDimension: (
    pointAId: string,
    pointBId: string,
  ) => Promise<void>;
  onUpdateSketchDimensionDisplay: (
    dimensionId: string,
    displayAs: string,
  ) => Promise<void>;
  onSetSketchTool: (tool: SketchTool) => Promise<void>;
  onUpdateSketchPoint: (
    pointId: string,
    x: number,
    y: number,
  ) => Promise<void>;
  onFinishSketch: () => Promise<void>;
  moveGizmo?: MoveGizmoDescriptor | null;
  onMoveGizmoChange?: (parameters: MoveFeatureParameters) => Promise<void> | void;
  onMoveBody?: (bodyId: string) => Promise<void> | void;
  onCopyBody?: (
    bodyId: string,
    copyMode: "linked" | "standalone",
  ) => Promise<void> | void;
  onExportBodyMesh?: (bodyId: string) => Promise<void> | void;
  onUnlinkBodyCopy?: (featureId: string) => Promise<void> | void;
  hiddenFeatureIds?: ReadonlySet<string>;
  hiddenSketchPlaneIds?: ReadonlySet<string>;
  hideReferences?: boolean;
}

export function ViewportPanel({
  status,
  document,
  viewport,
  showStock = true,
  wcsOrientation = "model",
  onSnapshotCaptureReady,
  onSelectPrimitive,
  onSelectReference,
  onSelectFace,
  onSelectEdge,
  onSelectVertex,
  onStartSketch,
  onStartSketchOnFace,
  onAddSketchLine,
  onSetSketchMidpointAnchor,
  onSetSketchPointLineAnchor,
  onAddSketchAngleDimension,
  onAddSketchDistanceDimension,
  onAddSketchLineLengthDimension,
  onAddSketchCircleRadiusDimension,
  onAddSketchPolygonRadiusDimension,
  onSetSketchLineConstraint,
  onSetSketchPerpendicularConstraint,
  onSetSketchTangentConstraint,
  onSetSketchParallelConstraint,
  onAddSketchRectangle,
  onAddSketchCircle,
  onAddSketchArc,
  arcToolMode,
  onSetArcToolMode,
  rectangleToolMode,
  onSetRectangleToolMode,
  circleToolMode,
  onSetCircleToolMode,
  polygonToolMode,
  onSetPolygonToolMode,
  onAddSketchPolygon,
  onAddSketchFillet,
  onSelectSketchEntity,
  onBatchSelectEntities,
  onPickSketchPoint,
  armedSketchConstraint,
  mirrorFocusedSlot,
  inactiveSketchEntityPickEnabled = false,
  onPickInactiveSketchLine,
  onMirrorEntityPick,
  onCancelSketchConstraint,
  onClearSketchConstraint,
  onSelectSketchDimension,
  onUpdateSketchDimension,
  onUpdateSketchDimensionLabelPosition,
  onSelectSketchProfile,
  onTrimSketchEntity,
  onDeleteSketchSelection,
  onDeleteSketchDimension,
  onAddSketchPointDistanceDimension,
  onUpdateSketchDimensionDisplay,
  onSetSketchTool,
  onUpdateSketchPoint,
  onFinishSketch,
  moveGizmo = null,
  onMoveGizmoChange,
  onMoveBody,
  onCopyBody,
  onExportBodyMesh,
  onUnlinkBodyCopy,
  hiddenFeatureIds,
  hiddenSketchPlaneIds,
  hideReferences,
}: ViewportPanelProps) {
  const { config, activeTheme, updateConfig } = useAppConfig();
  const addMessage = useCadCoreStore((state) => state.addMessage);
  const { t: translate } = useTranslation();
  const [showReferencePlanes, setShowReferencePlanes] = useState(true);
  const showViewportGrid = config.viewport.showGrid;
  const showSketchGrid = config.viewport.showSketchGrid;
  const [contextMenu, setContextMenu] =
    useState<ViewportContextMenuState | null>(null);
  const [sketchSnapLabel, setSketchSnapLabel] = useState<string | null>(null);
  // Floating constraint-preview badge tracked relative to the
  // viewport container. Shown next to the cursor whenever the snap
  // resolver is producing a midpoint or perpendicular snap so the
  // user sees *which* constraint the next click would auto-create
  // (CAD convention). `kind` controls the glyph; `x`/`y` are
  // container-local pixel offsets so the overlay scrolls with the
  // viewport.
  // First line picked while the dimension tool is armed. After a
  // line click we wait for a *second* line click to know whether the
  // user wants a length dim (same line clicked again) or an angle
  // dim (different line). Cleared when the dim tool exits or when a
  // dimension is created. Stored as a ref so the click handler reads
  // the latest value without re-attaching listeners.
  const dimensionToolFirstLineRef = useRef<string | null>(null);
  const [dimensionToolFirstLine, setDimensionToolFirstLine] = useState<
    string | null
  >(null);
  // First point picked in point_distance dimension mode.
  const dimensionToolFirstPointRef = useRef<{
    id: string;
    x: number;
    y: number;
  } | null>(null);
  const [constraintPreview, setConstraintPreview] =
    useState<ConstraintPreviewState | null>(null);
  const [crosshairPointer, setCrosshairPointer] = useState<{
    x: number;
    y: number;
  } | null>(null);
  const [viewportSize, setViewportSize] = useState({ width: 1, height: 1 });
  // Whether the next drawable sketch entity will be flagged as
  // construction geometry. The core owns the resulting CAD state;
  // this UI state is only the pending tool option sent with the
  // add_* IPC command.
  const [sketchToolConstruction, setSketchToolConstruction] = useState(false);
  const sketchToolConstructionRef = useRef(false);
  const [polygonSides, setPolygonSides] = useState(6);
  const polygonSidesRef = useRef(6);
  // Held while the user holds the wireframe-toggle key (Tab) during a
  // pending fillet/chamfer panel session. Reveals every ghost edge
  // so the user can see and click the original sharp edges that
  // were hidden by default to keep the rounded preview readable.
  // Kept as a ref because the keydown/keyup handlers repaint edge
  // materials directly (no React state read). Painting goes through
  // `paintEdgeMaterials` which reads this ref.
  const revealGhostEdgesRef = useRef(false);
  const [dimensionDraftValue, setDimensionDraftValue] = useState("");
  const [dimensionSuggestionIndex, setDimensionSuggestionIndex] = useState(0);
  const [draftSuggestionState, setDraftSuggestionState] = useState<{
    field: DraftDimensionField;
    index: number;
  } | null>(null);
  const [isDimensionEditorOpen, setIsDimensionEditorOpen] = useState(false);
  const [dimensionLabelPositions, setDimensionLabelPositions] = useState<
    Record<string, [number, number, number]>
  >({});
  const dimensionLabelPositionsRef = useRef<
    Record<string, [number, number, number]>
  >({});
  // Angle dimensions store just the arc radius during drag, not a
  // directional control point.  This eliminates the disconnect between
  // the cursor direction and the bisector that caused arc drift.
  const [angleDragRadii, setAngleDragRadii] = useState<Record<string, number>>({});
  const angleDragRadiiRef = useRef<Record<string, number>>({});
  const [draftDimensionSession, setDraftDimensionSession] =
    useState<DraftDimensionSession | null>(null);
  const pendingCircleDimensionPlacementRef = useRef<{
    fromCircleCount: number;
    center: [number, number];
    end: [number, number];
  } | null>(null);
  const hostRef = useRef<HTMLDivElement | null>(null);
  const canvasRef = useRef<HTMLCanvasElement | null>(null);
  const dimensionEditorRef = useRef<HTMLFormElement | null>(null);
  const dimensionInputRef = useRef<HTMLInputElement | null>(null);
  const dimensionInputSelectionLockedRef = useRef(false);
  const dimensionExpressionTimeoutRef = useRef<ReturnType<typeof setTimeout> | null>(null);
  const rendererRef = useRef<THREE.WebGLRenderer | null>(null);
  const sceneRef = useRef<THREE.Scene | null>(null);
  const cameraRef = useRef<THREE.OrthographicCamera | null>(null);
  const controlsRef = useRef<OrbitControls | null>(null);
  const contentGroupRef = useRef<THREE.Group | null>(null);
  const referenceGroupRef = useRef<THREE.Group | null>(null);
  const sketchGroupRef = useRef<THREE.Group | null>(null);
  const previewLineRef = useRef<THREE.Line | null>(null);
  const previewCircleRef = useRef<THREE.LineLoop | null>(null);
  const previewDimensionRef = useRef<{
    line: THREE.Object3D;
    label: THREE.Sprite;
  } | null>(null);
  // Mirrors `previewLineRef` / `previewCircleRef` for the arc tool.
  // Carries the dashed in-progress arc preview rendered while the
  // user is between clicks 2 and 3 (or, in center+start+end mode, a
  // dashed circle while between clicks 1 and 2).
  const previewArcRef = useRef<THREE.Line | null>(null);
  const trimSegmentHighlightRef = useRef<THREE.Line | null>(null);
  const trimArcHighlightRef = useRef<THREE.Line | null>(null);
  /** Latest trim_preview_result payload from the core (null when idle). */
  const trimPreviewResultRef = useRef<any>(null);
  /** Throttle: skip IPC send if the cursor hasn't moved enough. */
  const trimPreviewLastSentRef = useRef<{ x: number; y: number } | null>(null);
  const draftDimGroupRef = useRef<THREE.Group | null>(null);
  /** Reusable scene object for draft dimension lines (create once, update positions in-place). */
  const draftDimSceneObjRef = useRef<{
    lines: THREE.LineSegments;
  } | null>(null);
  const draftArcTestRef = useRef<THREE.LineSegments | null>(null);
  // Projected screen positions for draft dimension labels, updated
  // every frame by the render loop so React can position inputs.
  const draftDimScreenPositionsRef = useRef<
    Partial<Record<DraftDimensionField, { x: number; y: number }>>
  >({});
  const viewCubeGroupRef = useRef<THREE.Group | null>(null);
  const viewCubeSceneRef = useRef<THREE.Scene | null>(null);
  const viewCubeCameraRef = useRef<THREE.OrthographicCamera | null>(null);
  const viewCubeRaycasterRef = useRef<THREE.Raycaster | null>(null);
  const cubeRenderTargetRef = useRef<THREE.WebGLRenderTarget | null>(null);
  const cubeBlitSceneRef = useRef<CubeBlitScene | null>(null);
  const viewCubeHoveredRef = useRef<ViewCubeHit>(null);
  const viewCubeAnimatingRef = useRef(false);
  const viewCubeAnimStartRef = useRef(0);
  const viewCubeAnimStartPosRef = useRef(new THREE.Vector3());
  const viewCubeAnimTargetPosRef = useRef(new THREE.Vector3());
  const viewCubeAnimStartUpRef = useRef(new THREE.Vector3(0, 1, 0));
  const viewCubeAnimTargetUpRef = useRef(new THREE.Vector3(0, 1, 0));
  const viewCubeDraggingRef = useRef(false);
  const viewCubeDragStartRef = useRef<{ x: number; y: number } | null>(null);
  const lineDraftStartRef = useRef<[number, number] | null>(null);
  // Track click timing and position for double-click detection during
  // line drafting. Two clicks <300ms apart at the same location break
  // the chain and start an independent line on the next click.
  const lastPointerDownTimeRef = useRef(0);
  const lastPointerDownPosRef = useRef<{ x: number; y: number } | null>(null);
  const chainBreakRequestedRef = useRef(false);
  // 2D sketch-plane angle (radians) of the last committed line segment,
  // used as the reference for the next chained line's angle arc.
  // null for the first / independent line (defaults to horizontal, 0 rad).
  const previousLineAngleRef = useRef<number | null>(null);
  const currentGridSpacingRef = useRef(10);
  const draftDimensionSessionRef = useRef<DraftDimensionSession | null>(null);
  const draftDimensionInputRefs = useRef<
    Partial<Record<DraftDimensionField, HTMLInputElement | null>>
  >({});
  /** Set while the user is actively typing into a draft field. Prevents
   *  the display-unit reconversion from overwriting partial input like
   *  "2." (which would round-trip through mm and lose the decimal). */
  const draftFieldFocusedRef = useRef<DraftDimensionField | null>(null);
  /** Raw user-typed input preserved during editing so the round-trip
   *  through mm doesn't drop the decimal from partial values like "2.". */
  const draftRawInputRef = useRef<Partial<Record<DraftDimensionField, string>>>({});
  const draftParameterExpressionRef = useRef<
    Partial<Record<DraftDimensionField, string>>
  >({});
  const draftStartedOnPointerDownRef = useRef(false);
  const previousReferencePlaneVisibilityRef = useRef<boolean | null>(null);
  const primitiveVisualsRef = useRef(new Map<string, PrimitiveVisual>());
  const primitiveStatesRef = useRef(
    new Map<string, PrimitiveInteractionState>(),
  );
  const referencePlaneVisualsRef = useRef(
    new Map<string, ReferencePlaneVisual>(),
  );
  const referencePlaneStatesRef = useRef(
    new Map<string, ReferencePlaneInteractionState>(),
  );
  const solidFaceVisualsRef = useRef(new Map<string, SolidFaceVisual>());
  const solidFaceStatesRef = useRef(
    new Map<string, SolidFaceInteractionState>(),
  );
  const worldGridRef = useRef<DynamicGridRef | null>(null);
  const sketchGridRef = useRef<DynamicGridRef | null>(null);
  const meshesRef = useRef<THREE.Mesh[]>([]);
  const referencePlaneMeshesRef = useRef<THREE.Mesh[]>([]);
  const sketchEntityObjectsRef = useRef<Array<THREE.Line | THREE.LineLoop>>([]);
  const sketchEntityObjectByIdRef = useRef(
    new Map<string, THREE.Line | THREE.LineLoop>(),
  );
  const sketchDimensionObjectsRef = useRef<Array<THREE.Object3D>>([]);
  const sketchConstraintObjectsRef = useRef<Array<THREE.Object3D>>([]);
  const sketchPointObjectsRef = useRef<THREE.Mesh[]>([]);
  const sketchPointObjectByIdRef = useRef(new Map<string, THREE.Mesh>());
  const sketchProfileObjectsRef = useRef<THREE.Group[]>([]);
  const sketchProfileVisualsRef = useRef(new Map<string, SketchProfileVisual>());
  const sketchProfileStatesRef = useRef(
    new Map<string, SketchProfileInteractionState>(),
  );
  const faceMeshesRef = useRef<THREE.Mesh[]>([]);
  // Body edges materialized as THREE.Line objects. Raycasting against
  // these (with a small `params.Line.threshold`) drives edge picking
  // for the upcoming fillet/chamfer features. Edges are checked before
  // faces in the pick chain because they sit ON the faces and would
  // otherwise be visually occluded.
  const edgeLineObjectsRef = useRef<THREE.Line[]>([]);
  // Body vertices materialized as small sphere meshes. Raycast first so
  // a vertex picks ahead of any edge or face that lies underneath.
  const vertexObjectsRef = useRef<THREE.Mesh[]>([]);
  // Translucent red overlay meshes for in-progress cut extrudes. Built
  // from `cut_previews` and rendered without participating in raycasts.
  const cutPreviewObjectsRef = useRef<THREE.Mesh[]>([]);
  const toolpathLinesRef = useRef<THREE.Line[]>([]);
  const moveGizmoObjectsRef = useRef<THREE.Object3D[]>([]);
  const moveGizmoDragRef = useRef<MoveGizmoDragState | null>(null);
  const moveGizmoRef = useRef<MoveGizmoDescriptor | null>(moveGizmo);
  const moveGizmoChangeRef = useRef(onMoveGizmoChange);
  const moveBodyRef = useRef(onMoveBody);
  const copyBodyRef = useRef(onCopyBody);
  const exportBodyMeshRef = useRef(onExportBodyMesh);
  const unlinkBodyCopyRef = useRef(onUnlinkBodyCopy);
  const pendingMoveGizmoParametersRef = useRef<MoveFeatureParameters | null>(
    null,
  );
  const pendingMoveGizmoFrameRef = useRef<number | null>(null);
  const lastGeometryKeyRef = useRef("");
  const selectPrimitiveRef = useRef(onSelectPrimitive);
  const selectReferenceRef = useRef(onSelectReference);
  const selectFaceRef = useRef(onSelectFace);
  const selectEdgeRef = useRef(onSelectEdge);
  const selectVertexRef = useRef(onSelectVertex);
  const startSketchRef = useRef(onStartSketch);
  const startSketchOnFaceRef = useRef(onStartSketchOnFace);
  const addSketchLineRef = useRef(onAddSketchLine);
  const addSketchRectangleRef = useRef(onAddSketchRectangle);
  const addSketchCircleRef = useRef(onAddSketchCircle);
  
  const addSketchArcRef = useRef(onAddSketchArc);
  const selectionDragRef = useRef<SelectionDrag | null>(null);

  // Endpoint drag state — active when the user grabs a sketch
  // line endpoint in Select mode and drags it to a new position.
  const endpointDragRef = useRef<EndpointDrag | null>(null);
  // rAF batching for endpoint drag — same pattern as flushMoveGizmoChange.
  const pendingDragRef = useRef<{
    pointId: string;
    x: number;
    y: number;
  } | null>(null);
  const pendingDragFrameRef = useRef<number | null>(null);
  // Latest snap result from the core — used on pointerup to avoid
  // overriding the core's constrained position with raw mouse coords.
  const dragSnapResultRef = useRef<{
    snapX: number;
    snapY: number;
  } | null>(null);
  // Cursor canvas position during endpoint drag — used to position the
  // floating constraint-preview badge near the pointer.
  const dragCursorRef = useRef<{ x: number; y: number } | null>(null);
  // Local preview lines rendered during endpoint drag (dashed overlay).
  const dragPreviewLinesRef = useRef<THREE.Line[]>([]);
  // Set on mouse-up commit; cleared when the next viewport rebuild
  // arrives.  Keeps the drag preview alive across the async IPC gap
  // so the user doesn't see the entity snap back to its old position.
  const pendingEndpointCommitRef = useRef(false);

  const [selectionRect, setSelectionRect] =
    useState<SelectionRectOverlay | null>(null);

  const arcToolModeRef = useRef(arcToolMode);
  const rectangleToolModeRef = useRef(rectangleToolMode);
  const circleToolModeRef = useRef(circleToolMode);
  const polygonToolModeRef = useRef(polygonToolMode);
  const addSketchPolygonRef = useRef(onAddSketchPolygon);
  const addSketchFilletRef = useRef(onAddSketchFillet);
  // Arc placement requires three clicks. The first click goes through
  // `lineDraftStartRef` (shared with line/rect/circle to keep the
  // start-snap pipeline uniform); the second click lands here and
  // captures the end point so the third click can resolve to the
  // anchor (interior point or center, depending on `arcToolMode`).
  // Cleared after every committed arc and whenever the user switches
  // away from the arc tool.
  const arcSecondPointRef = useRef<[number, number] | null>(null);
  const rectSecondPointRef = useRef<[number, number] | null>(null);
  const circleSecondPointRef = useRef<[number, number] | null>(null);
  const selectSketchEntityRef = useRef(onSelectSketchEntity);
  const pickInactiveSketchLineRef = useRef(onPickInactiveSketchLine);
  const inactiveSketchEntityPickEnabledRef = useRef(
    inactiveSketchEntityPickEnabled,
  );
  const pickSketchPointRef = useRef(onPickSketchPoint);
  const updateSketchPointRef = useRef(onUpdateSketchPoint);
  const selectSketchDimensionRef = useRef(onSelectSketchDimension);
  const updateSketchDimensionRef = useRef(onUpdateSketchDimension);
  const updateSketchDimensionLabelPositionRef = useRef(
    onUpdateSketchDimensionLabelPosition,
  );
  const selectSketchProfileRef = useRef(onSelectSketchProfile);
  const trimSketchEntityRef = useRef(onTrimSketchEntity);
  const deleteSketchSelectionRef = useRef(onDeleteSketchSelection);
  const deleteSketchDimensionRef = useRef(onDeleteSketchDimension);
  const addSketchPointDistanceDimensionRef = useRef(
    onAddSketchPointDistanceDimension,
  );
  const updateSketchDimensionDisplayRef = useRef(
    onUpdateSketchDimensionDisplay,
  );
  const selectedSketchDimensionRef = useRef<SketchDimensionScene | null>(null);
  const displayedSketchDimensionsRef = useRef<SketchDimensionScene[]>([]);
  const dimensionLabelDragRef = useRef<DimensionLabelDragState | null>(null);
  const dimensionRelationPreviewRef =
    useRef<DimensionRelationPreview | null>(null);
  const dimensionRelationPreviewLabelRef =
    useRef<[number, number, number] | null>(null);
  const pendingRelationPlacementLabelRef =
    useRef<[number, number, number] | null>(null);
  const pendingAngleIsReflexRef = useRef(false);
  const pendingReflexAngleRef = useRef(0);
  const pendingRelationPlacementMatchRef =
    useRef<DimensionRelationPreview | null>(null);
  const pendingRelationPlacementRetryRef = useRef<number | null>(null);
  const hiddenRelationPreviewDimensionIdsRef = useRef<Set<string>>(new Set());
  const pendingDimensionPlacementRef = useRef(false);
  // The dimension ID that was just created (before the IPC round-trip).
  // Used to delete it on Escape even before the response arrives.
  const pendingDimensionIdRef = useRef<string | null>(null);
  // The entity that was just dimensioned (the source of the pending dimension).
  // Used by the regroup path: if user clicks a different entity, delete the
  // pending dimension and create a two-entity/point dimension instead.
  const pendingDimSourceEntityIdRef = useRef<string | null>(null);
  const dimensionPlacementOriginalPositionRef = useRef<
    [number, number, number] | null
  >(null);
  const dimensionEditOriginalValueRef = useRef<{
    dimensionId: string;
    value: number;
    expression: string;
  } | null>(null);
  const lastPointerEventRef = useRef<PointerEvent | null>(null);
  const isDimensionEditorOpenRef = useRef(false);
  const suppressNextDimensionEditorOpenRef = useRef(false);
  useEffect(() => {
    isDimensionEditorOpenRef.current = isDimensionEditorOpen;
  }, [isDimensionEditorOpen]);
  const setSketchToolRef = useRef(onSetSketchTool);
  const armedSketchConstraintRef = useRef(armedSketchConstraint);
  const mirrorFocusedSlotRef = useRef(mirrorFocusedSlot);
  const mirrorEntityPickRef = useRef(onMirrorEntityPick);
  const cancelSketchConstraintRef = useRef(onCancelSketchConstraint);
  const clearSketchConstraintRef = useRef(onClearSketchConstraint);
  /** Selected constraint for deletion on Delete key. */
  const [selectedConstraint, setSelectedConstraint] =
    useState<SelectedConstraintState | null>(null);
  const selectedConstraintRef = useRef(selectedConstraint);
  selectedConstraintRef.current = selectedConstraint;
  const activeSketchToolRef = useRef<SketchTool>("select");
  const sketchSnapCandidatesRef = useRef<SketchSnapCandidate[]>([]);
  // Track host line ids for midpoint snaps that were committed during
  // a line draft. The first click of a line stores the start's host
  // (if any); the second click stores the end's host. After the
  // resulting `add_sketch_line` IPC settles, the post-add effect
  // reads the new line's start_point_id / end_point_id and dispatches
  // `set_sketch_midpoint_anchor` for each side that snapped to a
  // midpoint. The line count baseline at dispatch time guards against
  // misattributing the anchor to a later line.
  const pendingMidpointAnchorRef = useRef<{
    fromLineCount: number;
    startHostLineId: string | null;
    endHostLineId: string | null;
  } | null>(null);
  const draftStartMidpointHostRef = useRef<string | null>(null);
  // Host line id under the *start* point of the active draft. Stored
  // on pointer-down when the start snaps to an existing line's endpoint,
  // so the *next* click's commit logic can apply a perpendicular
  // constraint between the new line and the host.
  const draftStartEndpointHostRef = useRef<string | null>(null);
  // Pending perpendicular-constraint state, keyed against the line
  // count baseline for the same reasons as the midpoint anchor
  // pending state above. The post-add effect dispatches
  // `set_sketch_perpendicular_constraint` once the new line lands.
  const pendingPerpendicularConstraintRef = useRef<{
    fromLineCount: number;
    hostLineId: string;
  } | null>(null);
  // Pending point-on-line anchor state. Captured at click-time when
  // either end of the just-committed draft snapped to a line body.
  // The post-add effect dispatches one `set_sketch_point_line_anchor`
  // per side once the new line lands. Same baseline-on-line-count
  // guard as the other pending refs.
  const pendingPointLineAnchorRef = useRef<{
    fromLineCount: number;
    startHost: { lineId: string; t: number } | null;
    endHost: { lineId: string; t: number } | null;
  } | null>(null);
  // Mirror of `draftStartMidpointHostRef` for the line-body snap.
  // Holds the host line + t at the time the start was committed so
  // the *next* click (which only sees the end's snap) can still
  // attribute the start-side anchor to the correct host.
  const draftStartLineBodyHostRef = useRef<{
    lineId: string;
    t: number;
  } | null>(null);
  // Latest line count for the active sketch, mirrored as a ref so the
  // pointer handler (which captures stale closures) can baseline new
  // lines for the post-add midpoint-anchor effect.
  const sketchLineCountRef = useRef(0);
  // Stable ref to `onSetSketchMidpointAnchor` so the post-add effect
  // can issue the IPC without remounting on every re-render.
  const setSketchMidpointAnchorRef = useRef(onSetSketchMidpointAnchor);
  const setSketchPointLineAnchorRef = useRef(onSetSketchPointLineAnchor);
  const addSketchAngleDimensionRef = useRef(onAddSketchAngleDimension);
  const addSketchDistanceDimensionRef = useRef(onAddSketchDistanceDimension);
  const addSketchLineLengthDimensionRef = useRef(
    onAddSketchLineLengthDimension,
  );
  const addSketchCircleRadiusDimensionRef = useRef(
    onAddSketchCircleRadiusDimension,
  );
  const addSketchPolygonRadiusDimensionRef = useRef(
    onAddSketchPolygonRadiusDimension,
  );
  const setSketchPerpendicularConstraintRef = useRef(
    onSetSketchPerpendicularConstraint,
  );
  const setSketchLineConstraintRef = useRef(onSetSketchLineConstraint);
  const setSketchTangentConstraintRef = useRef(onSetSketchTangentConstraint);
  const setSketchParallelConstraintRef = useRef(onSetSketchParallelConstraint);
  // Track Alt key for object snap override (invert all snap toggles
  // while held). Updated by keydown/keyup listeners below.
  const altHeldRef = useRef(false);

  // Captured at click-time when the resolved sketch point indicates
  // the line's end snapped to a circle tangent. The post-add effect
  // dispatches `set_sketch_tangent_constraint` so the relation
  // sticks. Same baseline-on-line-count guard as the other refs.
  const pendingTangentConstraintRef = useRef<{
    fromLineCount: number;
    circleId: string;
  } | null>(null);
  // Set at click-time when the resolved sketch point indicates an
  // axis lock; the post-add effect dispatches `set_sketch_line_constraint`
  // for the just-added line. Same baseline-on-line-count guard as
  // the other pending refs to avoid mis-attribution if the line
  // count ticks twice between commit and refresh.
  const pendingAxisConstraintRef = useRef<{
    fromLineCount: number;
    kind: "horizontal" | "vertical";
  } | null>(null);
  // Pending parallel-constraint state. Captured at click-time when the
  // resolved sketch point indicates the line's end is parallel to an
  // existing line. The post-add effect dispatches
  // `set_sketch_parallel_constraint` so the relation sticks.
  // Same baseline-on-line-count guard as the other refs.
  const pendingParallelConstraintRef = useRef<{
    fromLineCount: number;
    hostLineId: string;
  } | null>(null);
  // Pending dimension deletion after a sketch entity commit. Set by
  // `commitDraftDimensionSession` when the user dragged without typing
  // (lockedFields is empty for the relevant field). The post-add effect
  // below reads this and calls `onDeleteSketchDimension` once the new
  // entity lands, removing the auto-dimension the core created.
  const pendingDimensionDeletionRef = useRef<{
    shouldDeleteLine: boolean;
    shouldDeleteCircle: boolean;
    shouldDeletePolygon: boolean;
    shouldDeleteRectangle: boolean;
    shouldDeleteLineAngle: boolean;
  } | null>(null);
  const pendingDraftDimensionExpressionsRef = useRef<{
    tool: DraftDimensionTool;
    fromLineCount: number;
    fromCircleCount: number;
    fromPolygonCount: number;
    expressions: Partial<Record<DraftDimensionField, string>>;
  } | null>(null);
  // Snapshot of the sketch feature's lines for the post-add effect to
  // index into. Same pattern as the count ref above.
  const sketchLinesRef = useRef<
    NonNullable<typeof sketchFeature>["sketch_parameters"] | null
  >(null);
  // Bodies whose edges are picked against a stable pre-fillet topology
  // because a fillet / chamfer feature targeting them is in its
  // pending panel session. Used to flag those edges as ghosts in the
  // scene, which the renderer hides by default and reveals when the
  // user holds Tab. Recomputed alongside the scene so it stays in
  // sync with `feature_history`.
  const pendingEdgeOpBodyIds = useMemo(() => {
    const result = new Set<string>();
    if (!document) {
      return result;
    }
    for (const feature of document.feature_history) {
      const params =
        feature.fillet_parameters ?? feature.chamfer_parameters ?? null;
      if (params && params.is_pending && params.target_body_id) {
        result.add(params.target_body_id);
      }
    }
    return result;
  }, [document]);
  const sceneData = useMemo(
    () =>
      viewport?.has_active_document
        ? createViewportScene(viewport, {
            hiddenFeatureIds,
            hiddenSketchPlaneIds,
            hideReferences,
            pendingEdgeOpBodyIds,
            document,
          })
        : null,
    [
      viewport,
      hiddenFeatureIds,
      hiddenSketchPlaneIds,
      hideReferences,
      pendingEdgeOpBodyIds,
      document,
    ],
  );
  const sceneDataRef = useRef(sceneData);
  useEffect(() => {
    sceneDataRef.current = sceneData;
  }, [sceneData]);
  useEffect(() => {
    applyTheme(activeTheme);
  }, [activeTheme]);
  const hasActiveDocument = Boolean(viewport?.has_active_document);
  const activeSketchPlaneId = document?.active_sketch_plane_id ?? null;
  const activeSketchTool = document?.active_sketch_tool ?? "select";
  const sketchFeature = useMemo(
    () =>
      document?.feature_history.find(
        (feature) => feature.feature_id === document.active_sketch_feature_id,
      ) ?? null,
    [document],
  );
  const activeSketchPlaneFrame =
    sketchFeature?.sketch_parameters?.plane_frame ?? null;
  useEffect(() => {
    const pending = pendingCircleDimensionPlacementRef.current;
    const sketch = sketchFeature?.sketch_parameters;
    if (!pending || !sketch || sketch.circles.length <= pending.fromCircleCount) {
      return;
    }

    const circle =
      sketch.circles[pending.fromCircleCount] ??
      sketch.circles[sketch.circles.length - 1];
    if (!circle) {
      return;
    }
    const radius = distanceBetweenPoints(pending.center, pending.end);
    if (radius <= 1e-6) {
      pendingCircleDimensionPlacementRef.current = null;
      return;
    }
    const dx = pending.end[0] - pending.center[0];
    const dy = pending.end[1] - pending.center[1];
    const length = Math.hypot(dx, dy);
    if (length <= 1e-6) {
      pendingCircleDimensionPlacementRef.current = null;
      return;
    }
    const labelLocal: [number, number] = [
      pending.center[0] + (dx / length) * (radius + 4),
      pending.center[1] + (dy / length) * (radius + 4),
    ];
    setDimensionLabelPositions((current) => ({
      ...current,
      [`dim-circle-${circle.circle_id}`]: toWorldPoint(
        sketch.plane_id,
        labelLocal,
        sketch.plane_frame,
      ),
    }));
    pendingCircleDimensionPlacementRef.current = null;
  }, [sketchFeature]);
  // Post-commit dimension deletion for drag-only shapes that have no
  // typed value (Fusion 360 behavior). When the user commits a shape
  // by dragging without typing into a draft dimension field, the core
  // still creates an auto-dimension — we delete it here.
  useEffect(() => {
    const pending = pendingDimensionDeletionRef.current;
    if (!pending) {
      return;
    }
    const sketch = sketchFeature?.sketch_parameters;
    if (!sketch) {
      pendingDimensionDeletionRef.current = null;
      return;
    }
    // Look up dimensions by entity_id + kind instead of predicting
    // the ID format (dim-line-{id}, etc.). The response always
    // contains the full dimensions array, so this tolerates ID
    // format changes and avoids race conditions where the predicted
    // ID doesn't match what the core actually created.
    const findDimId = (entityId: string, kind: string): string | undefined =>
      sketch.dimensions.find(
        (d) => d.entity_id === entityId && d.kind === kind,
      )?.dimension_id;

    // Use the last entity instead of fromLineCount to avoid race
    // conditions when React hasn't re-rendered between rapid commits.
    if (pending.shouldDeleteLine && sketch.lines.length > 0) {
      const line = sketch.lines[sketch.lines.length - 1];
      if (line && !line.is_construction) {
        const dimId = findDimId(line.line_id, "line_length");
        if (dimId) void deleteSketchDimensionRef.current(dimId);
      }
    }
    if (pending.shouldDeleteLineAngle && sketch.lines.length > 0) {
      const line = sketch.lines[sketch.lines.length - 1];
      if (line && !line.is_construction) {
        const dimId = findDimId(line.line_id, "line_angle");
        if (dimId) void deleteSketchDimensionRef.current(dimId);
      }
    }
    if (pending.shouldDeleteCircle && sketch.circles.length > 0) {
      const circle = sketch.circles[sketch.circles.length - 1];
      if (circle && !circle.is_construction) {
        const dimId = findDimId(circle.circle_id, "circle_radius");
        if (dimId) void deleteSketchDimensionRef.current(dimId);
      }
    }
    if (pending.shouldDeletePolygon && (sketch.polygons?.length ?? 0) > 0) {
      const polygon = sketch.polygons?.[(sketch.polygons?.length ?? 1) - 1];
      if (polygon && !polygon.is_construction) {
        const dimId = findDimId(polygon.polygon_id, "polygon_radius");
        if (dimId) void deleteSketchDimensionRef.current(dimId);
      }
    }
    if (pending.shouldDeleteRectangle && sketch.lines.length >= 4) {
      for (let i = sketch.lines.length - 4; i < sketch.lines.length; i++) {
        const line = sketch.lines[i];
        if (line && !line.is_construction) {
          const dimId = findDimId(line.line_id, "line_length");
          if (dimId) void deleteSketchDimensionRef.current(dimId);
        }
      }
    }
    pendingDimensionDeletionRef.current = null;
  }, [sketchFeature]);
  useEffect(() => {
    const pending = pendingDraftDimensionExpressionsRef.current;
    const sketch = sketchFeature?.sketch_parameters;
    if (!pending || !sketch) {
      return;
    }

    const updateDimensionExpression = (dimensionId: string, expression?: string) => {
      if (!expression) {
        return;
      }
      void updateSketchDimensionRef.current(dimensionId, expression).catch(() => {});
    };

    if (pending.tool === "line") {
      if (sketch.lines.length <= pending.fromLineCount) {
        return;
      }
      const line =
        sketch.lines[pending.fromLineCount] ??
        sketch.lines[sketch.lines.length - 1];
      if (line) {
        updateDimensionExpression(
          `dim-line-${line.line_id}`,
          pending.expressions.length,
        );
        updateDimensionExpression(
          `dim-line-angle-${line.line_id}`,
          pending.expressions.angle,
        );
      }
    } else if (pending.tool === "rectangle") {
      if (sketch.lines.length < pending.fromLineCount + 4) {
        return;
      }
      const topLine = sketch.lines[pending.fromLineCount];
      const rightLine = sketch.lines[pending.fromLineCount + 1];
      if (topLine) {
        updateDimensionExpression(
          `dim-line-${topLine.line_id}`,
          pending.expressions.width,
        );
      }
      if (rightLine) {
        updateDimensionExpression(
          `dim-line-${rightLine.line_id}`,
          pending.expressions.length,
        );
      }
    } else if (pending.tool === "circle") {
      if (sketch.circles.length <= pending.fromCircleCount) {
        return;
      }
      const circle =
        sketch.circles[pending.fromCircleCount] ??
        sketch.circles[sketch.circles.length - 1];
      if (circle) {
        updateDimensionExpression(
          `dim-circle-${circle.circle_id}`,
          pending.expressions.diameter,
        );
      }
    } else if (pending.tool === "polygon") {
      const polygons = sketch.polygons ?? [];
      if (polygons.length <= pending.fromPolygonCount) {
        return;
      }
      const polygon =
        polygons[pending.fromPolygonCount] ?? polygons[polygons.length - 1];
      if (polygon) {
        updateDimensionExpression(
          `dim-polygon-${polygon.polygon_id}`,
          pending.expressions.radius,
        );
      }
    }

    pendingDraftDimensionExpressionsRef.current = null;
  }, [sketchFeature]);
  const selectedPrimitiveLabel = useMemo(() => {
    const selectedBox = viewport?.boxes.find((box) => box.is_selected);
    if (selectedBox) {
      return selectedBox.label;
    }

    const selectedCylinder = viewport?.cylinders.find(
      (cylinder) => cylinder.is_selected,
    );
    if (selectedCylinder) {
      return selectedCylinder.label;
    }

    const selectedPolygonExtrude = viewport?.polygon_extrudes.find(
      (primitive) => primitive.is_selected,
    );
    return selectedPolygonExtrude?.label ?? null;
  }, [viewport]);
  const selectedReference = useMemo(
    () =>
      viewport?.reference_planes.find(
        (referencePlane) => referencePlane.is_selected,
      ) ?? null,
    [viewport],
  );
  // Live "quick measurement" readout for the bottom-right Selection
  // panel, mirroring common CAD workflow's behavior where a single edge shows its
  // length and two vertices show their straight-line distance. Edge
  // length is computed by the core (BRepGProp) and shipped on the
  // viewport edge primitive; vertex distance is a trivial Euclidean
  // calc on world-space positions the core already gave us, so we do
  // it inline rather than round-tripping a `measure` command. Anything
  // outside those two cases returns null so the row is hidden.
  const measurementText = useMemo(() => {
    if (!document || !viewport) {
      return null;
    }
    if (document.selected_edge_ids.length === 1) {
      const edge = viewport.edges.find(
        (entry) => entry.id === document.selected_edge_ids[0],
      );
      if (edge) {
        return `Length: ${edge.length.toFixed(2)} mm`;
      }
    }
    if (document.selected_vertex_ids.length === 2) {
      const [aId, bId] = document.selected_vertex_ids;
      const a = viewport.vertices.find((entry) => entry.id === aId);
      const b = viewport.vertices.find((entry) => entry.id === bId);
      if (a && b) {
        const dx = a.position.x - b.position.x;
        const dy = a.position.y - b.position.y;
        const dz = a.position.z - b.position.z;
        const distance = Math.sqrt(dx * dx + dy * dy + dz * dz);
        return `Distance: ${distance.toFixed(2)} mm`;
      }
    }
    return null;
  }, [document, viewport]);
  function angleDimensionFrame(
    dimension: SketchDimensionScene,
  ): AngleDimensionFrame | null {
    return buildAngleDimensionFrame({
      dimension,
      sketchParameters: sketchFeature?.sketch_parameters,
    });
  }

  const displayedSketchDimensions = useMemo(() => {
    if (!sceneData) {
      return [];
    }
    return sceneData.sketchDimensions.map((dimension) => {
      // Angle dimensions use a radius-only drag state — no directional
      // control point, so the arc stays centred on the bisector and the
      // radius tracks the cursor distance 1:1.
      if (dimension.kind === "angle" || dimension.kind === "line_angle") {
        const dragRadius = angleDragRadii[dimension.dimensionId];
        if (dragRadius !== undefined) {
          const frame = angleDimensionFrame(dimension);
          if (frame) {
            const r = clampAngleRadius(dragRadius);
            const toTuple = (p: THREE.Vector3): [number, number, number] => [p.x, p.y, p.z];
            return {
              ...dimension,
              arcRadius: r,
              anchorStart: toTuple(frame.pivot.clone().add(frame.startUnit.clone().multiplyScalar(frame.anchorRadius))),
              anchorEnd:   toTuple(frame.pivot.clone().add(frame.endUnit.clone().multiplyScalar(frame.anchorRadius))),
              dimensionStart: toTuple(frame.pivot.clone().add(frame.startUnit.clone().multiplyScalar(r))),
              dimensionEnd:   toTuple(frame.pivot.clone().add(frame.endUnit.clone().multiplyScalar(r))),
              labelPosition:  toTuple(frame.pivot.clone().add(frame.bisector.clone().multiplyScalar(r))),
            };
          }
          return dimension;
        }
      }

      // Angle dimensions are handled above via angleDragRadii;
      // never let them reach the generic offset path.
      if (dimension.kind === "angle" || dimension.kind === "line_angle") {
        return dimension;
      }

      const labelPosition = dimensionLabelPositions[dimension.dimensionId];
      if (!labelPosition) {
        return dimension;
      }
      const originalLabel = new THREE.Vector3(...dimension.labelPosition);
      const nextLabel = new THREE.Vector3(...labelPosition);
      let offset = nextLabel.sub(originalLabel);
      if (dimension.kind === "circle_radius") {
        const projection = circleRadiusDimensionProjection({
          dimension,
          worldPoint: labelPosition,
          planeFrame: activeSketchPlaneFrame,
        });
        if (projection) {
          const start = projection.center
            .clone()
            .add(projection.direction.clone().multiplyScalar(-projection.radius));
          const end = projection.center
            .clone()
            .add(projection.direction.clone().multiplyScalar(projection.radius));
          const toTuple = (point: THREE.Vector3): [number, number, number] => [
            point.x,
            point.y,
            point.z,
          ];
          return {
            ...dimension,
            anchorStart: toTuple(start),
            anchorEnd: toTuple(end),
            dimensionStart: toTuple(start),
            dimensionEnd: toTuple(end),
            labelPosition,
          };
        }
        // Fall through to generic offset if circle radius computation fails
      }

      // Generic offset for linear dimensions (line_length, distance, etc.)
      const extensionAxis = new THREE.Vector3(
        ...dimension.dimensionStart,
      ).sub(new THREE.Vector3(...dimension.anchorStart));
      const dimensionDirection = new THREE.Vector3(
        ...dimension.dimensionEnd,
      ).sub(new THREE.Vector3(...dimension.dimensionStart));
      const placementAxis =
        extensionAxis.lengthSq() > 1e-8
          ? extensionAxis.normalize()
          : getSketchGridFrame(
              dimension.planeId,
              activeSketchPlaneFrame,
            ).normal
              .cross(dimensionDirection)
              .normalize();
      if (placementAxis.lengthSq() > 1e-8) {
        offset = placementAxis.multiplyScalar(offset.dot(placementAxis));
      }
      const shiftPoint = (point: [number, number, number]) => {
        const shifted = new THREE.Vector3(...point).add(offset);
        return [shifted.x, shifted.y, shifted.z] as [number, number, number];
      };
      const shiftedLabel = shiftPoint(dimension.labelPosition);
      if (
        dimension.kind === "line_line_distance" ||
        dimension.kind === "circle_line_distance"
      ) {
        return {
          ...dimension,
          dimensionStart: shiftPoint(dimension.dimensionStart),
          dimensionEnd: shiftPoint(dimension.dimensionEnd),
          labelPosition: shiftedLabel,
        };
      }
      return {
        ...dimension,
        dimensionStart: shiftPoint(dimension.dimensionStart),
        dimensionEnd: shiftPoint(dimension.dimensionEnd),
        labelPosition: shiftedLabel,
      };
    });
  }, [activeSketchPlaneFrame, angleDragRadii, dimensionLabelPositions, sceneData]);
  useEffect(() => {
    displayedSketchDimensionsRef.current = displayedSketchDimensions;
  }, [displayedSketchDimensions]);
  const selectedSketchDimension = useMemo(
    () =>
      document?.selected_sketch_dimension_id
        ? (displayedSketchDimensions.find(
            (dimension) =>
              dimension.dimensionId === document.selected_sketch_dimension_id,
          ) ?? null)
        : null,
    [displayedSketchDimensions, document?.selected_sketch_dimension_id],
  );
  const selectedSketchDimensionValue = useMemo(
    () =>
      document?.selected_sketch_dimension_id && sketchFeature?.sketch_parameters
        ? (sketchFeature.sketch_parameters.dimensions.find(
            (dimension) =>
              dimension.dimension_id === document.selected_sketch_dimension_id,
          )?.value ?? null)
        : null,
    [document?.selected_sketch_dimension_id, sketchFeature],
  );
  const selectedSketchDimensionExpression = useMemo(
    () =>
      document?.selected_sketch_dimension_id && sketchFeature?.sketch_parameters
        ? (sketchFeature.sketch_parameters.dimensions.find(
            (dimension) =>
              dimension.dimension_id === document.selected_sketch_dimension_id,
          )?.expression ?? "")
        : "",
    [document?.selected_sketch_dimension_id, sketchFeature],
  );
  const getParameterSuggestions = (
    value: string,
    cursor: number,
    isAngleDimension: boolean,
  ): ParameterSuggestion[] => {
    if (!document?.parameters.length) {
      return [];
    }
    const token = parameterTokenAtCursor(value, cursor);
    if (!token) {
      return [];
    }
    const normalizedQuery = token.query.toLowerCase();
    if (
      document.parameters.some(
        (parameter) =>
          !parameter.has_error &&
          parameter.name.toLowerCase() === normalizedQuery,
      )
    ) {
      return [];
    }
    return document.parameters
      .filter((parameter) => !parameter.has_error)
      .filter((parameter) =>
        isAngleDimension ? parameter.kind === "angle" : parameter.kind !== "angle",
      )
      .map((parameter) => ({
        parameter,
        score: fuzzyParameterScore(token.query, parameter.name),
      }))
      .filter((entry) => entry.score > 0)
      .sort((left, right) => right.score - left.score)
      .slice(0, 6)
      .map(({ parameter }) => ({
        name: parameter.name,
        expression: parameter.expression,
        kind: parameter.kind,
        value: parameter.resolved_value,
      }));
  };
  const dimensionParameterSuggestions = useMemo<ParameterSuggestion[]>(() => {
    if (!selectedSketchDimension) {
      return [];
    }
    const cursor =
      dimensionInputRef.current?.selectionStart ?? dimensionDraftValue.length;
    const isAngleDimension =
      selectedSketchDimension.kind === "angle" ||
      selectedSketchDimension.kind === "line_angle";
    return getParameterSuggestions(
      dimensionDraftValue,
      cursor,
      isAngleDimension,
    );
  }, [
    dimensionDraftValue,
    document?.parameters,
    selectedSketchDimension,
  ]);
  useEffect(() => {
    setDimensionSuggestionIndex(0);
  }, [dimensionDraftValue, selectedSketchDimension?.dimensionId]);
  /** Stable DOF map ref — updated on every viewport change, read by
   *  paintSketchEntityMaterials so hover never sees an empty map. */
  const dofMapRef = useRef<Map<string, "full" | "over">>(new Map());
  useEffect(() => {
    const map = new Map<string, "full" | "over">();
    for (const ds of (viewport?.dof_statuses ?? [])) {
      if (ds.status === "full" || ds.status === "over") {
        map.set(ds.entity_id, ds.status);
      }
    }
    dofMapRef.current = map;
  }, [viewport?.dof_statuses]);

  /** DOF status for the currently selected sketch entity, if any. */
  const selectedEntityDof = useMemo(() => {
    const id = document?.selected_sketch_entity_id;
    const statuses = viewport?.dof_statuses;
    if (!id || !statuses) return null;
    return statuses.find((s) => s.entity_id === id) ?? null;
  }, [document?.selected_sketch_entity_id, viewport?.dof_statuses]);

  const sketchSnapCandidates = useMemo(
    () =>
      buildSketchSnapCandidates({
        sketchParameters: sketchFeature?.sketch_parameters,
        coreCandidates: viewport?.snap_candidates,
        translate,
      }),
    [sketchFeature, translate, viewport?.snap_candidates],
  );
  const activeSketchPlaneIdRef = useRef(activeSketchPlaneId);
  const activeSketchPlaneFrameRef = useRef(activeSketchPlaneFrame);
  const showViewportGridRef = useRef(showViewportGrid);
  const showSketchGridRef = useRef(showSketchGrid);
  const documentRef = useRef(document);
  useEffect(() => {
    activeSketchPlaneIdRef.current = activeSketchPlaneId;
    activeSketchPlaneFrameRef.current = activeSketchPlaneFrame;
  }, [activeSketchPlaneId, activeSketchPlaneFrame]);
  useEffect(() => {
    showViewportGridRef.current = showViewportGrid;
  }, [showViewportGrid]);
  useEffect(() => {
    showSketchGridRef.current = showSketchGrid;
  }, [showSketchGrid]);
  useEffect(() => {
    documentRef.current = document;
  }, [document]);
  useEffect(() => {
    draftDimensionSessionRef.current = draftDimensionSession;
  }, [draftDimensionSession]);
  useEffect(() => {
    if (!draftDimensionSession) {
      return;
    }
    renderDraftPreview(draftDimensionSession);
  }, [draftDimensionSession, sketchToolConstruction]);

  function toggleGridVisibility(kind: "viewport" | "sketch") {
    updateConfig((current) => ({
      ...current,
      viewport: {
        ...current.viewport,
        showGrid:
          kind === "viewport"
            ? !current.viewport.showGrid
            : current.viewport.showGrid,
        showSketchGrid:
          kind === "sketch"
            ? !current.viewport.showSketchGrid
            : current.viewport.showSketchGrid,
      },
    }));
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
      if (
        isTypingTarget(event.target) ||
        !matchesHotkey(event, config.hotkeys.viewport.toggleGrid)
      ) {
        return;
      }
      event.preventDefault();
      updateConfig((current) => {
        const isSketchMode = Boolean(activeSketchPlaneIdRef.current);
        return {
          ...current,
          viewport: {
            ...current.viewport,
            showGrid: isSketchMode
              ? current.viewport.showGrid
              : !current.viewport.showGrid,
            showSketchGrid: isSketchMode
              ? !current.viewport.showSketchGrid
              : current.viewport.showSketchGrid,
          },
        };
      });
    }

    window.addEventListener("keydown", handleKeyDown);
    return () => {
      window.removeEventListener("keydown", handleKeyDown);
    };
  }, [config.hotkeys.viewport.toggleGrid, updateConfig]);

  // Track Alt key for object snap override.
  useEffect(() => {
    function handleAltDown(e: KeyboardEvent) {
      if (e.key === "Alt") altHeldRef.current = true;
    }
    function handleAltUp(e: KeyboardEvent) {
      if (e.key === "Alt") altHeldRef.current = false;
    }
    window.addEventListener("keydown", handleAltDown);
    window.addEventListener("keyup", handleAltUp);
    return () => {
      window.removeEventListener("keydown", handleAltDown);
      window.removeEventListener("keyup", handleAltUp);
    };
  }, []);

  useEffect(() => {
    const scene = sceneRef.current;
    const cubeScene = viewCubeSceneRef.current;

    if (cubeScene) {
      const previousCubeGroup = viewCubeGroupRef.current;
      if (previousCubeGroup) {
        cubeScene.remove(previousCubeGroup);
        disposeViewCubeGroup(previousCubeGroup);
      }

      const nextCubeGroup = buildViewCubeGroup();
      cubeScene.add(nextCubeGroup);
      viewCubeGroupRef.current = nextCubeGroup;
      viewCubeHoveredRef.current = null;
    }

    if (scene) {
      const worldGrid = worldGridRef.current;
      if (worldGrid) {
        scene.remove(worldGrid.group);
        disposeDynamicGrid(worldGrid);
        worldGridRef.current = null;
      }

      const sketchGrid = sketchGridRef.current;
      if (sketchGrid) {
        scene.remove(sketchGrid.group);
        disposeDynamicGrid(sketchGrid);
        sketchGridRef.current = null;
      }
    }

    syncPrimitiveVisuals();
    syncReferencePlaneVisuals();
    syncSolidFaceVisuals();
    syncSketchProfileVisuals();
    paintEdgeMaterials(hoveredEdgeIdRef.current);
    paintVertexMaterials(hoveredVertexIdRef.current);
    paintSketchEntityMaterials();
    paintSketchPointMaterials();
    paintDofStatusColors();
  }, [activeTheme.id]);

  // Update constraint badge highlights whenever selection changes.
  useEffect(() => {
    for (const obj of sketchConstraintObjectsRef.current) {
      const conEntityId =
        obj.userData.sketchConstraintEntityId as string | undefined;
      const conKind =
        obj.userData.sketchConstraintKind as string | undefined;
      const isSelected =
        selectedConstraint !== null &&
        conEntityId === selectedConstraint.entityId &&
        conKind === selectedConstraint.kind;
      if (obj instanceof THREE.Sprite && obj.material instanceof THREE.SpriteMaterial) {
        if (isSelected) {
          obj.material.color.set(0x60e0ff); // bright cyan
          obj.scale.set(7.5, 7.5, 1);
        } else {
          obj.material.color.set(0xffffff);
          obj.scale.set(6, 6, 1);
        }
      }
    }
  }, [selectedConstraint]);

  function clearDragPreviewLines() {
    const sketchGroup = sketchGroupRef.current;
    for (const line of dragPreviewLinesRef.current) {
      if (sketchGroup) sketchGroup.remove(line);
      line.geometry.dispose();
      disposeMaterial(line.material);
    }
    dragPreviewLinesRef.current = [];
  }

  function clearPreviewLine() {
    const previewLine = previewLineRef.current;
    const sketchGroup = sketchGroupRef.current;
    if (!previewLine || !sketchGroup) {
      return;
    }

    sketchGroup.remove(previewLine);
    previewLine.geometry.dispose();
    disposeMaterial(previewLine.material);
    previewLineRef.current = null;
  }

  function clearPreviewCircle() {
    const previewCircle = previewCircleRef.current;
    const sketchGroup = sketchGroupRef.current;
    if (!previewCircle || !sketchGroup) {
      return;
    }

    sketchGroup.remove(previewCircle);
    previewCircle.geometry.dispose();
    disposeMaterial(previewCircle.material);
    previewCircleRef.current = null;
  }

  function clearPreviewArc() {
    const previewArc = previewArcRef.current;
    const sketchGroup = sketchGroupRef.current;
    if (!previewArc || !sketchGroup) {
      return;
    }

    sketchGroup.remove(previewArc);
    previewArc.geometry.dispose();
    disposeMaterial(previewArc.material);
    previewArcRef.current = null;
  }

  function clearTrimSegmentHighlight() {
    const hl = trimSegmentHighlightRef.current;
    const sketchGroup = sketchGroupRef.current;
    if (!hl || !sketchGroup) return;
    sketchGroup.remove(hl);
    hl.geometry.dispose();
    disposeMaterial(hl.material);
    trimSegmentHighlightRef.current = null;
  }
  function clearTrimArcHighlight() {
    const hl = trimArcHighlightRef.current;
    const sketchGroup = sketchGroupRef.current;
    if (!hl || !sketchGroup) return;
    sketchGroup.remove(hl);
    hl.geometry.dispose();
    disposeMaterial(hl.material);
    trimArcHighlightRef.current = null;
  }

  function updateTrimSegmentHighlight(
    _lineId: string,
    segments: Array<{ sx: number; sy: number; sz: number; ex: number; ey: number; ez: number }>,
    hoveredSegIdx: number,
  ) {
    clearTrimSegmentHighlight();
    if (hoveredSegIdx < 0 || hoveredSegIdx >= segments.length) return;
    const seg = segments[hoveredSegIdx];
    const sketchGroup = sketchGroupRef.current;
    if (!sketchGroup) return;

    const material = new THREE.LineBasicMaterial({
      color: 0xff3333,
      transparent: true,
      opacity: 0.9,
      linewidth: 3,
      depthTest: false,
    });
    const points = [
      new THREE.Vector3(seg.sx, seg.sy, seg.sz),
      new THREE.Vector3(seg.ex, seg.ey, seg.ez),
    ];
    const geometry = new THREE.BufferGeometry().setFromPoints(points);
    const hl = new THREE.Line(geometry, material);
    hl.renderOrder = 8; // above sketch entities (7)
    trimSegmentHighlightRef.current = hl;
    sketchGroup.add(hl);
  }

  function updateTrimArcHighlight(worldPts: Array<[number, number, number]>) {
    clearTrimArcHighlight();
    if (worldPts.length < 2) return;
    const sketchGroup = sketchGroupRef.current;
    if (!sketchGroup) return;
    const material = new THREE.LineBasicMaterial({
      color: 0xff3333, transparent: true, opacity: 0.9,
      linewidth: 3, depthTest: false,
    });
    const geometry = new THREE.BufferGeometry().setFromPoints(
      worldPts.map(([x, y, z]) => new THREE.Vector3(x, y, z)),
    );
    const hl = new THREE.Line(geometry, material);
    hl.renderOrder = 8;
    trimArcHighlightRef.current = hl;
    sketchGroup.add(hl);
  }

  /**
   * Render the trim preview highlight from core-computed segment data.
   * Called by the `polysmith-trim-preview` event listener.
   */
  function renderTrimPreviewHighlight() {
    const data = trimPreviewResultRef.current;
    if (!data) { clearTrimSegmentHighlight(); clearTrimArcHighlight(); return; }

    const frame = activeSketchPlaneFrameRef.current;
    const toWorld = (lx: number, ly: number): THREE.Vector3 => {
      if (frame) {
        const origin = new THREE.Vector3(frame.origin.x, frame.origin.y, frame.origin.z);
        const xAxis = new THREE.Vector3(frame.x_axis.x, frame.x_axis.y, frame.x_axis.z);
        const yAxis = new THREE.Vector3(frame.y_axis.x, frame.y_axis.y, frame.y_axis.z);
        return origin.clone().addScaledVector(xAxis, lx).addScaledVector(yAxis, ly);
      }
      return new THREE.Vector3(lx, 0, ly);
    };

    const hoveredIdx = data.hovered_index;
    if (hoveredIdx == null || hoveredIdx < 0) {
      clearTrimSegmentHighlight(); clearTrimArcHighlight(); return;
    }

    if (data.entity_kind === "line" && data.segments) {
      const seg = data.segments[hoveredIdx];
      if (!seg) { clearTrimSegmentHighlight(); return; }
      clearTrimArcHighlight();
      updateTrimSegmentHighlight(data.entity_id, [{
        sx: seg.start[0], sy: seg.start[1], sz: 0,
        ex: seg.end[0],   ey: seg.end[1],   ez: 0,
      }], 0);
      return;
    }

    // Circle or arc: sample the arc segment at the plane frame.
    const scn = sceneData;
    if (!scn) return;
    const r =
      data.entity_kind === "circle"
        ? scn.sketchCircles?.find((c: any) => c.circleId === data.entity_id)
        : scn.sketchArcs?.find((a: any) => a.arcId === data.entity_id);
    if (!r) { clearTrimArcHighlight(); return; }

    const cx = r.center[0], cy = r.center[1], cz = r.center[2];
    const radius = r.radius;
    const full = data.full_circle || data.full_arc;

    const pts: Array<[number, number, number]> = [];
    if (full) {
      for (let i = 0; i <= 48; i++) {
        const a = (i / 48) * 2 * Math.PI;
        pts.push([
          cx + Math.cos(a) * radius,
          cy + Math.sin(a) * radius,
          cz,
        ]);
      }
    } else {
      const seg = data.segments?.[hoveredIdx];
      if (!seg) { clearTrimArcHighlight(); return; }
      const s = seg.param_start, e = seg.param_end;
      const ee = e <= s ? e + 2 * Math.PI : e;
      for (let i = 0; i <= 48; i++) {
        const a = s + (ee - s) * (i / 48);
        pts.push([
          cx + Math.cos(a) * radius,
          cy + Math.sin(a) * radius,
          cz,
        ]);
      }
    }
    clearTrimSegmentHighlight();
    updateTrimArcHighlight(pts);
  }

  function clearPreviewDimension() {
    const previewDimension = previewDimensionRef.current;
    const sketchGroup = sketchGroupRef.current;
    dimensionRelationPreviewRef.current = null;
    dimensionRelationPreviewLabelRef.current = null;
    restoreRelationPreviewHiddenDimensions();
    if (!previewDimension || !sketchGroup) {
      return;
    }

    sketchGroup.remove(previewDimension.line);
    sketchGroup.remove(previewDimension.label);
    disposeGeometryTreeResources(previewDimension.line);
    const labelMaterial = previewDimension.label.material;
    if (labelMaterial instanceof THREE.SpriteMaterial) {
      labelMaterial.map?.dispose();
    }
    disposeMaterial(labelMaterial);
    previewDimensionRef.current = null;
  }

  function clearDimensionToolFirstPick() {
    dimensionToolFirstLineRef.current = null;
    setDimensionToolFirstLine(null);
    dimensionToolFirstPointRef.current = null;
  }

  function clearDimensionToolFirstEntity() {
    dimensionToolFirstLineRef.current = null;
    setDimensionToolFirstLine(null);
  }

  function clearPendingDimensionPlacement() {
    pendingDimensionIdRef.current = null;
    pendingDimSourceEntityIdRef.current = null;
    pendingDimensionPlacementRef.current = false;
  }

  function setSketchDimensionObjectVisibility(
    dimensionId: string,
    visible: boolean,
  ) {
    for (const object of sketchDimensionObjectsRef.current) {
      let matches = object.userData.sketchDimensionId === dimensionId;
      if (!matches) {
        object.traverse((child) => {
          if (child.userData.sketchDimensionId === dimensionId) {
            matches = true;
          }
        });
      }
      if (matches) {
        object.visible = visible;
      }
    }
  }

  function hideRelationPreviewDimension(dimensionId: string | null) {
    if (!dimensionId) {
      return;
    }
    hiddenRelationPreviewDimensionIdsRef.current.add(dimensionId);
    setSketchDimensionObjectVisibility(dimensionId, false);
  }

  function restoreRelationPreviewHiddenDimensions() {
    for (const dimensionId of hiddenRelationPreviewDimensionIdsRef.current) {
      setSketchDimensionObjectVisibility(dimensionId, true);
    }
    hiddenRelationPreviewDimensionIdsRef.current.clear();
  }

  function clearDraftDimGroup() {
    const group = draftDimGroupRef.current;
    const sketchGroup = sketchGroupRef.current;
    if (!group || !sketchGroup) {
      return;
    }
    sketchGroup.remove(group);
    disposeGeometryTreeResources(group);
    draftDimGroupRef.current = null;

    // Also clean up the reusable scene object
    const sceneObj = draftDimSceneObjRef.current;
    if (sceneObj && sketchGroup) {
      sketchGroup.remove(sceneObj.lines);
      sceneObj.lines.geometry.dispose();
      disposeMaterial(sceneObj.lines.material);
      draftDimSceneObjRef.current = null;
    }
    const arcTest = draftArcTestRef.current;
    if (arcTest && sketchGroup) {
      sketchGroup.remove(arcTest);
      arcTest.geometry.dispose();
      disposeMaterial(arcTest.material);
      draftArcTestRef.current = null;
    }
  }

  function clearViewportSceneObjectRefs() {
    primitiveVisualsRef.current.clear();
    primitiveStatesRef.current.clear();
    referencePlaneVisualsRef.current.clear();
    referencePlaneStatesRef.current.clear();
    solidFaceVisualsRef.current.clear();
    solidFaceStatesRef.current.clear();
    sketchProfileVisualsRef.current.clear();
    sketchProfileStatesRef.current.clear();
    referencePlaneMeshesRef.current = [];
    sketchEntityObjectsRef.current = [];
    sketchDimensionObjectsRef.current = [];
    sketchConstraintObjectsRef.current = [];
    sketchPointObjectsRef.current = [];
    sketchProfileObjectsRef.current = [];
    meshesRef.current = [];
    faceMeshesRef.current = [];
    edgeLineObjectsRef.current = [];
    vertexObjectsRef.current = [];
    cutPreviewObjectsRef.current = [];
    toolpathLinesRef.current = [];
    moveGizmoObjectsRef.current = [];
    previewLineRef.current = null;
    previewCircleRef.current = null;
    previewArcRef.current = null;
  }


  /** Returns the snapped point and a boolean indicating whether grid
   *  snap actually fired. When gridSnap is disabled, returns raw point
   *  with snapped = false. */
  function snapRawPointToGrid(
    rawPoint: {
      local: [number, number];
      world: [number, number, number];
    },
    worldUnitsPerPixel: number,
    gridSnapEnabled: boolean,
  ): { point: typeof rawPoint; snapped: boolean } {
    const spacing = currentGridSpacingRef.current;
    if (!gridSnapEnabled) {
      return { point: rawPoint, snapped: false };
    }
    if (!Number.isFinite(spacing) || spacing <= 0) {
      return { point: rawPoint, snapped: false };
    }
    const threshold = worldUnitsPerPixel * GRID_SNAP_SCREEN_DISTANCE_PX;
    const nearestX = Math.round(rawPoint.local[0] / spacing) * spacing;
    const nearestY = Math.round(rawPoint.local[1] / spacing) * spacing;
    const local: [number, number] = [
      Math.abs(rawPoint.local[0] - nearestX) <= threshold
        ? nearestX
        : rawPoint.local[0],
      Math.abs(rawPoint.local[1] - nearestY) <= threshold
        ? nearestY
        : rawPoint.local[1],
    ];
    if (local[0] === rawPoint.local[0] && local[1] === rawPoint.local[1]) {
      return { point: rawPoint, snapped: false };
    }
    return {
      point: {
        local,
        world: toWorldPoint(
          activeSketchPlaneId ?? "ref-plane-xy",
          local,
          activeSketchPlaneFrame,
        ),
      },
      snapped: true,
    };
  }

  function createDraftDimensionSession(
    tool: DraftDimensionTool,
    start: [number, number],
    current: [number, number],
  ): DraftDimensionSession {
    const fields = draftSessionFields(tool);
    return {
      tool,
      start,
      current,
      values: draftSessionValues(tool, start, current),
      activeField: fields[0],
      lockedFields: {},
      touchedFields: {},
    };
  }

  function clearDraftDimensionSession() {
    Object.values(draftDimensionInputRefs.current).forEach((input) => {
      input?.blur();
    });
    clearDraftDimGroup();
    setDraftDimensionSession(null);
    draftDimensionSessionRef.current = null;
    draftFieldFocusedRef.current = null;
    draftRawInputRef.current = {};
    draftParameterExpressionRef.current = {};
    previousLineAngleRef.current = null;
    setDraftSuggestionState(null);
  }

  // Centralized helper: schedule deletion of auto-dimensions after a
  // shape commit when the user dragged/clicked without typing a value.
  // Call this BEFORE clearing the draft session so lockedFields are
  // still available. Pass the tool and optionally a pre-captured
  // session (for chained-line paths where the session is about to be
  // replaced).
  function scheduleDimensionDeletion(
    tool: DraftDimensionTool,
    preCapturedSession?: DraftDimensionSession | null,
  ) {
    const session = preCapturedSession ?? draftDimensionSessionRef.current;
    // Delete auto-dimensions only when the user never touched the
    // corresponding field. `touchedFields` tracks whether the user
    // typed into a field at all during this draft session, even if
    // they later cleared the value. This handles the "typed '10'
    // then backspaced" edge case: the dimension is preserved because
    // the user demonstrated intent to control it.
    pendingDimensionDeletionRef.current = {
      shouldDeleteLine:
        tool === "line" && !session?.touchedFields.length,
      shouldDeleteCircle:
        tool === "circle" && !session?.touchedFields.diameter,
      shouldDeletePolygon:
        tool === "polygon" && !session?.touchedFields.radius,
      shouldDeleteRectangle:
        tool === "rectangle" &&
        !session?.touchedFields.width &&
        !session?.touchedFields.length,
      shouldDeleteLineAngle:
        tool === "line" && !session?.touchedFields.angle,
    };
  }

  function scheduleDraftDimensionExpressionUpdate(tool: DraftDimensionTool) {
    const entries = Object.entries(draftParameterExpressionRef.current).filter(
      ([, expression]) => expression.trim().length > 0,
    );
    if (entries.length === 0) {
      pendingDraftDimensionExpressionsRef.current = null;
      return;
    }
    pendingDraftDimensionExpressionsRef.current = {
      tool,
      fromLineCount: sketchLineCountRef.current,
      fromCircleCount: sketchFeature?.sketch_parameters?.circles.length ?? 0,
      fromPolygonCount: sketchFeature?.sketch_parameters?.polygons?.length ?? 0,
      expressions: Object.fromEntries(entries) as Partial<
        Record<DraftDimensionField, string>
      >,
    };
    draftParameterExpressionRef.current = {};
  }

  function suppressDimensionEditorAfterSketchCommit() {
    suppressNextDimensionEditorOpenRef.current = true;
    dimensionInputRef.current?.blur();
    setIsDimensionEditorOpen(false);
  }

  // Look up a dimension's display_as preference from the document state.
  // Falls back to "" (diameter display) when the dimension isn't found
  // or the field is absent (backward compat with older documents).
  function resolveDimensionDisplayAs(dimensionId: string): string {
    const sketch = sketchLinesRef.current;
    if (!sketch) return "";
    const dim = sketch.dimensions.find(
      (d) => d.dimension_id === dimensionId,
    );
    return dim?.display_as ?? "";
  }

  // --- Dimension tool action helpers (shared by entity + sketch-point paths) ---

  function dimCreateCircle(entityId: string, displayAs: string) {
    const dimensionId = `dim-circle-${entityId}`;
    pendingDimensionIdRef.current = dimensionId;
    pendingDimSourceEntityIdRef.current = entityId;
    pendingDimensionPlacementRef.current = true;
    // Stage for possible two-entity follow-up pick while the diameter
    // placement is active.
    dimensionToolFirstLineRef.current = entityId;
    setDimensionToolFirstLine(entityId);
    void addSketchCircleRadiusDimensionRef
      .current(entityId, displayAs)
      .catch(() => {
        pendingDimensionIdRef.current = null;
        pendingDimSourceEntityIdRef.current = null;
        pendingDimensionPlacementRef.current = false;
        dimensionToolFirstLineRef.current = null;
        setDimensionToolFirstLine(null);
      });
  }

  function dimSelectCircle(entityId: string) {
    const dimensionId = `dim-circle-${entityId}`;
    handleDimensionClick(dimensionId);
    // Stage for possible two-entity follow-up pick
    dimensionToolFirstLineRef.current = entityId;
    setDimensionToolFirstLine(entityId);
  }

  function dimCreateLine(entityId: string) {
    const dimensionId = `dim-line-${entityId}`;
    pendingDimensionIdRef.current = dimensionId;
    pendingDimSourceEntityIdRef.current = entityId;
    pendingDimensionPlacementRef.current = true;
    // Stage for possible two-entity follow-up pick (angle / distance).
    dimensionToolFirstLineRef.current = entityId;
    setDimensionToolFirstLine(entityId);
    void addSketchLineLengthDimensionRef
      .current(entityId)
      .catch(() => {
        pendingDimensionIdRef.current = null;
        pendingDimSourceEntityIdRef.current = null;
        pendingDimensionPlacementRef.current = false;
        dimensionToolFirstLineRef.current = null;
        setDimensionToolFirstLine(null);
      });
  }

  function dimSelectLine(entityId: string) {
    const dimensionId = `dim-line-${entityId}`;
    handleDimensionClick(dimensionId);
    // Stage for possible two-entity follow-up pick
    dimensionToolFirstLineRef.current = entityId;
    setDimensionToolFirstLine(entityId);
  }

  function dimCreatePolygon(entityId: string) {
    const dimensionId = `dim-polygon-${entityId}`;
    pendingDimensionIdRef.current = dimensionId;
    pendingDimSourceEntityIdRef.current = entityId;
    pendingDimensionPlacementRef.current = true;
    void addSketchPolygonRadiusDimensionRef
      .current(entityId)
      .catch(() => {
        pendingDimensionIdRef.current = null;
        pendingDimSourceEntityIdRef.current = null;
        pendingDimensionPlacementRef.current = false;
      });
  }

  function dimSelectPolygon(entityId: string) {
    const dimensionId = `dim-polygon-${entityId}`;
    handleDimensionClick(dimensionId);
    // Stage for possible two-entity follow-up pick
    dimensionToolFirstLineRef.current = entityId;
    setDimensionToolFirstLine(entityId);
  }

  function dimCreateAngleOrDistance(
    firstEntityId: string,
    secondEntityId: string,
  ) {
    if (
      firstEntityId.startsWith("line-") &&
      sketchLinesShareEndpoint(firstEntityId, secondEntityId)
    ) {
      pendingDimensionPlacementRef.current = true;
      pendingDimSourceEntityIdRef.current = null;
      const isReflex = pendingAngleIsReflexRef.current;
      const reflexAngle = pendingReflexAngleRef.current;
      pendingAngleIsReflexRef.current = false;
      void addSketchAngleDimensionRef
        .current(firstEntityId, secondEntityId)
        .then(() => {
          if (isReflex) {
            const ids = [firstEntityId, secondEntityId].sort();
            const dimId = `dim-angle-${ids[0]}-${ids[1]}`;
            void updateSketchDimensionRef.current(dimId, reflexAngle);
          }
        })
        .catch(() => {
          pendingDimensionPlacementRef.current = false;
          pendingRelationPlacementLabelRef.current = null;
          pendingRelationPlacementMatchRef.current = null;
        });
    } else {
      pendingDimensionPlacementRef.current = true;
      pendingDimSourceEntityIdRef.current = null;
      void addSketchDistanceDimensionRef
        .current(firstEntityId, secondEntityId)
        .catch(() => {
          pendingDimensionPlacementRef.current = false;
          pendingRelationPlacementLabelRef.current = null;
          pendingRelationPlacementMatchRef.current = null;
        });
    }
  }

  function readDimensionPreviewFilter() {
    const filter = readStoredFilter();
    return altHeldRef.current ? invertSelectionFilter(filter) : filter;
  }

  function renderDimensionRelationPreview(
    relation: DimensionRelationPreview,
    dimension: SketchDimensionScene,
  ) {
    clearPreviewDimension();
    const sketchGroup = sketchGroupRef.current;
    if (!sketchGroup) {
      dimensionRelationPreviewRef.current = null;
      return;
    }
    const preview = buildSketchDimensionObject(dimension, config.displayUnits, {
      variant: "muted-preview",
      pickable: false,
    });
    hideRelationPreviewDimension(unaryDimensionIdForEntity(relation.firstEntityId));
    previewDimensionRef.current = preview;
    dimensionRelationPreviewRef.current = relation;
    dimensionRelationPreviewLabelRef.current = dimension.labelPosition;
    sketchGroup.add(preview.line);
    sketchGroup.add(preview.label);
  }

  function updateDimensionRelationPreview(cursor: [number, number]) {
    clearPreviewDimension();
    dimensionRelationPreviewRef.current = null;
    const firstEntityId = dimensionToolFirstLineRef.current;
    const params = sketchLinesRef.current;
    const filter = readDimensionPreviewFilter();
    const preview = buildDimensionRelationPreview({
      firstEntityId,
      activeSketchTool: activeSketchToolRef.current,
      sketchParameters: params,
      filter,
      cursor,
      planeId: activeSketchPlaneIdRef.current ?? "ref-plane-xy",
      planeFrame: activeSketchPlaneFrameRef.current,
    });
    if (!preview) {
      return null;
    }
    if (preview.anglePreview) {
      pendingAngleIsReflexRef.current = preview.anglePreview.isReflex;
      if (preview.anglePreview.isReflex) {
        pendingReflexAngleRef.current = preview.anglePreview.angle;
      }
    }
    renderDimensionRelationPreview(preview.relation, preview.dimension);
    return preview.relation;
  }

  function commitDimensionRelationPreview() {
    const relation = dimensionRelationPreviewRef.current;
    if (!relation) {
      return false;
    }
    const pointerEvent = lastPointerEventRef.current;
    const renderer = rendererRef.current;
    const camera = cameraRef.current;
    const sketchPlaneId = activeSketchPlaneIdRef.current;
    const clickedSketchPoint =
      pointerEvent && renderer && camera && sketchPlaneId
        ? resolveSketchPlanePoint(
            pointerEvent,
            renderer,
            camera,
            sketchPlaneId,
            activeSketchPlaneFrameRef.current,
          )
        : null;
    pendingRelationPlacementLabelRef.current =
      relation.kind === "line_angle"
        ? clickedSketchPoint?.world ?? dimensionRelationPreviewLabelRef.current
        : clickedSketchPoint?.world ?? dimensionRelationPreviewLabelRef.current;
    pendingRelationPlacementMatchRef.current = relation;
    clearPreviewDimension();
    dimensionRelationPreviewRef.current = null;
    const unaryDimensionId = unaryDimensionIdForEntity(relation.firstEntityId);
    if (unaryDimensionId) {
      void deleteSketchDimensionRef.current(unaryDimensionId);
    }
    pendingDimensionIdRef.current = null;
    pendingDimSourceEntityIdRef.current = null;
    pendingDimensionPlacementRef.current = false;
    dimensionLabelDragRef.current = null;
    dimensionPlacementOriginalPositionRef.current = null;
    controlsRef.current && (controlsRef.current.enabled = true);
    setCanvasCursor("");
    dimensionToolFirstLineRef.current = null;
    setDimensionToolFirstLine(null);
    dimensionToolFirstPointRef.current = null;
    dimCreateAngleOrDistance(relation.firstEntityId, relation.targetEntityId);
    schedulePendingRelationPlacementRetry();
    return true;
  }

  function selectedDimensionMatchesPendingRelation(
    dimension: SketchDimensionScene,
    relation: DimensionRelationPreview,
  ) {
    const params = sketchLinesRef.current;
    const coreDimension = params?.dimensions.find(
      (candidate) => candidate.dimension_id === dimension.dimensionId,
    );
    if (!coreDimension) {
      return false;
    }
    const isSamePair =
      (coreDimension.entity_id === relation.firstEntityId &&
        coreDimension.secondary_entity_id === relation.targetEntityId) ||
      (coreDimension.entity_id === relation.targetEntityId &&
        coreDimension.secondary_entity_id === relation.firstEntityId);
    if (!isSamePair) {
      return false;
    }
    if (relation.kind === "parallel_line_distance") {
      return coreDimension.kind === "line_line_distance";
    }
    if (relation.kind === "line_angle") {
      return coreDimension.kind === "angle";
    }
    if (relation.kind === "circle_center_distance") {
      return coreDimension.kind === "circle_center_distance";
    }
    return coreDimension.kind === "circle_line_distance";
  }

  function pendingRelationDimension(
    relation: DimensionRelationPreview,
    dimensions: SketchDimensionScene[],
  ) {
    const params = sketchLinesRef.current;
    if (!params) {
      return null;
    }
    const coreDimension = params.dimensions.find((candidate) => {
      const isSamePair =
        (candidate.entity_id === relation.firstEntityId &&
          candidate.secondary_entity_id === relation.targetEntityId) ||
        (candidate.entity_id === relation.targetEntityId &&
          candidate.secondary_entity_id === relation.firstEntityId);
      if (!isSamePair) {
        return false;
      }
      if (relation.kind === "parallel_line_distance") {
        return candidate.kind === "line_line_distance";
      }
      if (relation.kind === "line_angle") {
        return candidate.kind === "angle";
      }
      if (relation.kind === "circle_center_distance") {
        return candidate.kind === "circle_center_distance";
      }
      return candidate.kind === "circle_line_distance";
    });
    if (!coreDimension) {
      return null;
    }
    return (
      dimensions.find(
        (dimension) => dimension.dimensionId === coreDimension.dimension_id,
      ) ?? null
    );
  }

  function stopPendingRelationPlacementRetry() {
    if (pendingRelationPlacementRetryRef.current !== null) {
      window.cancelAnimationFrame(pendingRelationPlacementRetryRef.current);
      pendingRelationPlacementRetryRef.current = null;
    }
  }

  function startPendingRelationPlacementIfReady() {
    if (
      !pendingDimensionPlacementRef.current ||
      activeSketchToolRef.current !== "dimension"
    ) {
      return true;
    }
    const relation = pendingRelationPlacementMatchRef.current;
    if (!relation) {
      return true;
    }
    const placementDimension = pendingRelationDimension(
      relation,
      displayedSketchDimensionsRef.current,
    );
    if (!placementDimension) {
      return false;
    }
    pendingRelationPlacementMatchRef.current = null;
    pendingDimensionIdRef.current = null;
    pendingDimensionPlacementRef.current = false;
    pendingDimSourceEntityIdRef.current = null;
    beginDimensionPlacement(placementDimension);
    return true;
  }

  function schedulePendingRelationPlacementRetry() {
    stopPendingRelationPlacementRetry();
    let attempts = 0;
    const tick = () => {
      if (startPendingRelationPlacementIfReady()) {
        pendingRelationPlacementRetryRef.current = null;
        return;
      }
      attempts += 1;
      if (attempts >= 90) {
        pendingRelationPlacementRetryRef.current = null;
        return;
      }
      pendingRelationPlacementRetryRef.current = window.requestAnimationFrame(tick);
    };
    pendingRelationPlacementRetryRef.current = window.requestAnimationFrame(tick);
  }

  function dimCreatePointDistance(pointAId: string, pointBId: string) {
    pendingDimensionIdRef.current =
      `dim-point-distance-${pointAId}-${pointBId}`;
    pendingDimensionPlacementRef.current = true;
    pendingDimSourceEntityIdRef.current = null;
    void addSketchPointDistanceDimensionRef
      .current(pointAId, pointBId)
      .catch(() => {
        pendingDimensionIdRef.current = null;
        pendingDimensionPlacementRef.current = false;
      });
  }

  function isProjectedCircleDimension(dimensionId: string) {
    const sketch = sketchFeature?.sketch_parameters;
    if (!sketch) {
      return false;
    }
    const dimension = sketch.dimensions.find(
      (candidate) => candidate.dimension_id === dimensionId,
    );
    if (!dimension || dimension.kind !== "circle_radius") {
      return false;
    }
    return sketch.projections.some((projection) =>
      projection.generated_circle_ids.includes(dimension.entity_id),
    );
  }

  // Track which dimension was last clicked, so a second click on the
  // same dimension opens the editor (click to select, re-click to edit).
  const lastClickedDimensionRef = useRef<string | null>(null);

  function handleDimensionClick(dimensionId: string) {
    if (isProjectedCircleDimension(dimensionId)) {
      void selectSketchDimensionRef.current(dimensionId);
      return;
    }

    // Check BOTH the store (accurate after IPC round-trip) AND the
    // local ref (accurate for rapid re-clicks before IPC completes).
    const isAlreadySelected =
      selectedSketchDimension?.dimensionId === dimensionId ||
      lastClickedDimensionRef.current === dimensionId;

    if (isAlreadySelected) {
      // Second click on the already-selected dimension → open editor
      suppressNextDimensionEditorOpenRef.current = false;
      setIsDimensionEditorOpen(true);
    } else {
      // First click → select it (highlight), no editor
      suppressNextDimensionEditorOpenRef.current = true;
      setIsDimensionEditorOpen(false);
      void selectSketchDimensionRef.current(dimensionId);
    }

    lastClickedDimensionRef.current = dimensionId;
  }

  function dimensionDisplayValue(
    dimension: SketchDimensionScene,
    coreValue: number,
  ) {
    if (dimension.kind === "angle" || dimension.kind === "line_angle") {
      return coreValue * (180 / Math.PI);
    }
    if (dimension.kind === "circle_radius") {
      // Per-dimension display_as controls radius vs diameter display.
      // "" (default) = diameter, "radius" = show raw radius.
      const displayAs = resolveDimensionDisplayAs(dimension.dimensionId);
      return displayAs === "radius" ? coreValue : coreValue * 2;
    }
    return coreValue;
  }

  function dimensionCoreValue(
    dimension: SketchDimensionScene,
    displayValue: number,
  ) {
    if (dimension.kind === "angle" || dimension.kind === "line_angle") {
      return displayValue * (Math.PI / 180);
    }
    if (dimension.kind === "circle_radius") {
      // Per-dimension display_as controls radius vs diameter conversion.
      const displayAs = resolveDimensionDisplayAs(dimension.dimensionId);
      return displayAs === "radius" ? displayValue : displayValue / 2;
    }
    return displayValue;
  }

  function formattedDimensionDisplayValue(
    dimension: SketchDimensionScene,
    coreValue: number,
  ) {
    const displayVal = dimensionDisplayValue(dimension, coreValue);
    // Convert mm to user's display unit for non-angle dimensions
    const isAngleKind = dimension.kind === "angle" ||
      dimension.kind === "line_angle";
    const adjusted =
      !isAngleKind
        ? mmToDisplay(displayVal, config.displayUnits)
        : displayVal;
    return String(parseFloat(adjusted.toFixed(2)));
  }

  function setCanvasCursor(cursor: string) {
    const canvas = rendererRef.current?.domElement as
      | HTMLCanvasElement
      | undefined;
    if (canvas) {
      canvas.style.cursor = cursor;
    }
  }

  function getDimensionPlacementAxis(dimension: SketchDimensionScene) {
    if (dimension.kind === "angle" || dimension.kind === "line_angle") {
      return angleDimensionFrame(dimension)?.bisector ?? null;
    }

    const extensionAxis = new THREE.Vector3(...dimension.dimensionStart).sub(
      new THREE.Vector3(...dimension.anchorStart),
    );
    if (extensionAxis.lengthSq() > 1e-8) {
      return extensionAxis.normalize();
    }

    const sketchPlaneId = activeSketchPlaneIdRef.current;
    const dimensionDirection = new THREE.Vector3(...dimension.dimensionEnd).sub(
      new THREE.Vector3(...dimension.dimensionStart),
    );
    if (!sketchPlaneId || dimensionDirection.lengthSq() <= 1e-8) {
      return null;
    }

    const planeNormal = getSketchGridFrame(
      sketchPlaneId,
      activeSketchPlaneFrameRef.current,
    ).normal;
    const placementAxis = planeNormal.cross(dimensionDirection).normalize();
    return placementAxis.lengthSq() > 1e-8 ? placementAxis : null;
  }

  function setDimensionLabelPosition(
    dimensionId: string,
    position: [number, number, number],
  ) {
    dimensionLabelPositionsRef.current = {
      ...dimensionLabelPositionsRef.current,
      [dimensionId]: position,
    };
    setDimensionLabelPositions((current) => ({
      ...current,
      [dimensionId]: position,
    }));
  }

  function persistDimensionLabelPosition(
    dimensionId: string,
    position: [number, number, number] | undefined,
  ) {
    if (!position) {
      return;
    }
    const labelLocal = worldPointToSketchLocal(
      position,
      activeSketchPlaneIdRef.current,
      activeSketchPlaneFrameRef.current,
    );
    if (!labelLocal) {
      return;
    }
    void updateSketchDimensionLabelPositionRef.current(
      dimensionId,
      labelLocal[0],
      labelLocal[1],
    );
  }

  function setAngleDimensionDragRadius(
    dimension: SketchDimensionScene,
    dimensionId: string,
    worldPoint: readonly [number, number, number],
  ) {
    const frame = angleDimensionFrame(dimension);
    if (!frame) {
      return;
    }
    const radius = clampAngleRadius(
      new THREE.Vector3(worldPoint[0], worldPoint[1], worldPoint[2]).distanceTo(
        frame.pivot,
      ),
    );
    angleDragRadiiRef.current = {
      ...angleDragRadiiRef.current,
      [dimensionId]: radius,
    };
    setAngleDragRadii((prev) => ({
      ...prev,
      [dimensionId]: radius,
    }));
  }

  function persistAngleDimensionLabelRadius(
    dimensionId: string,
    dragRadius: number,
  ) {
    const dragged = displayedSketchDimensionsRef.current.find(
      (dimension) => dimension.dimensionId === dimensionId,
    );
    const frame = dragged ? angleDimensionFrame(dragged) : null;
    if (!frame) {
      return;
    }
    const labelWorld = frame.pivot
      .clone()
      .add(frame.bisector.clone().multiplyScalar(dragRadius));
    persistDimensionLabelPosition(dimensionId, [
      labelWorld.x,
      labelWorld.y,
      labelWorld.z,
    ]);
  }

  function persistDimensionDragLabelPosition(
    dimensionDrag: DimensionLabelDragState,
  ) {
    const dragRadius = angleDragRadiiRef.current[dimensionDrag.dimensionId];
    if (dragRadius !== undefined) {
      persistAngleDimensionLabelRadius(dimensionDrag.dimensionId, dragRadius);
      return;
    }
    persistDimensionLabelPosition(
      dimensionDrag.dimensionId,
      dimensionLabelPositionsRef.current[dimensionDrag.dimensionId],
    );
  }

  function finishDimensionPlacement() {
    const dimensionDrag = dimensionLabelDragRef.current;
    if (!dimensionDrag?.isPlacement) {
      return false;
    }
    persistDimensionDragLabelPosition(dimensionDrag);
    clearPreviewDimension();
    dimensionLabelDragRef.current = null;
    dimensionPlacementOriginalPositionRef.current = null;
    controlsRef.current && (controlsRef.current.enabled = true);
    setCanvasCursor("");
    return true;
  }

  function cancelDimensionPlacement() {
    const dimensionDrag = dimensionLabelDragRef.current;
    if (!dimensionDrag?.isPlacement) {
      return false;
    }
    clearPreviewDimension();
    // Clear angle drag radius if active
    if (angleDragRadiiRef.current[dimensionDrag.dimensionId] !== undefined) {
      const next = { ...angleDragRadiiRef.current };
      delete next[dimensionDrag.dimensionId];
      angleDragRadiiRef.current = next;
      setAngleDragRadii(next);
    }
    const originalPosition = dimensionPlacementOriginalPositionRef.current;
    if (originalPosition) {
      setDimensionLabelPositions((current) => ({
        ...current,
        [dimensionDrag.dimensionId]: originalPosition,
      }));
    }
    dimensionLabelDragRef.current = null;
    dimensionPlacementOriginalPositionRef.current = null;
    controlsRef.current && (controlsRef.current.enabled = true);
    setCanvasCursor("");
    return true;
  }

  function beginDimensionPlacement(dimension: SketchDimensionScene) {
    const renderer = rendererRef.current;
    const camera = cameraRef.current;
    const sketchPlaneId = activeSketchPlaneIdRef.current;
    const controls = controlsRef.current;
    const pointerEvent = lastPointerEventRef.current;
    if (!renderer || !camera || !sketchPlaneId || !controls || !pointerEvent) {
      return;
    }
    const placement = buildDimensionPlacementStart({
      event: pointerEvent,
      renderer,
      camera,
      activeSketchPlaneId: sketchPlaneId,
      activeSketchPlaneFrame: activeSketchPlaneFrameRef.current,
      dimension,
      relationPosition: pendingRelationPlacementLabelRef.current,
      getDimensionPlacementAxis,
    });
    if (!placement) {
      return;
    }

    if (placement.isAngleKind && placement.angleWorldPoint) {
      setAngleDimensionDragRadius(
        dimension,
        dimension.dimensionId,
        placement.angleWorldPoint,
      );
    }
    pendingRelationPlacementLabelRef.current = null;
    dimensionPlacementOriginalPositionRef.current = placement.originalPosition;
    if (!placement.isAngleKind) {
      setDimensionLabelPosition(dimension.dimensionId, placement.nextPosition);
    }
    dimensionLabelDragRef.current = placement.dragState;
    controls.enabled = false;
    setCanvasCursor("grabbing");
  }

  function sketchLinesShareEndpoint(firstLineId: string, secondLineId: string) {
    const params = sketchLinesRef.current;
    const first = params?.lines.find((line) => line.line_id === firstLineId);
    const second = params?.lines.find((line) => line.line_id === secondLineId);
    if (!first || !second) {
      return false;
    }
    return relationPreviewLinesShareEndpoint(first, second);
  }

  function cancelActiveSketchDraft() {
    if (armedSketchConstraintRef.current) {
      cancelSketchConstraintRef.current();
      return;
    }
    lineDraftStartRef.current = null;
    arcSecondPointRef.current = null;
    rectSecondPointRef.current = null;
    circleSecondPointRef.current = null;
    clearPreviewLine();
    clearPreviewCircle();
    clearPreviewArc();
    clearPreviewDimension();
    clearDraftDimensionSession();
    setSketchSnapLabel(null);
    setConstraintPreview(null);
    dragSnapResultRef.current = null;
    setHoveredSketchEntity(null);
    setHoveredSketchPoint(null);
    void setSketchToolRef.current("select");
  }

  function renderDraftPreview(session: DraftDimensionSession) {
    const sketchGroup = sketchGroupRef.current;
    if (!sketchGroup || !activeSketchPlaneId) {
      return;
    }

    clearPreviewLine();
    clearPreviewCircle();
    clearPreviewArc();
    clearPreviewDimension();
    renderDraftPointerPreview({
      activeSketchTool: session.tool,
      activeSketchPlaneId,
      activeSketchPlaneFrame,
      draftStart: session.start,
      draftPreviewLocal: session.current,
      sketchGroup,
      arcToolMode: arcToolModeRef.current,
      circleToolMode: circleToolModeRef.current,
      rectangleToolMode: rectangleToolModeRef.current,
      arcSecondPoint: arcSecondPointRef.current,
      circleSecondPoint: circleSecondPointRef.current,
      rectSecondPoint: rectSecondPointRef.current,
      isConstruction: sketchToolConstructionRef.current,
      previewLineRef,
      previewCircleRef,
      previewArcRef,
      clearPreviewLine,
      clearPreviewCircle,
      clearPreviewArc,
      clearPreviewDimension,
      renderCircleDraftDimension,
    });
  }

  function renderCircleDraftDimension(
    sketchGroup: THREE.Group,
    center: [number, number],
    edge: [number, number],
  ) {
    if (!activeSketchPlaneId) {
      return;
    }
    const radius = distanceBetweenPoints(center, edge);
    const dx = edge[0] - center[0];
    const dy = edge[1] - center[1];
    const length = Math.hypot(dx, dy);
    if (radius <= 0.001 || length <= 1e-6) {
      return;
    }

    const ux = dx / length;
    const uy = dy / length;
    const dimensionStartLocal: [number, number] = [
      center[0] - ux * radius,
      center[1] - uy * radius,
    ];
    const dimensionEndLocal: [number, number] = [
      center[0] + ux * radius,
      center[1] + uy * radius,
    ];
    const labelLocal: [number, number] = [
      center[0] + ux * (radius + 4),
      center[1] + uy * (radius + 4),
    ];
    const draftDimension = buildSketchDimensionObject({
      dimensionId: "preview-circle-diameter",
      planeId: activeSketchPlaneId,
      kind: "circle_radius",
      entityId: "preview-circle",
      label: `D ${formatDraftDimension(radius * 2)} mm`,
      rawValue: radius * 2,
      unitSuffix: "mm",
      isSelected: false,
      anchorStart: toWorldPoint(
        activeSketchPlaneId,
        dimensionStartLocal,
        activeSketchPlaneFrame,
      ),
      anchorEnd: toWorldPoint(
        activeSketchPlaneId,
        dimensionEndLocal,
        activeSketchPlaneFrame,
      ),
      dimensionStart: toWorldPoint(
        activeSketchPlaneId,
        dimensionStartLocal,
        activeSketchPlaneFrame,
      ),
      dimensionEnd: toWorldPoint(
        activeSketchPlaneId,
        dimensionEndLocal,
        activeSketchPlaneFrame,
      ),
      labelPosition: toWorldPoint(
        activeSketchPlaneId,
        labelLocal,
        activeSketchPlaneFrame,
      ),
    });
    previewDimensionRef.current = draftDimension;
    sketchGroup.add(draftDimension.line);
    sketchGroup.add(draftDimension.label);
  }

  function updateDraftSessionFromPoint(point: [number, number]) {
    const session = draftDimensionSessionRef.current;
    if (!session) {
      return;
    }
    const next = updateDraftSessionCurrent(session, point);
    draftDimensionSessionRef.current = next;
    setDraftDimensionSession(next);
    if (!next.lockedFields[next.activeField]) {
      focusDraftField(next.activeField);
    }
  }

  function applyDraftDimensionField(
    session: DraftDimensionSession,
    field: DraftDimensionField,
    rawValue: string,
    lockField = true,
  ): DraftDimensionSession {
    return applyDraftDimensionFieldValue(session, field, rawValue, lockField);
  }

  async function commitDraftDimensionSession(
    session = draftDimensionSessionRef.current,
  ) {
    if (!session) {
      return;
    }
    const [startX, startY] = session.start;
    const [endX, endY] = session.current;
    clearPreviewLine();
    clearPreviewCircle();
    clearPreviewArc();
    clearPreviewDimension();
    lineDraftStartRef.current = null;
    scheduleDimensionDeletion(session.tool, session);
    scheduleDraftDimensionExpressionUpdate(session.tool);
    clearDraftDimensionSession();
    suppressDimensionEditorAfterSketchCommit();
    rendererRef.current?.domElement.focus();

    if (session.tool === "rectangle") {
      if (rectangleToolModeRef.current === "three_point") {
        // 3-point mode can't commit from drag; handled in snap handler.
        return;
      }
      const rectStartX =
        rectangleToolModeRef.current === "center_point"
          ? 2 * startX - endX
          : startX;
      const rectStartY =
        rectangleToolModeRef.current === "center_point"
          ? 2 * startY - endY
          : startY;
      await addSketchRectangleRef.current(
        rectStartX,
        rectStartY,
        endX,
        endY,
        sketchToolConstructionRef.current,
      );
      return;
    }
    if (session.tool === "circle") {
      const circleMode = circleToolModeRef.current;
      let cx = startX;
      let cy = startY;
      let r = distanceBetweenPoints(session.start, session.current);
      if (circleMode === "two_point") {
        // 2-point circle: start/end are diameter endpoints
        cx = (startX + endX) / 2;
        cy = (startY + endY) / 2;
        r = distanceBetweenPoints(session.start, session.current) / 2;
      }
      // 3-point and tangent modes can't commit from a 2-click drag
      if (circleMode === "three_point" || circleMode === "tangent_two_lines" || circleMode === "tangent_three_lines") {
        return;
      }
      pendingCircleDimensionPlacementRef.current = {
        fromCircleCount: sketchFeature?.sketch_parameters?.circles.length ?? 0,
        center: [cx, cy],
        end: session.current,
      };
      await addSketchCircleRef.current(
        cx,
        cy,
        r,
        sketchToolConstructionRef.current,
      );
      return;
    }
    if (session.tool === "polygon") {
      void addSketchPolygonRef.current(
        polygonSidesRef.current,
        polygonToolModeRef.current,
        startX,
        startY,
        endX,
        endY,
        sketchToolConstructionRef.current,
      );
      return;
    }
    await addSketchLineRef.current(
      startX,
      startY,
      endX,
      endY,
      sketchToolConstructionRef.current,
    );
  }

	  function resolveSnappedSketchPoint(
	    rawPoint: {
	      local: [number, number];
      world: [number, number, number];
    },
    draftStartLocal?: [number, number] | null,
  ) {
    // Read filter from localStorage (instant, no IPC round trip).
    const localFilter: SelectionFilter = readStoredFilter();
    const effectiveFilter = altHeldRef.current
      ? invertSelectionFilter(localFilter)
      : localFilter;

    const gridSnapEnabled = effectiveFilter.snap_grid;

    // Apply grid snap first — all subsequent geometric snaps resolve
    // against grid-aligned coordinates so the user gets both.
    const worldUnitsPerPixel =
      cameraRef.current && rendererRef.current
        ? getOrthographicViewHeight(cameraRef.current) /
            Math.max(rendererRef.current.domElement.clientHeight, 1)
        : 1;
    const gridResult = snapRawPointToGrid(rawPoint, worldUnitsPerPixel, gridSnapEnabled);
    let gridDidSnap = gridResult.snapped;
    if (gridResult.snapped) {
      rawPoint.local = gridResult.point.local;
      rawPoint.world = gridResult.point.world;
    }

    const closestStatic = closestStaticSnapCandidate(
      sketchSnapCandidatesRef.current,
      rawPoint.local,
      effectiveFilter,
      distanceBetweenPoints,
    );

    // Endpoint / midpoint candidates win immediately (high priority).
    if (closestStatic && closestStatic.distance <= SKETCH_SNAP_DISTANCE) {
      if (closestStatic.candidate.kind === "endpoint" ||
          closestStatic.candidate.kind === "midpoint") {
        return previewPointFromStaticCandidate({
          candidate: closestStatic.candidate,
          world: toWorldPoint(
            activeSketchPlaneId ?? "ref-plane-xy",
            closestStatic.candidate.local,
            activeSketchPlaneFrame,
          ),
          includeEndpointMetadata: true,
        });
      }
    }

    // --- Dynamic snap computation ---
    // These depend on cursor position relative to the draft start and
    // cannot be pre-computed as static candidates. They compete with
    // each other and with non-endpoint/midpoint static candidates.

    const params = sketchLinesRef.current;
    const AXIS_ANGLE_THRESHOLD = 5 * Math.PI / 180; // 5 degrees
    const PARALLEL_ANGLE_THRESHOLD = 8 * Math.PI / 180; // 8 degrees (wider for parallel)
    const bestDynamic = params
      ? dynamicSnapCandidate({
          lines: params.lines,
          circles: params.circles,
          filter: effectiveFilter,
          draftStart: draftStartLocal,
          cursor: rawPoint.local,
          threshold: SKETCH_SNAP_DISTANCE,
          axisAngleThresholdRadians: AXIS_ANGLE_THRESHOLD,
          parallelAngleThresholdRadians: PARALLEL_ANGLE_THRESHOLD,
          labels: {
            axisLockHorizontal: translate("snap.axisLockHorizontal"),
            axisLockVertical: translate("snap.axisLockVertical"),
            onLine: translate("snap.onLine"),
            tangent: translate("snap.tangent"),
            perpendicular: translate("snap.perpendicular"),
            parallel: translate("snap.parallel"),
          },
        })
      : null;

    // If a dynamic snap fired and is closer than any static candidate,
    // return the dynamic result.
    if (bestDynamic && bestDynamic.distance <= SKETCH_SNAP_DISTANCE) {
      if (!closestStatic || bestDynamic.distance < closestStatic.distance) {
        return {
          local: bestDynamic.local,
          world: toWorldPoint(
            activeSketchPlaneId ?? "ref-plane-xy",
            bestDynamic.local,
            activeSketchPlaneFrame,
          ),
          snapLabel: bestDynamic.snapLabel,
          snapMidpointHostLineId: null,
          snapMidpointT: null,
          snapPerpendicularHostLineId: bestDynamic.snapPerpendicularHostLineId,
          snapEndpointHostLineId: null,
          snapLineBodyHostLineId: bestDynamic.snapLineBodyHostLineId,
          snapLineBodyT: bestDynamic.snapLineBodyT,
          snapAxisLock: bestDynamic.snapAxisLock,
          snapTangentCircleId: bestDynamic.snapTangentCircleId,
          snapParallelHostLineId: bestDynamic.snapParallelHostLineId,
        } satisfies SketchPreviewPoint;
      }
    }

    // Fallback: if a lower-priority static candidate (center, quadrant,
    // intersection, etc.) matched but dynamic snaps didn't fire, return
    // the static candidate now.
    if (closestStatic && closestStatic.distance <= SKETCH_SNAP_DISTANCE) {
      return previewPointFromStaticCandidate({
        candidate: closestStatic.candidate,
        world: toWorldPoint(
          activeSketchPlaneId ?? "ref-plane-xy",
          closestStatic.candidate.local,
          activeSketchPlaneFrame,
        ),
        includeEndpointMetadata: false,
      });
    }

    return {
      ...rawPoint,
      snapLabel: gridDidSnap ? translate("snap.grid") : null,
      snapMidpointHostLineId: null,
      snapPerpendicularHostLineId: null,
      snapEndpointHostLineId: null,
      snapLineBodyHostLineId: null,
      snapLineBodyT: null,
      snapAxisLock: null,
      snapTangentCircleId: null,
      snapParallelHostLineId: null,
    } satisfies SketchPreviewPoint;
  }

  function capturePendingLineCommitRelations(
    sketchPoint: ReturnType<typeof resolveSnappedSketchPoint>,
  ) {
    const relations = lineCommitRelations({
      sketchPoint,
      fromLineCount: sketchLineCountRef.current,
      startMidpointHostLineId: draftStartMidpointHostRef.current,
      startLineBodyHost: draftStartLineBodyHostRef.current,
    });
    pendingMidpointAnchorRef.current = relations.midpointAnchor;
    pendingPerpendicularConstraintRef.current = relations.perpendicularConstraint;
    pendingPointLineAnchorRef.current = relations.pointLineAnchor;
    pendingAxisConstraintRef.current = relations.axisConstraint;
    pendingTangentConstraintRef.current = relations.tangentConstraint;
    pendingParallelConstraintRef.current = relations.parallelConstraint;
    return relations;
  }

  function syncPrimitiveVisuals() {
    for (const [primitiveId, visual] of primitiveVisualsRef.current.entries()) {
      const state = primitiveStatesRef.current.get(primitiveId);
      if (!state) {
        continue;
      }

      applyPrimitiveVisualState(visual, state);
    }
  }

  function syncReferencePlaneVisuals() {
    for (const [
      referenceId,
      visual,
    ] of referencePlaneVisualsRef.current.entries()) {
      const state = referencePlaneStatesRef.current.get(referenceId);
      if (!state) {
        continue;
      }

      applyReferencePlaneVisualState(visual, state);
    }
  }

  function syncSolidFaceVisuals() {
    for (const [faceId, visual] of solidFaceVisualsRef.current.entries()) {
      const state = solidFaceStatesRef.current.get(faceId);
      if (!state) {
        continue;
      }

      applySolidFaceVisualState(visual, state);
    }
  }

  function syncSketchProfileVisuals() {
    for (const [
      profileId,
      visual,
    ] of sketchProfileVisualsRef.current.entries()) {
      const state = sketchProfileStatesRef.current.get(profileId);
      if (!state) {
        continue;
      }

      applySketchProfileVisualState(visual, state);
    }
  }

  function setHoveredFace(faceId: string | null) {
    let changed = false;

    for (const [id, state] of solidFaceStatesRef.current.entries()) {
      const nextHovered = id === faceId;
      if (state.isHovered !== nextHovered) {
        solidFaceStatesRef.current.set(id, {
          ...state,
          isHovered: nextHovered,
        });
        changed = true;
      }
    }

    if (changed) {
      syncSolidFaceVisuals();
    }
  }

  function setHoveredSketchProfile(profileId: string | null) {
    let changed = false;

    for (const [id, state] of sketchProfileStatesRef.current.entries()) {
      const nextHovered = id === profileId;
      if (state.isHovered !== nextHovered) {
        sketchProfileStatesRef.current.set(id, {
          ...state,
          isHovered: nextHovered,
        });
        changed = true;
      }
    }

    if (changed) {
      syncSketchProfileVisuals();
    }
  }

  const hoveredSketchEntityIdRef = useRef<string | null>(null);
  function paintSketchEntityMaterials() {
    // Use the stable ref (updated from useMemo on viewport change)
    // so hover never sees an empty DOF map when viewport is stale.
    const dofMap = dofMapRef.current;
    for (const object of sketchEntityObjectsRef.current) {
      const id = object.userData.sketchEntityId as string | undefined;
      const isSelected = object.userData.isSelected === true;
      const isProjected = object.userData.sketchEntityIsProjected === true;
      const isHovered =
        id !== undefined && id === hoveredSketchEntityIdRef.current;
      const material = object.material as
        | THREE.LineBasicMaterial
        | THREE.LineDashedMaterial;
      if (isSelected) {
        material.color.set(themeColor("--color-primary-edge-active", "#c3f5ff"));
      } else if (isHovered) {
        material.color.set(themeColor("--color-tertiary-plane-edge-hover", "#fff2b2"));
      } else if (isProjected) {
        material.color.set(themeColor("--cad-sketch-projected", "#ff4fd8"));
      } else if (id && dofMap.has(id)) {
        const status = dofMap.get(id)!;
        if (status === "full") {
          material.color.set(0x8899aa);
        } else {
          material.color.set(0xff4444);
        }
      } else {
        material.color.set(themeColor("--color-tertiary-plane-fill", "#fff7c0"));
      }
      material.opacity = isSelected || isHovered ? 1 : 0.98;
      material.linewidth = isSelected ? 3 : isHovered ? 2.5 : 1;
    }
  }

  function setHoveredSketchEntity(entityId: string | null) {
    if (hoveredSketchEntityIdRef.current === entityId) {
      return;
    }
    hoveredSketchEntityIdRef.current = entityId;
    paintSketchEntityMaterials();
    paintDofStatusColors();
  }

  const hoveredSketchPointIdRef = useRef<string | null>(null);

  /** No-op — DOF colors are now applied in paintSketchEntityMaterials directly. */
  function paintDofStatusColors() {}

  function paintSketchPointMaterials() {
    for (const mesh of sketchPointObjectsRef.current) {
      const id = mesh.userData.sketchPointId as string | undefined;
      const kind = mesh.userData.sketchPointKind as string | undefined;
      const isSelected = mesh.userData.isSelected === true;
      const isHovered =
        id !== undefined && id === hoveredSketchPointIdRef.current;
      const material = mesh.material as THREE.MeshBasicMaterial;
      material.color.set(
        isSelected
          ? themeColor("--color-primary-edge-active", "#c3f5ff")
          : isHovered
            ? themeColor("--color-tertiary-plane-edge-hover", "#fff2b2")
            : kind === "center" || kind === "projected" || kind === "quadrant"
              ? themeColor("--color-axis-z", "#6db4ff")
              : themeColor("--color-tertiary-plane-edge", "#ffe784"),
      );
      material.opacity = isSelected || isHovered ? 1 : 0.95;
      const scale = isSelected ? 1.35 : isHovered ? 1.25 : 1;
      mesh.scale.setScalar(scale);
    }
  }

  function setHoveredSketchPoint(pointId: string | null) {
    if (hoveredSketchPointIdRef.current === pointId) {
      return;
    }
    hoveredSketchPointIdRef.current = pointId;
    paintSketchPointMaterials();
  }

  function setHoveredPrimitive(primitiveId: string | null) {
    let changed = false;

    for (const [id, state] of primitiveStatesRef.current.entries()) {
      const nextHovered = id === primitiveId;
      if (state.isHovered !== nextHovered) {
        primitiveStatesRef.current.set(id, {
          ...state,
          isHovered: nextHovered,
        });
        changed = true;
      }
    }

    if (changed) {
      syncPrimitiveVisuals();
    }
  }

  // Hover state for body edges / vertices. Unlike face / primitive
  // hover (which keeps a per-object state map and re-runs the visual
  // helper en masse), edges and vertices are simple THREE objects
  // built once per geometry rebuild — so we recolor materials in
  // place. `userData.isSelected` was stashed at build time so we can
  // resolve the (selected, hovered) tuple per object without reading
  // the document state here.
  const hoveredEdgeIdRef = useRef<string | null>(null);
  // Recolor every edge from its userData (selection / ghost flags) plus
  // the current hover id and the live ghost-reveal flag. Single source
  // of truth so hover changes and Tab-toggle changes don't have to
  // duplicate the visual logic.
  function paintEdgeMaterials(hoveredId: string | null) {
    const revealGhost = revealGhostEdgesRef.current;
    for (const line of edgeLineObjectsRef.current) {
      const id = line.userData.edgeId as string | undefined;
      const isSelected = line.userData.isSelected === true;
      const isGhost = line.userData.isGhost === true;
      const isHovered = id !== undefined && id === hoveredId;
      const material = line.material as THREE.LineBasicMaterial;
      applyEdgeVisualColor(material, {
        isSelected,
        isHovered,
        isGhost,
        revealGhost,
      });
    }
  }
  function setHoveredEdge(edgeId: string | null) {
    if (hoveredEdgeIdRef.current === edgeId) {
      return;
    }
    hoveredEdgeIdRef.current = edgeId;
    paintEdgeMaterials(edgeId);
  }

  const hoveredVertexIdRef = useRef<string | null>(null);
  function paintVertexMaterials(hoveredId: string | null) {
    for (const mesh of vertexObjectsRef.current) {
      const id = mesh.userData.vertexId as string | undefined;
      const isSelected = mesh.userData.isSelected === true;
      const isHovered = id !== undefined && id === hoveredId;
      const material = mesh.material as THREE.MeshBasicMaterial;
      applyVertexVisualColor(material, { isSelected, isHovered });
    }
  }

  function setHoveredVertex(vertexId: string | null) {
    if (hoveredVertexIdRef.current === vertexId) {
      return;
    }
    hoveredVertexIdRef.current = vertexId;
    paintVertexMaterials(vertexId);
  }

  function setHoveredReference(referenceId: string | null) {
    let changed = false;

    for (const [id, state] of referencePlaneStatesRef.current.entries()) {
      const nextHovered = id === referenceId;
      if (state.isHovered !== nextHovered) {
        referencePlaneStatesRef.current.set(id, {
          ...state,
          isHovered: nextHovered,
        });
        changed = true;
      }
    }

    if (changed) {
      syncReferencePlaneVisuals();
    }
  }

  useEffect(() => {
    selectPrimitiveRef.current = onSelectPrimitive;
    selectReferenceRef.current = onSelectReference;
    selectFaceRef.current = onSelectFace;
    moveBodyRef.current = onMoveBody;
    copyBodyRef.current = onCopyBody;
    exportBodyMeshRef.current = onExportBodyMesh;
    unlinkBodyCopyRef.current = onUnlinkBodyCopy;
    selectEdgeRef.current = onSelectEdge;
    selectVertexRef.current = onSelectVertex;
    startSketchRef.current = onStartSketch;
    startSketchOnFaceRef.current = onStartSketchOnFace;
    addSketchLineRef.current = onAddSketchLine;
    addSketchRectangleRef.current = onAddSketchRectangle;
    addSketchCircleRef.current = onAddSketchCircle;
    addSketchArcRef.current = onAddSketchArc;
    arcToolModeRef.current = arcToolMode;
    rectangleToolModeRef.current = rectangleToolMode;
    circleToolModeRef.current = circleToolMode;
    polygonToolModeRef.current = polygonToolMode;
    polygonSidesRef.current = polygonSides;
    addSketchPolygonRef.current = onAddSketchPolygon;
    addSketchFilletRef.current = onAddSketchFillet;
    selectSketchEntityRef.current = onSelectSketchEntity;
    pickInactiveSketchLineRef.current = onPickInactiveSketchLine;
    inactiveSketchEntityPickEnabledRef.current =
      inactiveSketchEntityPickEnabled;
    pickSketchPointRef.current = onPickSketchPoint;
    updateSketchPointRef.current = onUpdateSketchPoint;
    selectSketchDimensionRef.current = onSelectSketchDimension;
    updateSketchDimensionRef.current = onUpdateSketchDimension;
    updateSketchDimensionLabelPositionRef.current =
      onUpdateSketchDimensionLabelPosition;
    addSketchPointDistanceDimensionRef.current =
      onAddSketchPointDistanceDimension;
    updateSketchDimensionDisplayRef.current =
      onUpdateSketchDimensionDisplay;
    selectSketchProfileRef.current = onSelectSketchProfile;
    trimSketchEntityRef.current = onTrimSketchEntity;
    deleteSketchSelectionRef.current = onDeleteSketchSelection;
    setSketchToolRef.current = onSetSketchTool;
    armedSketchConstraintRef.current = armedSketchConstraint;
    mirrorFocusedSlotRef.current = mirrorFocusedSlot;
    mirrorEntityPickRef.current = onMirrorEntityPick;
    cancelSketchConstraintRef.current = onCancelSketchConstraint;
    clearSketchConstraintRef.current = onClearSketchConstraint;
    moveGizmoRef.current = moveGizmo;
    moveGizmoChangeRef.current = onMoveGizmoChange;
    moveBodyRef.current = onMoveBody;
    copyBodyRef.current = onCopyBody;
    exportBodyMeshRef.current = onExportBodyMesh;
    unlinkBodyCopyRef.current = onUnlinkBodyCopy;
  }, [
    onSelectPrimitive,
    onSelectReference,
    onSelectFace,
    onSelectEdge,
    onSelectVertex,
    onStartSketch,
    onStartSketchOnFace,
    onAddSketchLine,
    onAddSketchRectangle,
    onAddSketchCircle,
    onAddSketchArc,
    arcToolMode,
    rectangleToolMode,
    circleToolMode,
    polygonToolMode,
    polygonSides,
    onAddSketchPolygon,
    onAddSketchFillet,
    onSelectSketchEntity,
    onPickInactiveSketchLine,
    inactiveSketchEntityPickEnabled,
    onPickSketchPoint,
    onSelectSketchDimension,
    onUpdateSketchDimension,
    onUpdateSketchDimensionLabelPosition,
    onSelectSketchProfile,
    onDeleteSketchSelection,
    onSetSketchTool,
    onUpdateSketchPoint,
    armedSketchConstraint,
    mirrorFocusedSlot,
    onMirrorEntityPick,
    onCancelSketchConstraint,
    onClearSketchConstraint,
    moveGizmo,
    onMoveGizmoChange,
    onMoveBody,
    onCopyBody,
    onExportBodyMesh,
    onUnlinkBodyCopy,
  ]);

  function flushMoveGizmoChange(parameters: MoveFeatureParameters) {
    pendingMoveGizmoParametersRef.current = parameters;
    if (pendingMoveGizmoFrameRef.current !== null) {
      return;
    }
    pendingMoveGizmoFrameRef.current = window.requestAnimationFrame(() => {
      pendingMoveGizmoFrameRef.current = null;
      const next = pendingMoveGizmoParametersRef.current;
      pendingMoveGizmoParametersRef.current = null;
      if (next) {
        void moveGizmoChangeRef.current?.(next);
      }
    });
  }

  useEffect(() => {
    activeSketchToolRef.current = activeSketchTool;
    sketchSnapCandidatesRef.current = sketchSnapCandidates;
  }, [activeSketchTool, sketchSnapCandidates]);

  useEffect(() => {
    setCrosshairPointer(null);
  }, [activeSketchPlaneId, activeSketchTool, config.viewport.crosshair]);

  useEffect(() => {
    setSketchMidpointAnchorRef.current = onSetSketchMidpointAnchor;
  }, [onSetSketchMidpointAnchor]);

  useEffect(() => {
    setSketchPointLineAnchorRef.current = onSetSketchPointLineAnchor;
  }, [onSetSketchPointLineAnchor]);

  useEffect(() => {
    addSketchAngleDimensionRef.current = onAddSketchAngleDimension;
    addSketchDistanceDimensionRef.current = onAddSketchDistanceDimension;
    addSketchLineLengthDimensionRef.current = onAddSketchLineLengthDimension;
    addSketchCircleRadiusDimensionRef.current =
      onAddSketchCircleRadiusDimension;
    addSketchPolygonRadiusDimensionRef.current =
      onAddSketchPolygonRadiusDimension;
  }, [
    onAddSketchAngleDimension,
    onAddSketchDistanceDimension,
    onAddSketchLineLengthDimension,
    onAddSketchCircleRadiusDimension,
    onAddSketchPolygonRadiusDimension,
  ]);

  useEffect(() => {
    setSketchPerpendicularConstraintRef.current =
      onSetSketchPerpendicularConstraint;
  }, [onSetSketchPerpendicularConstraint]);

  useEffect(() => {
    setSketchLineConstraintRef.current = onSetSketchLineConstraint;
  }, [onSetSketchLineConstraint]);

  useEffect(() => {
    setSketchTangentConstraintRef.current = onSetSketchTangentConstraint;
  }, [onSetSketchTangentConstraint]);

  useEffect(() => {
    setSketchParallelConstraintRef.current = onSetSketchParallelConstraint;
  }, [onSetSketchParallelConstraint]);

  // Post-add line relation dispatch. Pending snap relations are captured
  // at click-time and consumed once the new line appears after IPC settles.
  useEffect(() => {
    const params = sketchFeature?.sketch_parameters ?? null;
    sketchLinesRef.current = params;
    const newCount = params?.lines.length ?? 0;
    const previousCount = sketchLineCountRef.current;
    sketchLineCountRef.current = newCount;

    applyPendingLineCommitRelations({
      sketchParameters: params,
      previousLineCount: previousCount,
      currentLineCount: newCount,
      refs: {
        midpointAnchor: pendingMidpointAnchorRef,
        perpendicularConstraint: pendingPerpendicularConstraintRef,
        pointLineAnchor: pendingPointLineAnchorRef,
        axisConstraint: pendingAxisConstraintRef,
        tangentConstraint: pendingTangentConstraintRef,
        parallelConstraint: pendingParallelConstraintRef,
      },
      actions: {
        setSketchMidpointAnchor: setSketchMidpointAnchorRef.current,
        setSketchPerpendicularConstraint:
          setSketchPerpendicularConstraintRef.current,
        setSketchPointLineAnchor: setSketchPointLineAnchorRef.current,
        setSketchLineConstraint: setSketchLineConstraintRef.current,
        setSketchTangentConstraint: setSketchTangentConstraintRef.current,
        setSketchParallelConstraint: setSketchParallelConstraintRef.current,
      },
    });
  }, [sketchFeature]);

  useEffect(() => {
    sketchToolConstructionRef.current = sketchToolConstruction;
  }, [sketchToolConstruction]);

  // Auto-clear the construction toggle when the user leaves drawable
  // sketch tools so the option doesn't silently apply next time.
  useEffect(() => {
    if (!isDrawableSketchTool(activeSketchTool)) {
      setSketchToolConstruction(false);
    }
  }, [activeSketchTool]);

  useEffect(() => {
    selectedSketchDimensionRef.current = selectedSketchDimension;
  }, [selectedSketchDimension]);

  useEffect(() => {
    dimensionLabelPositionsRef.current = dimensionLabelPositions;
  }, [dimensionLabelPositions]);
  useEffect(() => {
    angleDragRadiiRef.current = angleDragRadii;
  }, [angleDragRadii]);

  useEffect(() => {
    if (selectedSketchDimensionValue === null) {
      setDimensionDraftValue("");
      dimensionEditOriginalValueRef.current = null;
      return;
    }
    if (!selectedSketchDimension) {
      return;
    }
    const originalValue = dimensionEditOriginalValueRef.current;
    if (originalValue?.dimensionId !== selectedSketchDimension.dimensionId) {
      dimensionEditOriginalValueRef.current = {
        dimensionId: selectedSketchDimension.dimensionId,
        value: selectedSketchDimensionValue,
        expression: selectedSketchDimensionExpression,
      };
    }
    if (window.document.activeElement === dimensionInputRef.current) {
      return;
    }

    // Round to 2 decimals and strip trailing zeros so 12.000000001 →
    // "12" and 3.4567 → "3.46", instead of leaking the full IEEE-754
    // representation into the input. `parseFloat` of a fixed-precision
    // string is the canonical way to drop trailing zeros without
    // building a regex.
    setDimensionDraftValue(
      selectedSketchDimensionExpression.trim().length > 0
        ? selectedSketchDimensionExpression
        : formattedDimensionDisplayValue(
            selectedSketchDimension,
            selectedSketchDimensionValue,
          ),
    );
  }, [
    selectedSketchDimensionValue,
    selectedSketchDimensionExpression,
    document?.selected_sketch_dimension_id,
    selectedSketchDimension,
    selectedSketchDimension?.dimensionId,
    selectedSketchDimension?.kind,
  ]);

  useEffect(() => {
    if (
      !pendingDimensionPlacementRef.current ||
      activeSketchTool !== "dimension"
    ) {
      return;
    }
    const pendingRelation = pendingRelationPlacementMatchRef.current;
    let placementDimension = selectedSketchDimension;
    if (pendingRelation) {
      if (!startPendingRelationPlacementIfReady()) {
        return;
      }
      stopPendingRelationPlacementRetry();
      return;
    }
    if (!placementDimension) {
      return;
    }
    pendingDimensionIdRef.current = null;
    pendingDimensionPlacementRef.current = false;
    pendingDimSourceEntityIdRef.current = null;
    beginDimensionPlacement(placementDimension);
  }, [activeSketchTool, displayedSketchDimensions, selectedSketchDimension]);

  useEffect(() => {
    if (!selectedSketchDimension) {
      setIsDimensionEditorOpen(false);
      dimensionInputSelectionLockedRef.current = false;
      return;
    }

    if (isProjectedCircleDimension(selectedSketchDimension.dimensionId)) {
      dimensionInputRef.current?.blur();
      setIsDimensionEditorOpen(false);
      dimensionInputSelectionLockedRef.current = false;
      return;
    }

    if (suppressNextDimensionEditorOpenRef.current) {
      suppressNextDimensionEditorOpenRef.current = false;
      dimensionInputRef.current?.blur();
      setIsDimensionEditorOpen(false);
      return;
    }

    dimensionInputSelectionLockedRef.current = true;
    setIsDimensionEditorOpen(true);
  }, [selectedSketchDimension?.dimensionId]);

  useEffect(() => {
    if (!isDimensionEditorOpen || !selectedSketchDimension) {
      return;
    }

    const input = dimensionInputRef.current;
    if (!input) {
      return;
    }

    input.focus();
    input.select();
  }, [isDimensionEditorOpen, selectedSketchDimension?.dimensionId]);

  useEffect(() => {
    const host = hostRef.current;
    const canvas = canvasRef.current;

    if (!host || !canvas) {
      return undefined;
    }

    const renderer = new THREE.WebGLRenderer({
      canvas,
      antialias: true,
      alpha: true,
      preserveDrawingBuffer: true,
    });
    const scene = new THREE.Scene();
    const camera = new THREE.OrthographicCamera(
      -ORTHO_FRUSTUM_HEIGHT / 2,
      ORTHO_FRUSTUM_HEIGHT / 2,
      ORTHO_FRUSTUM_HEIGHT / 2,
      -ORTHO_FRUSTUM_HEIGHT / 2,
      -10000,
      10000,
    );
    const controls = new OrbitControls(camera, renderer.domElement);
    const contentGroup = new THREE.Group();
    const referenceGroup = new THREE.Group();
    const sketchGroup = new THREE.Group();
    const raycaster = new THREE.Raycaster();
    const pointer = new THREE.Vector2();
    let pointerDown: { x: number; y: number } | null = null;
    let frameId = 0;

    rendererRef.current = renderer;
    sceneRef.current = scene;
    cameraRef.current = camera;
    controlsRef.current = controls;
    contentGroupRef.current = contentGroup;
    referenceGroupRef.current = referenceGroup;
    sketchGroupRef.current = sketchGroup;

    renderer.setPixelRatio(window.devicePixelRatio);
    scene.add(contentGroup);
    scene.add(referenceGroup);
    scene.add(sketchGroup);
    // Neutral studio lighting so MeshStandardMaterial bodies render as
    // true contextual modeling gray. The previous cyan-tinted ambient + key
    // + rim lights were leaking cyan into the body fill, which made
    // the new gray material look like the old translucent cyan even
    // after the material itself was switched to opaque.
    scene.add(new THREE.AmbientLight(0xffffff, 0.85));

    const keyLight = new THREE.DirectionalLight(0xffffff, 0.95);
    keyLight.position.set(1.2, 1.8, 1.4);
    scene.add(keyLight);

    const fillLight = new THREE.DirectionalLight(0xffffff, 0.45);
    fillLight.position.set(-1.5, 0.8, -1.1);
    scene.add(fillLight);

    setupViewportSnapshotCapture({
      host,
      renderer,
      scene,
      camera,
      onSnapshotCaptureReady,
    });

    configureViewportControls({ controls, canvas });

    // -- view cube setup -------------------------------------------------
    const cubeGroup = buildViewCubeGroup();
    const cubeScene = createViewCubeScene(cubeGroup);
    const cubeCamera = createViewCubeCamera();
    const cubeRaycaster = new THREE.Raycaster();
    viewCubeGroupRef.current = cubeGroup;
    viewCubeSceneRef.current = cubeScene;
    viewCubeCameraRef.current = cubeCamera;
    viewCubeRaycasterRef.current = cubeRaycaster;

    // Render-target-based cube rendering (no scissor test)
    const cubeTarget = createCubeRenderTarget(renderer);
    const blitScene = createCubeBlitScene(cubeTarget);
    cubeRenderTargetRef.current = cubeTarget;
    cubeBlitSceneRef.current = blitScene;

    function resizeRenderer() {
      resizeViewportRenderer({
        host,
        renderer,
        camera,
        orthoFrustumHeight: ORTHO_FRUSTUM_HEIGHT,
        setViewportSize,
      });
    }

    function handleWheel(event: WheelEvent) {
      handleViewportWheelZoom({
        event,
        renderer,
        camera,
        controls,
        minZoom: ORTHO_MIN_ZOOM,
        maxZoom: ORTHO_MAX_ZOOM,
        zoomSpeed: WHEEL_ZOOM_SPEED,
        pointerPan: WHEEL_ZOOM_POINTER_PAN,
      });
    }

    function renderDraftDimensions() {
      const session = draftDimensionSessionRef.current;
      const sketchGroup = sketchGroupRef.current;
      const camera = cameraRef.current;
      if (!session || !sketchGroup || !camera) {
        clearDraftDimGroup();
        return;
      }

      // Clear previous frame's geometry
      clearDraftDimGroup();

      const preview = buildDraftDimensionPreview({
        session,
        camera,
        renderer,
        activeSketchPlaneId,
        activeSketchPlaneFrame,
        previousLineAngle: previousLineAngleRef.current,
      });

      if (preview.kind === "none") {
        return;
      }

      draftDimScreenPositionsRef.current = preview.screenPositions;
      if (preview.kind === "positions") {
        return;
      }

      sketchGroup.add(preview.group);
      draftDimGroupRef.current = preview.group;
    }

    function render() {
      controls.update();
      updateDynamicGrids({
        scene,
        sceneData: sceneDataRef.current,
        camera,
        target: controls.target,
        worldGridRef,
        sketchGridRef,
        currentGridSpacingRef,
        activeSketchPlaneId: activeSketchPlaneIdRef.current,
        activeSketchPlaneFrame: activeSketchPlaneFrameRef.current,
        showViewportGrid: showViewportGridRef.current,
        showSketchGrid: showSketchGridRef.current,
      });
      updateScreenSpaceSketchSprites({
        renderer,
        camera,
        sketchDimensionObjects: sketchDimensionObjectsRef.current,
        sketchConstraintObjects: sketchConstraintObjectsRef.current,
      });
      try {
        renderDraftDimensions();
      } catch (err) {
        console.warn("renderDraftDimensions error:", err);
        clearDraftDimGroup();
      }

      renderer.render(scene, camera);
      renderViewCube();

      const editor = dimensionEditorRef.current;
      const dimension = selectedSketchDimensionRef.current;
      const isOpen = isDimensionEditorOpenRef.current;
      if (!editor || !dimension || !isOpen) {
        if (editor) {
          editor.style.opacity = "0";
        }
        return;
      }

      const projectedPosition = projectWorldPointToViewport(
        dimension.labelPosition,
        camera,
        renderer,
      );

      if (!projectedPosition) {
        editor.style.opacity = "0";
        return;
      }

      editor.style.opacity = "1";
      editor.style.transform = `translate(${projectedPosition.x}px, ${projectedPosition.y}px) translate(-50%, -50%)`;
    }


    async function performRectangleSelect(drag: SelectionDrag, additive: boolean) {
      if (!activeSketchPlaneIdRef.current) return;

      const camera = cameraRef.current;
      const renderer = rendererRef.current;
      if (!camera || !renderer) return;

      const sceneData = sceneDataRef.current;
      const selected = collectRectangleSelectionIds({
        drag,
        sceneData,
        camera,
        renderer,
      });

      if (selected.length > 0) {
        onBatchSelectEntities(selected, additive);
      }
    }

    const viewCubeAnimationRefs = {
      animating: viewCubeAnimatingRef,
      start: viewCubeAnimStartRef,
      startPos: viewCubeAnimStartPosRef,
      targetPos: viewCubeAnimTargetPosRef,
      startUp: viewCubeAnimStartUpRef,
      targetUp: viewCubeAnimTargetUpRef,
    };

    function rotateCameraAroundCurrentViewFromViewport(direction: -1 | 1) {
      rotateCameraAroundCurrentView({
        camera,
        controls,
        direction,
        animationRefs: viewCubeAnimationRefs,
      });
    }

    function renderViewCube() {
      renderViewCubeFrame({
        renderer,
        camera,
        controls,
        cubeGroupRef: viewCubeGroupRef,
        cubeSceneRef: viewCubeSceneRef,
        cubeCameraRef: viewCubeCameraRef,
        cubeRenderTargetRef,
        cubeBlitSceneRef,
        animationRefs: viewCubeAnimationRefs,
      });
    }

    function intersectSceneTargets(event: PointerEvent) {
      return intersectViewportSceneTargets({
        event,
        renderer,
        camera,
        pointer,
        raycaster,
        sceneData: sceneDataRef.current,
        activeSketchPlaneId,
        activeSketchTool: activeSketchToolRef.current,
        armedSketchConstraintKind: armedSketchConstraintRef.current?.kind ?? null,
        inactiveSketchEntityPickEnabled:
          inactiveSketchEntityPickEnabledRef.current,
        sketchPointObjects: sketchPointObjectsRef.current,
        sketchEntityObjects: sketchEntityObjectsRef.current,
        sketchDimensionObjects: sketchDimensionObjectsRef.current,
        sketchConstraintObjects: sketchConstraintObjectsRef.current,
        sketchProfileObjects: sketchProfileObjectsRef.current,
        referencePlaneMeshes: referencePlaneMeshesRef.current,
        vertexObjects: vertexObjectsRef.current,
        edgeLineObjects: edgeLineObjectsRef.current,
        faceMeshes: faceMeshesRef.current,
        meshes: meshesRef.current,
      });
    }

    function handlePointerDown(event: PointerEvent) {
      // Clear selected constraint on any pointer-down (except on the
      // constraint badge itself, which sets it in the click path above).
      setSelectedConstraint(null);
      lastPointerEventRef.current = event;
      setContextMenu(null);

      if (dimensionLabelDragRef.current?.isPlacement) {
        // Capture the pointer so the subsequent pointerup reaches the
        // canvas handler even when the cursor is over the dimension
        // editor input — the editor must not steal the commit click.
        renderer.domElement.setPointerCapture(event.pointerId);
        pointerDown = { x: event.clientX, y: event.clientY };
        return;
      }

      if (event.button === 1) {
        controls.mouseButtons.MIDDLE =
          event.ctrlKey || event.metaKey ? THREE.MOUSE.ROTATE : THREE.MOUSE.PAN;
        return;
      }

      if (event.button !== 0) {
        pointerDown = null;
        return;
      }

      updateDraftChainBreakRequest({
        event,
        activeSketchTool: activeSketchToolRef.current,
        draftStartRef: lineDraftStartRef,
        lastPointerDownTimeRef,
        lastPointerDownPosRef,
        chainBreakRequestedRef,
      });

      // Cube-area drag start
      if (
        isPointerInCubeArea(
          event,
          renderer.domElement.getBoundingClientRect(),
          renderer.getPixelRatio(),
        )
      ) {
        viewCubeDraggingRef.current = true;
        viewCubeDragStartRef.current = { x: event.clientX, y: event.clientY };
        controls.enabled = false;
        pointerDown = null;
        return;
      }

      if (
        beginMoveGizmoPointerDown({
          event,
          renderer,
          camera,
          controls,
          raycaster,
          pointer,
          moveGizmo: moveGizmoRef.current,
          moveGizmoObjects: moveGizmoObjectsRef.current,
          moveGizmoDragRef,
        })
      ) {
        pointerDown = null;
        return;
      }

      pointerDown = { x: event.clientX, y: event.clientY };
      // --- Select mode: endpoint drag OR rectangle selection ---
      if (activeSketchToolRef.current === "select") {
        const selectPointerDown = beginSelectPointerDown({
          event,
          renderer,
          camera,
          controls,
          activeSketchPlaneId: activeSketchPlaneIdRef.current,
          activeSketchPlaneFrame: activeSketchPlaneFrameRef.current,
          sketch: sketchLinesRef.current,
          endpointDragRef,
          selectionDragRef,
          intersectSceneTargets,
        });
        if (selectPointerDown.handled) {
          if (selectPointerDown.clearPointerDown) {
            pointerDown = null;
          }
          return;
        }
      }

      renderer.domElement.setPointerCapture(event.pointerId);
      if (
        activeSketchPlaneIdRef.current &&
        (activeSketchToolRef.current === "select" ||
          activeSketchToolRef.current === "dimension")
      ) {
        const hit = intersectSceneTargets(event);
        if (
          hit?.kind === "sketch_dimension" &&
          beginDimensionLabelDragPointerDown({
            event,
            renderer,
            camera,
            controls,
            hit,
            activeSketchPlaneId: activeSketchPlaneIdRef.current,
            activeSketchPlaneFrame: activeSketchPlaneFrameRef.current,
            dimensions: displayedSketchDimensionsRef.current,
            suppressNextDimensionEditorOpenRef,
            dimensionLabelDragRef,
            setIsDimensionEditorOpen,
            selectSketchDimension: selectSketchDimensionRef.current,
            setAngleDimensionDragRadius,
            getDimensionPlacementAxis,
          })
        ) {
          return;
        }
      }
      if (
        beginDraftPointerDown({
          event,
          renderer,
          camera,
          activeSketchPlaneId: activeSketchPlaneIdRef.current,
          activeSketchPlaneFrame: activeSketchPlaneFrameRef.current,
          activeSketchTool: activeSketchToolRef.current,
          draftStartRef: lineDraftStartRef,
          draftStartedOnPointerDownRef,
          draftDimensionSessionRef,
          resolveSnappedSketchPoint,
          createDraftDimensionSession,
          setDraftDimensionSession,
          focusDraftField,
        })
      ) {
        return;
      }

    }

    function handlePointerMove(event: PointerEvent) {

      // --- Rectangle selection drag tracking ---
      if (selectionDragRef.current?.active) {
        selectionDragRef.current.currentX = event.clientX;
        selectionDragRef.current.currentY = event.clientY;
        setSelectionRect(
          selectionRectOverlayFromDrag(selectionDragRef.current),
        );
        return;
      }

      lastPointerEventRef.current = event;
      // -- cube-area interaction ---------------------------------------
      const cubeDpr = renderer.getPixelRatio();
      const cubeCanvasRect = renderer.domElement.getBoundingClientRect();
      const inCube = isPointerInCubeArea(event, cubeCanvasRect, cubeDpr);
      if (
        activeSketchPlaneIdRef.current &&
        activeSketchToolRef.current !== "select" &&
        activeSketchToolRef.current !== "project" &&
        !inCube
      ) {
        setCrosshairPointer({
          x: event.clientX - cubeCanvasRect.left,
          y: event.clientY - cubeCanvasRect.top,
        });
      } else {
        setCrosshairPointer(null);
      }

      if (
        handleViewCubeDragPointerMove({
          event,
          camera,
          controls,
          viewCubeDraggingRef,
          viewCubeDragStartRef,
        })
      ) {
        return;
      }

      const moveGizmoDrag = moveGizmoDragRef.current;
      if (moveGizmoDrag) {
        flushMoveGizmoChange(
          moveGizmoParametersFromDrag(event, moveGizmoDrag, camera, renderer),
        );
        return;
      }

      if (
        handleViewCubeHoverPointerMove({
          event,
          renderer,
          inCube,
          cubePixelRatio: cubeDpr,
          cubeGroupRef: viewCubeGroupRef,
          cubeCameraRef: viewCubeCameraRef,
          cubeRaycasterRef: viewCubeRaycasterRef,
          viewCubeHoveredRef,
        })
      ) {
        return;
      }

      if (
        handleDimensionLabelDragPointerMove({
          event,
          renderer,
          camera,
          activeSketchPlaneId: activeSketchPlaneIdRef.current,
          activeSketchPlaneFrame: activeSketchPlaneFrameRef.current,
          dimensionLabelDragRef,
          dimensions: displayedSketchDimensionsRef.current,
          angleDragRadiiRef,
          setAngleDragRadii,
          updateDimensionRelationPreview,
          angleFrameForDimension: angleDimensionFrame,
          setDimensionLabelPosition,
        })
      ) {
        return;
      }

      if (
        handleEndpointDragPointerMove({
          event,
          renderer,
          camera,
          endpointDragRef,
          activeSketchPlaneIdRef,
          activeSketchPlaneFrameRef,
          sketchLinesRef,
          pendingDragRef,
          pendingDragFrameRef,
          dragSnapResultRef,
          dragCursorRef,
          dragPreviewLinesRef,
          sketchGroupRef,
          resolveSnappedSketchPoint,
          setSketchSnapLabel,
          clearDragPreviewLines,
        })
      ) {
        return;
      }

      const hoverActions = {
        clearPreviewLine,
        clearPreviewCircle,
        clearPreviewArc,
        clearPreviewDimension,
        setSketchSnapLabel,
        setConstraintPreview,
        clearDraftDimensionSession,
        setHoveredReference,
        setHoveredPrimitive,
        setHoveredFace,
        setHoveredEdge,
        setHoveredVertex,
        setHoveredSketchProfile,
        setHoveredSketchPoint,
        setHoveredSketchEntity,
      };

      if (activeSketchPlaneId) {
        if (activeSketchToolRef.current === "select") {
          const hit = intersectSceneTargets(event);
          applySelectToolHover(hit, hoverActions);
          return;
        }

        // Project tool is a picker, not a draftsman: skip every
        // draft preview / snap-label resolution and run the body
        // face / edge / vertex hover highlight directly so the
        // user sees what they're about to pick. We can't fall
        // through to the bottom of this function because the
        // line-draft branches below `return` early before we get
        // there — instead we mirror the same hover dispatch here
        // and bail out.
        if (activeSketchToolRef.current === "project") {
          const projectHit = intersectSceneTargets(event);
          applyProjectToolHover(projectHit, hoverActions);
          return;
        }

        // Trim tool hover: highlight the segment under the cursor in red.
        if (activeSketchToolRef.current === "trim") {
          handleTrimPointerMove({
            event,
            renderer,
            camera,
            activeSketchPlaneId,
            activeSketchPlaneFrame,
            activeSketchPlaneFrameRef,
            sceneDataRef,
            trimPreviewLastSentRef,
            hoverActions,
            intersectSceneTargets,
            clearTrimSegmentHighlight,
            clearTrimArcHighlight,
            updateTrimSegmentHighlight,
            updateTrimArcHighlight,
          });
          return;
        }

        clearSketchEntityHover(hoverActions);
        const draftMove = resolveDraftPointerMove({
          event,
          renderer,
          camera,
          activeSketchPlaneId,
          activeSketchPlaneFrame,
          activeSketchTool: activeSketchToolRef.current,
          draftStartRef: lineDraftStartRef,
          draftDimensionSessionRef,
          resolveSnappedSketchPoint,
          updateDraftSessionFromPoint,
          setSketchSnapLabel,
          setConstraintPreview,
        });
        if (!draftMove) {
          return;
        }
        const { draftStart, draftPreviewLocal } = draftMove;

        if (!draftStart) {
          setHoveredPrimitive(null);
          setHoveredReference(null);
          return;
        }

        const sketchGroupRefValue = sketchGroupRef.current;
        if (!sketchGroupRefValue) {
          return;
        }

        renderDraftPointerPreview({
          activeSketchTool: activeSketchToolRef.current,
          activeSketchPlaneId,
          activeSketchPlaneFrame,
          draftStart,
          draftPreviewLocal,
          sketchGroup: sketchGroupRefValue,
          arcToolMode: arcToolModeRef.current,
          circleToolMode: circleToolModeRef.current,
          rectangleToolMode: rectangleToolModeRef.current,
          arcSecondPoint: arcSecondPointRef.current,
          circleSecondPoint: circleSecondPointRef.current,
          rectSecondPoint: rectSecondPointRef.current,
          isConstruction: sketchToolConstructionRef.current,
          previewLineRef,
          previewCircleRef,
          previewArcRef,
          clearPreviewLine,
          clearPreviewCircle,
          clearPreviewArc,
          clearPreviewDimension,
          renderCircleDraftDimension,
        });
        return;
      }

      const hit = intersectSceneTargets(event);
      applySceneHover(hit, hoverActions);
    }

    function handlePointerLeave() {
      pointerDown = null;
      if (moveGizmoDragRef.current) {
        moveGizmoDragRef.current = null;
        controls.enabled = true;
      }
      if (!dimensionLabelDragRef.current?.isPlacement) {
        dimensionLabelDragRef.current = null;
        controls.enabled = true;
      }
      (renderer.domElement as HTMLCanvasElement).style.cursor = "";
      setSketchSnapLabel(null);
      setConstraintPreview(null);
      setCrosshairPointer(null);
      setHoveredSketchProfile(null);
      setHoveredSketchPoint(null);
      setHoveredSketchEntity(null);
      if (!activeSketchPlaneId) {
        setHoveredReference(null);
        setHoveredPrimitive(null);
        setHoveredFace(null);
        setHoveredEdge(null);
        setHoveredVertex(null);
      }
      if (viewCubeGroupRef.current) {
        clearCubeHover(viewCubeGroupRef.current);
      }
      viewCubeHoveredRef.current = null;
    }

    function finishDimensionLabelDragPointerUp() {
      const dimensionDrag = dimensionLabelDragRef.current;
      if (!dimensionDrag) {
        return "inactive" as const;
      }

      if (dimensionDrag.isPlacement) {
        if (commitDimensionRelationPreview()) {
          setIsDimensionEditorOpen(false);
          pointerDown = null;
          return "consumed" as const;
        }
        finishDimensionPlacement();
        setIsDimensionEditorOpen(false);
        // Fall through to entity handling so two-pick workflows
        // (angle, distance) can process the second click.
      }

      // finishDimensionPlacement nulls the ref; only clean up if the drag
      // is still active (non-placement label drag).
      if (!dimensionLabelDragRef.current) {
        return "continue" as const;
      }

      if (dimensionDrag.hasMoved) {
        persistDimensionDragLabelPosition(dimensionDrag);
      }

      dimensionLabelDragRef.current = null;
      controls.enabled = true;
      (renderer.domElement as HTMLCanvasElement).style.cursor = "";
      pointerDown = null;
      if (!dimensionDrag.hasMoved) {
        if (dimensionDrag.hitPart === "label") {
          suppressNextDimensionEditorOpenRef.current = false;
          dimensionInputSelectionLockedRef.current = true;
          void selectSketchDimensionRef.current(dimensionDrag.dimensionId);
          setIsDimensionEditorOpen(true);
        } else {
          suppressNextDimensionEditorOpenRef.current = true;
          setIsDimensionEditorOpen(false);
          void selectSketchDimensionRef.current(dimensionDrag.dimensionId);
        }
      }
      return "consumed" as const;
    }

    function finishEndpointDragPointerUpFromViewport(event: PointerEvent) {
      return finishEndpointDragPointerUp({
        event,
        renderer,
        camera,
        controls,
        endpointDragRef,
        dragSnapResultRef,
        activeSketchPlaneIdRef,
        activeSketchPlaneFrameRef,
        pendingEndpointCommitRef,
        dragCursorRef,
        updateSketchPoint: updateSketchPointRef.current,
        clearDragPreviewLines,
        setConstraintPreview,
        setSketchSnapLabel,
        setHoveredSketchEntity,
        setHoveredSketchPoint,
        setPointerDown: (point) => {
          pointerDown = point;
        },
      });
    }

    function finishViewCubePointerUpFromViewport(event: PointerEvent) {
      return finishViewCubePointerUp({
        event,
        renderer,
        camera,
        controls,
        viewCubeDraggingRef,
        viewCubeDragStartRef,
        viewCubeGroupRef,
        viewCubeCameraRef,
        viewCubeRaycasterRef,
        viewCubeAnimatingRef,
        viewCubeAnimStartRef,
        viewCubeAnimStartPosRef,
        viewCubeAnimTargetPosRef,
        viewCubeAnimStartUpRef,
        viewCubeAnimTargetUpRef,
        rotateCameraAroundCurrentView: rotateCameraAroundCurrentViewFromViewport,
      });
    }

    function handlePointerUp(event: PointerEvent) {

      // --- Rectangle selection finalize ---
      if (
        finishRectangleSelectionDrag({
          event,
          selectionDragRef,
          setSelectionRect,
          controls,
          performRectangleSelect,
        })
      ) {
        return;
      }

      lastPointerEventRef.current = event;
      if (event.button === 1) {
        controls.mouseButtons.MIDDLE = THREE.MOUSE.PAN;
        pointerDown = null;
        return;
      }

      if (event.button !== 0) {
        pointerDown = null;
        return;
      }
      if (renderer.domElement.hasPointerCapture(event.pointerId)) {
        renderer.domElement.releasePointerCapture(event.pointerId);
      }

      if (
        finishMoveGizmoPointerUp({
          renderer,
          controls,
          moveGizmoDragRef,
        })
      ) {
        pointerDown = null;
        return;
      }

      const dimensionDragResult = finishDimensionLabelDragPointerUp();
      if (dimensionDragResult === "consumed") {
        return;
      }

      // --- Endpoint drag finalize ---
      const endpointDragResult = finishEndpointDragPointerUpFromViewport(event);
      if (endpointDragResult === "consumed") {
        return;
      }

      // -- cube-area click ---------------------------------------------
      if (finishViewCubePointerUpFromViewport(event) === "consumed") {
        return;
      }

      if (!pointerDown) {
        return;
      }

      const deltaX = Math.abs(event.clientX - pointerDown.x);
      const deltaY = Math.abs(event.clientY - pointerDown.y);
      pointerDown = null;

      if (
        finishDraftStartedPointerUp({
          deltaX,
          deltaY,
          draftStartedOnPointerDownRef,
          draftDimensionSessionRef,
          draftDimensionInputRefs,
        })
      ) {
        return;
      }

      if (deltaX > 4 || deltaY > 4) {
        return;
      }

      if (activeSketchPlaneId) {
        const hit = intersectSceneTargets(event);
        const additiveSelection =
          event.shiftKey || event.ctrlKey || event.metaKey;
        if (
          handleActiveSketchPointerUpTool({
            activeSketchTool: activeSketchToolRef.current,
            hit: hit as ActiveSketchSelectHit,
            additiveSelection,
            planeId: activeSketchPlaneId,
            planeFrame: activeSketchPlaneFrameRef.current,
            sketch: sketchLinesRef.current,
            armedSketchConstraint: armedSketchConstraintRef.current,
            mirrorFocusedSlot: mirrorFocusedSlotRef.current,
            inactiveSketchEntityPickEnabled:
              inactiveSketchEntityPickEnabledRef.current,
            sketchEntityObjectById: sketchEntityObjectByIdRef.current,
            sketchPointObjects: sketchPointObjectsRef.current,
            resolveFilletPoint: () => {
              const filletRawPoint = resolveSketchPlanePoint(
                event,
                renderer,
                camera,
                activeSketchPlaneId,
                activeSketchPlaneFrame,
              );
              return filletRawPoint
                ? resolveSnappedSketchPoint(filletRawPoint)
                : null;
            },
            selectSketchProfile: selectSketchProfileRef.current,
            selectVertex: selectVertexRef.current,
            selectEdge: selectEdgeRef.current,
            selectFace: selectFaceRef.current,
            trimSketchEntity: trimSketchEntityRef.current,
            mirrorEntityPick: mirrorEntityPickRef.current,
            selectSketchEntity: selectSketchEntityRef.current,
            pickSketchPoint: pickSketchPointRef.current,
            handleDimensionClick,
            setSelectedConstraint,
            paintSketchEntityMaterials,
            paintSketchPointMaterials,
            addMessage,
            addSketchFillet: addSketchFilletRef.current,
            pendingDimensionPlacement: pendingDimensionPlacementRef.current,
            pendingDimensionSourceId: pendingDimSourceEntityIdRef.current,
            pendingDimensionId: pendingDimensionIdRef.current,
            getDimensionFirstEntityId: () => dimensionToolFirstLineRef.current,
            getDimensionFirstPoint: () => dimensionToolFirstPointRef.current,
            clearDimensionFirstPick: clearDimensionToolFirstPick,
            clearDimensionFirstEntity: clearDimensionToolFirstEntity,
            clearPendingDimensionPlacement: clearPendingDimensionPlacement,
            stageDimensionFirstEntity: (entityId) => {
              dimensionToolFirstLineRef.current = entityId;
              setDimensionToolFirstLine(entityId);
            },
            stageDimensionFirstPoint: (point) => {
              dimensionToolFirstPointRef.current = point;
            },
            deleteSketchDimension: (dimensionId) => {
              void deleteSketchDimensionRef.current(dimensionId);
            },
            createDimensionAngleOrDistance: dimCreateAngleOrDistance,
            createDimensionPointDistance: dimCreatePointDistance,
            createDimensionLine: dimCreateLine,
            createDimensionCircle: dimCreateCircle,
            selectDimensionCircle: dimSelectCircle,
            createDimensionPolygon: dimCreatePolygon,
            selectDimensionPolygon: dimSelectPolygon,
            selectDimensionLine: dimSelectLine,
          })
        ) {
          return;
        }

        // In a draft tool (line / rectangle / circle), clicks on an
        // existing line / dimension MUST NOT select. They should
        // fall through to the plane-projection path so the snap
        // resolver can use the entity as a snap source (line body,
        // endpoint, midpoint). Selection is reserved for the select
        // tool. Dimensions in draft mode are simply ignored — the
        // user can press S to switch back to select if they want to
        // edit one.
        const rawPoint = resolveSketchPlanePoint(
          event,
          renderer,
          camera,
          activeSketchPlaneId,
          activeSketchPlaneFrame,
        );
        if (!rawPoint) {
          return;
        }
        const sketchPoint = resolveSnappedSketchPoint(
          rawPoint,
          lineDraftStartRef.current,
        );
        setSketchSnapLabel(sketchPoint.snapLabel);

        commitDraftPointerUp({
          activeSketchTool: activeSketchToolRef.current,
          sketchPoint,
          draftDimensionSession: draftDimensionSessionRef.current,
          sketchCircleCount:
            sketchFeature?.sketch_parameters?.circles.length ?? 0,
          refs: {
            draftStart: lineDraftStartRef,
            arcSecondPoint: arcSecondPointRef,
            rectSecondPoint: rectSecondPointRef,
            circleSecondPoint: circleSecondPointRef,
            chainBreakRequested: chainBreakRequestedRef,
            previousLineAngle: previousLineAngleRef,
            draftStartMidpointHost: draftStartMidpointHostRef,
            draftStartEndpointHost: draftStartEndpointHostRef,
            draftStartLineBodyHost: draftStartLineBodyHostRef,
            draftDimensionSession: draftDimensionSessionRef,
            draftDimensionInputs: draftDimensionInputRefs,
          },
          modes: {
            arc: arcToolModeRef.current,
            rectangle: rectangleToolModeRef.current,
            circle: circleToolModeRef.current,
            polygon: polygonToolModeRef.current,
          },
          polygonSides: polygonSidesRef.current,
          isConstruction: sketchToolConstructionRef.current,
          clearPreviews: () => {
            clearPreviewLine();
            clearPreviewCircle();
            clearPreviewArc();
            clearPreviewDimension();
          },
          clearDraftDimensionSession,
          suppressDimensionEditorAfterSketchCommit,
          scheduleDimensionDeletion,
          scheduleDraftDimensionExpressionUpdate,
          setPendingCircleDimensionPlacement: (placement) => {
            pendingCircleDimensionPlacementRef.current = placement;
          },
          captureLineCommitRelations: capturePendingLineCommitRelations,
          createLineDraftDimensionSession: (start, current) =>
            createDraftDimensionSession("line", start, current),
          clearDraftDimGroup,
          setDraftDimensionSession,
          focusDraftField,
          addSketchArc: addSketchArcRef.current,
          addSketchRectangle: addSketchRectangleRef.current,
          addSketchCircle: addSketchCircleRef.current,
          addSketchPolygon: addSketchPolygonRef.current,
          addSketchLine: addSketchLineRef.current,
        });
        return;
      }

      if (
        handlePointerUpSceneSelection({
          event,
          sceneData: sceneDataRef.current,
          camera,
          renderer,
          inactiveSketchEntityPickEnabled:
            inactiveSketchEntityPickEnabledRef.current,
          pickInactiveSketchLine: pickInactiveSketchLineRef.current,
          intersectSceneTargets,
          selectSketchEntity: selectSketchEntityRef.current,
          selectSketchProfile: selectSketchProfileRef.current,
          selectReference: selectReferenceRef.current,
          selectVertex: selectVertexRef.current,
          selectEdge: selectEdgeRef.current,
          selectFace: selectFaceRef.current,
          selectPrimitive: selectPrimitiveRef.current,
        })
      ) {
        return;
      }
    }

    function handleContextMenu(event: MouseEvent) {
      event.preventDefault();

      // Use the ref so right-clicks after a batch select see the
      // latest document state even when the handler closure is stale.
      const doc = documentRef.current;
      const rect = renderer.domElement.getBoundingClientRect();
      const hit = intersectSceneTargets(event as PointerEvent);
      const result = buildViewportContextMenuState({
        activeSketchPlaneId,
        document: doc,
        hit,
        x: event.clientX - rect.left,
        y: event.clientY - rect.top,
        solidFaces: sceneDataRef.current?.solidFaces ?? [],
      });
      setContextMenu(result.contextMenu);
      if (result.selectedConstraint !== undefined) {
        setSelectedConstraint(result.selectedConstraint);
      }
    }

    const resizeObserver = new ResizeObserver(() => {
      resizeRenderer();
      render();
    });

    resizeObserver.observe(host);
    function handleDoubleClick(event: MouseEvent) {
      if (activeSketchPlaneId) {
        return;
      }

      const hit = intersectSceneTargets(event as PointerEvent);
      if (hit?.kind !== "face") {
        return;
      }

      const solidFace = sceneDataRef.current?.solidFaces.find(
        (face) => face.faceId === hit.id,
      );
      if (!solidFace) {
        return;
      }

      void selectFaceRef.current(solidFace.faceId);
      void startSketchOnFaceRef.current(solidFace.faceId, solidFace.planeFrame);
    }

    renderer.domElement.addEventListener("pointerdown", handlePointerDown);
    renderer.domElement.addEventListener("pointermove", handlePointerMove);
    renderer.domElement.addEventListener("pointerleave", handlePointerLeave);
    renderer.domElement.addEventListener("pointerup", handlePointerUp);
    renderer.domElement.addEventListener("contextmenu", handleContextMenu);
    renderer.domElement.addEventListener("dblclick", handleDoubleClick);
    renderer.domElement.addEventListener("wheel", handleWheel, { passive: false });
    const onTrimPreview = (e: Event) => {
      trimPreviewResultRef.current = (e as CustomEvent).detail;
      // Render the highlight immediately from the core's data.
      renderTrimPreviewHighlight();
    };
    window.addEventListener("polysmith-trim-preview", onTrimPreview);

    resizeRenderer();

    const animate = () => {
      render();
      frameId = window.requestAnimationFrame(animate);
    };

    frameId = window.requestAnimationFrame(animate);

    return () => {
      onSnapshotCaptureReady?.(null);
      window.cancelAnimationFrame(frameId);
      if (pendingMoveGizmoFrameRef.current !== null) {
        window.cancelAnimationFrame(pendingMoveGizmoFrameRef.current);
        pendingMoveGizmoFrameRef.current = null;
      }
      pendingMoveGizmoParametersRef.current = null;
      resizeObserver.disconnect();
      renderer.domElement.removeEventListener("pointerdown", handlePointerDown);
      renderer.domElement.removeEventListener("pointermove", handlePointerMove);
      renderer.domElement.removeEventListener(
        "pointerleave",
        handlePointerLeave,
      );
      renderer.domElement.removeEventListener("pointerup", handlePointerUp);
      renderer.domElement.removeEventListener("contextmenu", handleContextMenu);
      renderer.domElement.removeEventListener("dblclick", handleDoubleClick);
      renderer.domElement.removeEventListener("wheel", handleWheel);
      window.removeEventListener("polysmith-trim-preview", onTrimPreview);
      clearDragPreviewLines();
      controls.dispose();
      disposeGroup(contentGroup);
      disposeGroup(referenceGroup);
      disposeGroup(sketchGroup);
      if (viewCubeGroupRef.current) {
        disposeViewCubeGroup(viewCubeGroupRef.current);
        viewCubeGroupRef.current = null;
      }
      viewCubeSceneRef.current = null;
      viewCubeCameraRef.current = null;
      viewCubeRaycasterRef.current = null;
      if (cubeBlitSceneRef.current) {
        disposeCubeBlitScene(cubeBlitSceneRef.current);
        cubeBlitSceneRef.current = null;
      }
      if (cubeRenderTargetRef.current) {
        cubeRenderTargetRef.current.dispose();
        cubeRenderTargetRef.current = null;
      }
      renderer.dispose();
      disposeDynamicGrid(worldGridRef.current);
      disposeDynamicGrid(sketchGridRef.current);
      rendererRef.current = null;
      sceneRef.current = null;
      cameraRef.current = null;
      controlsRef.current = null;
      contentGroupRef.current = null;
      referenceGroupRef.current = null;
      sketchGroupRef.current = null;
      clearViewportSceneObjectRefs();
      sketchEntityObjectByIdRef.current.clear();
      sketchPointObjectByIdRef.current.clear();
      moveGizmoDragRef.current = null;
      worldGridRef.current = null;
      sketchGridRef.current = null;
      lineDraftStartRef.current = null;
      lastGeometryKeyRef.current = "";
    };
  }, [activeSketchPlaneId]);

  useEffect(() => {
    syncViewportScene({
      groups: {
        scene: sceneRef.current,
        camera: cameraRef.current,
        controls: controlsRef.current,
        contentGroup: contentGroupRef.current,
        referenceGroup: referenceGroupRef.current,
        sketchGroup: sketchGroupRef.current,
      },
      refs: {
        pendingEndpointCommit: pendingEndpointCommitRef,
        endpointDrag: endpointDragRef,
        activeSketchPlaneFrame: activeSketchPlaneFrameRef,
        sketchEntityObjectById: sketchEntityObjectByIdRef,
        sketchPointObjectById: sketchPointObjectByIdRef,
        sketchConstraintObjects: sketchConstraintObjectsRef,
        dragCursor: dragCursorRef,
        lastGeometryKey: lastGeometryKeyRef,
        hoveredEdgeId: hoveredEdgeIdRef,
        hoveredVertexId: hoveredVertexIdRef,
        hoveredSketchEntityId: hoveredSketchEntityIdRef,
        hoveredSketchPointId: hoveredSketchPointIdRef,
        meshes: meshesRef,
        primitiveVisuals: primitiveVisualsRef,
        primitiveStates: primitiveStatesRef,
        referencePlaneMeshes: referencePlaneMeshesRef,
        referencePlaneVisuals: referencePlaneVisualsRef,
        referencePlaneStates: referencePlaneStatesRef,
        faceMeshes: faceMeshesRef,
        solidFaceVisuals: solidFaceVisualsRef,
        solidFaceStates: solidFaceStatesRef,
        edgeLineObjects: edgeLineObjectsRef,
        cutPreviewObjects: cutPreviewObjectsRef,
        toolpathLines: toolpathLinesRef,
        moveGizmoObjects: moveGizmoObjectsRef,
        hiddenRelationPreviewDimensionIds: hiddenRelationPreviewDimensionIdsRef,
        selectedConstraint: selectedConstraintRef,
        sketchEntityObjects: sketchEntityObjectsRef,
        sketchDimensionObjects: sketchDimensionObjectsRef,
        sketchProfileObjects: sketchProfileObjectsRef,
        sketchProfileVisuals: sketchProfileVisualsRef,
        sketchProfileStates: sketchProfileStatesRef,
        sketchPointObjects: sketchPointObjectsRef,
      },
      sceneData,
      document,
      viewport,
      displayedSketchDimensions,
      displayUnits: config.displayUnits,
      activeSketchPlaneId,
      activeSketchPlaneFrame,
      showReferencePlanes,
      showStock,
      wcsOrientation,
      moveGizmo,
      clearViewportSceneObjectRefs,
      clearDragPreviewLines,
      setConstraintPreview,
      syncPrimitiveVisuals,
      syncReferencePlaneVisuals,
      syncSolidFaceVisuals,
      syncSketchProfileVisuals,
      paintSketchEntityMaterials,
      paintSketchPointMaterials,
      paintDofStatusColors,
    });
  }, [activeTheme.id, config.displayUnits, displayedSketchDimensions, moveGizmo, sceneData, showReferencePlanes, document, viewport, showStock, wcsOrientation]);

  useEffect(() => {
    lineDraftStartRef.current = null;
    arcSecondPointRef.current = null;
    rectSecondPointRef.current = null;
    circleSecondPointRef.current = null;
    clearDragPreviewLines();
    clearPreviewLine();
    clearPreviewCircle();
    clearPreviewArc();
    clearPreviewDimension();
    setSketchSnapLabel(null);
    setConstraintPreview(null);
    clearDraftDimensionSession();
    cancelDimensionPlacement();
    pendingDimensionPlacementRef.current = false;
    // Reset the dimension tool's pending first-line on every tool
    // switch so it can't leak across tools or sketches.
    dimensionToolFirstLineRef.current = null;
    setDimensionToolFirstLine(null);
    dimensionToolFirstPointRef.current = null;
  }, [activeSketchPlaneId, activeSketchTool]);

  useEffect(
    () =>
      bindSketchHotkeys({
        activeSketchPlaneId,
        sketchToolbarHotkeys: config.hotkeys.sketchToolbar,
        document,
        activeSketchToolRef,
        dimensionLabelDragRef,
        dimensionPlacementOriginalPositionRef,
        pendingDimensionIdRef,
        pendingDimSourceEntityIdRef,
        pendingDimensionPlacementRef,
        controlsRef,
        selectedConstraintRef,
        sketchToolConstructionRef,
        deleteSketchDimensionRef,
        clearSketchConstraintRef,
        deleteSketchSelectionRef,
        setSketchToolRef,
        clearPreviewDimension,
        finishDimensionPlacement,
        setCanvasCursor,
        setSelectedConstraint,
        cancelActiveSketchDraft,
        setSketchToolConstruction,
      }),
    [activeSketchPlaneId, config.hotkeys.sketchToolbar],
  );

  // Tab toggles ghost-edge visibility while a fillet/chamfer panel is
  // open. The handler is mounted only when at least one body has a
  // pending edge-op feature so the key keeps its default browser
  // behavior in every other context. Hold to reveal, release to hide.
  useEffect(() => {
    if (pendingEdgeOpBodyIds.size === 0) {
      return;
    }
    function isTypingTarget(target: EventTarget | null) {
      return (
        target instanceof HTMLInputElement ||
        target instanceof HTMLTextAreaElement ||
        target instanceof HTMLSelectElement ||
        (target instanceof HTMLElement && target.isContentEditable)
      );
    }
    function setReveal(next: boolean) {
      if (revealGhostEdgesRef.current === next) {
        return;
      }
      revealGhostEdgesRef.current = next;
      paintEdgeMaterials(hoveredEdgeIdRef.current);
    }
    function handleKeyDown(event: KeyboardEvent) {
      if (event.code !== "Tab") {
        return;
      }
      if (isTypingTarget(event.target)) {
        return;
      }
      // Suppress the default Tab focus shuffle so the panel session
      // stays in control of the input the user just typed into.
      event.preventDefault();
      setReveal(true);
    }
    function handleKeyUp(event: KeyboardEvent) {
      if (event.code !== "Tab") {
        return;
      }
      setReveal(false);
    }
    function handleBlur() {
      // Window blur (e.g. user switched apps mid-hold) loses keyup
      // events; reset so the wireframe doesn't get stuck "on".
      setReveal(false);
    }
    window.addEventListener("keydown", handleKeyDown);
    window.addEventListener("keyup", handleKeyUp);
    window.addEventListener("blur", handleBlur);
    return () => {
      window.removeEventListener("keydown", handleKeyDown);
      window.removeEventListener("keyup", handleKeyUp);
      window.removeEventListener("blur", handleBlur);
      // Drop the reveal state so the next pending session starts
      // hidden by default.
      revealGhostEdgesRef.current = false;
    };
  }, [pendingEdgeOpBodyIds]);

  useEffect(() => {
    if (activeSketchPlaneId) {
      if (previousReferencePlaneVisibilityRef.current === null) {
        previousReferencePlaneVisibilityRef.current = showReferencePlanes;
      }

      if (showReferencePlanes) {
        setShowReferencePlanes(false);
      }
      return;
    }

    if (previousReferencePlaneVisibilityRef.current !== null) {
      setShowReferencePlanes(previousReferencePlaneVisibilityRef.current);
      previousReferencePlaneVisibilityRef.current = null;
    }
  }, [activeSketchPlaneId, showReferencePlanes]);

  const lastFramedSketchPlaneRef = useRef<string | null>(null);
  useEffect(() => {
    const camera = cameraRef.current;
    const controls = controlsRef.current;

    if (!camera || !controls) {
      return;
    }

    if (!activeSketchPlaneId) {
      lastFramedSketchPlaneRef.current = null;
      return;
    }

    if (lastFramedSketchPlaneRef.current === activeSketchPlaneId) {
      return;
    }

    if (!sceneData) {
      return;
    }

    lastFramedSketchPlaneRef.current = activeSketchPlaneId;

    frameCameraToSketchPlane(
      camera,
      controls,
      activeSketchPlaneId,
      activeSketchPlaneFrame,
      sceneData.bounds.maxDimension,
    );
  }, [activeSketchPlaneId, activeSketchPlaneFrame, sceneData]);

  async function handleCreateSketchFromContextMenu() {
    if (contextMenu?.referenceId) {
      setContextMenu(null);
      await selectReferenceRef.current(contextMenu.referenceId);
      await startSketchRef.current(contextMenu.referenceId);
      return;
    }

    if (!contextMenu?.faceId) {
      return;
    }

    setContextMenu(null);
    await selectFaceRef.current(contextMenu.faceId);

    const solidFace = sceneData?.solidFaces.find(
      (face) => face.faceId === contextMenu.faceId,
    );
    if (!solidFace) {
      return;
    }

    await startSketchOnFaceRef.current(solidFace.faceId, solidFace.planeFrame);
  }

  async function handleMoveBodyFromContextMenu() {
    const bodyId = contextMenu?.bodyId;
    if (!bodyId) {
      return;
    }
    setContextMenu(null);
    await moveBodyRef.current?.(bodyId);
  }

  async function handleCopyBodyFromContextMenu(copyMode: "linked" | "standalone") {
    const bodyId = contextMenu?.bodyId;
    if (!bodyId) {
      return;
    }
    setContextMenu(null);
    await copyBodyRef.current?.(bodyId, copyMode);
  }

  async function handleExportBodyMeshFromContextMenu() {
    const bodyId = contextMenu?.bodyId;
    if (!bodyId) {
      return;
    }
    setContextMenu(null);
    await exportBodyMeshRef.current?.(bodyId);
  }

  async function handleUnlinkBodyCopyFromContextMenu() {
    const bodyId = contextMenu?.bodyId;
    if (!bodyId) {
      return;
    }
    setContextMenu(null);
    await unlinkBodyCopyRef.current?.(bodyId);
  }

  async function handleDeleteSketchFromContextMenu() {
    const selection = contextMenu?.sketchDeleteSelection;
    if (!selection) {
      return;
    }
    setContextMenu(null);
    await deleteSketchSelectionRef.current(selection);
  }

  async function handleDeleteDimensionFromContextMenu() {
    const dimensionId = contextMenu?.dimensionId;
    if (!dimensionId) {
      return;
    }
    setContextMenu(null);
    setIsDimensionEditorOpen(false);
    await deleteSketchDimensionRef.current(dimensionId);
  }

  async function handleDeleteConstraintFromContextMenu() {
    const kind = contextMenu?.constraintKind;
    const entityId = contextMenu?.constraintEntityId;
    const constraintId = contextMenu?.constraintId;
    if (!kind || !entityId) {
      return;
    }
    setContextMenu(null);
    setSelectedConstraint(null);
    // For mirror and coincident, use the constraint_id (which encodes
    // the axis/point). For line-mounted constraints (H/V), use entityId.
    const deleteId =
      kind === "mirror" || kind === "coincident"
        ? constraintId ?? entityId
        : entityId;
    await clearSketchConstraintRef.current(
      kind as ConstraintType,
      deleteId,
      contextMenu?.constraintRelatedEntityId ?? null,
    );
  }

  async function handleToggleDimensionDisplayFromContextMenu() {
    const dimensionId = contextMenu?.dimensionId;
    if (!dimensionId) return;
    const sketch = sketchLinesRef.current;
    if (!sketch) return;
    const dim = sketch.dimensions.find(
      (d) => d.dimension_id === dimensionId,
    );
    if (!dim || dim.kind !== "circle_radius") return;

    // Toggle: "" (diameter) → "radius" → "" (diameter)
    const newDisplayAs = dim.display_as === "radius" ? "" : "radius";
    setContextMenu(null);
    await updateSketchDimensionDisplayRef.current(dimensionId, newDisplayAs);
  }

  const lineCount = sketchFeature?.sketch_parameters?.lines.length ?? 0;
  const circleCount = sketchFeature?.sketch_parameters?.circles.length ?? 0;
  const pointCount = sketchFeature?.sketch_parameters?.points.length ?? 0;
  const arcCount = sketchFeature?.sketch_parameters?.arcs.length ?? 0;

  function getCircleDimensionToggleLabel(dimensionId: string) {
    const sketch = sketchLinesRef.current;
    if (!sketch) {
      return null;
    }
    const dim = sketch.dimensions.find(
      (dimension) => dimension.dimension_id === dimensionId,
    );
    if (!dim || dim.kind !== "circle_radius") {
      return null;
    }
    return dim.display_as === "radius" ? "Show Diameter" : "Show Radius";
  }

  function isLinkedBodyCopy(bodyId: string | null | undefined) {
    if (!bodyId) {
      return false;
    }
    const feature = document?.feature_history.find(
      (entry) => entry.feature_id === bodyId,
    );
    return (
      feature?.kind === "body_copy" &&
      feature.body_copy_parameters?.copy_mode === "linked"
    );
  }

  async function handleSubmitDimensionEdit() {
    if (!selectedSketchDimension) {
      setIsDimensionEditorOpen(false);
      return;
    }

    const rawValue = dimensionDraftValue.trim();
    if (!rawValue) {
      setIsDimensionEditorOpen(false);
      return;
    }

    // If the value parses as a plain number, send it as a number
    // (backward compatible). Parse with display-unit conversion.
    // If it contains non-numeric characters (e.g. "width * 2"), send it
    // as a formula expression.
    // Angles are unitless (same in mm and inch) — skip displayToMm
    // and let dimensionCoreValue handle the degrees→radians conversion.
    const isAngle = selectedSketchDimension?.kind === "angle" ||
      selectedSketchDimension?.kind === "line_angle";
    let parsed: number | null;
    if (isAngle) {
      const normalized = rawValue.replace(",", ".");
      const p = parseFloat(normalized);
      parsed = isNaN(p) ? null : p;
    } else {
      parsed = parseDimensionInput(rawValue, config.displayUnits);
    }
    if (parsed !== null && parsed > 0) {
      await updateSketchDimensionRef.current(
        selectedSketchDimension.dimensionId,
        dimensionCoreValue(selectedSketchDimension, parsed),
      );
    } else {
      // Send as expression string — the core will evaluate it
      await updateSketchDimensionRef.current(
        selectedSketchDimension.dimensionId,
        rawValue,
      );
    }
    finishDimensionPlacement();
    dimensionEditOriginalValueRef.current = null;
    setIsDimensionEditorOpen(false);
  }

  function handleDimensionDraftChange(value: string) {
    setDimensionDraftValue(value);
    if (!selectedSketchDimension) {
      return;
    }
    const trimmed = value.trim();
    if (!trimmed) {
      return;
    }
    // Only send numeric values as live preview. Expressions
    // (parameter names, formulas) are held until Enter — partial
    // keystrokes like "t", "te" would otherwise flood the core
    // with "unknown parameter" errors before the name is complete.
    // Angles are unitless — skip displayToMm.
    const isAngle = selectedSketchDimension?.kind === "angle" ||
      selectedSketchDimension?.kind === "line_angle";
    let parsed: number | null;
    if (isAngle) {
      const normalized = trimmed.replace(",", ".");
      const p = parseFloat(normalized);
      parsed = isNaN(p) ? null : p;
    } else {
      parsed = parseDimensionInput(trimmed, config.displayUnits);
    }
    if (parsed !== null && parsed > 0) {
      void updateSketchDimensionRef.current(
        selectedSketchDimension.dimensionId,
        dimensionCoreValue(selectedSketchDimension, parsed),
      );
    }
    // Send parameter names / expressions with a 300ms debounce so the
    // core doesn't flood with partial parameter names ("t", "te", ...).
    else if (/[a-zA-Z_]/.test(trimmed)) {
      if (dimensionExpressionTimeoutRef.current !== null) {
        clearTimeout(dimensionExpressionTimeoutRef.current);
      }
      dimensionExpressionTimeoutRef.current = setTimeout(() => {
        dimensionExpressionTimeoutRef.current = null;
        void updateSketchDimensionRef.current(
          selectedSketchDimension.dimensionId,
          trimmed,
        ).catch(() => {});
      }, 300);
    }
  }

  function insertDimensionParameterSuggestion(name: string) {
    const input = dimensionInputRef.current;
    const cursor = input?.selectionStart ?? dimensionDraftValue.length;
    const token = parameterTokenAtCursor(dimensionDraftValue, cursor);
    const start = token?.start ?? cursor;
    const end = token?.end ?? cursor;
    const nextValue =
      dimensionDraftValue.slice(0, start) +
      name +
      dimensionDraftValue.slice(end);
    setDimensionDraftValue(nextValue);
    handleDimensionDraftChange(nextValue);
    window.requestAnimationFrame(() => {
      const nextCursor = start + name.length;
      input?.focus();
      input?.setSelectionRange(nextCursor, nextCursor);
    });
  }

  function cancelDimensionEdit() {
    // Cancel any pending debounced expression send
    if (dimensionExpressionTimeoutRef.current !== null) {
      clearTimeout(dimensionExpressionTimeoutRef.current);
      dimensionExpressionTimeoutRef.current = null;
    }
    const dimension = selectedSketchDimension;
    const originalValue = dimensionEditOriginalValueRef.current;
    cancelDimensionPlacement();
    if (dimension && originalValue?.dimensionId === dimension.dimensionId) {
      void updateSketchDimensionRef.current(
        dimension.dimensionId,
        originalValue.expression.trim().length > 0
          ? originalValue.expression
          : originalValue.value,
      );
      setDimensionLabelPositions((current) => {
        if (!(dimension.dimensionId in current)) {
          return current;
        }
        const next = { ...current };
        delete next[dimension.dimensionId];
        return next;
      });
      setDimensionDraftValue(
        originalValue.expression.trim().length > 0
          ? originalValue.expression
          : formattedDimensionDisplayValue(dimension, originalValue.value),
      );
    } else if (dimension && selectedSketchDimensionValue !== null) {
      setDimensionDraftValue(
        selectedSketchDimensionExpression.trim().length > 0
          ? selectedSketchDimensionExpression
          : formattedDimensionDisplayValue(dimension, selectedSketchDimensionValue),
      );
    } else {
      setDimensionDraftValue("");
    }
    dimensionEditOriginalValueRef.current = null;
    setIsDimensionEditorOpen(false);
  }

  function draftFieldScreenPosition(field: DraftDimensionField) {
    const session = draftDimensionSession;
    // For line tool, use the render-loop-computed screen positions from
    // the 3D dimension lines. These are updated every frame and match
    // the Three.js geometry drawn in renderDraftDimensions().
    if (session?.tool === "line") {
      const fromRef = draftDimScreenPositionsRef.current[field];
      if (fromRef) {
        return fromRef;
      }
      // Fall through to legacy path if the render loop hasn't computed
      // positions yet (e.g., first frame).
    }

    if (!cameraRef.current || !rendererRef.current) {
      return null;
    }
    if (!session) return null;

    const [sx, sy] = session.start;
    const [ex, ey] = session.current;
    let local: [number, number] = session.current;
    let offset: [number, number] = [0, -DRAFT_DIMENSION_OFFSET_PX];

    if (session.tool === "rectangle") {
      if (field === "width") {
        local = [(sx + ex) / 2, sy];
        offset = [0, -DRAFT_DIMENSION_OFFSET_PX];
      } else {
        local = [ex, (sy + ey) / 2];
        offset = [DRAFT_DIMENSION_OFFSET_PX, 0];
      }
    } else if (session.tool === "line") {
      if (field === "angle") {
        local = [sx, sy];
        offset = [0, -DRAFT_DIMENSION_OFFSET_PX];
      } else {
        local = [(sx + ex) / 2, (sy + ey) / 2];
        offset = [0, -DRAFT_DIMENSION_OFFSET_PX];
      }
    } else {
      local = session.start;
      offset = [0, -DRAFT_DIMENSION_OFFSET_PX];
    }

    const world = toWorldPoint(
      activeSketchPlaneId ?? "ref-plane-xy",
      local,
      activeSketchPlaneFrame,
    );
    const point = projectWorldPointToViewport(
      world,
      cameraRef.current,
      rendererRef.current,
    );
    if (!point) {
      return null;
    }
    return {
      x: point.x + offset[0],
      y: point.y + offset[1],
    };
  }

  function draftDisplayValue(rawValue: string): string {
    if (config.displayUnits === "mm") return rawValue;
    const num = Number(rawValue);
    if (!Number.isFinite(num) || num <= 0) return rawValue;
    const display = mmToDisplay(num, config.displayUnits);
    return String(parseFloat(display.toFixed(3)));
  }

  function handleDraftDimensionChange(
    field: DraftDimensionField,
    value: string,
  ) {
    const session = draftDimensionSessionRef.current;
    if (!session) {
      return;
    }
    // Preserve raw input during editing so partial values like "2."
    // don't lose the decimal when the round-trip through mm converts
    // them back to display.
    draftRawInputRef.current[field] = value;
    // Convert display-unit input to mm for internal storage
    const parsed = parseDimensionInput(value, config.displayUnits);
    let mmValue: string;
    if (parsed !== null) {
      mmValue = String(parsed);
      delete draftParameterExpressionRef.current[field];
    } else if (/[a-zA-Z_]/.test(value)) {
      draftParameterExpressionRef.current[field] = value.trim();
      // Try to resolve as a parameter name for live draft preview.
      // The draft dimension system is client-side, so we look up the
      // parameter in the current document state.  Angle parameters
      // store degrees, length parameters store mm — both match what
      // applyDraftDimensionFieldValue expects for their respective fields.
      const param = document?.parameters.find((p) => p.name === value.trim());
      if (param && !param.has_error && Number.isFinite(param.resolved_value) && param.resolved_value > 0) {
        mmValue = String(param.resolved_value);
      } else {
        mmValue = value;
      }
    } else {
      delete draftParameterExpressionRef.current[field];
      mmValue = value;
    }
    const next = applyDraftDimensionField(session, field, mmValue);
    draftDimensionSessionRef.current = next;
    // Clear all render-loop positions so both fields get fresh
    // fallback positions.  Changing the length also moves the angle
    // arc endpoint — clearing only the changed field leaves the
    // other stuck at its old screen position until the next frame.
    draftDimScreenPositionsRef.current = {};
    setDraftDimensionSession(next);
    setDraftSuggestionState({ field, index: 0 });
  }

  function getDraftFieldInputValue(
    session: DraftDimensionSession,
    field: DraftDimensionField,
  ) {
    if (
      draftFieldFocusedRef.current === field &&
      draftRawInputRef.current[field] !== undefined
    ) {
      return draftRawInputRef.current[field] ?? "";
    }
    const expression = draftParameterExpressionRef.current[field];
    if (expression && expression.trim().length > 0) {
      return expression;
    }
    return draftDisplayValue(session.values[field]);
  }

  function getDraftParameterSuggestions(
    field: DraftDimensionField,
    value: string,
  ) {
    const input = draftDimensionInputRefs.current[field];
    const cursor = input?.selectionStart ?? value.length;
    return getParameterSuggestions(value, cursor, field === "angle");
  }

  function insertDraftParameterSuggestion(
    field: DraftDimensionField,
    name: string,
  ) {
    const input = draftDimensionInputRefs.current[field];
    const currentValue = input?.value ?? draftRawInputRef.current[field] ?? "";
    const cursor = input?.selectionStart ?? currentValue.length;
    const token = parameterTokenAtCursor(currentValue, cursor);
    const start = token?.start ?? cursor;
    const end = token?.end ?? cursor;
    const nextValue =
      currentValue.slice(0, start) + name + currentValue.slice(end);
    handleDraftDimensionChange(field, nextValue);
    window.requestAnimationFrame(() => {
      const nextCursor = start + name.length;
      input?.focus();
      input?.setSelectionRange(nextCursor, nextCursor);
    });
  }

  function focusDraftField(field: DraftDimensionField) {
    window.requestAnimationFrame(() => {
      draftDimensionInputRefs.current[field]?.focus();
      draftDimensionInputRefs.current[field]?.select();
    });
  }

  function handleDraftDimensionKeyDown(
    event: ReactKeyboardEvent<HTMLInputElement>,
    field: DraftDimensionField,
  ) {
    const session = draftDimensionSessionRef.current;
    if (!session) {
      return;
    }
    const suggestions = getDraftParameterSuggestions(
      field,
      event.currentTarget.value,
    );
    if (
      suggestions.length > 0 &&
      (event.key === "ArrowDown" || event.key === "ArrowUp")
    ) {
      event.preventDefault();
      setDraftSuggestionState((current) => {
        const currentIndex =
          current?.field === field ? current.index : 0;
        const delta = event.key === "ArrowDown" ? 1 : -1;
        return {
          field,
          index:
            (currentIndex + delta + suggestions.length) %
            suggestions.length,
        };
      });
      return;
    }
    if (
      suggestions.length > 0 &&
      (event.key === "Tab" || event.key === "Enter")
    ) {
      event.preventDefault();
      const suggestionIndex =
        draftSuggestionState?.field === field
          ? draftSuggestionState.index
          : 0;
      const suggestion = suggestions[suggestionIndex] ?? suggestions[0];
      insertDraftParameterSuggestion(field, suggestion.name);
      return;
    }
    if (event.key === "Enter") {
      event.preventDefault();
      void commitDraftDimensionSession(session);
      void setSketchToolRef.current("select");
      return;
    }
    if (event.key === "Escape") {
      event.preventDefault();
      event.stopPropagation();
      cancelActiveSketchDraft();
      return;
    }
    if (event.key !== "Tab") {
      return;
    }
    event.preventDefault();
    const fields = draftSessionFields(session.tool);
    const index = fields.indexOf(field);
    const nextField =
      fields[
        (index + (event.shiftKey ? -1 : 1) + fields.length) % fields.length
      ];
    const next = { ...session, activeField: nextField };
    draftDimensionSessionRef.current = next;
    setDraftDimensionSession(next);
    focusDraftField(nextField);
  }

  const isSketchDrawingCursor =
    Boolean(activeSketchPlaneId) &&
    activeSketchTool !== "select" &&
    activeSketchTool !== "project";
  const crosshairMode = config.viewport.crosshair;
  const usesCrosshairGuide =
    crosshairMode === "viewport-25" ||
    crosshairMode === "viewport-50" ||
    crosshairMode === "viewport-75" ||
    crosshairMode === "infinite";
  const crosshairGuideSize =
    crosshairMode === "infinite"
      ? Math.max(viewportSize.width, viewportSize.height) * 2
      : viewportSize.height * (CROSSHAIR_SIZE_FACTORS[crosshairMode] ?? 0);
  const crosshairCanvasClass = isSketchDrawingCursor
    ? [
        "cad-viewport-canvas-drawing",
        usesCrosshairGuide ? "cad-viewport-canvas-drawing-guide" : "",
      ]
        .filter(Boolean)
        .join(" ")
    : "";
  const isSketchMode = Boolean(activeSketchPlaneId);

  return (
    <section className="relative flex h-full min-h-0 flex-col overflow-hidden">
      {showViewportGrid && !activeSketchPlaneId ? (
        <div className="pointer-events-none absolute inset-0 cad-grid-stage opacity-70" />
      ) : null}
      <div
        ref={hostRef}
        className="absolute inset-0 min-h-0 min-w-0 overflow-hidden rounded-[18px]"
      >
        {contextMenu ? (
          <ViewportContextMenu
            contextMenu={contextMenu}
            translate={translate}
            getCircleDimensionToggleLabel={getCircleDimensionToggleLabel}
            isLinkedBodyCopy={isLinkedBodyCopy}
            onToggleDimensionDisplay={handleToggleDimensionDisplayFromContextMenu}
            onDeleteDimension={handleDeleteDimensionFromContextMenu}
            onDeleteConstraint={handleDeleteConstraintFromContextMenu}
            onDeleteSketchSelection={handleDeleteSketchFromContextMenu}
            onMoveBody={handleMoveBodyFromContextMenu}
            onCopyBody={handleCopyBodyFromContextMenu}
            onUnlinkBodyCopy={handleUnlinkBodyCopyFromContextMenu}
            onExportBodyMesh={handleExportBodyMeshFromContextMenu}
            onCreateSketch={handleCreateSketchFromContextMenu}
          />
        ) : null}
        <canvas
          ref={canvasRef}
          className={`cad-viewport-canvas absolute inset-0 h-full w-full ${crosshairCanvasClass}`}
        />
        {!isSketchMode ? (
          <div className="pointer-events-auto absolute bottom-4 left-1/2 z-20 -translate-x-1/2">
            <div className="cad-view-mini-toolbar flex items-center gap-1 px-1.5 py-1.5 backdrop-blur-xl">
              <ToolbarTooltip
                label={`${
                  showViewportGrid
                    ? translate("viewport.hideViewportGrid")
                    : translate("viewport.showViewportGrid")
                } (${formatHotkey(config.hotkeys.viewport.toggleGrid)})`}
              >
                <button
                  type="button"
                  className={
                    showViewportGrid
                      ? "cad-view-mini-button cad-view-mini-button-active"
                      : "cad-view-mini-button"
                  }
                  aria-label={
                    showViewportGrid
                      ? translate("viewport.hideViewportGrid")
                      : translate("viewport.showViewportGrid")
                  }
                  aria-pressed={showViewportGrid}
                  onClick={() => {
                    toggleGridVisibility("viewport");
                  }}
                >
                  <GridMiniIcon />
                </button>
              </ToolbarTooltip>
            </div>
          </div>
        ) : (
          <div className="pointer-events-auto absolute bottom-4 left-1/2 z-20 -translate-x-1/2">
            <div className="cad-view-mini-toolbar flex items-center gap-1 px-1.5 py-1.5 backdrop-blur-xl">
              <ToolbarTooltip
                label={`${
                  showSketchGrid
                    ? translate("viewport.hideSketchGrid")
                    : translate("viewport.showSketchGrid")
                } (${formatHotkey(config.hotkeys.viewport.toggleGrid)})`}
              >
                <button
                  type="button"
                  className={
                    showSketchGrid
                      ? "cad-view-mini-button cad-view-mini-button-active"
                      : "cad-view-mini-button"
                  }
                  aria-label={
                    showSketchGrid
                      ? translate("viewport.hideSketchGrid")
                      : translate("viewport.showSketchGrid")
                  }
                  aria-pressed={showSketchGrid}
                  onClick={() => {
                    toggleGridVisibility("sketch");
                  }}
                >
                  <GridMiniIcon />
                </button>
              </ToolbarTooltip>
            </div>
          </div>
        )}
        {isSketchDrawingCursor &&
        usesCrosshairGuide &&
        crosshairPointer &&
        crosshairGuideSize > 0 ? (
          <div
            className="cad-crosshair-guide"
            style={{
              left: crosshairPointer.x,
              top: crosshairPointer.y,
              width: crosshairGuideSize,
              height: crosshairGuideSize,
              transform: "translate(-50%, -50%)",
            }}
          />
        ) : null}
        {/* Selection rectangle overlay */}
        {selectionRect?.visible ? (
          <div
            className="pointer-events-none fixed z-30"
            style={{
              left: selectionRect.left + 'px',
              top: selectionRect.top + 'px',
              width: selectionRect.width + 'px',
              height: selectionRect.height + 'px',
              border: selectionRect.direction === "window"
                ? "1px solid var(--color-primary-edge-active, #4fc3f7)"
                : "1px dashed var(--color-destructive, #4caf50)",
              background: selectionRect.direction === "window"
                ? "rgba(79, 195, 247, 0.07)"
                : "rgba(76, 175, 80, 0.07)",
            }}
          />
        ) : null}

        {/*
          Cursor-following constraint preview badge. Only visible
          while a sketch tool is producing a midpoint, perpendicular,
          or on-line snap. The badge is offset 12px down-right from
          the cursor so it doesn't sit under the actual snap dot, and
          is `pointer-events-none` so it never steals clicks from the
          underlying canvas. The colors mirror the in-scene
          constraint-badge palette to keep the language consistent.
        */}
        {constraintPreview ? (
          <div
            className="pointer-events-none absolute z-30 flex h-5 w-5 items-center justify-center rounded-full border border-cyan-300/70 bg-slate-900/85 text-[10px] font-semibold text-cyan-200 shadow-md"
            style={{
              left: `${constraintPreview.x + 12}px`,
              top: `${constraintPreview.y + 12}px`,
            }}
          >
            {constraintPreview.kind === "midpoint"
              ? "M"
              : constraintPreview.kind === "perpendicular"
                ? "\u22a5"
                : constraintPreview.kind === "horizontal"
                  ? "H"
                  : constraintPreview.kind === "vertical"
                    ? "V"
                    : constraintPreview.kind === "tangent"
                      ? "T"
                      : constraintPreview.kind === "endpoint"
                        ? "\u25cf"
                        : constraintPreview.kind === "parallel"
                          ? "\u2225"
              : "/"}
          </div>
        ) : null}
        {draftDimensionSession
          ? draftSessionFields(draftDimensionSession.tool).map((field) => {
              const position = draftFieldScreenPosition(field);
              if (!position) {
                return null;
              }
              const inputValue = getDraftFieldInputValue(
                draftDimensionSession,
                field,
              );
              const suggestions = getDraftParameterSuggestions(
                field,
                inputValue,
              );
              const suggestionIndex =
                draftSuggestionState?.field === field
                  ? draftSuggestionState.index
                  : 0;
              return (
                <form
                  key={field}
                  className="pointer-events-auto absolute z-30 flex w-[120px] items-center rounded-md border px-2 py-1 backdrop-blur-md"
                  style={{
                    left: position.x,
                    top: position.y,
                    transform: "translate(-50%, -50%)",
                    opacity: 0.65,
                    background: "var(--cad-dimension-editor-bg)",
                    borderColor: "var(--cad-dimension-editor-border)",
                    boxShadow:
                      "0 4px 12px var(--cad-dimension-editor-shadow)",
                  }}
                  onSubmit={(event) => {
                    event.preventDefault();
                    void commitDraftDimensionSession();
                  }}
                >
                  <input
                    ref={(input) => {
                      draftDimensionInputRefs.current[field] = input;
                    }}
                    className="h-6 w-full bg-transparent text-center text-sm font-semibold text-on-surface tabular-nums outline-none"
                    value={inputValue}
                    inputMode="text"
                    onChange={(event) => {
                      handleDraftDimensionChange(field, event.target.value);
                    }}
                    onFocus={() => {
                      draftFieldFocusedRef.current = field;
                      const next = {
                        ...draftDimensionSession,
                        activeField: field,
                      };
                      draftDimensionSessionRef.current = next;
                      setDraftDimensionSession(next);
                      setDraftSuggestionState({ field, index: 0 });
                    }}
                    onBlur={() => {
                      draftFieldFocusedRef.current = null;
                      if (!draftParameterExpressionRef.current[field]) {
                        delete draftRawInputRef.current[field];
                      }
                    }}
                    onKeyDown={(event) => {
                      handleDraftDimensionKeyDown(event, field);
                    }}
                  />
                  {suggestions.length > 0 ? (
                    <div
                      className="absolute left-0 top-[calc(100%+0.35rem)] w-[220px] overflow-hidden rounded-lg border border-surface-high bg-surface-container py-1 text-left shadow-xl"
                      onMouseDown={(event) => event.preventDefault()}
                    >
                      {suggestions.map((suggestion, index) => (
                        <button
                          key={suggestion.name}
                          type="button"
                          className={`flex w-full items-center justify-between gap-3 px-3 py-1.5 text-xs ${
                            index === suggestionIndex
                              ? "bg-surface-bright text-on-surface"
                              : "text-on-surface-muted hover:bg-surface-high hover:text-on-surface"
                          }`}
                          onClick={() =>
                            insertDraftParameterSuggestion(field, suggestion.name)
                          }
                        >
                          <span className="min-w-0 truncate font-mono">
                            {suggestion.name}
                          </span>
                          <span className="shrink-0 text-[10px] uppercase tracking-[0.12em] text-on-surface-dim">
                            {suggestion.kind}
                          </span>
                        </button>
                      ))}
                    </div>
                  ) : null}
                </form>
              );
            })
          : null}
        {activeSketchPlaneId ? (
          <SketchToolPanel
            translate={translate}
            activeSketchTool={activeSketchTool}
            sketchToolConstruction={sketchToolConstruction}
            arcToolMode={arcToolMode}
            circleToolMode={circleToolMode}
            rectangleToolMode={rectangleToolMode}
            polygonToolMode={polygonToolMode}
            polygonSides={polygonSides}
            onConstructionChange={(checked) => {
              sketchToolConstructionRef.current = checked;
              setSketchToolConstruction(checked);
            }}
            onSetArcToolMode={onSetArcToolMode}
            onSetCircleToolMode={onSetCircleToolMode}
            onSetRectangleToolMode={onSetRectangleToolMode}
            onSetPolygonToolMode={onSetPolygonToolMode}
            onPolygonSidesChange={(value) => {
              setPolygonSides(value);
              polygonSidesRef.current = value;
            }}
          />
        ) : null}
        {/*
          Floating Dimension Tool hint panel. Active only while the
          dim tool is armed; updates from "click a line / circle" to
          "click second line for angle, or same line for length"
          after the first line is picked. Mirrors the Line Tool
          panel's placement so the user always knows where to look.
        */}
        {activeSketchPlaneId && activeSketchTool === "dimension" ? (
          <div className="cad-floating-panel pointer-events-auto absolute left-4 top-4 z-20 flex flex-col gap-1 px-3 py-2 text-xs">
            <p className="text-[10px] uppercase tracking-[0.18em] text-on-surface-dim">
              {translate("viewport.dimensionTool")}{" "}
              <span className="opacity-60">
                ({formatHotkey(config.hotkeys.sketchToolbar.dimension)})
              </span>
            </p>
            <p className="text-on-surface">
              {dimensionToolFirstLine === null ? (
                <>{translate("viewport.placeDimension")}</>
              ) : (
                <>
                  {translate("viewport.dimensionReady")}
                </>
              )}
            </p>
          </div>
        ) : null}
        {selectedSketchDimension &&
        activeSketchPlaneId &&
        isDimensionEditorOpen ? (
          <form
            ref={dimensionEditorRef}
            className="pointer-events-none absolute z-20 flex w-[172px] items-center gap-1 rounded-md border px-2 py-1 backdrop-blur-md"
            style={{
              left: 0,
              top: 0,
              opacity: 0,
              background: "var(--cad-dimension-editor-bg)",
              borderColor: "var(--cad-dimension-editor-border)",
              boxShadow: "0 4px 12px var(--cad-dimension-editor-shadow)",
            }}
            onSubmit={(event) => {
              event.preventDefault();
              void handleSubmitDimensionEdit();
            }}
          >
            <input
              ref={dimensionInputRef}
              className="h-6 min-w-0 flex-1 bg-transparent text-center text-sm font-medium text-on-surface tabular-nums outline-none pointer-events-none"
              type="text"
              inputMode="text"
              value={dimensionDraftValue}
              onChange={(event) => {
                dimensionInputSelectionLockedRef.current = false;
                handleDimensionDraftChange(event.target.value);
              }}
              onFocus={(event) => {
                if (dimensionInputSelectionLockedRef.current) {
                  event.currentTarget.select();
                }
              }}
              onKeyDown={(event) => {
                dimensionInputSelectionLockedRef.current = false;
                if (
                  dimensionParameterSuggestions.length > 0 &&
                  (event.key === "ArrowDown" || event.key === "ArrowUp")
                ) {
                  event.preventDefault();
                  setDimensionSuggestionIndex((current) => {
                    const delta = event.key === "ArrowDown" ? 1 : -1;
                    return (
                      current +
                      delta +
                      dimensionParameterSuggestions.length
                    ) % dimensionParameterSuggestions.length;
                  });
                  return;
                }
                if (
                  dimensionParameterSuggestions.length > 0 &&
                  (event.key === "Tab" || event.key === "Enter")
                ) {
                  event.preventDefault();
                  const suggestion =
                    dimensionParameterSuggestions[dimensionSuggestionIndex] ??
                    dimensionParameterSuggestions[0];
                  insertDimensionParameterSuggestion(suggestion.name);
                  return;
                }
                if (event.key === "Escape") {
                  event.preventDefault();
                  // During placement, the dimension hasn't been committed
                  // yet — delete it entirely, matching the global Escape
                  // handler's behaviour when the input isn't focused.
                  const drag = dimensionLabelDragRef.current;
                  if (drag?.isPlacement && drag.dimensionId) {
                    const dimId = drag.dimensionId;
                    dimensionLabelDragRef.current = null;
                    dimensionPlacementOriginalPositionRef.current = null;
                    if (controlsRef.current) controlsRef.current.enabled = true;
                    setCanvasCursor("");
                    setIsDimensionEditorOpen(false);
                    dimensionEditOriginalValueRef.current = null;
                    void deleteSketchDimensionRef.current(dimId);
                  } else {
                    cancelDimensionEdit();
                  }
                }
              }}
            />

            {dimensionParameterSuggestions.length > 0 ? (
              <div
                className="pointer-events-auto absolute left-0 top-[calc(100%+0.35rem)] w-[220px] overflow-hidden rounded-lg border border-surface-high bg-surface-container py-1 text-left shadow-xl"
                onMouseDown={(event) => event.preventDefault()}
              >
                {dimensionParameterSuggestions.map((suggestion, index) => (
                  <button
                    key={suggestion.name}
                    type="button"
                    className={`flex w-full items-center justify-between gap-3 px-3 py-1.5 text-xs ${
                      index === dimensionSuggestionIndex
                        ? "bg-surface-bright text-on-surface"
                        : "text-on-surface-muted hover:bg-surface-high hover:text-on-surface"
                    }`}
                    onClick={() =>
                      insertDimensionParameterSuggestion(suggestion.name)
                    }
                  >
                    <span className="min-w-0 truncate font-mono">
                      {suggestion.name}
                    </span>
                    <span className="shrink-0 text-[10px] uppercase tracking-[0.12em] text-on-surface-dim">
                      {suggestion.kind}
                    </span>
                  </button>
                ))}
              </div>
            ) : null}
          </form>
        ) : null}
        {!hasActiveDocument ? (
          <div
            className="absolute inset-0 flex items-center justify-center backdrop-blur-sm"
            style={{ background: "var(--cad-overlay-strong)" }}
          >
            <div className="text-center">
              <p className="cad-kicker">{translate("viewport.title")}</p>
              <p className="mt-4 text-sm text-on-surface-muted">
                {translate("viewport.noActiveDocument")}
              </p>
            </div>
          </div>
        ) : null}
        {status === "starting" ? (
          <div
            className="pointer-events-none absolute inset-0 flex items-center justify-center backdrop-blur-sm"
            style={{ background: "var(--cad-overlay-soft)" }}
          >
            <div className="cad-floating-panel flex min-w-[220px] items-center gap-4 px-5 py-4">
              <span className="cad-loader-spinner" aria-hidden="true" />
              <div>
                <p className="cad-kicker">{translate("viewport.coreStartup")}</p>
                <p className="mt-2 text-sm text-on-surface-muted">
                  {translate("viewport.startingCore")}
                </p>
              </div>
            </div>
          </div>
        ) : null}
        {hasActiveDocument ? (
          <>
            <div className="pointer-events-none absolute bottom-4 right-4 cad-floating-panel px-4 py-3 text-right">
              <p className="text-[11px] uppercase tracking-[0.18em] text-on-surface-dim">
                {translate("common.selection")}
              </p>
              <p className="mt-1 text-sm text-on-surface-muted">
                {selectedReference?.label ??
                  selectedPrimitiveLabel ??
                  translate("viewport.noSelection")}
              </p>
              {measurementText ? (
                <p className="mt-1 text-sm text-primary-soft">
                  {measurementText}
                </p>
              ) : null}
              <p className="mt-1 text-xs uppercase tracking-[0.14em] text-on-surface-dim">
                {/* Plane / face ids are internal — see
                    AGENTS.md UI Copy Rules. The status line just
                    reports the active tool and entity counts. */}
                {activeSketchPlaneId
                  ? translate("viewport.sketchStatus", {
                      tool: activeSketchTool,
                      lineCount, linePlural: lineCount === 1 ? "" : "s",
                      circleCount, circlePlural: circleCount === 1 ? "" : "s",
                      pointCount, pointPlural: pointCount === 1 ? "" : "s",
                      arcCount, arcPlural: arcCount === 1 ? "" : "s",
                    })
                  : translate("viewport.noActiveSketch")}
              </p>
              {activeSketchPlaneId ? (
                <>
                  <p className="mt-1 text-xs text-on-surface-dim">
                    {/* Status text — never embed internal ids; see
                        AGENTS.md "UI Copy Rules". The selection
                        details (entity / point / dimension /
                        profile) just acknowledge that something is
                        selected; specifics live in the floating
                        panels keyed off those selections. */}
                    {armedSketchConstraint
                      ? armedSketchConstraint.kind === "coincident"
                        ? armedSketchConstraint.firstPointId
                          ? translate("constraints.coincidentSecondPoint")
                          : translate("constraints.coincidentFirstPoint")
                        : armedSketchConstraint.kind === "equal_length" ||
                            armedSketchConstraint.kind === "perpendicular" ||
                            armedSketchConstraint.kind === "parallel"
                          ? armedSketchConstraint.firstLineId
                            ? translate("constraints.lineSecond", {
                                label:
                                  armedSketchConstraint.kind === "equal_length"
                                    ? translate("toolbar.equalLength")
                                    : armedSketchConstraint.kind ===
                                        "perpendicular"
                                      ? translate("toolbar.perpendicular")
                                      : translate("toolbar.parallel"),
                              })
                            : translate("constraints.lineFirst", {
                                label:
                                  armedSketchConstraint.kind === "equal_length"
                                    ? translate("toolbar.equalLength")
                                    : armedSketchConstraint.kind ===
                                        "perpendicular"
                                      ? translate("toolbar.perpendicular")
                                      : translate("toolbar.parallel"),
                              })
                          : translate("constraints.lineConstraint", {
                              kind: armedSketchConstraint.kind,
                            })
                      : document?.selected_sketch_entity_id
                        ? (
                            (document?.selected_sketch_dimension_id
                              ? translate("viewport.dimensionSelected")
                              : (selectedEntityDof
                                ? translate("viewport.entitySelectedDof", {
                                    entity: selectedEntityDof.entity_kind,
                                    dof: selectedEntityDof.total_dof,
                                    consumed: selectedEntityDof.consumed_dof,
                                    status: selectedEntityDof.status === "over"
                                      ? translate("viewport.dofOver")
                                      : selectedEntityDof.status === "full"
                                        ? translate("viewport.dofFull") : "",
                                  })
                                : translate("viewport.entitySelected"))))
                        : sketchSnapLabel
                          ? `Snap: ${sketchSnapLabel}`
                          : document?.selected_sketch_point_id
                            ? translate("viewport.pointSelected")
                            : document?.selected_sketch_profile_id
                              ? translate("viewport.profileSelected")
                              : selectedConstraint
                                ? translate("viewport.constraintSelected", { kind: selectedConstraint.kind })
                                : activeSketchTool === "select"
                                ? translate("viewport.selectionMode")
                                : activeSketchTool === "project"
                                  ? translate("viewport.projectPrompt")
                                  : activeSketchTool === "line" &&
                                      lineDraftStartRef.current
                                    ? translate("viewport.lineChainActive")
                                    : translate("viewport.clickPlaceGeometry")}
                </p>
                  <button
                    type="button"
                    className="pointer-events-auto mt-3 ml-auto flex cad-ribbon-action cad-ribbon-action-primary"
                    disabled={status !== "connected"}
                    onClick={() => {
                      void onFinishSketch();
                    }}
                  >
                    {translate("toolbar.finishSketch")}
                  </button>
                </>
              ) : null}
            </div>
          </>
        ) : null}
      </div>
    </section>
  );
}
