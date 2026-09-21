import {
  useCallback,
  useEffect,
  useMemo,
  useRef,
  useState,
} from "react";
import { useTranslation } from "react-i18next";
import * as THREE from "three";
import { OrbitControls } from "three/examples/jsm/controls/OrbitControls.js";
import { sheetSceneOffsetX } from "@/lib/drawingViewMath";
import { getShowHiddenEdges } from "@/utils/viewport/primitiveObjects";
import { setPointerNdcFromEvent } from "@/utils/viewport/viewportMath";
import {
  applyTheme,
  useAppConfig,
} from "@/config";
import type {
  CornerTrimPreviewResultEvent,
  DocumentState,
  SketchConstraintScene,
  SketchTool,
  TrimPreviewResultEvent,
  ViewportState,
  SketchDimensionScene,
  PrimitiveVisual,
  PrimitiveInteractionState,
  ReferencePlaneVisual,
  ReferencePlaneInteractionState,
  SolidFaceVisual,
  SolidFaceInteractionState,
  SketchProfileVisual,
  SketchProfileInteractionState,
  ViewportContextMenuState,
  MoveFeatureParameters,
  SelectionFilter,
} from "@/types";
import {
  axisAlignedRectangleCorners2d,
  disposeGroup,
  disposeMaterial,
  distanceBetweenPoints,
  frameCameraToSketchPlane,
  projectWorldPointToViewport,
  SKETCH_PLANE_OFFSET,
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
  resolveSketchPlanePoint,
  buildSketchDimensionObject,
} from "@/utils";
import type { ViewCubeHit, CubeBlitScene } from "@/utils";
import { makeGetViewportStateCommand } from "@/lib/ipcProtocol";
import { useCadCoreStore } from "@/state/cadCoreStore";
import {
  disposeDynamicGrid,
  getOrthographicViewHeight,
  getSketchGridFrame,
  type ActiveSketchGridPlaneFrame,
  type DynamicGridRef,
} from "./viewport/grid";
import {
  moveGizmoParametersFromDrag,
  type MoveGizmoDescriptor,
  type MoveGizmoDragState,
} from "./viewport/moveGizmo";
import {
  draftStartRelations,
  lineCommitRelations,
} from "./viewport/lineCommitRelations";
import { buildSplineDraftPreview } from "./viewport/splineDraftPreview";
import { usePendingLineCommitRelations } from "./viewport/lineCommitRelationEffects";
import {
  selectionRectOverlayFromDrag,
  type SelectionRectOverlay,
  type SelectionDrag,
} from "./viewport/selectionGeometry";
import { type EndpointDrag } from "./viewport/endpointDrag";
import { handleEndpointDragPointerMove } from "./viewport/endpointDragPointerMove";
import { finishEndpointDragPointerUp } from "./viewport/endpointDragPointerUp";
import { handleSketchMovePointerMove } from "./viewport/sketchMovePointerMove";
import { finishSketchMovePointerUp } from "./viewport/sketchMovePointerUp";
import { notifyTrimPreviewResponse } from "./viewport/trimPointerMove";
import { notifyCornerTrimPreviewResponse } from "./viewport/cornerTrimPointerMove";
import type {
  PendingSketchMove,
  SketchMoveDrag,
  SketchMoveFrameResult,
  SketchMoveRingState,
} from "./viewport/sketchMoveTool";
import {
  buildSketchMoveRingObject,
  createPendingSketchMove,
  disposeSketchMoveRingObject,
  sketchMoveConstraintDeltas,
  sketchMoveRingRadius,
  sketchMoveRingStateForSelection,
  solvePendingSketchMove,
} from "./viewport/sketchMoveTool";
import { applySolvedPointsToSketchScene } from "./viewport/sketchPreviewSceneUpdate";
import {
  handleViewCubeDragPointerMove,
  handleViewCubeHoverPointerMove,
} from "./viewport/viewCubePointerMove";
import { finishViewCubePointerUp } from "./viewport/viewCubePointerUp";
import {
  buildSketchSnapCandidates,
  resolveSnappedSketchPoint as resolveSnappedSketchPointFromContext,
  type ResolveSnapOptions,
  type SketchSnapCandidate,
} from "./viewport/snapResolution";
import {
  isDraftDimensionTool,
  isDrawableSketchTool,
  typedChordSecondPoint,
  typedRadiusSecondPoint,
  updateDraftSessionCurrent,
  type DimensionLabelDragState,
  type DimensionRelationPreview,
  type DraftDimensionField,
  type DraftDimensionSession,
  type DraftDimensionTool,
  type ParameterSuggestion,
} from "./viewport/draftDimensions";
import {
  buildDraftDimensionPreview,
  type DraftDimensionScreenPositions,
} from "./viewport/draftDimensionPreview";
import {
  createDraftDimensionSession,
  createDraftDimensionSessionActions,
} from "./viewport/draftDimensionSessionActions";
import {
  buildAngleDimensionFrame,
  handleDimensionLabelDragPointerMove,
  type AngleDimensionFrame,
} from "./viewport/dimensionLabelDrag";
import { handleActiveSketchPointerMove } from "./viewport/activeSketchPointerMove";
import { renderTrimPreviewHighlight } from "./viewport/trimPreviewHighlight";
import { renderCornerTrimPreview } from "./viewport/cornerTrimPreviewHighlight";
import { createDimensionRelationPreviewActions } from "./viewport/dimensionRelationPreviewActions";
import { createLineAnglePreview } from "./viewport/dimensionRelationPreviewGeometry";
import { beginLinearPlacement, updateLinearPlacementPreview, cancelLinearPlacement as cancelLinearPlacementPreview, resolveLinearPlacementCommit } from "./viewport/linearDimensionPlacement";
import type { LinearPlacementState } from "./viewport/linearDimensionPlacement";
import { createDimensionRelationPlacementActions } from "./viewport/dimensionRelationPlacementActions";
import { createDimensionToolActions } from "./viewport/dimensionToolActions";
import {
  buildViewportContextMenuState,
  type SelectedConstraintState,
} from "./viewport/contextMenuState";
import {
  type ConstraintPreviewState,
} from "./viewport/constraintPreview";
import {
  type DraftSuggestionState,
} from "./viewport/draftDimensionInput";
import { createDraftDimensionActions } from "./viewport/draftDimensionActions";
import { advanceArcDraftSession } from "./viewport/draftCommit";
import { draftDimensionFieldScreenPosition } from "./viewport/draftDimensionScreenPosition";
import {
  type DimensionEditOriginalValue,
  handleDimensionEditorKeyDown as handleDimensionEditorInputKeyDown,
} from "./viewport/dimensionEditorInput";
import { createDimensionEditorActions } from "./viewport/dimensionEditorActions";
import { useDimensionEditorEffects } from "./viewport/dimensionEditorEffects";
import {
  applyPendingDraftDimensionExpressions,
  deletePendingAutoDimensions,
  placePendingCircleDimensionLabel,
  type PendingCircleDimensionPlacement,
  type PendingDimensionDeletion,
  type PendingDraftDimensionExpressions,
} from "./viewport/draftDimensionPostCommit";
import { renderDraftPointerPreview } from "./viewport/draftPointerPreview";
import {
  intersectViewportSceneTargets,
} from "./viewport/sceneTargetPicking";
import { handleViewportPointerDown } from "./viewport/viewportPointerDown";
import { handleViewportPointerUp } from "./viewport/viewportPointerUp";
import {
  applySceneHover,
} from "./viewport/pointerMoveHover";
import { disposeGeometryTreeResources } from "./viewport/threeDisposal";
import {
  invertSelectionFilter,
  readStoredFilter,
} from "./selectionFilterState";
import { ensureBridge } from "@/lib/planegcsSolver";
import type { SketchConstraintData } from "@/lib/planegcsBridge";
import { makeUiLogEntry } from "@/lib/logger";
import { updateScreenSpaceSketchSprites } from "./viewport/screenSpaceSketchSprites";
import {
  buildCamOriginSnapCandidates,
  camOriginSnapLabelKey,
  drillHoleCandidates,
  liftDrillCandidates,
  resolveCamOriginSnap,
} from "./viewport/camOriginSnap";
import { updateDynamicGrids } from "./viewport/dynamicGridUpdate";
import { bindSketchHotkeys } from "./viewport/sketchHotkeys";
import { syncViewportScene } from "./viewport/sceneSync";
import { fitCameraToDrawingSheet } from "./viewport/drawingSceneObjects";
import {
  renderViewCubeFrame,
  rotateCameraAroundCurrentView,
} from "./viewport/viewCubeRender";
import { createViewportPreviewActions } from "./viewport/previewObjectCleanup";
import { clearTrimHighlights } from "./viewport/previewObjectCleanup";
import { createViewportVisualStateActions } from "./viewport/viewportVisualState";
import { createDimensionPlacementActions } from "./viewport/dimensionPlacementActions";
import {
  configureViewportControls,
  handleViewportWheelZoom,
  resizeViewportRenderer,
  setupViewportSnapshotCapture,
} from "./viewport/viewportRenderer";
import { useViewportCallbackRefs } from "./viewport/viewportCallbackRefs";
import {
  useAltSnapOverride,
  useGhostEdgeRevealHotkey,
  useViewportGridHotkey,
} from "./viewport/viewportKeyboardEffects";
import { createViewportContextMenuActions } from "./viewport/viewportContextMenuActions";
import { computeViewportDerivedState } from "./viewport/viewportDerivedState";
import { getDimensionParameterSuggestions } from "./viewport/dimensionParameterSuggestions";
import {
  GRID_SNAP_SCREEN_DISTANCE_PX,
  ORTHO_FRUSTUM_HEIGHT,
  ORTHO_MAX_ZOOM,
  ORTHO_MIN_ZOOM,
  WHEEL_ZOOM_POINTER_PAN,
  WHEEL_ZOOM_SPEED,
  type ViewportPanelProps,
} from "./viewport/viewportPanelTypes";
import { useViewportSceneData } from "./viewport/useViewportSceneData";
import { ViewportPanelShell } from "./viewport/ViewportPanelShell";
import { computeViewportCrosshairState } from "./viewport/viewportCrosshairState";
import {
  dimensionCoreValue as computeDimensionCoreValue,
  formattedDimensionDisplayValue as formatDimensionDisplayValue,
  isProjectedCircleDimension as isProjectedCircleDimensionForSketch,
} from "./viewport/dimensionValueDisplay";

export function ViewportPanel({
  status,
  document,
  viewport,
  showStock = true,
  showCamToolpath = true,
  showDrawingSheet = false,
  drawingDimensionPreview = null,
  drawingAnnotationPreview = null,
  drawingViewPreview = null,
  drawingViewDrag = null,
  drawingGhostFrame = null,
  drawingGhostAnchored = false,
  drawingInsertArmed = false,
  onDrawingInsertMove,
  onDrawingInsertCommit,
  drawingViewDragArmed = false,
  onDrawingViewDragStart,
  onDrawingViewDragMove,
  onDrawingViewDrop,
  drawingFramePickArmed = false,
  onDrawingFramePick,
  drawingSelectedViewId = null,
  cameraFrameCaptureRef,
  drawingPickArmed = false,
  onDrawingPick,
  drawingDimensionTextDrag = null,
  onDrawingDimensionTextDragStart,
  onDrawingDimensionTextDragMove,
  onDrawingDimensionTextDrop,
  drawingDetailDragArmed = false,
  drawingDetailDrag = null,
  onDrawingDetailDragStart,
  onDrawingDetailDragMove,
  onDrawingDetailDragFinish,
  wcsOrientation = "z_up",
  activeCamSetupId = null,
  onSnapshotCaptureReady,
  onSelectPrimitive,
  onSelectReference,
  onSelectFace,
  onSelectEdge,
  onSelectVertex,
  originPickPointEnabled,
  onOriginPickPoint,
  wcsPickPointEnabled,
  onWcsPickPoint,
  drillPickPointEnabled,
  onDrillPickPoint,
  onStartSketch,
  onStartSketchOnFace,
  onAddSketchLine,
  onSetSketchMidpointAnchor,
  onSetSketchPointLineAnchor,
  onAddSketchAngleDimension,
  onAddSketchDistanceDimension,
  onAddSketchLineLengthDimension,
  onAddSketchLineAngleDimension,
  onAddSketchCircleRadiusDimension,
  onAddSketchArcRadiusDimension,
  onAddSketchArcLengthDimension,
  onAddSketchPolygonRadiusDimension,
  onSetSketchLineConstraint,
  onSetSketchPerpendicularConstraint,
  onSetSketchTangentConstraint,
  onSetSketchParallelConstraint,
  onAddSketchRectangle,
  onAddSketchCircle,
  onAddSketchCircleMode,
  onAddSketchArc,
  onAddSketchEllipse,
  onAddSketchSlot,
  onAddSketchSpline,
  arcToolMode,
  onSetArcToolMode,
  rectangleToolMode,
  onSetRectangleToolMode,
  circleToolMode,
  onSetCircleToolMode,
  polygonToolMode,
  onSetPolygonToolMode,
  dimensionToolMode,
  onSetDimensionToolMode,
  onAddSketchPolygon,
  onAddSketchFillet,
  onAddSketchChamfer,
  onAddSketchText,
  onPickSketchText,
  onPickSketchSlot,
  onPickSketchChamfer,
  onExtendSketchEntity,
  onCornerTrimSketchEntities,
  onSplitSketchEntity,
  onTrimSketchStroke,
  onOffsetSketchEntity,
  sketchTextPathPicking,
  onPickSketchTextPath,
  onSelectSketchEntity,
  onSelectSketchPoint,
  onSelectSketchRect,
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
  onConfirmDeleteSketchSelection,
  onDeleteSketchDimension,
  onBeginUndoGroup,
  onEndUndoGroup,
  onToggleSketchDimensionDriven,
  onSetSketchLineConstruction,
  onAddSketchVertexDistanceDimension,
  onUpdateSketchDimensionDisplay,
  onSetSketchTool,
  onOpenTransformArray,
  arrayCenterPicking,
  onArrayCenterPicked,
  onUpdateSketchPoint,
  onMoveSketchEntities,
  onFinishSketch,
  onClearSelection,
  moveGizmo = null,
  onMoveGizmoChange,
  onMoveBody,
  onCopyBody,
  onExportBodyMesh,
  onExportBodyStep,
  onSendBodyToSlicer,
  onUnlinkBodyCopy,
  onRemoveSketchProjections,
  showSketchProjectionActions,
  hiddenFeatureIds,
  hiddenSketchPlaneIds,
  hideReferences,
}: ViewportPanelProps) {
  const { config, activeTheme, updateConfig } = useAppConfig();
  const addMessage = useCadCoreStore((state) => state.addMessage);
  const addLogEntry = useCadCoreStore((state) => state.addLogEntry);
  const { t: translate } = useTranslation();
  const [showReferencePlanes, setShowReferencePlanes] = useState(true);
  const showViewportGrid = config.viewport.showGrid;
  const showSketchGrid = config.viewport.showSketchGrid;
  const showConstraints = config.viewport.showConstraints;
  const [contextMenu, setContextMenu] =
    useState<ViewportContextMenuState | null>(null);
  const [sketchSnapLabel, setSketchSnapLabel] = useState<string | null>(null);
  // True while the snap label currently shows a CAM origin-snap kind
  // (not a sketch tool snap) — gates the disarm cleanup below so it
  // never wipes a sketch snap label.
  const camOriginSnapLabelActiveRef = useRef(false);
  // Clear the origin-pick snap label when the pick disarms (apply,
  // cancel, or closing the setup panel) so a stale label cannot stick
  // at the last pointer position.
  useEffect(() => {
    if (originPickPointEnabled || drillPickPointEnabled) {
      // Scene hover is suppressed while the pick is armed (see the
      // pointer-move handler) — clear any highlight from before the
      // arm so a stale surface highlight doesn't sit frozen under
      // the cursor for the whole pick.
      setHoveredReference(null);
      setHoveredPrimitive(null);
      setHoveredFace(null);
      setHoveredEdge(null);
      setHoveredVertex(null);
      setHoveredSketchProfile(null);
      setHoveredSketchPoint(null);
      setHoveredSketchEntity(null);
    }
    if (
      !originPickPointEnabled &&
      !drillPickPointEnabled &&
      camOriginSnapLabelActiveRef.current
    ) {
      camOriginSnapLabelActiveRef.current = false;
      setSketchSnapLabel(null);
    }
    if (!originPickPointEnabled && !drillPickPointEnabled) {
      // The armed pick tracks the pointer position itself; a stale
      // position would keep the snap square visible after disarm.
      setCrosshairPointer(null);
    }
  }, [originPickPointEnabled, drillPickPointEnabled]);
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
  // Linear placement: when dimension tool is in "linear" mode and user
  // clicks a line, we defer IPC and show a live preview during drag.
  const linearPlacementRef = useRef<LinearPlacementState | null>(null);
  const linearPlacementPreviewRef = useRef<{
    line: THREE.Object3D;
    label: THREE.Sprite;
  } | null>(null);
  const [constraintPreview, setConstraintPreviewState] =
    useState<ConstraintPreviewState | null>(null);
  const constraintPreviewRef = useRef<ConstraintPreviewState | null>(null);
  function setConstraintPreview(preview: ConstraintPreviewState | null) {
    if (constraintPreviewEquals(constraintPreviewRef.current, preview)) {
      return;
    }
    constraintPreviewRef.current = preview;
    setConstraintPreviewState(preview);
  }
  const [crosshairPointer, setCrosshairPointerState] = useState<{
    x: number;
    y: number;
  } | null>(null);
  const crosshairPointerRef = useRef<{ x: number; y: number } | null>(null);
  function setCrosshairPointer(point: { x: number; y: number } | null) {
    if (screenPointEquals(crosshairPointerRef.current, point)) {
      return;
    }
    crosshairPointerRef.current = point;
    setCrosshairPointerState(point);
  }
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
  const [draftSuggestionState, setDraftSuggestionState] =
    useState<DraftSuggestionState>(null);
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
  const [anglePlacementPreviews, setAnglePlacementPreviews] = useState<
    Record<string, SketchDimensionScene>
  >({});
  const anglePlacementPreviewsRef = useRef<Record<string, SketchDimensionScene>>(
    {},
  );
  const anglePlacementPreviewValuesRef = useRef<Record<string, number>>({});
  const [draftDimensionSession, setDraftDimensionSession] =
    useState<DraftDimensionSession | null>(null);
  const pendingCircleDimensionPlacementRef =
    useRef<PendingCircleDimensionPlacement | null>(null);
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
  // R1 "Current 3D view": the LAST CAD-viewport camera snapshot —
  // updated on every controls change while the MODEL is shown (the
  // drawing workspace's sheet-only scene always fits the camera
  // top-down, so it must never count as a capture source).
  const lastCadCameraRef = useRef<{
    target: [number, number, number];
    position: [number, number, number];
    right: [number, number, number];
    up: [number, number, number];
  } | null>(null);
  const contentGroupRef = useRef<THREE.Group | null>(null);
  const referenceGroupRef = useRef<THREE.Group | null>(null);
  const sketchGroupRef = useRef<THREE.Group | null>(null);
  const drawingGroupRef = useRef<THREE.Group | null>(null);
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
  // Carries the slot draft preview — a stadium group (2 lines + 2
  // arcs), cleared recursively.
  const previewSlotRef = useRef<THREE.Group | null>(null);
  // Spline draft preview (curve strip + control polygon group).
  const previewSplineRef = useRef<THREE.Group | null>(null);
  /** Inference / tracking guide lines (dotted alignment hints). */
  const previewInferenceRef = useRef<THREE.Line[]>([]);
  const trimSegmentHighlightRef = useRef<THREE.Line | null>(null);
  const trimArcHighlightRef = useRef<THREE.Line | null>(null);
  // Corner-trim hover preview: the two ghost segments + corner marker
  // group, and the latest corner_trim_preview_result payload.
  const cornerPreviewGroupRef = useRef<THREE.Group | null>(null);
  const cornerTrimPreviewRef = useRef<
    | (NonNullable<CornerTrimPreviewResultEvent["payload"]> & { id?: string })
    | null
  >(null);
  // Corner preview throttle (same shape as the trim preview throttle).
  const cornerPreviewLastSentRef = useRef<{
    x: number;
    y: number;
    entityId: string;
    requestId: string | null;
  } | null>(null);
  // Corner tool two-pick state (first entity awaiting its partner).
  const cornerFirstEntityIdRef = useRef<string | null>(null);
  // Split tool two-click state (first click for circles/full ellipses).
  const splitFirstPickRef = useRef<{
    entityId: string;
    x: number;
    y: number;
  } | null>(null);
  // Drag-paint trim stroke: entities crossed while the pointer was
  // held, keyed by entity id (latest crossing position wins).
  const trimStrokeRef = useRef<Map<string, { x: number; y: number }> | null>(
    null,
  );
  /** Latest trim_preview_result payload from the core (null when idle),
   *  including the echoed command id and the document revision the
   *  preview was computed against. */
  const trimPreviewResultRef = useRef<
    | (NonNullable<TrimPreviewResultEvent["payload"]> & { id?: string })
    | null
  >(null);
  /** Throttle: skip IPC send if the cursor hasn't moved enough. */
  const trimPreviewLastSentRef = useRef<{
    x: number;
    y: number;
    entityId: string;
    requestId: string | null;
  } | null>(null);
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
  const viewCubeAnimStartUpRef = useRef(new THREE.Vector3(0, 0, 1));
  const viewCubeAnimTargetUpRef = useRef(new THREE.Vector3(0, 0, 1));
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
  const [currentGridSpacing, setCurrentGridSpacing] = useState(10);
  const draftDimensionSessionRef = useRef<DraftDimensionSession | null>(null);
  const draftDimensionInputRefs = useRef<
    Partial<Record<DraftDimensionField, HTMLInputElement | null>>
  >({});
  /** Cache for renderDraftDimensions: the key of the inputs that
   *  produced the current draft-dimension group plus its projected
   *  badge positions, so unchanged frames skip the dispose/rebuild
   *  (constant per-frame allocations read as micro-stutter). */
  const lastDraftDimKeyRef = useRef("");
  const lastDraftDimPositionsRef = useRef<DraftDimensionScreenPositions>({});
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
  /** dimensionId → {line, label} for in-place mutation during label drag */
  const dimensionObjectByIdRef = useRef<
    Map<string, { line: THREE.Group; label: THREE.Sprite }>
  >(new Map());
  const sketchConstraintObjectsRef = useRef<Array<THREE.Object3D>>([]);
  const sketchPointObjectsRef = useRef<THREE.Mesh[]>([]);
  const sketchPointObjectByIdRef = useRef(new Map<string, THREE.Mesh>());
  const sketchProfileObjectsRef = useRef<THREE.Group[]>([]);
  const sketchProfileVisualsRef = useRef(new Map<string, SketchProfileVisual>());
  const sketchProfileStatesRef = useRef(
    new Map<string, SketchProfileInteractionState>(),
  );
  const faceMeshesRef = useRef<THREE.Mesh[]>([]);
  // Stock box mesh (face-tagged via userData.stockFaceNames) for the WCS
  // pick raycast — populated by addCamSceneObjects, cleared with the rest
  // of the scene refs in clearViewportSceneObjectRefs.
  const stockFaceMeshesRef = useRef<THREE.Mesh[]>([]);
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
  const toolpathLinesRef = useRef<THREE.Object3D[]>([]);
  const moveGizmoObjectsRef = useRef<THREE.Object3D[]>([]);
  const moveGizmoDragRef = useRef<MoveGizmoDragState | null>(null);
  const moveGizmoRef = useRef<MoveGizmoDescriptor | null>(moveGizmo);
  const moveGizmoChangeRef = useRef(onMoveGizmoChange);
  const moveBodyRef = useRef(onMoveBody);
  const copyBodyRef = useRef(onCopyBody);
  const exportBodyMeshRef = useRef(onExportBodyMesh);
  const exportBodyStepRef = useRef(onExportBodyStep);
  const sendBodyToSlicerRef = useRef(onSendBodyToSlicer);
  const unlinkBodyCopyRef = useRef(onUnlinkBodyCopy);
  const removeSketchProjectionsRef = useRef(onRemoveSketchProjections);
  const pendingMoveGizmoParametersRef = useRef<MoveFeatureParameters | null>(
    null,
  );
  const pendingMoveGizmoFrameRef = useRef<number | null>(null);
  const pendingDraftPointerMoveEventRef = useRef<PointerEvent | null>(null);
  const pendingDraftPointerMoveFrameRef = useRef<number | null>(null);
  const objectSnapLatchRef = useRef<string | null>(null);
  const lastGeometryKeyRef = useRef("");
  const lastSceneBuildKeyRef = useRef("");
  // Dedicated overlay groups for the drawing workspace's cursor-driven
  // ghosts (dimension preview, Insert View ghost, view-drag ghost).
  // Registered by addDrawingSheetObjects and updated IN PLACE by
  // syncDrawingOverlays — a preview reply repaints only these groups
  // instead of rebuilding the whole sheet scene (the jerky-crawl fix).
  const drawingPreviewGroupRef = useRef<THREE.Group | null>(null);
  const drawingDragGroupRef = useRef<THREE.Group | null>(null);
  const requestViewportRenderRef = useRef<(() => void) | null>(null);
  const selectPrimitiveRef = useRef(onSelectPrimitive);
  const selectReferenceRef = useRef(onSelectReference);
  const selectFaceRef = useRef(onSelectFace);
  const originPickPointEnabledRef = useRef(originPickPointEnabled);
  originPickPointEnabledRef.current = originPickPointEnabled;
  const originPickPointRef = useRef(onOriginPickPoint);
  originPickPointRef.current = onOriginPickPoint;
  const wcsPickPointEnabledRef = useRef(wcsPickPointEnabled);
  wcsPickPointEnabledRef.current = wcsPickPointEnabled;
  const wcsPickPointRef = useRef(onWcsPickPoint);
  wcsPickPointRef.current = onWcsPickPoint;
  const drillPickPointEnabledRef = useRef(drillPickPointEnabled);
  drillPickPointEnabledRef.current = drillPickPointEnabled;
  const drillPickPointRef = useRef(onDrillPickPoint);
  drillPickPointRef.current = onDrillPickPoint;
  // P6 dimension tool: while armed, every pointer-up in the drawing
  // workspace delivers the clicked sheet-mm point to the app (the
  // core resolves the nearest edge — the pick itself is just a point).
  const drawingPickArmedRef = useRef(drawingPickArmed);
  drawingPickArmedRef.current = drawingPickArmed;
  const drawingPickRef = useRef(onDrawingPick);
  drawingPickRef.current = onDrawingPick;
  // Whole-dimension drag: a press on a dimension text starts a drag
  // whose ghost is a label sprite; pointer-up commits the placement
  // (drawing_dimension_update with text_offset).
  const drawingDimensionTextDragStartRef = useRef(
    onDrawingDimensionTextDragStart,
  );
  drawingDimensionTextDragStartRef.current = onDrawingDimensionTextDragStart;
  const drawingDimensionTextDragMoveRef = useRef(
    onDrawingDimensionTextDragMove,
  );
  drawingDimensionTextDragMoveRef.current = onDrawingDimensionTextDragMove;
  const drawingDimensionTextDropRef = useRef(onDrawingDimensionTextDrop);
  drawingDimensionTextDropRef.current = onDrawingDimensionTextDrop;
  const dimensionTextDragRef = useRef<{
    drawingId: string;
    id: string;
  } | null>(null);
  // Mouse-first Insert View: hover feeds the ghost position, a click
  // commits the view at the clicked sheet-mm point (no typing).  A
  // click-vs-pan guard uses the pointer-down client position.
  const drawingInsertArmedRef = useRef(drawingInsertArmed);
  drawingInsertArmedRef.current = drawingInsertArmed;
  const drawingInsertMoveRef = useRef(onDrawingInsertMove);
  drawingInsertMoveRef.current = onDrawingInsertMove;
  // rAF coalescing: pointermove fires faster than 60 Hz and each
  // dispatch re-renders the whole App tree — the ghost position is
  // delivered once per animation frame with the LATEST point, which
  // keeps the cursor-follow smooth instead of event-flooded.
  const drawingInsertMoveFrameRef = useRef<number | null>(null);
  const drawingInsertMoveLatestRef = useRef<[number, number] | null>(null);
  const drawingInsertCommitRef = useRef(onDrawingInsertCommit);
  drawingInsertCommitRef.current = onDrawingInsertCommit;
  const drawingPointerDownClientRef = useRef<{ x: number; y: number } | null>(
    null,
  );
  // Mouse-first view reposition: grabbing a view frame starts a drag
  // (the frame follows the cursor); pointer-up commits the move.
  const drawingViewDragArmedRef = useRef(drawingViewDragArmed);
  drawingViewDragArmedRef.current = drawingViewDragArmed;
  const drawingViewDragStartRef = useRef(onDrawingViewDragStart);
  drawingViewDragStartRef.current = onDrawingViewDragStart;
  const drawingViewDragMoveRef = useRef(onDrawingViewDragMove);
  drawingViewDragMoveRef.current = onDrawingViewDragMove;
  const drawingViewDropRef = useRef(onDrawingViewDrop);
  drawingViewDropRef.current = onDrawingViewDrop;
  const viewFrameDragRef = useRef<{ viewId: string } | null>(null);
  // A frame press awaiting click-vs-drag resolution (the frame-pick
  // modes): movement promotes it to a drag, a stationary release
  // dispatches the pick.
  const framePressRef = useRef<{
    viewId: string;
    min: [number, number];
    max: [number, number];
    grabOffset: [number, number];
    pressPoint: [number, number];
    clientX: number;
    clientY: number;
  } | null>(null);
  // R1 tool dispatch: while armed (projected-view parent picking or
  // delete-view selection), a press on a view frame delivers the
  // frame to the app (the drag branch stays disarmed in these
  // modes — see App.tsx gating).
  const drawingFramePickArmedRef = useRef(drawingFramePickArmed);
  drawingFramePickArmedRef.current = drawingFramePickArmed;
  const drawingFramePickRef = useRef(onDrawingFramePick);
  drawingFramePickRef.current = onDrawingFramePick;
  // Detail View circle drag: while armed, a press inside a projection
  // view's content bounds starts the circle; pointer-up finishes it
  // (the app-side action decides whether the radius clears the commit
  // threshold).  The gesture state itself lives in the tool hook —
  // this ref only routes the pointer events.
  const drawingDetailDragArmedRef = useRef(drawingDetailDragArmed);
  drawingDetailDragArmedRef.current = drawingDetailDragArmed;
  const drawingDetailDragStartRef = useRef(onDrawingDetailDragStart);
  drawingDetailDragStartRef.current = onDrawingDetailDragStart;
  const drawingDetailDragMoveRef = useRef(onDrawingDetailDragMove);
  drawingDetailDragMoveRef.current = onDrawingDetailDragMove;
  const drawingDetailDragFinishRef = useRef(onDrawingDetailDragFinish);
  drawingDetailDragFinishRef.current = onDrawingDetailDragFinish;
  const detailDragRef = useRef<{ viewId: string } | null>(null);
  // Disarming mid-drag (Esc → hook.cancel) must end the pointer
  // routing and hand the controls back — the pointer-capture release
  // below never runs for a cancelled gesture.
  useEffect(() => {
    if (!drawingDetailDragArmed && detailDragRef.current) {
      detailDragRef.current = null;
      if (controlsRef.current) {
        controlsRef.current.enabled = true;
      }
    }
  }, [drawingDetailDragArmed]);
  // The sheet-only drawing scene never counts as a CAD camera —
  // record snapshots only while the model is shown.
  const showDrawingSheetRef = useRef(showDrawingSheet);
  showDrawingSheetRef.current = showDrawingSheet;
  const selectEdgeRef = useRef(onSelectEdge);
  const selectVertexRef = useRef(onSelectVertex);
  const startSketchRef = useRef(onStartSketch);
  const startSketchOnFaceRef = useRef(onStartSketchOnFace);
  const addSketchLineRef = useRef(onAddSketchLine);
  const addSketchRectangleRef = useRef(onAddSketchRectangle);
  const addSketchCircleRef = useRef(onAddSketchCircle);
  const addSketchCircleModeRef = useRef(onAddSketchCircleMode);
  // Circle tool tangent modes: defining lines picked so far.
  const circleTangentLineIdsRef = useRef<string[]>([]);

  const addSketchArcRef = useRef(onAddSketchArc);
  const addSketchEllipseRef = useRef(onAddSketchEllipse);
  const addSketchSplineRef = useRef(onAddSketchSpline);
  const addSketchSlotRef = useRef(onAddSketchSlot);
  const selectionDragRef = useRef<SelectionDrag | null>(null);

  // Endpoint drag state — active when the user grabs a sketch
  // line endpoint in Select mode and drags it to a new position.
  const endpointDragRef = useRef<EndpointDrag | null>(null);
  // rAF batching for endpoint drag — same pattern as flushMoveGizmoChange.
  const pendingDragRef = useRef<{
    vertexId: string;
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
  // Live-preview flags: set while an endpoint-drag or Move-tool preview is
  // mutating committed scene objects (mid-drag viewport_state syncs must
  // not overwrite the preview), plus a one-shot force-rebuild flag that
  // restores committed geometry after a cancelled preview.
  const dragPreviewMutatingRef = useRef(false);
  const moveDragPreviewActiveRef = useRef(false);
  const forceSceneRebuildRef = useRef(false);
  // Scene constraint data for badge-follow deltas during live previews.
  const sceneConstraintsRef = useRef<readonly SketchConstraintScene[]>([]);

  // Sketch Move tool state.
  const sketchMoveDragRef = useRef<SketchMoveDrag | null>(null);
  // Rotation-ring group, live during a move drag.
  const sketchMoveRingGroupRef = useRef<THREE.Group | null>(null);
  // Fusion-style persistent manipulator: rotation ring shown while the
  // Move tool is armed with a selection; grabbing the ring rotates.
  const persistentRingGroupRef = useRef<THREE.Group | null>(null);
  const persistentRingPickablesRef = useRef<THREE.Object3D[]>([]);
  const persistentRingStateRef = useRef<SketchMoveRingState | null>(null);
  // Move/Copy dialog state: the pending transform accumulated by drags
  // and the numeric fields, committed on OK / reverted on Cancel.
  const pendingSketchMoveRef = useRef<PendingSketchMove | null>(null);
  const [movePanelValues, setMovePanelValues] = useState({
    dx: 0,
    dy: 0,
    angleDeg: 0,
  });
  // rAF batching for the Move tool — same pattern as endpoint drag.
  const pendingMoveDragRef = useRef<{
    x: number;
    y: number;
    clientX: number;
    clientY: number;
  } | null>(null);
  const pendingMoveDragFrameRef = useRef<number | null>(null);
  // Last resolved Move frame — committed on pointer-up.
  const moveFrameResultRef = useRef<SketchMoveFrameResult | null>(null);
  // Set on Move commit; cleared when the next viewport rebuild arrives
  // (keeps the preview alive across the async IPC gap).
  const pendingMoveCommitRef = useRef(false);

  const [selectionRect, setSelectionRect] =
    useState<SelectionRectOverlay | null>(null);

  const arcToolModeRef = useRef(arcToolMode);
  const rectangleToolModeRef = useRef(rectangleToolMode);
  const circleToolModeRef = useRef(circleToolMode);
  const polygonToolModeRef = useRef(polygonToolMode);
  const dimensionToolModeRef = useRef(dimensionToolMode);
  const addSketchPolygonRef = useRef(onAddSketchPolygon);
  const addSketchFilletRef = useRef(onAddSketchFillet);
  const addSketchChamferRef = useRef(onAddSketchChamfer);
  const addSketchTextRef = useRef(onAddSketchText);
  const pickSketchTextRef = useRef(onPickSketchText);
  const pickSketchSlotRef = useRef(onPickSketchSlot);
  const pickSketchChamferRef = useRef(onPickSketchChamfer);
  const extendSketchEntityRef = useRef(onExtendSketchEntity);
  const cornerTrimSketchEntitiesRef = useRef(onCornerTrimSketchEntities);
  const splitSketchEntityRef = useRef(onSplitSketchEntity);
  const trimSketchStrokeRef = useRef(onTrimSketchStroke);
  const offsetSketchEntityRef = useRef(onOffsetSketchEntity);
  const sketchTextPathPickingRef = useRef(sketchTextPathPicking);
  const pickSketchTextPathRef = useRef(onPickSketchTextPath);
  useEffect(() => {
    sketchTextPathPickingRef.current = sketchTextPathPicking;
    pickSketchTextPathRef.current = onPickSketchTextPath;
  }, [sketchTextPathPicking, onPickSketchTextPath]);
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
  // Major-axis click of the 3-click ellipse draft (mirrors the arc's
  // second-point ref; cleared on commit and tool switch).
  const ellipseSecondPointRef = useRef<[number, number] | null>(null);
  // Placed spline control poles for the in-progress spline draft.
  // Click the first pole again to commit; Escape cancels.
  const splineDraftPolesRef = useRef<[number, number][]>([]);
  const selectSketchEntityRef = useRef(onSelectSketchEntity);
  const selectSketchPointRef = useRef(onSelectSketchPoint);
  const pickInactiveSketchLineRef = useRef(onPickInactiveSketchLine);
  const inactiveSketchEntityPickEnabledRef = useRef(
    inactiveSketchEntityPickEnabled,
  );
  const pickSketchPointRef = useRef(onPickSketchPoint);
  const updateSketchPointRef = useRef(onUpdateSketchPoint);
  const moveSketchEntitiesRef = useRef(onMoveSketchEntities);
  const selectSketchDimensionRef = useRef(onSelectSketchDimension);
  const updateSketchDimensionRef = useRef(onUpdateSketchDimension);
  const updateSketchDimensionLabelPositionRef = useRef(
    onUpdateSketchDimensionLabelPosition,
  );
  const selectSketchProfileRef = useRef(onSelectSketchProfile);
  const trimSketchEntityRef = useRef(onTrimSketchEntity);
  const deleteSketchSelectionRef = useRef(onDeleteSketchSelection);
  const confirmDeleteSketchSelectionRef = useRef(onConfirmDeleteSketchSelection);
  const deleteSketchDimensionRef = useRef(onDeleteSketchDimension);
  const beginUndoGroupRef = useRef(onBeginUndoGroup);
  const endUndoGroupRef = useRef(onEndUndoGroup);
  // Set while a dimension draft commit's undo group is open; the
  // post-commit effect drains the scheduled delete/update and then
  // closes the group (one draft = ONE undo step, D2).
  const draftUndoGroupOpenRef = useRef(false);
  const toggleSketchDimensionDrivenRef = useRef(onToggleSketchDimensionDriven);
  const setSketchLineConstructionRef = useRef(onSetSketchLineConstruction);
  const addSketchVertexDistanceDimensionRef = useRef(
    onAddSketchVertexDistanceDimension,
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
  const worldUnitsPerPixelRef = useRef(1);
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
  const dimensionEditOriginalValueRef =
    useRef<DimensionEditOriginalValue | null>(null);
  const lastPointerEventRef = useRef<PointerEvent | null>(null);
  // Document revision pinned at pointer-down (M24). Sketch pointer-up
  // commits are discarded when the document changed mid-gesture — e.g.
  // an awaited undo landed while the pointer was held, so the geometry
  // under the cursor no longer matches what the gesture started on.
  const pointerDownRevisionRef = useRef(0);
  const isDimensionEditorOpenRef = useRef(false);
  const suppressNextDimensionEditorOpenRef = useRef(false);
  useEffect(() => {
    isDimensionEditorOpenRef.current = isDimensionEditorOpen;
  }, [isDimensionEditorOpen]);
  const setSketchToolRef = useRef(onSetSketchTool);
  const openTransformArrayRef = useRef<(() => void) | undefined>(
    onOpenTransformArray,
  );
  const armedSketchConstraintRef = useRef(armedSketchConstraint);
  const mirrorFocusedSlotRef = useRef(mirrorFocusedSlot);
  const mirrorEntityPickRef = useRef(onMirrorEntityPick);
  const cancelSketchConstraintRef = useRef(onCancelSketchConstraint);
  const clearSketchConstraintRef = useRef(onClearSketchConstraint);
  // Sketch-mode Escape deselect: clears highlighted geometry through the
  // core's clear_selection command (handled in sketchHotkeys.ts).
  const clearSketchSelectionRef = useRef(onClearSelection);
  // Spline draft commit (Enter / double-click) — stable ref for the
  // one-shot keydown listener.
  const commitSplineDraftRef = useRef<() => void>(() => {});
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
  // reads the new line's start_vertex_id / end_vertex_id and dispatches
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
  const addSketchLineAngleDimensionRef = useRef(
    onAddSketchLineAngleDimension,
  );
  const addSketchCircleRadiusDimensionRef = useRef(
    onAddSketchCircleRadiusDimension,
  );
  const addSketchArcRadiusDimensionRef = useRef(
    onAddSketchArcRadiusDimension,
  );
  const addSketchArcLengthDimensionRef = useRef(
    onAddSketchArcLengthDimension,
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
  const pendingDimensionDeletionRef =
    useRef<PendingDimensionDeletion | null>(null);
  const pendingDraftDimensionExpressionsRef =
    useRef<PendingDraftDimensionExpressions | null>(null);
  // Snapshot of the sketch feature's lines for the post-add effect to
  // index into. Same pattern as the count ref above.
  const sketchLinesRef = useRef<
    NonNullable<typeof sketchFeature>["sketch_parameters"] | null
  >(null);
  // planegcs constraint data from the viewport state, kept in sync
  // so the drag rAF can read it without depending on render-cycle state.
  const sketchConstraintsRef = useRef<SketchConstraintData[]>([]);
  const { pendingEdgeOpBodyIds, sceneData, sceneDataRef } =
    useViewportSceneData({
    document,
    viewport,
    hiddenFeatureIds,
    hiddenSketchPlaneIds,
    hideReferences,
    });
  useEffect(() => {
    applyTheme(activeTheme);
  }, [activeTheme]);
  // Init the planegcs WASM constraint solver (lazy, once).
  useEffect(() => {
    ensureBridge()
      .then((bridge) => {
        addLogEntry(makeUiLogEntry(
          "info", "planegcs",
          `WASM solver ready — ${bridge.config.maxIterations} iter, ` +
          `${bridge.config.convergenceThreshold} tol, ` +
          `${bridge.config.algorithm === 1 ? "Levenberg-Marquardt" : "DogLeg"}`,
        ));
        addMessage("[planegcs] WASM constraint solver ready");
        bridge.onFirstSolve = () => {
          addLogEntry(makeUiLogEntry(
            "info", "planegcs",
            "First WASM solve completed during drag — constraint preview active",
          ));
        };
      })
      .catch((err) => {
        addLogEntry(makeUiLogEntry(
          "error", "planegcs",
          `WASM solver init failed (drag will use TS-only fallback): ${String(err)}`,
        ));
        addMessage(
          `[planegcs] WASM solver init failed (drag will use TS-only fallback): ${String(err)}`,
        );
      });
  }, []); // eslint-disable-line react-hooks/exhaustive-deps
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
    placePendingCircleDimensionLabel({
      pendingCircleDimensionPlacementRef,
      sketch: sketchFeature?.sketch_parameters,
      setDimensionLabelPositions,
    });
  }, [sketchFeature]);
  // Post-commit dimension work for a draft commit (D2 — one draft =
  // ONE undo step). Drag-only shapes have their auto-dimension
  // deleted (Fusion 360 behavior); typed values re-apply expressions.
  // The drains run on every sketch change and self-gate on their
  // pending refs (the entity must have landed before the ids resolve).
  // The group closes only once BOTH pending pieces have drained — the
  // delete/update commands were already sent, so the core processes
  // them before the end command (sequential stream). Note the flag is
  // set AFTER the pending work is scheduled in
  // commitDraftDimensionSession: this effect also fires on the
  // begin_group's own document reply, when nothing is pending yet —
  // closing the group there would skip the deletion entirely.
  useEffect(() => {
    deletePendingAutoDimensions({
      pendingDimensionDeletionRef,
      sketch: sketchFeature?.sketch_parameters,
      deleteSketchDimension: deleteSketchDimensionRef.current,
    });
    applyPendingDraftDimensionExpressions({
      pendingDraftDimensionExpressionsRef,
      sketch: sketchFeature?.sketch_parameters,
      updateSketchDimension: updateSketchDimensionRef.current,
    });
    if (
      draftUndoGroupOpenRef.current &&
      pendingDimensionDeletionRef.current === null &&
      pendingDraftDimensionExpressionsRef.current === null
    ) {
      draftUndoGroupOpenRef.current = false;
      void endUndoGroupRef.current();
    }
  }, [sketchFeature]);
  // React to view-setting-changed events dispatched by the View panel
  // so edge visibility toggles take effect immediately without waiting
  // for the next viewport rebuild.
  useEffect(() => {
    const handler = () => {
      const hidden = getShowHiddenEdges();
      for (const line of edgeLineObjectsRef.current) {
        const mat = line.material as THREE.LineBasicMaterial;
        mat.depthTest = !hidden;
        mat.polygonOffset = !hidden;
        mat.needsUpdate = true;
      }
    };
    window.addEventListener("view-setting-changed", handler);
    return () => window.removeEventListener("view-setting-changed", handler);
  }, []);
  // Keep planegcs constraint data ref in sync with the viewport state
  // so the drag rAF can read it without render-cycle stale closures.
  useEffect(() => {
    sketchConstraintsRef.current = (viewport?.sketch_constraints ?? []).map(
      (c) => ({
        constraint_id: c.constraint_id,
        kind: c.kind,
        target_ids: (
          [c.entity_id, c.related_entity_id] as (string | null)[]
        ).filter((id): id is string => id !== null && id.length > 0),
      }),
    );
  }, [viewport?.sketch_constraints]);
  // Scene constraint data ref, for constraint-badge deltas during the
  // live drag/Move previews (same stale-closure avoidance).
  useEffect(() => {
    sceneConstraintsRef.current = sceneData?.sketchConstraints ?? [];
  }, [sceneData?.sketchConstraints]);

  // Arming the Move tool opens the Move/Copy dialog: capture the pending
  // state from the current selection (the context-menu action selects
  // the target first; the sceneData dependency re-runs when the
  // selection lands).  A dialog opened without a selection gets its
  // pending state from the first drag instead.
  useEffect(() => {
    if (activeSketchTool !== "move" || pendingSketchMoveRef.current) {
      return;
    }
    const state = sketchMoveRingStateForSelection(
      sketchLinesRef.current,
      sceneDataRef.current,
    );
    if (!state) {
      return;
    }
    pendingSketchMoveRef.current = createPendingSketchMove(
      sketchLinesRef.current,
      state.entityIds,
    );
  }, [activeSketchTool, sceneData]);
  const {
    selectedPrimitiveLabel,
    selectedReference,
    measurementText,
    displayedSketchDimensions,
    selectedSketchDimension,
    selectedSketchDimensionValue,
    selectedSketchDimensionExpression,
  } = useMemo(
    () =>
      computeViewportDerivedState({
        document,
        viewport,
        sceneData,
        sketchParameters: sketchFeature?.sketch_parameters,
        activeSketchPlaneFrame,
        angleDragRadii,
        anglePlacementPreviews,
        dimensionLabelPositions,
      }),
    [
      activeSketchPlaneFrame,
      angleDragRadii,
      anglePlacementPreviews,
      dimensionLabelPositions,
      document,
      sceneData,
      sketchFeature?.sketch_parameters,
      viewport,
    ],
  );
  useEffect(() => {
    displayedSketchDimensionsRef.current = displayedSketchDimensions;
  }, [displayedSketchDimensions]);
  const dimensionParameterSuggestions = useMemo<ParameterSuggestion[]>(() => {
    if (!selectedSketchDimension) {
      return [];
    }
    const cursor =
      dimensionInputRef.current?.selectionStart ?? dimensionDraftValue.length;
    const isAngleDimension =
      selectedSketchDimension.kind === "angle" ||
      selectedSketchDimension.kind === "line_angle";
    return getDimensionParameterSuggestions({
      parameters: document?.parameters,
      value: dimensionDraftValue,
      cursor,
      isAngleDimension,
    });
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
  const showConstraintsRef = useRef(showConstraints);
  const documentRef = useRef(document);
  // The pointer handlers below live in an effect keyed on
  // activeSketchPlaneId only — their closures would otherwise see the
  // mount-time document/viewport/stock state.  Stock snap candidates
  // and the stock-face raycast read these refs so they track live
  // values (body candidates come from scene refs and never had the
  // problem — which is why stock snap looked like the only failure).
  const viewportRef = useRef(viewport);
  const showStockRef = useRef(showStock);
  const activeCamSetupIdRef = useRef(activeCamSetupId);
  // Transform/Array center pick: armed from App while the panel's Pick
  // button is active. The next sketch-plane click reports the snapped
  // point instead of selecting.
  const arrayCenterPickingRef = useRef(false);
  const onArrayCenterPickedRef = useRef(onArrayCenterPicked);
  useEffect(() => {
    arrayCenterPickingRef.current = arrayCenterPicking;
  }, [arrayCenterPicking]);
  useEffect(() => {
    onArrayCenterPickedRef.current = onArrayCenterPicked;
  }, [onArrayCenterPicked]);
  // Crosshair while the Transform/Array center pick is armed.
  useEffect(() => {
    if (!rendererRef.current) {
      return;
    }
    const canvas = rendererRef.current.domElement as HTMLCanvasElement;
    if (arrayCenterPicking) {
      canvas.style.cursor = "crosshair";
    } else {
      canvas.style.cursor = "";
    }
  }, [arrayCenterPicking]);
  // Crosshair while the Insert View placement is armed (mouse-first —
  // the click places the view), and while the Detail View circle drag
  // is armed (press-drag draws the circle).
  useEffect(() => {
    if (!rendererRef.current) {
      return;
    }
    const canvas = rendererRef.current.domElement as HTMLCanvasElement;
    canvas.style.cursor =
      drawingInsertArmed || drawingDetailDragArmed ? "crosshair" : "";
  }, [drawingInsertArmed, drawingDetailDragArmed]);
  useEffect(() => {
    activeSketchPlaneIdRef.current = activeSketchPlaneId;
    activeSketchPlaneFrameRef.current = activeSketchPlaneFrame;
  }, [activeSketchPlaneId, activeSketchPlaneFrame]);
  // Inactive-sketch picking refs must track their props: the CAM
  // workspace arms picking long after mount, and a stale first-render
  // value would keep sketch clicks dead.
  useEffect(() => {
    pickInactiveSketchLineRef.current = onPickInactiveSketchLine;
  }, [onPickInactiveSketchLine]);
  useEffect(() => {
    inactiveSketchEntityPickEnabledRef.current =
      inactiveSketchEntityPickEnabled;
  }, [inactiveSketchEntityPickEnabled]);
  useEffect(() => {
    showViewportGridRef.current = showViewportGrid;
  }, [showViewportGrid]);
  useEffect(() => {
    showSketchGridRef.current = showSketchGrid;
  }, [showSketchGrid]);
  useEffect(() => {
    showConstraintsRef.current = showConstraints;
  }, [showConstraints]);
  useEffect(() => {
    documentRef.current = document;
  }, [document]);
  useEffect(() => {
    viewportRef.current = viewport;
  }, [viewport]);
  useEffect(() => {
    showStockRef.current = showStock;
  }, [showStock]);
  useEffect(() => {
    activeCamSetupIdRef.current = activeCamSetupId;
  }, [activeCamSetupId]);
  useEffect(() => {
    draftDimensionSessionRef.current = draftDimensionSession;
  }, [draftDimensionSession]);
  useEffect(() => {
    dimensionToolModeRef.current = dimensionToolMode;
  }, [dimensionToolMode]);
  useEffect(() => {
    if (!draftDimensionSession) {
      return;
    }
    renderDraftPreview(draftDimensionSession);
    // The preview objects are already in the scene — the canvas must
    // redraw NOW, not on the next pointer move (typing a value felt
    // like it "applies on the next mouse movement").
    requestViewportRenderRef.current?.();
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

  function toggleConstraintsVisibility() {
    updateConfig((current) => ({
      ...current,
      viewport: {
        ...current.viewport,
        showConstraints: !current.viewport.showConstraints,
      },
    }));
  }

  useViewportGridHotkey({
    toggleGridHotkey: config.hotkeys.viewport.toggleGrid,
    activeSketchPlaneIdRef,
    updateConfig,
  });
  useAltSnapOverride(altHeldRef);

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
    requestViewportRenderRef.current?.();
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
    requestViewportRenderRef.current?.();
  }, [selectedConstraint]);

  const {
    clearCornerPreview,
    clearDragPreviewLines,
    clearPreviewArc,
    clearPreviewCircle,
    clearPreviewDimension,
    clearPreviewInference,
    clearPreviewLine,
    clearPreviewSlot,
    clearPreviewSpline,
    clearTrimArcHighlight,
    clearTrimSegmentHighlight,
    updateCornerPreview,
    updateTrimArcHighlight,
    updateTrimSegmentHighlight,
  } = createViewportPreviewActions({
    sketchGroupRef,
    dragPreviewLinesRef,
    previewLineRef,
    previewCircleRef,
    previewArcRef,
    previewSlotRef,
    previewSplineRef,
    previewInferenceRef,
    trimSegmentHighlightRef,
    trimArcHighlightRef,
    cornerPreviewGroupRef,
    previewDimensionRef,
    dimensionRelationPreviewRef,
    dimensionRelationPreviewLabelRef,
    restoreRelationPreviewHiddenDimensions,
  });

  // After ANY geometry change (a trim, a move, a solve), the last
  // trim_preview_result belongs to the PREVIOUS entity state: its
  // segment index no longer points at the same segment. Without this
  // invalidation, a second trim click at an unmoved cursor reuses the
  // stale index against the new segment list and deletes a different —
  // often much larger — piece (the residual "excessive trimming" and
  // the open chains that killed the flower outline).
  const lastTrimInvalidationRevisionRef = useRef<number | null>(null);
  useEffect(() => {
    const revision = document?.revision ?? 0;
    if (
      lastTrimInvalidationRevisionRef.current !== null &&
      revision !== lastTrimInvalidationRevisionRef.current
    ) {
      trimPreviewResultRef.current = null;
      trimPreviewLastSentRef.current = null;
      clearTrimSegmentHighlight();
      clearTrimArcHighlight();
      cornerTrimPreviewRef.current = null;
      cornerPreviewLastSentRef.current = null;
      clearCornerPreview();
    }
    lastTrimInvalidationRevisionRef.current = revision;
  }, [
    document?.revision,
    clearTrimSegmentHighlight,
    clearTrimArcHighlight,
    clearCornerPreview,
  ]);

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
    // Clear the drag ref so a subsequent regroup into a two-entity
    // dimension doesn't commit stale placement state from the old
    // single-entity dimension.
    dimensionLabelDragRef.current = null;
    clearDimensionToolFirstPick();
    if (controlsRef.current) {
      controlsRef.current.enabled = true;
    }
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

  const { updateDimensionRelationPreview } =
    createDimensionRelationPreviewActions({
      displayUnits: config.displayUnits,
      sketchGroupRef,
      previewDimensionRef,
      dimensionRelationPreviewRef,
      dimensionRelationPreviewLabelRef,
      dimensionToolFirstLineRef,
      sketchLinesRef,
      activeSketchToolRef,
      activeSketchPlaneIdRef,
      activeSketchPlaneFrameRef,
      pendingAngleIsReflexRef,
      pendingReflexAngleRef,
      worldUnitsPerPixelRef,
      clearPreviewDimension,
      hideRelationPreviewDimension,
      readDimensionPreviewFilter,
      dimensionToolModeRef,
    });

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
    dimensionObjectByIdRef.current.clear();
    sketchConstraintObjectsRef.current = [];
    sketchPointObjectsRef.current = [];
    sketchProfileObjectsRef.current = [];
    meshesRef.current = [];
    faceMeshesRef.current = [];
    stockFaceMeshesRef.current = [];
    edgeLineObjectsRef.current = [];
    vertexObjectsRef.current = [];
    cutPreviewObjectsRef.current = [];
    toolpathLinesRef.current = [];
    moveGizmoObjectsRef.current = [];
    drawingPreviewGroupRef.current = null;
    drawingDragGroupRef.current = null;
    previewLineRef.current = null;
    previewCircleRef.current = null;
    previewArcRef.current = null;
    previewSlotRef.current = null;
    previewSplineRef.current = null;
    previewInferenceRef.current = [];
  }

  const {
    clearDraftDimensionSession,
    scheduleDimensionDeletion,
    scheduleDraftDimensionExpressionUpdate,
    suppressDimensionEditorAfterSketchCommit,
  } = createDraftDimensionSessionActions({
    sketchParameters: sketchFeature?.sketch_parameters ?? null,
    draftDimensionInputRefs,
    draftDimensionSessionRef,
    draftFieldFocusedRef,
    draftRawInputRef,
    draftParameterExpressionRef,
    previousLineAngleRef,
    pendingDimensionDeletionRef,
    pendingDraftDimensionExpressionsRef,
    sketchLineCountRef,
    setDraftDimensionSession,
    setDraftSuggestionState,
    setIsDimensionEditorOpen,
    suppressNextDimensionEditorOpenRef,
    dimensionInputRef,
    clearDraftDimGroup,
  });
  // Click-commit variant of the bare scheduler (D2). The drag path
  // (commitDraftDimensionSession) opens its "Dimension" undo group
  // explicitly; the click path (commitDraftPointerUp) has no such
  // hook, so its auto-dim deletion used to land as a standalone
  // "Delete Sketch Dimension" step after the entity add. Opening the
  // group here — BEFORE the add command is sent (every click commit
  // flow schedules the deletion ahead of the add) — folds both into
  // ONE step, and keeps history free of raw delete steps whose
  // repeated labels broke the undo dropdown's keying. Idempotent: the
  // flag guards against ever nesting a second group.
  const scheduleDimensionDeletionInGroup = (
    tool: "line" | "rectangle" | "circle" | "polygon" | "arc",
    preCapturedSession?: DraftDimensionSession | null,
  ) => {
    if (!draftUndoGroupOpenRef.current) {
      draftUndoGroupOpenRef.current = true;
      void beginUndoGroupRef.current("Dimension");
    }
    scheduleDimensionDeletion(tool, preCapturedSession);
  };

  function readDimensionPreviewFilter() {
    const filter = readStoredFilter();
    return altHeldRef.current ? invertSelectionFilter(filter) : filter;
  }

  function isProjectedCircleDimension(dimensionId: string) {
    return isProjectedCircleDimensionForSketch(
      sketchFeature?.sketch_parameters ?? null,
      dimensionId,
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

  const {
    createDimensionAngleOrDistance: dimCreateAngleOrDistance,
    createDimensionCircle: dimCreateCircle,
    createDimensionLine: dimCreateLine,
    createDimensionLineAngle: dimCreateLineAngle,
    createDimensionLinear: dimCreateLinearThin,
    createDimensionVertexDistance: dimCreatePointDistance,
    createDimensionPolygon: dimCreatePolygon,
    selectDimensionCircle: dimSelectCircle,
    selectDimensionLine: dimSelectLine,
    selectDimensionPolygon: dimSelectPolygon,
    createDimensionArc: dimCreateArc,
    createDimensionArcLength: dimCreateArcLength,
    selectDimensionArc: dimSelectArc,
  } = createDimensionToolActions({
    pendingDimensionIdRef,
    pendingDimSourceEntityIdRef,
    pendingDimensionPlacementRef,
    dimensionToolFirstLineRef,
    pendingAngleIsReflexRef,
    pendingReflexAngleRef,
    pendingRelationPlacementLabelRef,
    pendingRelationPlacementMatchRef,
    addSketchCircleRadiusDimensionRef,
    addSketchArcRadiusDimensionRef,
    addSketchArcLengthDimensionRef,
    addSketchLineLengthDimensionRef,
    addSketchLineAngleDimensionRef,
    addSketchPolygonRadiusDimensionRef,
    addSketchAngleDimensionRef,
    addSketchDistanceDimensionRef,
    addSketchVertexDistanceDimensionRef,
    updateSketchDimensionRef,
    setDimensionToolFirstLine,
  });

  // Override the thin createDimensionLinear from dimensionToolActions with
  // the real implementation that has access to ViewportPanel refs.
  function startLinearPlacement(lineId: string) {
    // Run the original thin action (sets refs, marks placement pending).
    dimCreateLinearThin(lineId);

    // Try to resolve line endpoints now; defer to pointer move if
    // sketch data isn't available yet.
    const sketch = sketchLinesRef.current;
    const line = sketch?.lines.find((l) => l.line_id === lineId);

    linearPlacementRef.current = {
      lineId,
      startPointId: line?.start_vertex_id ?? "",
      endPointId: line?.end_vertex_id ?? "",
      startX: line?.start_x ?? 0,
      startY: line?.start_y ?? 0,
      endX: line?.end_x ?? 0,
      endY: line?.end_y ?? 0,
      currentAxis: undefined,
      lastCursorX: 0,
      lastCursorY: 0,
    };

    // Disable orbit controls during placement.
    controlsRef.current!.enabled = false;
    const canvas = rendererRef.current?.domElement as HTMLCanvasElement | undefined;
    if (canvas) canvas.style.cursor = "grabbing";
  }

  function dimensionCoreValue(
    dimension: SketchDimensionScene,
    displayValue: number,
  ) {
    return computeDimensionCoreValue({
      dimension,
      displayValue,
      sketch: sketchLinesRef.current,
    });
  }

  function formattedDimensionDisplayValue(
    dimension: SketchDimensionScene,
    coreValue: number,
  ) {
    return formatDimensionDisplayValue({
      dimension,
      coreValue,
      sketch: sketchLinesRef.current,
      displayUnits: config.displayUnits,
    });
  }

  function setCanvasCursor(cursor: string) {
    const canvas = rendererRef.current?.domElement as
      | HTMLCanvasElement
      | undefined;
    if (canvas) {
      canvas.style.cursor = cursor;
    }
  }

  function angleDimensionFrame(
    dimension: SketchDimensionScene,
  ): AngleDimensionFrame | null {
    return buildAngleDimensionFrame({
      dimension,
      sketchParameters: sketchLinesRef.current,
    });
  }

  function setAnglePlacementPreview(
    dimensionId: string,
    dimension: SketchDimensionScene,
    angle: number,
  ) {
    anglePlacementPreviewsRef.current = {
      ...anglePlacementPreviewsRef.current,
      [dimensionId]: dimension,
    };
    anglePlacementPreviewValuesRef.current = {
      ...anglePlacementPreviewValuesRef.current,
      [dimensionId]: angle,
    };
    setAnglePlacementPreviews(anglePlacementPreviewsRef.current);
  }

  function updateAngleDimensionPlacementPreview(
    drag: DimensionLabelDragState,
    cursorLocal: [number, number],
  ) {
    const relation = drag.anglePlacementRelation;
    const sketch = sketchLinesRef.current;
    const planeId = activeSketchPlaneIdRef.current;
    if (!relation || relation.kind !== "line_angle" || !sketch || !planeId) {
      return false;
    }

    const firstLine = sketch.lines.find(
      (line) => line.line_id === relation.firstEntityId,
    );
    const secondLine = sketch.lines.find(
      (line) => line.line_id === relation.targetEntityId,
    );
    if (!firstLine || !secondLine) {
      return false;
    }

    const preview = createLineAnglePreview({
      first: firstLine,
      second: secondLine,
      cursor: cursorLocal,
      planeId,
      planeFrame: activeSketchPlaneFrameRef.current,
    });
    if (!preview) {
      return false;
    }

    const currentDimension = displayedSketchDimensionsRef.current.find(
      (dimension) => dimension.dimensionId === drag.dimensionId,
    );
    setAnglePlacementPreview(
      drag.dimensionId,
      {
        ...preview.dimension,
        dimensionId: drag.dimensionId,
        entityId: currentDimension?.entityId ?? preview.dimension.entityId,
        isSelected: currentDimension?.isSelected ?? true,
        driven: currentDimension?.driven,
      },
      preview.anglePreview.angle,
    );
    return true;
  }

  /** Rebuild the Three.js objects for a single dimension in-place —
   *  disposes the old geometry, builds new geometry from the shifted
   *  scene data, and swaps it into the sketch group.  Used during
   *  label drag to avoid full scene rebuilds. */
  function rebuildDimensionSceneObject(
    shifted: SketchDimensionScene,
    displayUnits?: "mm" | "in",
  ) {
    const sketchGroup = sketchGroupRef.current;
    if (!sketchGroup) return;

    const objects = dimensionObjectByIdRef.current.get(shifted.dimensionId);
    if (!objects) return;

    // Build replacement objects from the shifted dimension data
    const replacement = buildSketchDimensionObject(shifted, displayUnits);

    // Swap line group: remove old, dispose, add new
    sketchGroup.remove(objects.line);
    disposeGroup(objects.line);

    // Swap label sprite: remove old, dispose, add new
    sketchGroup.remove(objects.label);
    objects.label.geometry?.dispose();
    const mat = objects.label.material as THREE.SpriteMaterial;
    mat?.map?.dispose();
    mat?.dispose();

    sketchGroup.add(replacement.line);
    sketchGroup.add(replacement.label);

    // Update refs so the dimension is still findable
    dimensionObjectByIdRef.current.set(shifted.dimensionId, {
      line: replacement.line as THREE.Group,
      label: replacement.label,
    });

    // Update sketchDimensionObjectsRef (flat array) — swap old entries
    const all = sketchDimensionObjectsRef.current;
    const oldLineIdx = all.indexOf(objects.line);
    const oldLabelIdx = all.indexOf(objects.label);
    if (oldLineIdx !== -1) all[oldLineIdx] = replacement.line;
    if (oldLabelIdx !== -1) all[oldLabelIdx] = replacement.label;
  }

  const {
    beginDimensionPlacement,
    cancelDimensionPlacement,
    finishDimensionPlacement,
    getDimensionPlacementAxis,
    persistDimensionDragLabelPosition,
    setAngleDimensionDragRadius,
    setDimensionLabelPosition,
  } = createDimensionPlacementActions({
    rendererRef,
    cameraRef,
    controlsRef,
    activeSketchPlaneIdRef,
    activeSketchPlaneFrameRef,
    lastPointerEventRef,
    dimensionLabelDragRef,
    dimensionPlacementOriginalPositionRef,
    pendingRelationPlacementLabelRef,
    dimensionLabelPositionsRef,
    setDimensionLabelPositions,
    dimensionObjectByIdRef,
    angleDragRadiiRef,
    setAngleDragRadii,
    anglePlacementPreviewsRef,
    setAnglePlacementPreviews,
    anglePlacementPreviewValuesRef,
    displayedSketchDimensionsRef,
    updateSketchDimensionRef,
    updateSketchDimensionLabelPositionRef,
    angleDimensionFrame,
    clearPreviewDimension,
    setCanvasCursor,
    rebuildDimensionSceneObject,
    displayUnits: config.displayUnits,
  });

  const {
    commitDimensionRelationPreview,
    startPendingRelationPlacementIfReady,
    stopPendingRelationPlacementRetry,
  } = createDimensionRelationPlacementActions({
    rendererRef,
    cameraRef,
    controlsRef,
    activeSketchPlaneIdRef,
    activeSketchPlaneFrameRef,
    activeSketchToolRef,
    lastPointerEventRef,
    dimensionRelationPreviewRef,
    dimensionRelationPreviewLabelRef,
    pendingRelationPlacementLabelRef,
    pendingRelationPlacementMatchRef,
    pendingRelationPlacementRetryRef,
    pendingDimensionIdRef,
    pendingDimSourceEntityIdRef,
    pendingDimensionPlacementRef,
    dimensionLabelDragRef,
    dimensionPlacementOriginalPositionRef,
    dimensionToolFirstLineRef,
    dimensionToolFirstPointRef,
    displayedSketchDimensionsRef,
    sketchLinesRef,
    deleteSketchDimensionRef,
    setDimensionToolFirstLine,
    clearPreviewDimension,
    setCanvasCursor,
    createDimensionAngleOrDistance: dimCreateAngleOrDistance,
    beginDimensionPlacement,
  });

  function cancelActiveSketchDraft() {
    if (armedSketchConstraintRef.current) {
      cancelSketchConstraintRef.current();
      return;
    }
    lineDraftStartRef.current = null;
    arcSecondPointRef.current = null;
    rectSecondPointRef.current = null;
    circleSecondPointRef.current = null;
    circleTangentLineIdsRef.current = [];
    ellipseSecondPointRef.current = null;
    splineDraftPolesRef.current = [];
    clearPreviewLine();
    clearPreviewCircle();
    clearPreviewArc();
    clearPreviewSlot();
    clearPreviewSpline();
    clearPreviewDimension();
    clearPreviewInference();
    clearDraftDimensionSession();
    setSketchSnapLabel(null);
    setConstraintPreview(null);
    dragSnapResultRef.current = null;
    setHoveredSketchEntity(null);
    setHoveredSketchPoint(null);
    setHoveredSketchProfile(null);
    clearTrimHighlights(clearTrimSegmentHighlight, clearTrimArcHighlight);
    // Corner/split/stroke session state dies with the tool switch.
    cornerFirstEntityIdRef.current = null;
    splitFirstPickRef.current = null;
    trimStrokeRef.current = null;
    cornerPreviewLastSentRef.current = null;
    cornerTrimPreviewRef.current = null;
    clearCornerPreview();
    void setSketchToolRef.current("select");
  }

  // The three_point arc's chord is defined by the SECOND click, not by
  // `current` — a typed chord length must move that point itself for
  // the preview to follow. For center_start_end a typed radius
  // rescales the session's arc-start point, which is authoritative
  // over the ref (the ref holds the point as placed). All preview
  // paths (effect + pointer move + dimension render loop) read this
  // selector.
  function adjustedArcSecondPoint() {
    const session = draftDimensionSessionRef.current;
    if (session && session.tool === "arc") {
      if (session.toolMode === "center_start_end" && session.secondPoint) {
        return session.secondPoint;
      }
      return typedChordSecondPoint(session) ?? arcSecondPointRef.current;
    }
    return arcSecondPointRef.current;
  }

  function adjustedEllipseAxisPoint() {
    const session = draftDimensionSessionRef.current;
    if (session && session.tool === "ellipse" && session.touchedFields.radiusX) {
      return (
        typedRadiusSecondPoint(session, "radiusX") ??
        ellipseSecondPointRef.current
      );
    }
    return ellipseSecondPointRef.current;
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
    clearPreviewInference();
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
      arcSecondPoint: adjustedArcSecondPoint(),
      circleSecondPoint: circleSecondPointRef.current,
      rectSecondPoint: rectSecondPointRef.current,
      ellipseSecondPoint: adjustedEllipseAxisPoint(),
      isConstruction: sketchToolConstructionRef.current,
      previewLineRef,
      previewCircleRef,
      previewArcRef,
      previewSlotRef,
      previewSplineRef,
      splineDraftPolesRef,
      previewDimensionRef,
      previewInferenceRef,
      clearPreviewLine,
      clearPreviewCircle,
      clearPreviewArc,
      clearPreviewSlot,
      clearPreviewSpline,
      clearPreviewDimension,
      clearPreviewInference,
    });
  }

  // Rebuilds the spline draft preview from the placed poles (click-
  // driven, not mouse-follow — the preview only changes per click).
  function renderSplineDraftPreview() {
    const sketchGroup = sketchGroupRef.current;
    if (!sketchGroup || !activeSketchPlaneId) {
      return;
    }
    clearPreviewSpline();
    const preview = buildSplineDraftPreview({
      poles: splineDraftPolesRef.current,
      planeId: activeSketchPlaneId,
      planeFrame: activeSketchPlaneFrame,
      isConstruction: sketchToolConstructionRef.current,
      cursor: null,
    });
    if (preview) {
      previewSplineRef.current = preview;
      sketchGroup.add(preview);
    }
  }

  // Commits the in-progress spline draft (double-click, Enter, tool
  // switch).  Clears the poles + preview; the core applies the real
  // B-spline on the next document state.
  function commitSplineDraft() {
    const poles = splineDraftPolesRef.current;
    if (poles.length < 2) {
      // Nothing worth committing — clear the stub draft.
      splineDraftPolesRef.current = [];
      clearPreviewSpline();
      return;
    }
    void addSketchSplineRef.current(
      poles.map((p) => ({ x: p[0], y: p[1] })),
      sketchToolConstructionRef.current,
    );
    splineDraftPolesRef.current = [];
    clearPreviewSpline();
  }
  commitSplineDraftRef.current = commitSplineDraft;

  // Leaving the spline tool commits the draft (Fusion-style) instead
  // of discarding it — Escape cancels BEFORE this runs (it clears the
  // poles first), so cancel stays cancel.
  useEffect(() => {
    if (activeSketchTool !== "spline") {
      if (splineDraftPolesRef.current.length >= 2) {
        void addSketchSplineRef.current(
          splineDraftPolesRef.current.map((p) => ({ x: p[0], y: p[1] })),
          sketchToolConstructionRef.current,
        );
      }
      splineDraftPolesRef.current = [];
      clearPreviewSpline();
    }
    // clearPreviewSpline is a fresh closure per render — depending on
    // it would re-run the effect every render (harmless but wasteful).
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [activeSketchTool]);

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

  async function commitDraftDimensionSession(
    session = draftDimensionSessionRef.current,
  ) {
    if (!session) {
      return;
    }
    const [startX, startY] = session.start;
    const [endX, endY] = session.current;
    if (session.tool === "arc" || session.tool === "ellipse") {
      // 3-click drafts: Enter commits once the second defining point
      // exists (the badge's change handler already applied the typed
      // value to the live draft). At the arc's stage 1 Enter places
      // the second end — the same transition as the click commit, so
      // a typed length fixes the chord — instead of silently doing
      // nothing.
      if (!session.secondPoint) {
        if (session.tool === "arc") {
          const next = advanceArcDraftSession(session, session.current);
          arcSecondPointRef.current = next.secondPoint;
          setDraftDimensionSession(next);
          focusDraftField("length");
        }
        return;
      }
      clearPreviewLine();
      clearPreviewCircle();
      clearPreviewArc();
      clearPreviewDimension();
      lineDraftStartRef.current = null;
      if (session.tool === "arc") {
        await beginUndoGroupRef.current("Dimension");
        scheduleDimensionDeletion(session.tool, session);
        draftUndoGroupOpenRef.current = true;
        clearDraftDimensionSession();
        suppressDimensionEditorAfterSketchCommit();
        rendererRef.current?.domElement.focus();
        if (arcToolModeRef.current === "three_point") {
          // A typed chord length moves the second end onto the typed
          // distance; the anchor (current) is already reshaped live.
          const arcEnd =
            typedChordSecondPoint(session) ?? session.secondPoint;
          void addSketchArcRef.current(
            startX,
            startY,
            arcEnd[0],
            arcEnd[1],
            endX,
            endY,
            arcToolModeRef.current,
            sketchToolConstructionRef.current,
          );
        } else {
          // center_start_end: (arc start, arc end, center anchor) —
          // the end `current` already sits on the typed chord.
          void addSketchArcRef.current(
            session.secondPoint[0],
            session.secondPoint[1],
            endX,
            endY,
            startX,
            startY,
            arcToolModeRef.current,
            sketchToolConstructionRef.current,
          );
        }
        arcSecondPointRef.current = null;
        return;
      }
      // Ellipse — no dimension to drain; one add is one undo step.
      const axisEnd =
        session.touchedFields.radiusX
          ? (typedRadiusSecondPoint(session, "radiusX") ??
            session.secondPoint)
          : session.secondPoint;
      const axisDx = axisEnd[0] - startX;
      const axisDy = axisEnd[1] - startY;
      const axisLength = Math.hypot(axisDx, axisDy);
      const minorLength =
        Math.abs((axisDx * (endY - startY) - axisDy * (endX - startX)) /
          axisLength);
      if (axisLength <= 0.001 || minorLength <= 0.001) {
        // Degenerate (cursor on the major-axis line) — stay armed
        // instead of failing, same as the click commit.
        return;
      }
      clearDraftDimensionSession();
      suppressDimensionEditorAfterSketchCommit();
      rendererRef.current?.domElement.focus();
      void addSketchEllipseRef.current(
        startX,
        startY,
        axisEnd[0],
        axisEnd[1],
        endX,
        endY,
        sketchToolConstructionRef.current,
      );
      ellipseSecondPointRef.current = null;
      return;
    }
    clearPreviewLine();
    clearPreviewCircle();
    clearPreviewArc();
    clearPreviewDimension();
    lineDraftStartRef.current = null;
    // One draft commit = ONE undo step (D2): the group wraps the
    // entity add below plus the scheduled auto-dim deletion and
    // expression update that follow in the post-commit effect. The
    // flag goes on AFTER the pending work is scheduled — the effect
    // also runs on the begin_group's own document reply (nothing
    // pending yet) and must not close the group there.
    await beginUndoGroupRef.current("Dimension");
    scheduleDimensionDeletion(session.tool, session);
    scheduleDraftDimensionExpressionUpdate(session.tool);
    draftUndoGroupOpenRef.current = true;
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
    options?: ResolveSnapOptions,
  ) {
    const localFilter: SelectionFilter = readStoredFilter();
    const effectiveFilter = altHeldRef.current
      ? invertSelectionFilter(localFilter)
      : localFilter;
    const worldUnitsPerPixel =
      cameraRef.current && rendererRef.current
        ? getOrthographicViewHeight(cameraRef.current) /
            Math.max(rendererRef.current.domElement.clientHeight, 1)
        : 1;
    worldUnitsPerPixelRef.current = worldUnitsPerPixel;
    return resolveSnappedSketchPointFromContext({
      rawPoint,
      draftStartLocal,
      sketchSnapCandidates: sketchSnapCandidatesRef.current,
      sketchParameters: sketchLinesRef.current,
      sketchConstraints: sketchConstraintsRef.current,
      dynamicSnapsEnabled: options?.dynamicSnapsEnabled,
      objectSnapLatchKey: options?.objectSnapLatchKey,
      inferenceSnapsEnabled: options?.inferenceSnapsEnabled,
      excludeEntityIds: options?.excludeEntityIds,
      filter: effectiveFilter,
      activeSketchPlaneId,
      activeSketchPlaneFrame,
      currentGridSpacing: currentGridSpacingRef.current,
      worldUnitsPerPixel,
      gridSnapScreenDistancePx: GRID_SNAP_SCREEN_DISTANCE_PX,
      sketchSnapDistance: effectiveFilter.tolerance_px * worldUnitsPerPixel,
      labels: {
        grid: translate("snap.grid"),
        axisLockHorizontal: translate("snap.axisLockHorizontal"),
        axisLockVertical: translate("snap.axisLockVertical"),
        onLine: translate("snap.onLine"),
        onCircle: translate("snap.onCircle"),
        tangent: translate("snap.tangent"),
        perpendicular: translate("snap.perpendicular"),
        parallel: translate("snap.parallel"),
        intersection: translate("snap.intersection"),
      },
    });
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

  const hoveredSketchEntityIdRef = useRef<string | null>(null);
  const hoveredSketchPointIdRef = useRef<string | null>(null);
  const hoveredEdgeIdRef = useRef<string | null>(null);
  const hoveredVertexIdRef = useRef<string | null>(null);
  const {
    paintDofStatusColors,
    paintEdgeMaterials,
    paintSketchEntityMaterials,
    paintSketchPointMaterials,
    paintVertexMaterials,
    setHoveredEdge,
    setHoveredFace,
    setHoveredPrimitive,
    setHoveredReference,
    setHoveredSketchEntity,
    setHoveredSketchPoint,
    setHoveredSketchProfile,
    setHoveredVertex,
    syncPrimitiveVisuals,
    syncReferencePlaneVisuals,
    syncSketchProfileVisuals,
    syncSolidFaceVisuals,
  } = createViewportVisualStateActions({
    primitiveVisualsRef,
    primitiveStatesRef,
    referencePlaneVisualsRef,
    referencePlaneStatesRef,
    solidFaceVisualsRef,
    solidFaceStatesRef,
    sketchProfileVisualsRef,
    sketchProfileStatesRef,
    sketchEntityObjectsRef,
    sketchPointObjectsRef,
    edgeLineObjectsRef,
    vertexObjectsRef,
    revealGhostEdgesRef,
    dofMapRef,
    hoveredSketchEntityIdRef,
    hoveredSketchPointIdRef,
    hoveredEdgeIdRef,
    hoveredVertexIdRef,
  });

  useViewportCallbackRefs(
    {
      selectPrimitiveRef,
      selectReferenceRef,
      selectFaceRef,
      selectEdgeRef,
      selectVertexRef,
      startSketchRef,
      startSketchOnFaceRef,
      setSketchMidpointAnchorRef,
      setSketchPointLineAnchorRef,
      addSketchLineRef,
      addSketchRectangleRef,
      addSketchCircleRef,
      addSketchCircleModeRef,
      addSketchArcRef,
      addSketchEllipseRef,
      addSketchSplineRef,
      addSketchSlotRef,
      addSketchAngleDimensionRef,
      addSketchDistanceDimensionRef,
      addSketchLineLengthDimensionRef,
      addSketchLineAngleDimensionRef,
      addSketchCircleRadiusDimensionRef,
      addSketchArcRadiusDimensionRef,
      addSketchArcLengthDimensionRef,
      addSketchPolygonRadiusDimensionRef,
      setSketchLineConstraintRef,
      setSketchPerpendicularConstraintRef,
      setSketchTangentConstraintRef,
      setSketchParallelConstraintRef,
      arcToolModeRef,
      rectangleToolModeRef,
      circleToolModeRef,
      polygonToolModeRef,
      polygonSidesRef,
      addSketchPolygonRef,
      addSketchFilletRef,
      addSketchChamferRef,
      addSketchTextRef,
      pickSketchTextRef,
      pickSketchSlotRef,
      pickSketchChamferRef,
      extendSketchEntityRef,
      offsetSketchEntityRef,
      selectSketchEntityRef,
      pickInactiveSketchLineRef,
      inactiveSketchEntityPickEnabledRef,
      pickSketchPointRef,
      updateSketchPointRef,
      moveSketchEntitiesRef,
      selectSketchDimensionRef,
      updateSketchDimensionRef,
      updateSketchDimensionLabelPositionRef,
      addSketchVertexDistanceDimensionRef,
      updateSketchDimensionDisplayRef,
      selectSketchProfileRef,
      trimSketchEntityRef,
      deleteSketchSelectionRef,
      setSketchToolRef,
      armedSketchConstraintRef,
      mirrorFocusedSlotRef,
      mirrorEntityPickRef,
      cancelSketchConstraintRef,
      clearSketchConstraintRef,
      moveGizmoRef,
      moveGizmoChangeRef,
      moveBodyRef,
      copyBodyRef,
      exportBodyMeshRef,
      exportBodyStepRef,
      sendBodyToSlicerRef,
      unlinkBodyCopyRef,
      removeSketchProjectionsRef,
    },
    {
      onSelectPrimitive,
      onSelectReference,
      onSelectFace,
      onSelectEdge,
      onSelectVertex,
      onStartSketch,
      onStartSketchOnFace,
      onSetSketchMidpointAnchor,
      onSetSketchPointLineAnchor,
      onAddSketchLine,
      onAddSketchRectangle,
      onAddSketchCircle,
      onAddSketchCircleMode,
      onAddSketchArc,
      onAddSketchEllipse,
      onAddSketchSlot,
      onAddSketchSpline,
      onAddSketchChamfer,
      onAddSketchAngleDimension,
      onAddSketchDistanceDimension,
      onAddSketchLineLengthDimension,
      onAddSketchLineAngleDimension,
      onAddSketchCircleRadiusDimension,
      onAddSketchArcRadiusDimension,
      onAddSketchArcLengthDimension,
      onAddSketchPolygonRadiusDimension,
      onSetSketchLineConstraint,
      onSetSketchPerpendicularConstraint,
      onSetSketchTangentConstraint,
      onSetSketchParallelConstraint,
      arcToolMode,
      rectangleToolMode,
      circleToolMode,
      polygonToolMode,
      polygonSides,
      onAddSketchPolygon,
      onAddSketchFillet,
      onAddSketchText,
      onPickSketchText,
      onPickSketchSlot,
      onPickSketchChamfer,
      onExtendSketchEntity,
      onOffsetSketchEntity,
      onSelectSketchEntity,
      onPickInactiveSketchLine,
      inactiveSketchEntityPickEnabled,
      onPickSketchPoint,
      onUpdateSketchPoint,
      onMoveSketchEntities,
      onSelectSketchDimension,
      onUpdateSketchDimension,
      onUpdateSketchDimensionLabelPosition,
      onAddSketchVertexDistanceDimension,
      onUpdateSketchDimensionDisplay,
      onSelectSketchProfile,
      onTrimSketchEntity,
      onDeleteSketchSelection,
      onSetSketchTool,
      onOpenTransformArray,
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
      onExportBodyStep,
      onSendBodyToSlicer,
      onUnlinkBodyCopy,
      onRemoveSketchProjections,
    },
  );

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
        requestViewportRenderRef.current?.();
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

  usePendingLineCommitRelations({
    sketchParameters: sketchFeature?.sketch_parameters,
    sketchLinesRef,
    sketchLineCountRef,
    pendingRefs: {
      midpointAnchor: pendingMidpointAnchorRef,
      perpendicularConstraint: pendingPerpendicularConstraintRef,
      pointLineAnchor: pendingPointLineAnchorRef,
      axisConstraint: pendingAxisConstraintRef,
      tangentConstraint: pendingTangentConstraintRef,
      parallelConstraint: pendingParallelConstraintRef,
    },
    actionRefs: {
      setSketchMidpointAnchorRef,
      setSketchPerpendicularConstraintRef,
      setSketchPointLineAnchorRef,
      setSketchLineConstraintRef,
      setSketchTangentConstraintRef,
      setSketchParallelConstraintRef,
    },
  });

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
    anglePlacementPreviewsRef.current = anglePlacementPreviews;
  }, [anglePlacementPreviews]);

  useDimensionEditorEffects({
    selectedSketchDimension,
    selectedSketchDimensionId: document?.selected_sketch_dimension_id,
    selectedSketchDimensionValue,
    selectedSketchDimensionExpression,
    isDimensionEditorOpen,
    dimensionInputRef,
    dimensionInputSelectionLockedRef,
    dimensionEditOriginalValueRef,
    suppressNextDimensionEditorOpenRef,
    setDimensionDraftValue,
    setIsDimensionEditorOpen,
    isProjectedCircleDimension,
    formattedDimensionDisplayValue,
  });

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
    camera.up.set(0, 0, 1);
    const controls = new OrbitControls(camera, renderer.domElement);
    const contentGroup = new THREE.Group();
    const referenceGroup = new THREE.Group();
    const sketchGroup = new THREE.Group();
    const drawingGroup = new THREE.Group();
    const raycaster = new THREE.Raycaster();
    const pointer = new THREE.Vector2();
    let pointerDown: { x: number; y: number } | null = null;
    let frameId: number | null = null;
    let renderBurstUntil = 0;

    // Resolves a pointer event to a sheet-mm point on the drawing
    // sheet plane (z = 0, the FIRST sheet's coordinates) — shared by
    // the insert-view hover/click and the view-frame drag.
    const resolveSheetPoint = (
      pickEvent: PointerEvent,
    ): [number, number] | null => {
      setPointerNdcFromEvent(pointer, pickEvent, renderer);
      raycaster.setFromCamera(pointer, camera);
      const hit = new THREE.Vector3();
      if (
        raycaster.ray.intersectPlane(
          new THREE.Plane(new THREE.Vector3(0, 0, 1), 0),
          hit,
        )
      ) {
        return [
          Math.round(hit.x * 1000) / 1000,
          Math.round(hit.y * 1000) / 1000,
        ];
      }
      return null;
    };

    rendererRef.current = renderer;
    sceneRef.current = scene;
    cameraRef.current = camera;
    controlsRef.current = controls;
    contentGroupRef.current = contentGroup;
    referenceGroupRef.current = referenceGroup;
    sketchGroupRef.current = sketchGroup;
    drawingGroupRef.current = drawingGroup;

    renderer.setPixelRatio(window.devicePixelRatio);
    scene.add(contentGroup);
    scene.add(referenceGroup);
    scene.add(sketchGroup);
    scene.add(drawingGroup);
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
    controls.addEventListener("change", requestRenderOnControlsChange);
    // Record the CAD camera for the "Current 3D view" capture (R1):
    // the view direction is controls.target − camera.position, the
    // frame's x_direction is the camera's right axis projected onto
    // the view plane (fallback: the up axis).  The capture ref stays
    // NULL until the first CAD camera exists — the ribbon's strip
    // gates its button on that.
    const captureCadCameraFrame = () => {
      const snapshot = lastCadCameraRef.current;
      if (!snapshot) {
        return null;
      }
      const dx = snapshot.target[0] - snapshot.position[0];
      const dy = snapshot.target[1] - snapshot.position[1];
      const dz = snapshot.target[2] - snapshot.position[2];
      const normalLength = Math.hypot(dx, dy, dz);
      if (normalLength < 1e-9) {
        return null;
      }
      const normal: [number, number, number] = [
        dx / normalLength,
        dy / normalLength,
        dz / normalLength,
      ];
      const projectOntoViewPlane = (
        v: [number, number, number],
      ): [number, number, number] | null => {
        const dot = v[0] * normal[0] + v[1] * normal[1] + v[2] * normal[2];
        const projected: [number, number, number] = [
          v[0] - dot * normal[0],
          v[1] - dot * normal[1],
          v[2] - dot * normal[2],
        ];
        const length = Math.hypot(projected[0], projected[1], projected[2]);
        if (length < 1e-9) {
          return null;
        }
        return [projected[0] / length, projected[1] / length, projected[2] / length];
      };
      const xDirection =
        projectOntoViewPlane(snapshot.right) ??
        projectOntoViewPlane(snapshot.up);
      if (!xDirection) {
        return null;
      }
      return { normal, x_direction: xDirection };
    };
    controls.addEventListener("change", () => {
      if (showDrawingSheetRef.current) {
        return;
      }
      camera.updateMatrixWorld();
      const elements = camera.matrixWorld.elements;
      lastCadCameraRef.current = {
        target: [controls.target.x, controls.target.y, controls.target.z],
        position: [camera.position.x, camera.position.y, camera.position.z],
        right: [elements[0], elements[1], elements[2]],
        up: [elements[4], elements[5], elements[6]],
      };
      if (cameraFrameCaptureRef) {
        cameraFrameCaptureRef.current = captureCadCameraFrame;
      }
    });
    if (cameraFrameCaptureRef) {
      cameraFrameCaptureRef.current = null;
    }

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

      // Skip the dispose/rebuild when nothing that shapes the group
      // changed since the last frame. The key covers every input the
      // preview builder reads; camera/zoom/canvas-size are included so
      // the projected badge positions stay correct on pan/zoom. The
      // group must still be in the scene (a cleared group resets
      // draftDimGroupRef to null), which also makes the skip safe
      // across session boundaries.
      const dimArcSecondPoint = adjustedArcSecondPoint();
      const cacheKey = [
        session.tool,
        session.start.join(","),
        session.current.join(","),
        session.secondPoint ? session.secondPoint.join(",") : "none",
        session.toolMode ?? "",
        dimArcSecondPoint ? dimArcSecondPoint.join(",") : "none",
        arcToolModeRef.current,
        previousLineAngleRef.current ?? "",
        camera.position.toArray().join(","),
        camera.zoom,
        renderer.domElement.height,
        activeSketchPlaneIdRef.current ?? "",
        JSON.stringify(activeSketchPlaneFrameRef.current?.normal ?? null),
      ].join("|");
      if (cacheKey === lastDraftDimKeyRef.current && draftDimGroupRef.current) {
        // Group is already in the scene — just refresh the projected
        // badge positions.
        draftDimScreenPositionsRef.current =
          lastDraftDimPositionsRef.current;
        return;
      }
      lastDraftDimKeyRef.current = cacheKey;

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
        lastDraftDimPositionsRef.current = {};
        return;
      }

      draftDimScreenPositionsRef.current = preview.screenPositions;
      lastDraftDimPositionsRef.current = preview.screenPositions;
      if (preview.kind === "positions") {
        return;
      }

      sketchGroup.add(preview.group);
      draftDimGroupRef.current = preview.group;
    }

    function requestRender(burstMs = 0) {
      if (burstMs > 0) {
        renderBurstUntil = Math.max(renderBurstUntil, performance.now() + burstMs);
      }
      if (frameId !== null) {
        return;
      }
      frameId = window.requestAnimationFrame(renderScheduledFrame);
    }

    function renderScheduledFrame() {
      frameId = null;
      render();
      if (viewCubeAnimatingRef.current || performance.now() < renderBurstUntil) {
        requestRender();
      }
    }

    requestViewportRenderRef.current = requestRender;

    function requestRenderOnControlsChange() {
      requestRender();
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
        worldUnitsPerPixel:
          getOrthographicViewHeight(camera) /
          Math.max(renderer.domElement.clientHeight, 1),
      });
      // Sync grid spacing state for the scale indicator overlay.
      if (currentGridSpacingRef.current !== currentGridSpacing) {
        setCurrentGridSpacing(currentGridSpacingRef.current);
      }
      updateScreenSpaceSketchSprites({
        renderer,
        camera,
        sketchDimensionObjects: sketchDimensionObjectsRef.current,
        sketchConstraintObjects: sketchConstraintObjectsRef.current,
      });
      // Body vertex dots stay a constant SCREEN size. The fixed
      // world-space sphere (0.25 units) is sub-pixel on large parts —
      // corner clicks fell through to the adjacent edge/face picks,
      // making vertex projection unusable ("does not work"). A
      // screen-size dot matches the sketch-point behavior and makes
      // the Project-tool vertex target hittable at any zoom.
      // While a sketch is ACTIVE the dots are clutter (depthTest off
      // draws them over the sketch); their only sketch-mode job is
      // being Project-tool targets, so they show there only while the
      // Project tool is armed. 3D mode keeps them at all times.
      const showBodyVertexDots =
        activeSketchPlaneIdRef.current === null ||
        activeSketchToolRef.current === "project";
      const vertexWorldUnitsPerPixel =
        getOrthographicViewHeight(camera) /
        Math.max(renderer.domElement.clientHeight, 1);
      for (const mesh of vertexObjectsRef.current) {
        if (!showBodyVertexDots) {
          mesh.visible = false;
          continue;
        }
        mesh.visible = true;
        const id = mesh.userData.vertexId as string | undefined;
        const boosted =
          (id !== undefined && id === hoveredVertexIdRef.current) ||
          mesh.userData.isSelected === true;
        mesh.scale.setScalar(vertexWorldUnitsPerPixel * (boosted ? 4 : 3));
      }
      // Sketch points get the same treatment: their world-space radii
      // (0.7–0.9) balloon when zooming into a sketch ("all the dots
      // are huge again" after re-entering). Normalize by each mesh's
      // geometry radius so every point kind shares the body-dot pixel
      // size (3 px, 4 px hovered/selected).
      for (const mesh of sketchPointObjectsRef.current) {
        const radius =
          (mesh.geometry as THREE.SphereGeometry).parameters.radius ?? 0.7;
        const id = mesh.userData.sketchPointId as string | undefined;
        const boosted =
          (id !== undefined && id === hoveredSketchPointIdRef.current) ||
          mesh.userData.isSelected === true;
        mesh.scale.setScalar(
          (vertexWorldUnitsPerPixel * (boosted ? 4 : 3)) / radius,
        );
      }
      // Constraint glyphs (Fix/H/V/…) are OFF by default — they sit on
      // top of dense sketch geometry and read as noise. The toolbar
      // toggle flips this ref; the override runs per frame so it also
      // covers sprites rebuilt by scene sync.
      for (const obj of sketchConstraintObjectsRef.current) {
        obj.visible = showConstraintsRef.current;
      }
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

      // Resolve BOTH drag corners to sketch-local coordinates and let
      // the CORE do the rectangle selection against the exact sketch
      // geometry. The old UI-side screen-space collection walked the
      // scene data with projection math — a stale scene (or a rect
      // crossing the perimeter) mis-collected, and the following
      // delete then removed the perimeter instead of the marquee'd
      // entities. The core sees the real geometry, so the marquee can
      // never disagree with what gets deleted.
      const startPoint = resolveSketchPlanePoint(
        { clientX: drag.startX, clientY: drag.startY } as PointerEvent,
        renderer,
        camera,
        activeSketchPlaneIdRef.current,
        activeSketchPlaneFrameRef.current,
      );
      const endPoint = resolveSketchPlanePoint(
        { clientX: drag.currentX, clientY: drag.currentY } as PointerEvent,
        renderer,
        camera,
        activeSketchPlaneIdRef.current,
        activeSketchPlaneFrameRef.current,
      );
      if (!startPoint || !endPoint) return;

      const windowMode = drag.currentX >= drag.startX;
      await onSelectSketchRect(
        startPoint.local[0],
        startPoint.local[1],
        endPoint.local[0],
        endPoint.local[1],
        windowMode,
        additive,
      );
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
        activeSketchPlaneFrame:
          activeSketchPlaneFrameRef.current ?? activeSketchPlaneFrame,
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
        tolerancePx: readStoredFilter().tolerance_px,
      });
    }

    function hoverActions() {
      return {
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
    }

    function updateCrosshairPointer(event: PointerEvent, inCube?: boolean) {
      const canvasRect = renderer.domElement.getBoundingClientRect();
      const pointerInCube =
        inCube ??
        isPointerInCubeArea(event, canvasRect, renderer.getPixelRatio());
      if (
        activeSketchPlaneIdRef.current &&
        activeSketchToolRef.current !== "select" &&
        activeSketchToolRef.current !== "project" &&
        !pointerInCube
      ) {
        setCrosshairPointer({
          x: event.clientX - canvasRect.left,
          y: event.clientY - canvasRect.top,
        });
        return;
      }
      setCrosshairPointer(null);
    }

    function runActiveSketchPointerMove(event: PointerEvent) {
      const currentActiveSketchPlaneId = activeSketchPlaneIdRef.current;
      if (!currentActiveSketchPlaneId) {
        return false;
      }

      handleActiveSketchPointerMove({
        event,
        renderer,
        camera,
        activeSketchPlaneId: currentActiveSketchPlaneId,
        activeSketchPlaneFrame: activeSketchPlaneFrameRef.current,
        activeSketchTool: activeSketchToolRef.current,
        activeSketchPlaneFrameRef,
        sceneDataRef,
        trimPreviewLastSentRef,
        hoverActions: hoverActions(),
        intersectSceneTargets,
        draftStartRef: lineDraftStartRef,
        draftDimensionSessionRef,
        objectSnapLatchRef,
        resolveSnappedSketchPoint,
        updateDraftSessionFromPoint,
        setSketchSnapLabel,
        setConstraintPreview,
        setDraftCursorPoint: setCrosshairPointer,
        sketchGroupRef,
        arcToolMode: arcToolModeRef.current,
        circleToolMode: circleToolModeRef.current,
        rectangleToolMode: rectangleToolModeRef.current,
        arcSecondPoint: adjustedArcSecondPoint(),
        circleSecondPoint: circleSecondPointRef.current,
        rectSecondPoint: rectSecondPointRef.current,
        ellipseSecondPoint: adjustedEllipseAxisPoint(),
        isConstruction: sketchToolConstructionRef.current,
        previewLineRef,
        previewCircleRef,
        previewArcRef,
        previewSlotRef,
        previewSplineRef,
        splineDraftPolesRef,
        previewDimensionRef,
        previewInferenceRef,
        clearPreviewLine,
        clearPreviewCircle,
        clearPreviewArc,
        clearPreviewSlot,
        clearPreviewSpline,
        clearPreviewDimension,
        clearPreviewInference,
        clearTrimSegmentHighlight,
        clearTrimArcHighlight,
        updateTrimSegmentHighlight,
        updateTrimArcHighlight,
        trimStrokeRef,
        cornerFirstEntityIdRef,
        cornerPreviewLastSentRef,
        clearCornerPreview,
      });
      if (activeSketchToolRef.current === "spline" &&
          splineDraftPolesRef.current.length >= 2) {
        setSketchSnapLabel(translate("viewport.splineFinishHint"));
      }
      return true;
    }

    function requestDraftPointerMoveFrame(event: PointerEvent) {
      pendingDraftPointerMoveEventRef.current = event;
      if (pendingDraftPointerMoveFrameRef.current !== null) {
        return;
      }

      pendingDraftPointerMoveFrameRef.current = window.requestAnimationFrame(() => {
        pendingDraftPointerMoveFrameRef.current = null;
        const nextEvent = pendingDraftPointerMoveEventRef.current;
        pendingDraftPointerMoveEventRef.current = null;
        if (
          !nextEvent ||
          !activeSketchPlaneIdRef.current ||
          !isDrawableSketchTool(activeSketchToolRef.current)
        ) {
          return;
        }
        runActiveSketchPointerMove(nextEvent);
        requestRender();
      });
    }

    function cancelPendingDraftPointerMoveFrame() {
      pendingDraftPointerMoveEventRef.current = null;
      if (pendingDraftPointerMoveFrameRef.current !== null) {
        window.cancelAnimationFrame(pendingDraftPointerMoveFrameRef.current);
        pendingDraftPointerMoveFrameRef.current = null;
      }
    }

    // Hit-test a committed view's frame ribbon mesh (the named
    // `view-frame:<id>` meshes carry userData.viewFrame).  Shared by
    // the view-drag press and the R1 projected-parent pick.
    function hitTestViewFrame(event: PointerEvent): {
      viewId: string;
      min: [number, number];
      max: [number, number];
    } | null {
      setPointerNdcFromEvent(pointer, event, renderer);
      raycaster.setFromCamera(pointer, camera);
      const hits = raycaster.intersectObjects(
        drawingGroupRef.current?.children ?? [],
        true,
      );
      const frameHit = hits.find(
        (hit) =>
          typeof (hit.object as THREE.Mesh).userData?.viewFrame?.viewId ===
          "string",
      );
      if (!frameHit) {
        return null;
      }
      return (frameHit.object as THREE.Mesh).userData.viewFrame as {
        viewId: string;
        min: [number, number];
        max: [number, number];
      };
    }

    // Starts a view-frame drag: the drag ghost follows the cursor,
    // controls pause so the drag never pans the camera, and the
    // pointer is captured so the drop fires even off-canvas.
    function beginViewFrameDrag(
      event: PointerEvent,
      frame: { viewId: string; min: [number, number]; max: [number, number] },
      point: [number, number],
    ) {
      viewFrameDragRef.current = { viewId: frame.viewId };
      const grabOffset: [number, number] = [
        frame.min[0] - point[0],
        frame.min[1] - point[1],
      ];
      controls.enabled = false;
      renderer.domElement.setPointerCapture(event.pointerId);
      drawingViewDragStartRef.current?.(frame.viewId, point, grabOffset);
    }

    // Hit-test a committed dimension's / annotation's / note's TEXT —
    // the sheet text records carry the owning id in annotation_id
    // (title-block/section-label texts do not).  Proximity test in
    // sheet-mm with the same per-sheet scene x offsets the renderer
    // lays sheets out at.  The purpose maps to the drag owner:
    // "dimension" → dimension, "annotation" → annotation,
    // "note" → free note (its position is absolute sheet-mm).
    function hitTestDimensionText(event: PointerEvent): {
      owner: "dimension" | "annotation" | "note";
      drawingId: string;
      id: string;
      sheetIndex: number;
      text: string;
      heightMm: number;
    } | null {
      const point = resolveSheetPoint(event);
      if (!point) {
        return null;
      }
      const sheets = viewportRef.current?.drawing_sheets ?? [];
      for (let index = 0; index < sheets.length; index += 1) {
        const sheet = sheets[index];
        const offsetX = sheetSceneOffsetX(sheets, index);
        for (const text of sheet.texts) {
          const owner =
            text.purpose === "dimension"
              ? "dimension"
              : text.purpose === "annotation"
                ? "annotation"
                : text.purpose === "note"
                  ? "note"
                  : null;
          if (owner === null || !text.annotation_id) {
            continue;
          }
          const dx = text.position[0] + offsetX - point[0];
          const dy = text.position[1] - point[1];
          if (Math.hypot(dx, dy) <= 5) {
            return {
              owner,
              drawingId: sheet.drawing_id,
              id: text.annotation_id,
              sheetIndex: index,
              text: text.text,
              heightMm: text.height_mm,
            };
          }
        }
      }
      return null;
    }

    // Starts a dimension/annotation/note text drag: the ghost label
    // follows the cursor, controls pause so the drag never pans the
    // camera, and the pointer is captured so the drop fires even
    // off-canvas.
    function beginDimensionTextDrag(
      event: PointerEvent,
      hit: {
        owner: "dimension" | "annotation" | "note";
        drawingId: string;
        id: string;
        sheetIndex: number;
        text: string;
        heightMm: number;
      },
      point: [number, number],
    ) {
      dimensionTextDragRef.current = {
        drawingId: hit.drawingId,
        id: hit.id,
      };
      controls.enabled = false;
      renderer.domElement.setPointerCapture(event.pointerId);
      drawingDimensionTextDragStartRef.current?.(hit, point);
    }

    function handlePointerDown(event: PointerEvent) {
      cancelPendingDraftPointerMoveFrame();
      objectSnapLatchRef.current = null;
      // Pin the document revision for the gesture (M24) — read fresh
      // from the store so async core events land here immediately.
      pointerDownRevisionRef.current =
        useCadCoreStore.getState().document?.revision ?? 0;
      // The insert-view commit's click-vs-pan guard compares against
      // this position on pointer-up.
      drawingPointerDownClientRef.current = {
        x: event.clientX,
        y: event.clientY,
      };
      // Unified frame press (R1): a press on a committed view's frame
      // is EITHER a drag (any movement — works in every drawing mode)
      // OR, in the frame-pick modes (projected parent picking /
      // delete selection), a stationary release dispatches the pick.
      // In base/section/idle a stationary frame click does nothing —
      // dragging is the gesture.
      if (
        drawingViewDragArmedRef.current ||
        drawingFramePickArmedRef.current
      ) {
        const frame = hitTestViewFrame(event);
        if (frame) {
          const point = resolveSheetPoint(event);
          if (point) {
            if (drawingFramePickArmedRef.current) {
              // Click-vs-drag: record the press; the pointer-move
              // branch promotes it to a drag, the pointer-up branch
              // dispatches the pick on a stationary release.
              framePressRef.current = {
                viewId: frame.viewId,
                min: frame.min,
                max: frame.max,
                grabOffset: [
                  frame.min[0] - point[0],
                  frame.min[1] - point[1],
                ],
                pressPoint: point,
                clientX: event.clientX,
                clientY: event.clientY,
              };
              controls.enabled = false;
              renderer.domElement.setPointerCapture(event.pointerId);
            } else {
              beginViewFrameDrag(event, frame, point);
            }
            return;
          }
        }
      }
      // Whole-dimension drag: a press on a dimension/annotation/note
      // text starts a drag (active in every drawing mode except
      // dimension-pick-armed).  A committed text ALSO wins over an
      // armed Insert placement — with the Text tool armed, pressing an
      // existing note must drag it, not stack a duplicate note on it.
      // The frame press above wins on its own hit; this is the
      // fallback for everything else on the sheet.
      if (
        showDrawingSheetRef.current &&
        !drawingPickArmedRef.current
      ) {
        const dimText = hitTestDimensionText(event);
        if (dimText) {
          const point = resolveSheetPoint(event);
          if (point) {
            beginDimensionTextDrag(event, dimText, point);
            return;
          }
        }
      }
      // Detail View circle drag: a press INSIDE a projection view's
      // content bounds starts the circle (center = press point, the
      // radius follows the cursor).  The frame press above keeps its
      // drag behavior (view repositioning stays on in every mode);
      // committed texts keep their drag above; section and detail
      // views cannot parent a detail (the core validates too) so
      // their content area is inert here.
      if (
        showDrawingSheetRef.current &&
        drawingDetailDragArmedRef.current
      ) {
        const point = resolveSheetPoint(event);
        if (point) {
          const sheet0 = viewportRef.current?.drawing_sheets?.[0];
          if (sheet0) {
            const documentNow = documentRef.current;
            const drawingNow = documentNow?.drawing.drawings.find(
              (entry) =>
                entry.drawing_id === documentNow.drawing.active_drawing_id,
            );
            const projectionIds = new Set(
              (drawingNow?.views ?? [])
                .filter((view) => view.kind === "projection")
                .map((view) => view.view_id),
            );
            for (const view of sheet0.views) {
              if (
                projectionIds.has(view.view_id) &&
                point[0] > view.min[0] &&
                point[0] < view.max[0] &&
                point[1] > view.min[1] &&
                point[1] < view.max[1]
              ) {
                detailDragRef.current = { viewId: view.view_id };
                controls.enabled = false;
                renderer.domElement.setPointerCapture(event.pointerId);
                drawingDetailDragStartRef.current?.(view.view_id, point);
                return;
              }
            }
          }
        }
      }
      // Drag-paint trim (R5): pressing inside the trim tool starts a
      // stroke; the pointer-move path records every crossed entity
      // and the pointer-up commits them as one batch.
      if (activeSketchToolRef.current === "trim") {
        trimStrokeRef.current = new Map();
      }
      // Transform/Array center pick consumes the click: resolve the
      // pointer through the snap machinery (circle/arc centers,
      // endpoints, grid) and report the sketch-local point.
      if (arrayCenterPickingRef.current && activeSketchPlaneIdRef.current) {
        const rawPoint = resolveSketchPlanePoint(
          event,
          renderer,
          camera,
          activeSketchPlaneIdRef.current,
          activeSketchPlaneFrameRef.current,
        );
        if (rawPoint) {
          const snapped = resolveSnappedSketchPoint(
            { local: rawPoint.local, world: rawPoint.world },
            null,
            { dynamicSnapsEnabled: false },
          );
          if (snapped) {
            onArrayCenterPickedRef.current(snapped.local);
            setSketchSnapLabel(null);
            return;
          }
        }
        return;
      }
      handleViewportPointerDown({
        event,
        renderer,
        camera,
        controls,
        raycaster,
        pointer,
        setSelectedConstraint,
        setContextMenu,
        lastPointerEventRef,
        setPointerDown: (point) => {
          pointerDown = point;
        },
        dimensionLabelDragRef,
        activeSketchToolRef,
        circleToolMode: circleToolModeRef.current,
        activeSketchPlaneIdRef,
        activeSketchPlaneFrameRef,
        armedSketchConstraintRef,
        lineDraftStartRef,
        lastPointerDownTimeRef,
        lastPointerDownPosRef,
        chainBreakRequestedRef,
        isPointerInCubeArea,
        viewCubeDraggingRef,
        viewCubeDragStartRef,
        moveGizmoRef,
        moveGizmoObjectsRef,
        moveGizmoDragRef,
        sketchLinesRef,
        endpointDragRef,
        selectionDragRef,
        sketchMoveDragRef,
        sketchMoveRingGroupRef,
        pendingSketchMoveRef,
        persistentRingGroupRef,
        persistentRingPickablesRef,
        persistentRingStateRef,
        sketchGroupRef,
        sceneDataRef,
        restorePreviewScene,
        intersectSceneTargets,
        displayedSketchDimensionsRef,
        suppressNextDimensionEditorOpenRef,
        setIsDimensionEditorOpen,
        selectSketchDimension: selectSketchDimensionRef.current,
        setAngleDimensionDragRadius,
        getDimensionPlacementAxis,
        draftStartedOnPointerDownRef,
        draftDimensionSessionRef,
        resolveSnappedSketchPoint,
        // Inject the arc tool mode so the draft session knows which
        // geometry its radius field describes (circumradius for
        // three_point, center distance for center_start_end).
        createDraftDimensionSession: (tool, start, current) =>
          createDraftDimensionSession(tool, start, current, {
            toolMode: tool === "arc" ? arcToolModeRef.current : undefined,
          }),
        setDraftDimensionSession,
        focusDraftField,
      });
    }

    function handlePointerMove(event: PointerEvent) {

      // --- Frame press awaiting click-vs-drag resolution: movement
      // promotes it to a full drag (the frame-pick modes) ---
      const framePress = framePressRef.current;
      if (framePress) {
        const moved =
          Math.abs(event.clientX - framePress.clientX) +
            Math.abs(event.clientY - framePress.clientY) >=
          4;
        if (moved) {
          framePressRef.current = null;
          beginViewFrameDrag(
            event,
            { viewId: framePress.viewId, min: framePress.min, max: framePress.max },
            framePress.pressPoint,
          );
          // Feed the current position so the ghost starts where the
          // cursor already is (the promotion event itself would be
          // swallowed by the return below).
          const currentPoint = resolveSheetPoint(event);
          drawingViewDragMoveRef.current?.(currentPoint ?? [0, 0]);
        }
        return;  // pressed, not yet a drag — swallow the move either way
      }

      // --- Mouse-first view reposition: the dragged frame follows ---
      if (viewFrameDragRef.current) {
        const point = resolveSheetPoint(event);
        drawingViewDragMoveRef.current?.(point ?? [0, 0]);
        return;
      }

      // --- Mouse-first dimension reposition: the text ghost follows ---
      if (dimensionTextDragRef.current) {
        const point = resolveSheetPoint(event);
        drawingDimensionTextDragMoveRef.current?.(point ?? [0, 0]);
        return;
      }

      // --- Detail View circle drag: the ghost follows the cursor ---
      if (detailDragRef.current) {
        const point = resolveSheetPoint(event);
        drawingDetailDragMoveRef.current?.(point ?? [0, 0]);
        return;
      }

      // --- Mouse-first Insert View: the ghost follows the cursor ---
      if (drawingInsertArmedRef.current) {
        const point = resolveSheetPoint(event);
        drawingInsertMoveLatestRef.current = point;
        if (drawingInsertMoveFrameRef.current === null) {
          drawingInsertMoveFrameRef.current = window.requestAnimationFrame(
            () => {
              drawingInsertMoveFrameRef.current = null;
              const latest = drawingInsertMoveLatestRef.current;
              if (latest) {
                drawingInsertMoveRef.current?.(latest);
              }
            },
          );
        }
        return;
      }

      // --- Rectangle selection drag tracking ---
      if (selectionDragRef.current?.active) {
        selectionDragRef.current.currentX = event.clientX;
        selectionDragRef.current.currentY = event.clientY;
        setSelectionRect(
          selectionRectOverlayFromDrag(selectionDragRef.current),
        );
        return;
      }

      // --- Transform/Array center pick hover feedback ---
      if (arrayCenterPickingRef.current && activeSketchPlaneIdRef.current) {
        const rawPoint = resolveSketchPlanePoint(
          event,
          renderer,
          camera,
          activeSketchPlaneIdRef.current,
          activeSketchPlaneFrameRef.current,
        );
        if (rawPoint) {
          const snapped = resolveSnappedSketchPoint(
            { local: rawPoint.local, world: rawPoint.world },
            null,
            { dynamicSnapsEnabled: false },
          );
          setSketchSnapLabel(snapped?.snapLabel ?? null);
        }
        return;
      }

      lastPointerEventRef.current = event;
      // -- cube-area interaction ---------------------------------------
      const cubeDpr = renderer.getPixelRatio();
      const cubeCanvasRect = renderer.domElement.getBoundingClientRect();
      const inCube = isPointerInCubeArea(event, cubeCanvasRect, cubeDpr);

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
          updateAngleDimensionPlacementPreview,
          angleFrameForDimension: angleDimensionFrame,
          setDimensionLabelPosition,
        })
      ) {
        return;
      }

      // Linear placement live preview: update as cursor moves.
      if (linearPlacementRef.current && activeSketchPlaneIdRef.current) {
        // Refresh endpoint data in case the sketch state was stale at click time.
        const sketch = sketchLinesRef.current;
        const line = sketch?.lines.find((l) => l.line_id === linearPlacementRef.current!.lineId);
        if (line) {
          linearPlacementRef.current.startPointId = line.start_vertex_id;
          linearPlacementRef.current.endPointId = line.end_vertex_id;
          linearPlacementRef.current.startX = line.start_x;
          linearPlacementRef.current.startY = line.start_y;
          linearPlacementRef.current.endX = line.end_x;
          linearPlacementRef.current.endY = line.end_y;
        }

        const resolved = resolveSketchPlanePoint(
          event,
          renderer,
          camera,
          activeSketchPlaneIdRef.current,
          activeSketchPlaneFrameRef.current,
        );
        if (resolved && sketchGroupRef.current && line) {
          // Check for relation preview candidates first (e.g. angle
          // between two lines sharing an endpoint).  When the cursor
          // is near a second entity that forms a valid relation with
          // the staged first entity, the relation ghost takes priority
          // over the linear placement preview.
          const relation = updateDimensionRelationPreview(resolved.local);
          if (relation) {
            // Clean up linear placement preview so it doesn't overlap
            // the relation ghost.
            cancelLinearPlacementPreview(sketchGroupRef.current, linearPlacementPreviewRef);
          } else {
            updateLinearPlacementPreview(
              linearPlacementRef.current,
              resolved.local,
              activeSketchPlaneIdRef.current,
              activeSketchPlaneFrameRef.current,
              config.displayUnits,
              sketchGroupRef.current,
              linearPlacementPreviewRef,
            );
          }
        }
        // Always consume the event — user is in placement mode.
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
          sketchConstraintsRef,
          sceneConstraintsRef,
          pendingDragRef,
          pendingDragFrameRef,
          dragSnapResultRef,
          dragCursorRef,
          dragPreviewMutatingRef,
          sketchEntityObjectByIdRef,
          sketchPointObjectByIdRef,
          sketchConstraintObjectsRef,
          sketchProfileObjectsRef,
          resolveSnappedSketchPoint,
          setSketchSnapLabel,
          requestRender,
        })
      ) {
        return;
      }

      if (
        handleSketchMovePointerMove({
          event,
          renderer,
          camera,
          sketchMoveDragRef,
          pendingSketchMoveRef,
          activeSketchPlaneIdRef,
          activeSketchPlaneFrameRef,
          sketchLinesRef,
          sketchConstraintsRef,
          sceneConstraintsRef,
          pendingMoveDragRef,
          pendingMoveDragFrameRef,
          moveFrameResultRef,
          moveDragPreviewActiveRef,
          sketchEntityObjectByIdRef,
          sketchPointObjectByIdRef,
          sketchConstraintObjectsRef,
          sketchProfileObjectsRef,
          resolveSnappedSketchPoint,
          setSketchSnapLabel,
          reportMoveValues: setMovePanelValues,
          requestRender,
        })
      ) {
        return;
      }

      if (activeSketchPlaneIdRef.current) {
        if (isDrawableSketchTool(activeSketchToolRef.current)) {
          if (!objectSnapLatchRef.current) {
            updateCrosshairPointer(event, inCube);
          }
          requestDraftPointerMoveFrame(event);
          return;
        }
        updateCrosshairPointer(event, inCube);
        if (runActiveSketchPointerMove(event)) {
          return;
        }
      }

      if (
        originPickPointEnabledRef.current ||
        wcsPickPointEnabledRef.current ||
        drillPickPointEnabledRef.current
      ) {
        // Live snap preview while the origin, WCS, or drilling pick
        // is armed: label the snap kind whenever the cursor is within
        // the snap threshold, so the user sees what the next click
        // will snap to.  Also track the pointer position here — the
        // sketch-mode crosshair update never runs in the CAM
        // workspace, so without this the SnapCursorOverlay has no
        // position to render the square + label chip at.
        setPointerNdcFromEvent(pointer, event, renderer);
        const rect = renderer.domElement.getBoundingClientRect();
        setCrosshairPointer({
          x: event.clientX - rect.left,
          y: event.clientY - rect.top,
        });
        const rawCandidates = buildCamOriginSnapCandidates({
          document: documentRef.current,
          activeCamSetupId: activeCamSetupIdRef.current,
          viewport: viewportRef.current,
          showStock: showStockRef.current,
          sketchPointObjects: sketchPointObjectsRef.current,
          sketchPrimitives: sceneDataRef.current,
          vertexObjects: vertexObjectsRef.current,
          edgeLineObjects: edgeLineObjectsRef.current,
          faceMeshes: faceMeshesRef.current,
        });
        // The drill pick snaps to hole rims/walls only (body geometry,
        // lifted to the material top and read from the RAW payload —
        // a hidden sketch must never hide the holes); the origin/WCS
        // picks keep every target.
        const candidates = drillPickPointEnabledRef.current
          ? liftDrillCandidates(
              drillHoleCandidates(viewportRef.current),
              faceMeshesRef.current,
            )
          : rawCandidates;
        const snapped = resolveCamOriginSnap({
          candidates,
          camera,
          pointer,
          rect,
        });
        camOriginSnapLabelActiveRef.current = snapped !== null;
        setSketchSnapLabel(
          snapped ? translate(camOriginSnapLabelKey(snapped.kind)) : null,
        );
        // Suppress scene hover while the pick is armed: a surface or
        // profile highlight under the cursor would look like it is
        // "consuming" the pointer when the pick actually snaps to the
        // marker targets.  The visible markers + snap chip are the
        // pick feedback instead.
        return;
      }

      const hit = intersectSceneTargets(event);
      applySceneHover(hit, hoverActions());
      // Grab cursor over a draggable dimension text (the drawing
      // workspace has no other hover targets — a miss resets it).
      if (showDrawingSheetRef.current) {
        const overDimensionText =
          !drawingInsertArmedRef.current &&
          !drawingPickArmedRef.current &&
          hitTestDimensionText(event) !== null;
        setCanvasCursor(overDimensionText ? "grab" : "");
      }
    }

    function handlePointerLeave() {
      cancelPendingDraftPointerMoveFrame();
      objectSnapLatchRef.current = null;
      pointerDown = null;
      if (moveGizmoDragRef.current) {
        moveGizmoDragRef.current = null;
        controlsRef.current!.enabled = true;
      }
      // Defensive: if a live preview was mutating scene objects and the
      // pointer-up got lost, restore committed geometry.
      if (
        endpointDragRef.current ||
        dragPreviewMutatingRef.current ||
        moveDragPreviewActiveRef.current
      ) {
        endpointDragRef.current = null;
        restorePreviewScene();
      }
      if (!dimensionLabelDragRef.current?.isPlacement) {
        dimensionLabelDragRef.current = null;
        controlsRef.current!.enabled = true;
      }
      (renderer.domElement as HTMLCanvasElement).style.cursor = "";
      setSketchSnapLabel(null);
      setConstraintPreview(null);
      setCrosshairPointer(null);
      setHoveredSketchProfile(null);
      setHoveredSketchPoint(null);
      setHoveredSketchEntity(null);
      // Unconditional: hover repaints on the next pointer move, and a
      // project-tool body hover that outlives a sketch session would
      // otherwise stick as a stale highlight.
      setHoveredReference(null);
      setHoveredPrimitive(null);
      setHoveredFace(null);
      setHoveredEdge(null);
      setHoveredVertex(null);
      if (camOriginSnapLabelActiveRef.current) {
        camOriginSnapLabelActiveRef.current = false;
        setSketchSnapLabel(null);
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
        // Defer placement commit until after entity handling so
        // two-pick dimension workflows (angle, distance) can regroup
        // into a two-entity dimension before the old single-entity
        // placement is committed.  The regroup calls
        // clearPendingDimensionPlacement which cleans up the drag ref.
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
        // Clear the UI cache so future frames use core-computed
        // positions (stale cache would override after geometry changes).
        setDimensionLabelPositions((current) => {
          const next = { ...current };
          delete next[dimensionDrag.dimensionId];
          return next;
        });
      }

      dimensionLabelDragRef.current = null;
      controlsRef.current!.enabled = true;
      // Clear the dimension tool's staged first-pick so the next
      // entity click starts a fresh dimension instead of leaking
      // a stale two-pick workflow (e.g. "Angle dimension already
      // exists" when the user only wants a line length dimension).
      clearDimensionToolFirstPick();
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
        setConstraintPreview,
        setSketchSnapLabel,
        setHoveredSketchEntity,
        setHoveredSketchPoint,
        setPointerDown: (point) => {
          pointerDown = point;
        },
        restorePreviewScene,
      });
    }

    function finishSketchMovePointerUpFromViewport(event: PointerEvent) {
      return finishSketchMovePointerUp({
        event,
        renderer,
        controls,
        sketchMoveDragRef,
        sketchMoveRingGroupRef,
        pendingSketchMoveRef,
        setSketchSnapLabel,
        setHoveredSketchEntity,
        setHoveredSketchPoint,
        setPointerDown: (point) => {
          pointerDown = point;
        },
        restorePreviewScene,
        reportMoveValues: setMovePanelValues,
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
      cancelPendingDraftPointerMoveFrame();
      objectSnapLatchRef.current = null;

      // Linear placement commit: create the dimension with the chosen axis.
      // But first, check if the user clicked on a second entity that forms
      // a valid relation (e.g. angle between two lines sharing an endpoint).
      // If a relation preview is active, commit it instead of the linear
      // placement dimension.
      if (linearPlacementRef.current) {
        if (commitDimensionRelationPreview()) {
          // Relation committed — clean up linear placement state.
          cancelLinearPlacementPreview(sketchGroupRef.current!, linearPlacementPreviewRef);
          linearPlacementRef.current = null;
          return;
        }
        const state = linearPlacementRef.current;
        linearPlacementRef.current = null;
        cancelLinearPlacementPreview(sketchGroupRef.current!, linearPlacementPreviewRef);
        controlsRef.current!.enabled = true;
        (renderer.domElement as HTMLCanvasElement).style.cursor = "";
        const commit = resolveLinearPlacementCommit(state);

        if (commit.kind === "line_length") {
          const dimId = `dim-line-${commit.lineId}`;
          void addSketchLineLengthDimensionRef.current(commit.lineId).then(() => {
            updateSketchDimensionLabelPositionRef.current(
              dimId,
              commit.labelX,
              commit.labelY,
            );
            // Adding a new dimension triggers a solver pass that may
            // move any geometry — clear the entire UI cache.
            setDimensionLabelPositions({});
          });
        } else {
          const dimId = `dim-point-distance-${commit.pointAId}-${commit.pointBId}-${commit.axis}`;
          void addSketchVertexDistanceDimensionRef.current(
            commit.pointAId,
            commit.pointBId,
            commit.axis,
          ).then(() => {
            updateSketchDimensionLabelPositionRef.current(
              dimId,
              commit.labelX,
              commit.labelY,
            );
            // Adding a new dimension triggers a solver pass that may
            // move any geometry — clear the entire UI cache.
            setDimensionLabelPositions({});
          });
        }
        pendingDimensionPlacementRef.current = false;
        pendingDimSourceEntityIdRef.current = null;
        pendingDimensionIdRef.current = null;
        dimensionToolFirstLineRef.current = null;
        setDimensionToolFirstLine(null);
        return;
      }

      // Mouse-first view reposition: the drop fires HERE, before the
      // generic pointer-up routing — the frame-drag press branch
      // (above, in handleViewportPointerDown) returns before
      // setPointerDown runs, so finishClickPointerUp's null-pointerDown
      // bail would swallow the drop; and its 4 px pan guard would
      // swallow it anyway (a real drag moves further).  The commit
      // itself still refuses no-op moves (< 0.5 mm, App-side).
      if (viewFrameDragRef.current !== null) {
        const point = resolveSheetPoint(event);
        viewFrameDragRef.current = null;
        controls.enabled = true;
        if (renderer.domElement.hasPointerCapture(event.pointerId)) {
          renderer.domElement.releasePointerCapture(event.pointerId);
        }
        if (point) {
          drawingViewDropRef.current?.(point);
        }
        return;
      }

      // Mouse-first dimension reposition: the drop commits the
      // placement (the App-side action refuses no-op moves < 0.5 mm).
      if (dimensionTextDragRef.current !== null) {
        const point = resolveSheetPoint(event);
        dimensionTextDragRef.current = null;
        controls.enabled = true;
        if (renderer.domElement.hasPointerCapture(event.pointerId)) {
          renderer.domElement.releasePointerCapture(event.pointerId);
        }
        if (point) {
          drawingDimensionTextDropRef.current?.(point);
        }
        return;
      }

      // Detail View circle drag: the drop finishes the gesture — the
      // app-side action decides whether the radius clears the commit
      // threshold (≥ 2 mm); the tool stays armed either way.
      if (detailDragRef.current !== null) {
        const point = resolveSheetPoint(event);
        detailDragRef.current = null;
        controls.enabled = true;
        if (renderer.domElement.hasPointerCapture(event.pointerId)) {
          renderer.domElement.releasePointerCapture(event.pointerId);
        }
        if (point) {
          drawingDetailDragFinishRef.current?.(point);
        }
        return;
      }

      // A frame press that never became a drag: a stationary click —
      // the frame-pick modes (projected parent / delete selection)
      // act on it; other modes ignore it.
      if (framePressRef.current !== null) {
        const press = framePressRef.current;
        framePressRef.current = null;
        controls.enabled = true;
        if (renderer.domElement.hasPointerCapture(event.pointerId)) {
          renderer.domElement.releasePointerCapture(event.pointerId);
        }
        drawingFramePickRef.current?.(press.viewId, {
          min: press.min,
          max: press.max,
        });
        return;
      }

      handleViewportPointerUp({
        event,
        renderer,
        camera,
        controls,
        pointerDownRevisionRef,
        // Armed origin pick: every pointer-up places the stock origin
        // at the clicked point — snapped to nearby geometry (sketch
        // points, body vertices/edge midpoints/face centers, stock-box
        // top corners/midpoints) or on the bed plane (z = 0) as the
        // fallback for sketch-only jobs.
        originPickPointEnabled: originPickPointEnabledRef.current,
        originPickPoint: (pickEvent) => {
          setPointerNdcFromEvent(pointer, pickEvent, renderer);
          raycaster.setFromCamera(pointer, camera);
          // Snap to the nearest geometry candidate within 12 px
          // (screen space) — the user clicks near a corner and gets
          // the exact 3D point, LightBurn-style.
          const rect = renderer.domElement.getBoundingClientRect();
          const candidates = buildCamOriginSnapCandidates({
            document: documentRef.current,
            activeCamSetupId: activeCamSetupIdRef.current,
            viewport: viewportRef.current,
            showStock: showStockRef.current,
            sketchPointObjects: sketchPointObjectsRef.current,
            sketchPrimitives: sceneDataRef.current,
            vertexObjects: vertexObjectsRef.current,
            edgeLineObjects: edgeLineObjectsRef.current,
            faceMeshes: faceMeshesRef.current,
          });
          const snapped = resolveCamOriginSnap({
            candidates,
            camera,
            pointer,
            rect,
          });
          // Microns precision — long decimals in the origin fields are
          // noise, not accuracy.
          if (snapped) {
            // Snapped geometry keeps its full 3D point (an elevated
            // sketch point or a body vertex above the bed keeps z).
            const { x, y, z } = snapped.position;
            originPickPointRef.current({
              x: Math.round(x * 1000) / 1000,
              y: Math.round(y * 1000) / 1000,
              z: Math.round(z * 1000) / 1000,
            });
            return;
          }
          // No snap target: fall back to the bed plane (z = 0).
          const hit = new THREE.Vector3();
          if (
            !raycaster.ray.intersectPlane(
              new THREE.Plane(new THREE.Vector3(0, 0, 1), 0),
              hit,
            )
          ) {
            originPickPointRef.current(null);
            return;
          }
          // A grazing camera angle intersects the bed plane far away —
          // reject absurd coordinates instead of placing a garbage
          // origin (a real machine bed is well under 10 m).
          if (Math.abs(hit.x) > 10000 || Math.abs(hit.y) > 10000) {
            originPickPointRef.current(null);
            return;
          }
          originPickPointRef.current({
            x: Math.round(hit.x * 1000) / 1000,
            y: Math.round(hit.y * 1000) / 1000,
            z: 0,
          });
        },
        // Armed WCS pick: same pointer-up routing as the origin pick,
        // but face hits take priority — a body face keeps the TNP face
        // anchor, a stock face anchors to the stock face (or a snapped
        // stock corner/midpoint as a pinned point) — and the result is
        // a point anchor instead of a stock origin.
        wcsPickPointEnabled: wcsPickPointEnabledRef.current,
        wcsPickPoint: (pickEvent) => {
          setPointerNdcFromEvent(pointer, pickEvent, renderer);
          raycaster.setFromCamera(pointer, camera);
          const rect = renderer.domElement.getBoundingClientRect();

          // 1) Face raycast FIRST.  A body face keeps the TNP face
          // anchor (selectFace → placeWcsFromFacePick).  Snap-first
          // would degrade body-face picks into bare points — the snap
          // set includes body face centers.  The stock box is the
          // outermost surface when shown, so it intercepts first and
          // routes to the stock branch below; hiding the stock
          // restores body-face picks.
          let stockFaceName: string | null = null;
          let stockIntercepted = false;
          const hits = raycaster
            .intersectObjects(
              [
                ...faceMeshesRef.current,
                ...(showStockRef.current ? stockFaceMeshesRef.current : []),
              ],
              false,
            )
            .sort((a, b) => a.distance - b.distance);
          for (const hit of hits) {
            if (hit.object.userData.isStockBox === true) {
              stockIntercepted = true;
              const names = hit.object.userData.stockFaceNames as
                | Array<string | null>
                | undefined;
              stockFaceName =
                names?.[hit.face?.materialIndex ?? -1] ?? null;
              break;
            }
            if (typeof hit.object.userData.faceId === "string") {
              void selectFaceRef.current(hit.object.userData.faceId);
              return;
            }
          }

          // 2) Snap within 12 px.  When the stock face intercepted the
          // ray, only stock candidates are considered — they are the
          // outermost surface and must stay reachable; otherwise the
          // full set (body vertices/edges/faces, sketch points).
          const allCandidates = buildCamOriginSnapCandidates({
            document: documentRef.current,
            activeCamSetupId: activeCamSetupIdRef.current,
            viewport: viewportRef.current,
            showStock: showStockRef.current,
            sketchPointObjects: sketchPointObjectsRef.current,
            sketchPrimitives: sceneDataRef.current,
            vertexObjects: vertexObjectsRef.current,
            edgeLineObjects: edgeLineObjectsRef.current,
            faceMeshes: faceMeshesRef.current,
          });
          const candidates = stockIntercepted
            ? allCandidates.filter(
                (candidate) =>
                  candidate.kind === "stock_corner" ||
                  candidate.kind === "stock_midpoint",
              )
            : allCandidates;
          const snapped = resolveCamOriginSnap({
            candidates,
            camera,
            pointer,
            rect,
          });
          if (snapped) {
            const { x, y, z } = snapped.position;
            wcsPickPointRef.current({
              x: Math.round(x * 1000) / 1000,
              y: Math.round(y * 1000) / 1000,
              z: Math.round(z * 1000) / 1000,
            });
            return;
          }

          // 3) No snap + a named stock face → stock_face anchor (the
          // core resolves it against the live stock extents).
          if (stockFaceName) {
            void selectFaceRef.current(`stock:${stockFaceName}`);
            return;
          }

          // 4) Bed-plane fallback (z = 0, same 10 m guard as the
          // origin pick) — also covers stock hits on the cylinder side
          // (untagged: not a real cylinder face).
          const hit = new THREE.Vector3();
          if (
            !raycaster.ray.intersectPlane(
              new THREE.Plane(new THREE.Vector3(0, 0, 1), 0),
              hit,
            )
          ) {
            wcsPickPointRef.current(null);
            return;
          }
          if (Math.abs(hit.x) > 10000 || Math.abs(hit.y) > 10000) {
            wcsPickPointRef.current(null);
            return;
          }
          wcsPickPointRef.current({
            x: Math.round(hit.x * 1000) / 1000,
            y: Math.round(hit.y * 1000) / 1000,
            z: 0,
          });
        },
        // Armed drilling pick: the click reports a BODY reference when
        // it lands on a hole (snap dot over the lifted candidates →
        // rim edge / wall face; a rim edge line; a cylindrical wall
        // face) — the caller captures a re-resolvable attestation.
        // Every other surface reports the clicked world point (free
        // pick), and a missed hit falls back to the bed plane (z = 0).
        drillPickPointEnabled: drillPickPointEnabledRef.current,
        drillPickPoint: (pickEvent) => {
          setPointerNdcFromEvent(pointer, pickEvent, renderer);
          raycaster.setFromCamera(pointer, camera);
          const rect = renderer.domElement.getBoundingClientRect();
          const round = (value: number) => Math.round(value * 1000) / 1000;

          // 1) Snap within 12 px over the lifted hole candidates — the
          // dot IS the hole.  A rim reports its edge, a wall its face,
          // both lifted to the material top (a blind hole's wall axis
          // sits at the hole bottom) like the live markers.
          const candidates = liftDrillCandidates(
            drillHoleCandidates(viewportRef.current),
            faceMeshesRef.current,
          );
          const snapped = resolveCamOriginSnap({
            candidates,
            camera,
            pointer,
            rect,
          });
          if (snapped) {
            if (snapped.kind === "hole_rim" && snapped.sourceId) {
              drillPickPointRef.current({
                mode: "edge",
                id: snapped.sourceId,
              });
            } else if (snapped.kind === "hole_wall" && snapped.sourceId) {
              drillPickPointRef.current({
                mode: "face",
                id: snapped.sourceId,
              });
            } else {
              const { x, y, z } = snapped.position;
              drillPickPointRef.current({
                mode: "point",
                point: { x: round(x), y: round(y), z: round(z) },
              });
            }
            return;
          }

          // 2) Edge-line raycast: a click directly on a circular rim
          // edge reports the edge for attestation capture (the same
          // small line threshold as the selection pick).
          const previousLineThreshold = raycaster.params.Line?.threshold ?? 1;
          if (raycaster.params.Line) {
            raycaster.params.Line.threshold = 1.2;
          }
          const [edgeHit] = raycaster.intersectObjects(
            edgeLineObjectsRef.current,
            false,
          );
          if (raycaster.params.Line) {
            raycaster.params.Line.threshold = previousLineThreshold;
          }
          const edgeId = edgeHit?.object.userData.edgeId;
          if (typeof edgeId === "string") {
            drillPickPointRef.current({ mode: "edge", id: edgeId });
            return;
          }

          // 3) Face raycast: a cylindrical wall face reports the FACE
          // for attestation capture; every other surface (planar,
          // stock, spline, and primitive body meshes — STL/cylinder
          // bodies have no face primitives) reports the clicked point.
          const hits = raycaster
            .intersectObjects(
              [
                ...faceMeshesRef.current,
                ...(showStockRef.current ? stockFaceMeshesRef.current : []),
                ...meshesRef.current,
              ],
              false,
            )
            .sort((a, b) => a.distance - b.distance);
          const faceHit = hits[0];
          if (faceHit) {
            const surfaceKind = faceHit.object.userData.surfaceKind as
              | string
              | undefined;
            const faceId = faceHit.object.userData.faceId as
              | string
              | undefined;
            if (surfaceKind === "cylinder" && typeof faceId === "string") {
              drillPickPointRef.current({ mode: "face", id: faceId });
            } else {
              drillPickPointRef.current({
                mode: "point",
                point: {
                  x: round(faceHit.point.x),
                  y: round(faceHit.point.y),
                  z: round(faceHit.point.z),
                },
              });
            }
            return;
          }

          // 4) Bed-plane fallback (z = 0, same 10 m guard as the
          // origin pick).
          const hit = new THREE.Vector3();
          if (
            !raycaster.ray.intersectPlane(
              new THREE.Plane(new THREE.Vector3(0, 0, 1), 0),
              hit,
            )
          ) {
            drillPickPointRef.current(null);
            return;
          }
          if (Math.abs(hit.x) > 10000 || Math.abs(hit.y) > 10000) {
            drillPickPointRef.current(null);
            return;
          }
          drillPickPointRef.current({
            mode: "point",
            point: { x: round(hit.x), y: round(hit.y), z: 0 },
          });
        },
        // Armed drawing-sheet pick (P6 dimension tool): the click
        // resolves to a sheet-mm point on the sheet plane (z = 0) —
        // the core maps it to the nearest projected edge.
        drawingPickArmed: drawingPickArmedRef.current,
        drawingPick: (pickEvent) => {
          const point = resolveSheetPoint(pickEvent);
          if (point) {
            drawingPickRef.current?.(point);
          }
        },
        // Mouse-first Insert View: a click on the sheet commits the
        // view at the clicked point — guarded against pan/zoom drags
        // (a click moves < 6 px between down and up).
        drawingInsertArmed: drawingInsertArmedRef.current,
        drawingInsertCommit: (pickEvent) => {
          const down = drawingPointerDownClientRef.current;
          const moved =
            down !== null &&
            Math.abs(pickEvent.clientX - down.x) +
              Math.abs(pickEvent.clientY - down.y) >=
              6;
          if (moved) {
            return;  // a pan/zoom drag, not a placement click
          }
          const point = resolveSheetPoint(pickEvent);
          if (point) {
            drawingInsertCommitRef.current?.(point);
          }
        },
        // Mouse-first view reposition: the drop now fires in an early
        // branch before handleViewportPointerUp (see above) — the
        // gesture must not double-handle here.
        activeSketchPlaneId,
        activeSketchPlaneFrame,
        pointerDown,
        setPointerDown: (point) => {
          pointerDown = point;
        },
        lastPointerEventRef,
        selectionDragRef,
        setSelectionRect,
        performRectangleSelect,
        moveGizmoDragRef,
        finishDimensionLabelDragPointerUp,
        finishEndpointDragPointerUp: finishEndpointDragPointerUpFromViewport,
        finishSketchMovePointerUp: finishSketchMovePointerUpFromViewport,
        finishViewCubePointerUp: finishViewCubePointerUpFromViewport,
        draftStartedOnPointerDownRef,
        draftDimensionSessionRef,
        draftDimensionInputRefs,
        intersectSceneTargets,
        activeSketchToolRef,
        activeSketchPlaneFrameRef,
        sketchLinesRef,
        armedSketchConstraintRef,
        mirrorFocusedSlotRef,
        inactiveSketchEntityPickEnabledRef,
        sketchEntityObjectByIdRef,
        sketchPointObjectsRef,
        resolveSnappedSketchPoint,
        setSketchSnapLabel,
        selectSketchProfile: selectSketchProfileRef.current,
        selectVertex: selectVertexRef.current,
        selectEdge: selectEdgeRef.current,
        selectFace: selectFaceRef.current,
        trimSketchEntity: (entityId, localX, localY) => {
          // Deterministic clear BEFORE the core call: a failed/no-op
          // trim emits no geometry change, so the red hover overlay
          // would otherwise survive the click.
          clearTrimHighlights(clearTrimSegmentHighlight, clearTrimArcHighlight);
          // Pass the hovered segment index from the core's preview so
          // the trim deletes EXACTLY the highlighted segment instead of
          // re-deriving it from a slightly different click point (the
          // source of "highlight shows one piece, trim deletes another").
          // Only trust the preview when it describes the SAME document
          // revision the user is looking at — a stale preview's index
          // is meaningless against the new segment list, and the core
          // would cut the wrong piece (the "trim floods" regression).
          const preview = trimPreviewResultRef.current;
          const revision = document?.revision ?? 0;
          const previewFresh =
            preview !== null &&
            preview.entity_id === entityId &&
            typeof preview.hovered_index === "number" &&
            preview.hovered_index >= 0 &&
            preview.revision === revision;
          const segmentIndex = previewFresh ? preview.hovered_index : undefined;
          return trimSketchEntityRef.current(
            entityId,
            localX,
            localY,
            segmentIndex,
            previewFresh ? revision : undefined,
            preview?.id,
          );
        },
        mirrorEntityPick: mirrorEntityPickRef.current,
        selectSketchEntity: selectSketchEntityRef.current,
        pickSketchPoint: pickSketchPointRef.current,
        handleDimensionClick,
        setSelectedConstraint,
        paintSketchEntityMaterials,
        paintSketchPointMaterials,
        addMessage,
        addSketchFillet: addSketchFilletRef.current,
        addSketchChamfer: addSketchChamferRef.current,
        circleTangentLineIdsRef,
        addSketchTextAt: addSketchTextRef.current,
        onPickSketchText: pickSketchTextRef.current,
        onPickSketchSlot: pickSketchSlotRef.current,
        onPickSketchChamfer: pickSketchChamferRef.current,
        extendSketchEntity: extendSketchEntityRef.current,
        cornerFirstEntityIdRef,
        cornerTrimSketchEntities: (entityAId, entityBId, clickX, clickY) => {
          clearCornerPreview();
          return cornerTrimSketchEntitiesRef.current(
            entityAId,
            entityBId,
            clickX,
            clickY,
          );
        },
        clearCornerPreview,
        splitFirstPickRef,
        splitSketchEntity: splitSketchEntityRef.current,
        trimStrokeRef,
        trimSketchStroke: (entries) => {
          // Deterministic clear BEFORE the core call (same contract as
          // the single trim click).
          clearTrimHighlights(
            clearTrimSegmentHighlight,
            clearTrimArcHighlight,
          );
          return trimSketchStrokeRef.current(entries);
        },
        offsetSketchEntity: offsetSketchEntityRef.current,
        sketchTextPathPicking: sketchTextPathPickingRef.current,
        pickSketchTextPath: pickSketchTextPathRef.current,
        pendingDimensionPlacement: pendingDimensionPlacementRef.current,
        pendingDimensionSourceId: pendingDimSourceEntityIdRef.current,
        pendingDimensionId: pendingDimensionIdRef.current,
        getDimensionFirstEntityId: () => dimensionToolFirstLineRef.current,
        getDimensionFirstPoint: () => dimensionToolFirstPointRef.current,
        clearDimensionFirstPick: clearDimensionToolFirstPick,
        clearDimensionFirstEntity: clearDimensionToolFirstEntity,
        clearPendingDimensionPlacement,
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
	        createDimensionVertexDistance: dimCreatePointDistance,
	        createDimensionLine: dimCreateLine,
		createDimensionLineAngle: dimCreateLineAngle,
        createDimensionLinear: startLinearPlacement,
	createDimensionCircle: dimCreateCircle,
	selectDimensionCircle: dimSelectCircle,
	createDimensionArc: dimCreateArc,
	createDimensionArcLength: dimCreateArcLength,
	selectDimensionArc: dimSelectArc,
	createDimensionPolygon: dimCreatePolygon,
	selectDimensionPolygon: dimSelectPolygon,
	selectDimensionLine: dimSelectLine,
	        sketchCircleCount: sketchFeature?.sketch_parameters?.circles.length ?? 0,
	        lineDraftStartRef,
	        arcSecondPointRef,
	        rectSecondPointRef,
	        circleSecondPointRef,
	        ellipseSecondPointRef,
	        splineDraftPolesRef,
	        clearPreviewSpline,
	        updatePreviewSpline: renderSplineDraftPreview,
	        chainBreakRequestedRef,
	        previousLineAngleRef,
	        draftStartMidpointHostRef,
	        draftStartEndpointHostRef,
	        draftStartLineBodyHostRef,
	        draftDimensionInputRefsForCommit: draftDimensionInputRefs,
	        arcToolMode: arcToolModeRef.current,
	        rectangleToolMode: rectangleToolModeRef.current,
	        circleToolMode: circleToolModeRef.current,
	        polygonToolMode: polygonToolModeRef.current,
	        dimensionToolMode: dimensionToolModeRef.current,
	        polygonSides: polygonSidesRef.current,
	        isConstruction: sketchToolConstructionRef.current,
        clearPreviews: () => {
          clearPreviewLine();
          clearPreviewCircle();
          clearPreviewArc();
          clearPreviewSlot();
          clearPreviewDimension();
          clearPreviewInference();
        },
	        clearDraftDimensionSession,
	        suppressDimensionEditorAfterSketchCommit,
	        scheduleDimensionDeletion: scheduleDimensionDeletionInGroup,
	        scheduleDraftDimensionExpressionUpdate,
	        setPendingCircleDimensionPlacement: (placement) => {
	          pendingCircleDimensionPlacementRef.current = placement;
	        },
	        captureLineCommitRelations: capturePendingLineCommitRelations,
	        createLineDraftDimensionSession: (start, current) =>
	          createDraftDimensionSession("line", start, current),
	        clearDraftDimGroup,
	        // Ref-syncing wrapper: commit stages update the session (e.g.
	        // the arc-start/axis point landing), and the render loop
	        // reads the ref — the raw state setter would leave it stale
	        // until the next pointer move.
	        setDraftDimensionSession: (session) => {
	          draftDimensionSessionRef.current = session;
	          setDraftDimensionSession(session);
	        },
	        focusDraftField,
	        addSketchArc: addSketchArcRef.current,
	        addSketchRectangle: addSketchRectangleRef.current,
	        addSketchCircle: addSketchCircleRef.current,
        addSketchCircleMode: addSketchCircleModeRef.current,
	        addSketchPolygon: addSketchPolygonRef.current,
	        addSketchLine: addSketchLineRef.current,
	        addSketchEllipse: addSketchEllipseRef.current,
	        addSketchSlot: addSketchSlotRef.current,
	        addSketchSpline: addSketchSplineRef.current,
	        sceneDataRef,
	        pickInactiveSketchLine: pickInactiveSketchLineRef.current,
        selectReference: selectReferenceRef.current,
        selectPrimitive: selectPrimitiveRef.current,
        setIsDimensionEditorOpen,
      });
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

      // NOTE: right-click does NOT change the selection — opening a
      // menu is not a selection action. Replacing here broke the
      // marquee flow: right-clicking the surface BETWEEN marquee'd
      // circles flipped the selection to the profile, and the next
      // Delete killed the perimeter. The menu's Delete acts on the
      // live selection; with nothing selected it first selects the
      // clicked item (see viewportContextMenuActions).
      setContextMenu(result.contextMenu);
      if (result.selectedConstraint !== undefined) {
        setSelectedConstraint(result.selectedConstraint);
      }
    }

    const resizeObserver = new ResizeObserver(() => {
      resizeRenderer();
      requestRender();
    });

    resizeObserver.observe(host);
    function handleDoubleClick(event: MouseEvent) {
      // A double-click finishes the control-point spline draft.
      if (activeSketchPlaneId &&
          activeSketchToolRef.current === "spline" &&
          splineDraftPolesRef.current.length >= 2) {
        commitSplineDraft();
        return;
      }
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

    const onTrimPreview = (e: Event) => {
      const detail = (e as CustomEvent).detail as NonNullable<
        TrimPreviewResultEvent["payload"]
      > & { id?: string };
      // Every response — stale or not — releases the one-preview
      // in-flight slot so a pending newer request can go out (the
      // at-most-one-in-flight cap keeps the core queue from flooding
      // on dense sketches).
      if (detail.id) {
        notifyTrimPreviewResponse(detail.id);
      }
      // Drop responses that are not the newest request — hover
      // previews are coalesced per frame but the core answers them
      // asynchronously, so an older response can still arrive late.
      const lastSent = trimPreviewLastSentRef.current;
      if (lastSent?.requestId && detail.id !== lastSent.requestId) {
        return;
      }
      trimPreviewResultRef.current = detail;
      // Render the highlight immediately from the core's data.
      renderTrimPreviewHighlight({
        data: trimPreviewResultRef.current,
        sceneData: sceneDataRef.current,
        actions: {
          clearTrimSegmentHighlight,
          clearTrimArcHighlight,
          updateTrimSegmentHighlight,
          updateTrimArcHighlight,
        },
      });
      requestRender();
    };
    window.addEventListener("polysmith-trim-preview", onTrimPreview);

    const onCornerTrimPreview = (e: Event) => {
      const detail = (e as CustomEvent).detail as NonNullable<
        CornerTrimPreviewResultEvent["payload"]
      > & { id?: string };
      // Every response releases the one-in-flight slot (same queue
      // flood protection as the trim preview).
      if (detail.id) {
        notifyCornerTrimPreviewResponse(detail.id);
      }
      // Same coalescing contract as the trim preview: drop responses
      // that are not the newest request by id.
      const lastSent = cornerPreviewLastSentRef.current;
      if (lastSent?.requestId && detail.id !== lastSent.requestId) {
        return;
      }
      cornerTrimPreviewRef.current = detail;
      renderCornerTrimPreview({
        data: cornerTrimPreviewRef.current,
        actions: {
          clearCornerPreview,
          updateCornerPreview,
        },
      });
      requestRender();
    };
    window.addEventListener(
      "polysmith-corner-trim-preview",
      onCornerTrimPreview,
    );

    resizeRenderer();
    requestRender();

    const onPointerDown = (event: PointerEvent) => {
      handlePointerDown(event);
      requestRender();
    };
    const onPointerMove = (event: PointerEvent) => {
      handlePointerMove(event);
      requestRender(viewCubeDraggingRef.current ? 100 : 0);
    };
    const onPointerLeave = () => {
      handlePointerLeave();
      requestRender();
    };
    const onPointerUp = (event: PointerEvent) => {
      handlePointerUp(event);
      requestRender(viewCubeAnimatingRef.current ? 320 : 0);
    };
    const onContextMenu = (event: MouseEvent) => {
      if (draftDimensionSessionRef.current) {
        event.preventDefault();
        // Cancel the rubber band / chain break — keep tool armed
        lineDraftStartRef.current = null;
        arcSecondPointRef.current = null;
        rectSecondPointRef.current = null;
        circleSecondPointRef.current = null;
        circleTangentLineIdsRef.current = [];
        ellipseSecondPointRef.current = null;
        splineDraftPolesRef.current = [];
        clearPreviewLine();
        clearPreviewCircle();
        clearPreviewArc();
        clearPreviewSlot();
        clearPreviewSpline();
        clearPreviewDimension();
        clearPreviewInference();
        clearDraftDimensionSession();
        setSketchSnapLabel(null);
        setConstraintPreview(null);
        dragSnapResultRef.current = null;
        setHoveredSketchEntity(null);
        setHoveredSketchPoint(null);
        requestRender();
        return;
      }
      handleContextMenu(event);
      requestRender();
    };
    const onDoubleClick = (event: MouseEvent) => {
      handleDoubleClick(event);
      requestRender();
    };
    const onWheel = (event: WheelEvent) => {
      handleWheel(event);
      requestRender();
    };

    renderer.domElement.addEventListener("pointerdown", onPointerDown);
    renderer.domElement.addEventListener("pointermove", onPointerMove);
    renderer.domElement.addEventListener("pointerleave", onPointerLeave);
    renderer.domElement.addEventListener("pointerup", onPointerUp);
    renderer.domElement.addEventListener("contextmenu", onContextMenu);
    renderer.domElement.addEventListener("dblclick", onDoubleClick);
    renderer.domElement.addEventListener("wheel", onWheel, { passive: false });

    return () => {
      onSnapshotCaptureReady?.(null);
      requestViewportRenderRef.current = null;
      if (frameId !== null) {
        window.cancelAnimationFrame(frameId);
      }
      if (drawingInsertMoveFrameRef.current !== null) {
        window.cancelAnimationFrame(drawingInsertMoveFrameRef.current);
        drawingInsertMoveFrameRef.current = null;
      }
      drawingInsertMoveLatestRef.current = null;
      if (pendingMoveGizmoFrameRef.current !== null) {
        window.cancelAnimationFrame(pendingMoveGizmoFrameRef.current);
        pendingMoveGizmoFrameRef.current = null;
      }
      pendingMoveGizmoParametersRef.current = null;
      cancelPendingDraftPointerMoveFrame();
      resizeObserver.disconnect();
      controls.removeEventListener("change", requestRenderOnControlsChange);
      renderer.domElement.removeEventListener("pointerdown", onPointerDown);
      renderer.domElement.removeEventListener("pointermove", onPointerMove);
      renderer.domElement.removeEventListener("pointerleave", onPointerLeave);
      renderer.domElement.removeEventListener("pointerup", onPointerUp);
      renderer.domElement.removeEventListener("contextmenu", onContextMenu);
      renderer.domElement.removeEventListener("dblclick", onDoubleClick);
      renderer.domElement.removeEventListener("wheel", onWheel);
      window.removeEventListener("polysmith-trim-preview", onTrimPreview);
      window.removeEventListener(
        "polysmith-corner-trim-preview",
        onCornerTrimPreview,
      );
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
      lastCadCameraRef.current = null;
      if (cameraFrameCaptureRef) {
        cameraFrameCaptureRef.current = null;
      }
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
      lastSceneBuildKeyRef.current = "";
    };
  }, [activeSketchPlaneId]);

  // Latest scene-sync arguments, so imperative paths (restorePreviewScene)
  // can re-run the sync without render-cycle stale closures.
  const sceneSyncArgsRef = useRef<Parameters<typeof syncViewportScene>[0] | null>(
    null,
  );

  const runSceneSync = useCallback(() => {
    const args = sceneSyncArgsRef.current;
    if (!args) {
      return;
    }
    syncViewportScene(args);
    requestViewportRenderRef.current?.();
  }, []);

  // Rebuilds the persistent manipulator ring from the current selection
  // (Fusion-style): shown while the Move tool is armed and something
  // movable is selected; follows the selection after moves and commits.
  const updatePersistentMoveRing = useCallback(() => {
    disposeSketchMoveRingObject(persistentRingGroupRef.current);
    persistentRingGroupRef.current = null;
    persistentRingPickablesRef.current = [];
    persistentRingStateRef.current = null;
    const planeId = activeSketchPlaneIdRef.current;
    if (
      activeSketchToolRef.current !== "move" ||
      !planeId ||
      sketchMoveDragRef.current
    ) {
      return;
    }
    const state = sketchMoveRingStateForSelection(
      sketchLinesRef.current,
      sceneDataRef.current,
    );
    const sketchGroup = sketchGroupRef.current;
    if (!state || !sketchGroup) {
      return;
    }
    const centerWorld = toWorldPoint(
      planeId,
      state.centerLocal,
      activeSketchPlaneFrameRef.current,
    );
    const ring = buildSketchMoveRingObject({
      centerWorld,
      radius: sketchMoveRingRadius(sketchLinesRef.current, state.vertexIds),
      planeFrame: activeSketchPlaneFrameRef.current,
    });
    sketchGroup.add(ring.group);
    persistentRingGroupRef.current = ring.group;
    persistentRingPickablesRef.current = ring.pickables;
    persistentRingStateRef.current = state;
  }, []);

  // Applies the Move/Copy dialog's pending transform to the real scene
  // objects (solved from the base positions) — used by drag frames, the
  // numeric fields, and re-applies after scene rebuilds while the dialog
  // is open.
  const applyPendingSketchMovePreview = useCallback(() => {
    const pending = pendingSketchMoveRef.current;
    const planeId = activeSketchPlaneIdRef.current;
    if (!pending || !planeId) {
      return;
    }
    if (pending.dx === 0 && pending.dy === 0 && pending.angleDeg === 0) {
      return; // zero transform — the committed scene already matches
    }
    const solvedPoints = solvePendingSketchMove({
      pending,
      sketch: sketchLinesRef.current,
      constraints: sketchConstraintsRef.current,
    });
    moveDragPreviewActiveRef.current = true;
    applySolvedPointsToSketchScene({
      solvedPoints,
      sketch: sketchLinesRef.current,
      planeId,
      planeFrame: activeSketchPlaneFrameRef.current,
      sketchEntityObjectById: sketchEntityObjectByIdRef.current,
      sketchPointObjectById: sketchPointObjectByIdRef.current,
      sketchConstraintObjects: sketchConstraintObjectsRef.current,
      sketchProfileObjects: sketchProfileObjectsRef.current,
      constraintDeltas: sketchMoveConstraintDeltas({
        sceneConstraints: sceneConstraintsRef.current,
        sketch: sketchLinesRef.current,
        entityIds: pending.entityIds,
        vertexIds: pending.vertexIds,
        baseVertexPositions: pending.baseVertexPositions,
        dx: pending.dx,
        dy: pending.dy,
        center: pending.centerLocal,
        angleRad: (pending.angleDeg * Math.PI) / 180,
        planeId,
        planeFrame: activeSketchPlaneFrameRef.current,
      }),
    });
  }, []);

  // OK: commits the accumulated transform as ONE move (one undo step)
  // and closes the dialog by returning to the select tool.
  const commitPendingSketchMove = useCallback(async () => {
    const pending = pendingSketchMoveRef.current;
    pendingSketchMoveRef.current = null;
    moveDragPreviewActiveRef.current = false;
    setMovePanelValues({ dx: 0, dy: 0, angleDeg: 0 });
    try {
      if (
        pending &&
        (pending.dx !== 0 || pending.dy !== 0 || pending.angleDeg !== 0)
      ) {
        await moveSketchEntitiesRef.current({
          entityIds: pending.entityIds,
          dx: pending.dx,
          dy: pending.dy,
          centerX: pending.centerLocal[0],
          centerY: pending.centerLocal[1],
          angleDeg: pending.angleDeg,
        });
      }
    } finally {
      // Rebuild from store state: shows the committed result immediately
      // (and heals the scene if the commit failed).
      forceSceneRebuildRef.current = true;
      runSceneSync();
      updatePersistentMoveRing();
    }
    await setSketchToolRef.current("select");
  }, [runSceneSync, updatePersistentMoveRing]);

  // Cancel: revert the preview and close the dialog.
  const cancelPendingSketchMove = useCallback(() => {
    pendingSketchMoveRef.current = null;
    moveDragPreviewActiveRef.current = false;
    setMovePanelValues({ dx: 0, dy: 0, angleDeg: 0 });
    forceSceneRebuildRef.current = true;
    runSceneSync();
    updatePersistentMoveRing();
    void setSketchToolRef.current("select");
  }, [runSceneSync, updatePersistentMoveRing]);

  // Restores committed geometry after a live preview that mutated real
  // scene objects (drag released without moving, Escape, lost pointer-up).
  const restorePreviewScene = useCallback(() => {
    endpointDragRef.current = null;
    dragPreviewMutatingRef.current = false;
    moveDragPreviewActiveRef.current = false;
    forceSceneRebuildRef.current = true;
    runSceneSync();
    updatePersistentMoveRing();
    // The Move/Copy dialog's preview survives restores (e.g. pointer
    // leave): re-apply the pending transform right after the rebuild.
    applyPendingSketchMovePreview();
  }, [runSceneSync, updatePersistentMoveRing, applyPendingSketchMovePreview]);

  useEffect(() => {
    sceneSyncArgsRef.current = {
      groups: {
        scene: sceneRef.current,
        camera: cameraRef.current,
        controls: controlsRef.current,
        contentGroup: contentGroupRef.current,
        referenceGroup: referenceGroupRef.current,
        sketchGroup: sketchGroupRef.current,
        drawingGroup: drawingGroupRef.current,
      },
      refs: {
        pendingEndpointCommit: pendingEndpointCommitRef,
        pendingMoveCommit: pendingMoveCommitRef,
        endpointDrag: endpointDragRef,
        dragPreviewMutating: dragPreviewMutatingRef,
        moveDragPreviewActive: moveDragPreviewActiveRef,
        forceSceneRebuild: forceSceneRebuildRef,
        activeSketchPlaneFrame: activeSketchPlaneFrameRef,
        sketchEntityObjectById: sketchEntityObjectByIdRef,
        sketchPointObjectById: sketchPointObjectByIdRef,
        sketchConstraintObjects: sketchConstraintObjectsRef,
        dragCursor: dragCursorRef,
        lastGeometryKey: lastGeometryKeyRef,
        lastSceneBuildKey: lastSceneBuildKeyRef,
        drawingPreviewGroupRef,
        drawingDragGroupRef,
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
        stockFaceMeshes: stockFaceMeshesRef,
        solidFaceVisuals: solidFaceVisualsRef,
        solidFaceStates: solidFaceStatesRef,
        edgeLineObjects: edgeLineObjectsRef,
        vertexObjects: vertexObjectsRef,
        cutPreviewObjects: cutPreviewObjectsRef,
        toolpathLines: toolpathLinesRef,
        moveGizmoObjects: moveGizmoObjectsRef,
        hiddenRelationPreviewDimensionIds: hiddenRelationPreviewDimensionIdsRef,
        selectedConstraint: selectedConstraintRef,
        sketchEntityObjects: sketchEntityObjectsRef,
        sketchDimensionObjects: sketchDimensionObjectsRef,
        dimensionObjectById: dimensionObjectByIdRef,
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
      showCamToolpath,
      showDrawingSheet,
      drawingDimensionPreview,
      drawingAnnotationPreview,
      drawingViewPreview,
      drawingViewDrag,
      drawingDimensionTextDrag,
      drawingGhostFrame,
      drawingGhostAnchored,
      drawingSelectedViewId,
      drawingDetailDrag,
      wcsOrientation,
      activeCamSetupId,
      // All pick modes share the snap markers + hover suppression —
      // sceneSync only needs to know that SOME pick is armed.
      originPickArmed:
        originPickPointEnabled ||
        wcsPickPointEnabled ||
        drillPickPointEnabled,
      // The drill pick lifts its circle/face-center markers to the
      // material top (holes whose sketch circles sit at the bottom).
      drillPickArmed: drillPickPointEnabled,
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
    };
    runSceneSync();
    updatePersistentMoveRing();
    // The Move/Copy dialog's preview must survive scene rebuilds
    // (the scene is built from committed state).
    applyPendingSketchMovePreview();
  }, [activeTheme.id, config.displayUnits, displayedSketchDimensions, moveGizmo, sceneData, showReferencePlanes, document, viewport, showStock, showCamToolpath, showDrawingSheet, drawingDimensionPreview, drawingAnnotationPreview, drawingViewPreview, drawingViewDrag, drawingDimensionTextDrag, drawingGhostFrame, drawingGhostAnchored, drawingSelectedViewId, drawingDetailDrag, wcsOrientation, activeCamSetupId, originPickPointEnabled, wcsPickPointEnabled, drillPickPointEnabled, runSceneSync, updatePersistentMoveRing, applyPendingSketchMovePreview]);

  // Entering the drawing workspace fits the camera to the sheet — the
  // sheet is the workspace's whole content, so the default CAD framing
  // (which ignores sheet geometry) would leave the user staring at an
  // empty corner.
  useEffect(() => {
    const camera = cameraRef.current;
    const controls = controlsRef.current;
    const firstSheet = viewport?.drawing_sheets?.[0];
    if (!showDrawingSheet || !camera || !controls || !firstSheet) {
      return;
    }
    fitCameraToDrawingSheet({
      camera,
      controls,
      host: hostRef.current,
      widthMm: firstSheet.width_mm,
      heightMm: firstSheet.height_mm,
    });
    requestViewportRenderRef.current?.();
  }, [showDrawingSheet, viewport?.drawing_sheets?.[0]?.sheet_id, viewport?.drawing_sheets?.[0]?.width_mm, viewport?.drawing_sheets?.[0]?.height_mm]);

  useEffect(() => {
    lineDraftStartRef.current = null;
    arcSecondPointRef.current = null;
    rectSecondPointRef.current = null;
    circleSecondPointRef.current = null;
    circleTangentLineIdsRef.current = [];
    ellipseSecondPointRef.current = null;
    splineDraftPolesRef.current = [];
    clearDragPreviewLines();
    clearPreviewLine();
    clearPreviewCircle();
    clearPreviewArc();
    clearPreviewSlot();
    clearPreviewSpline();
    clearPreviewDimension();
    clearPreviewInference();
    clearTrimHighlights(clearTrimSegmentHighlight, clearTrimArcHighlight);
    setSketchSnapLabel(null);
    setConstraintPreview(null);
    clearDraftDimensionSession();
    cancelDimensionPlacement();
    // Also cancel any in-progress linear placement preview.
    if (linearPlacementRef.current) {
      linearPlacementRef.current = null;
      cancelLinearPlacementPreview(sketchGroupRef.current!, linearPlacementPreviewRef);
      controlsRef.current!.enabled = true;
    }
    pendingDimensionPlacementRef.current = false;
    // Reset the dimension tool's pending first-line on every tool
    // switch so it can't leak across tools or sketches.
    dimensionToolFirstLineRef.current = null;
    setDimensionToolFirstLine(null);
    dimensionToolFirstPointRef.current = null;
    // Cancel any in-progress Move drag (and its live preview) on tool
    // or plane switch.
    if (sketchMoveDragRef.current || moveDragPreviewActiveRef.current) {
      sketchMoveDragRef.current = null;
      disposeSketchMoveRingObject(sketchMoveRingGroupRef.current);
      sketchMoveRingGroupRef.current = null;
      moveDragPreviewActiveRef.current = false;
      forceSceneRebuildRef.current = true;
      runSceneSync();
    }
    // Leaving the Move tool reverts an open Move/Copy dialog (Escape or
    // a toolbar switch); OK/Cancel already clear the pending state
    // before switching tools, so this only fires for implicit exits.
    if (pendingSketchMoveRef.current && activeSketchTool !== "move") {
      pendingSketchMoveRef.current = null;
      moveDragPreviewActiveRef.current = false;
      setMovePanelValues({ dx: 0, dy: 0, angleDeg: 0 });
      forceSceneRebuildRef.current = true;
      runSceneSync();
    }
    // Show/hide the persistent manipulator ring for the new tool state.
    updatePersistentMoveRing();
    requestViewportRenderRef.current?.();
  }, [activeSketchPlaneId, activeSketchTool, updatePersistentMoveRing, runSceneSync]);

  useEffect(
    () =>
      bindSketchHotkeys({
        activeSketchPlaneId,
        sketchToolbarHotkeys: config.hotkeys.sketchToolbar,
        documentRef,
        activeSketchToolRef,
        dimensionLabelDragRef,
        dimensionPlacementOriginalPositionRef,
        pendingDimensionIdRef,
        pendingDimSourceEntityIdRef,
        pendingDimensionPlacementRef,
        controlsRef,
        selectedConstraintRef,
        sketchToolConstructionRef,
        selectionDragRef,
        deleteSketchDimensionRef,
        clearSketchConstraintRef,
        clearSketchSelectionRef,
        deleteSketchSelectionRef,
        confirmDeleteSketchSelectionRef,
        setSketchToolRef,
        clearPreviewDimension,
        finishDimensionPlacement,
        setCanvasCursor,
        setSelectedConstraint,
        cancelActiveSketchDraft,
        commitSplineDraft: () => commitSplineDraftRef.current(),
        setSketchToolConstruction,
      }),
    [activeSketchPlaneId, config.hotkeys.sketchToolbar],
  );

  useGhostEdgeRevealHotkey({
    pendingEdgeOpBodyIds,
    revealGhostEdgesRef,
    hoveredEdgeIdRef,
    paintEdgeMaterials,
  });

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

  const contextMenuActions = createViewportContextMenuActions({
    contextMenu,
    document,
    sceneData,
    sketchLinesRef,
    setContextMenu,
    setSelectedConstraint,
    setIsDimensionEditorOpen,
    selectReferenceRef,
    startSketchRef,
    selectFaceRef,
    startSketchOnFaceRef,
    moveBodyRef,
    copyBodyRef,
    exportBodyMeshRef,
    exportBodyStepRef,
    sendBodyToSlicerRef,
    unlinkBodyCopyRef,
    removeSketchProjectionsRef,
    deleteSketchSelectionRef,
    deleteSketchDimensionRef,
    toggleSketchDimensionDrivenRef,
    setSketchLineConstructionRef,
    clearSketchConstraintRef,
    updateSketchDimensionDisplayRef,
    selectSketchEntityRef,
    selectSketchPointRef,
    selectSketchProfileRef,
    pickSketchPointRef,
    setSketchToolRef,
    openTransformArrayRef,
  });

  const lineCount = sketchFeature?.sketch_parameters?.lines.length ?? 0;
  const circleCount = sketchFeature?.sketch_parameters?.circles.length ?? 0;
  const pointCount = sketchFeature?.sketch_parameters?.vertices.length ?? 0;
  const arcCount = sketchFeature?.sketch_parameters?.arcs.length ?? 0;

  const {
    cancelDimensionEdit,
    cancelDimensionPlacementFromEditor,
    handleDimensionDraftChange,
    handleSubmitDimensionEdit,
    insertDimensionParameterSuggestion,
  } = createDimensionEditorActions({
    selectedSketchDimension,
    selectedSketchDimensionValue,
    selectedSketchDimensionExpression,
    dimensionDraftValue,
    displayUnits: config.displayUnits,
    dimensionInputRef,
    dimensionExpressionTimeoutRef,
    dimensionEditOriginalValueRef,
    dimensionLabelDragRef,
    dimensionPlacementOriginalPositionRef,
    controlsRef,
    updateSketchDimension: updateSketchDimensionRef.current,
    deleteSketchDimension: deleteSketchDimensionRef.current,
    dimensionCoreValue,
    formattedDimensionDisplayValue,
    finishDimensionPlacement,
    cancelDimensionPlacement,
    setDimensionDraftValue,
    setDimensionLabelPositions,
    setIsDimensionEditorOpen,
    setCanvasCursor,
  });

  const {
    getDraftFieldInputValue,
    getDraftParameterSuggestions,
    handleDraftDimensionBlur,
    handleDraftDimensionChange,
    handleDraftDimensionFocus,
    handleDraftDimensionKeyDown,
    insertDraftParameterSuggestion,
    focusDraftField,
  } = createDraftDimensionActions({
    displayUnits: config.displayUnits,
    parameters: document?.parameters,
    draftDimensionSessionRef,
    draftRawInputRef,
    draftParameterExpressionRef,
    draftFieldFocusedRef,
    draftDimScreenPositionsRef,
    draftDimensionInputRefs,
    draftSuggestionState,
    setDraftSuggestionState,
    setDraftDimensionSession,
    commitDraftDimensionSession,
    selectTool: () => setSketchToolRef.current("select"),
    cancelActiveSketchDraft,
  });

  function draftFieldScreenPosition(field: DraftDimensionField) {
    return draftDimensionFieldScreenPosition({
      field,
      session: draftDimensionSession,
      screenPositions: draftDimScreenPositionsRef.current,
      camera: cameraRef.current,
      renderer: rendererRef.current,
      activeSketchPlaneId,
      activeSketchPlaneFrame,
    });
  }

  const {
    crosshairCanvasClass,
    crosshairGuideSize,
    isSketchDrawingCursor,
    isSnapping,
    usesCrosshairGuide,
  } = computeViewportCrosshairState({
    activeSketchPlaneId,
    activeSketchTool,
    crosshairMode: config.viewport.crosshair,
    isSnapping: sketchSnapLabel !== null,
    viewportSize,
  });
  const isSketchMode = Boolean(activeSketchPlaneId);

  return (
    <ViewportPanelShell
      activeSketchPlaneId={activeSketchPlaneId}
      activeSketchTool={activeSketchTool}
      arcCount={arcCount}
      arcToolMode={arcToolMode}
      armedSketchConstraint={armedSketchConstraint}
      canvasRef={canvasRef}
      circleCount={circleCount}
      circleToolMode={circleToolMode}
      constraintPreview={constraintPreview}
      contextMenu={contextMenu}
      currentGridSpacing={currentGridSpacing}
      contextMenuActions={contextMenuActions}
      showSketchProjectionActions={showSketchProjectionActions ?? false}
      sketchMovePanelOpen={activeSketchTool === "move"}
      sketchMovePanelValues={movePanelValues}
      onSketchMovePanelValuesChange={(values) => {
        setMovePanelValues(values);
        const pending = pendingSketchMoveRef.current;
        if (pending) {
          pending.dx = values.dx;
          pending.dy = values.dy;
          pending.angleDeg = values.angleDeg;
          applyPendingSketchMovePreview();
          requestViewportRenderRef.current?.();
        }
      }}
      onSketchMovePanelCommit={() => void commitPendingSketchMove()}
      onSketchMovePanelCancel={() => void cancelPendingSketchMove()}
      crosshairCanvasClass={crosshairCanvasClass}
      crosshairGuideSize={crosshairGuideSize}
      crosshairPointer={crosshairPointer}
      dimensionDraftValue={dimensionDraftValue}
      dimensionEditorRef={dimensionEditorRef}
      dimensionInputRef={dimensionInputRef}
      dimensionParameterSuggestions={dimensionParameterSuggestions}
      dimensionSuggestionIndex={dimensionSuggestionIndex}
      dimensionToolFirstLine={dimensionToolFirstLine}
      dimensionToolHotkey={config.hotkeys.sketchToolbar.dimension}
      document={document}
      draftDimensionInputRefs={draftDimensionInputRefs}
      draftDimensionSession={draftDimensionSession}
      draftSuggestionState={draftSuggestionState}
      finishDisabled={status !== "connected"}
      hasActiveDocument={hasActiveDocument}
      hostRef={hostRef}
      isDimensionEditorOpen={isDimensionEditorOpen}
      isSketchDrawingCursor={isSketchDrawingCursor}
      isSnapping={isSnapping}
      isSketchMode={isSketchMode}
      lineCount={lineCount}
      lineDraftActive={Boolean(lineDraftStartRef.current)}
      measurementText={measurementText}
      pointCount={pointCount}
      polygonSides={polygonSides}
      polygonToolMode={polygonToolMode}
      rectangleToolMode={rectangleToolMode}
      dimensionToolMode={dimensionToolMode}
      selectedConstraint={selectedConstraint}
      selectedEntityDof={selectedEntityDof}
      selectedPrimitiveLabel={selectedPrimitiveLabel}
      selectedReference={selectedReference}
      selectedSketchDimension={selectedSketchDimension}
      selectionRect={selectionRect}
      showConstraints={showConstraints}
      showSketchGrid={showSketchGrid}
      showViewportGrid={showViewportGrid}
      sketchSnapLabel={sketchSnapLabel}
      sketchToolConstruction={sketchToolConstruction}
      status={status}
      translate={translate}
      usesCrosshairGuide={usesCrosshairGuide}
      viewportGridHotkey={config.hotkeys.viewport.toggleGrid}
      getDraftFieldInputValue={getDraftFieldInputValue}
      getDraftParameterSuggestions={getDraftParameterSuggestions}
      getDraftScreenPosition={draftFieldScreenPosition}
      onCommitDraftDimensionSession={commitDraftDimensionSession}
      onDimensionDraftChange={(value) => {
        dimensionInputSelectionLockedRef.current = false;
        handleDimensionDraftChange(value);
      }}
      onDimensionEditorFocus={(event) => {
        if (dimensionInputSelectionLockedRef.current) {
          event.currentTarget.select();
        }
      }}
      onDimensionEditorKeyDown={(event) => {
        dimensionInputSelectionLockedRef.current = false;
        handleDimensionEditorInputKeyDown({
          event,
          suggestions: dimensionParameterSuggestions,
          suggestionIndex: dimensionSuggestionIndex,
          setSuggestionIndex: setDimensionSuggestionIndex,
          insertParameterSuggestion: insertDimensionParameterSuggestion,
          cancelPlacementDimension: cancelDimensionPlacementFromEditor,
          cancelEdit: cancelDimensionEdit,
        });
      }}
      onDraftDimensionBlur={handleDraftDimensionBlur}
      onDraftDimensionChange={handleDraftDimensionChange}
      onDraftDimensionFocus={handleDraftDimensionFocus}
      onDraftDimensionKeyDown={handleDraftDimensionKeyDown}
      onFinishSketch={() => {
        void onFinishSketch();
      }}
      onInsertDimensionParameterSuggestion={insertDimensionParameterSuggestion}
      onInsertDraftParameterSuggestion={insertDraftParameterSuggestion}
      onPolygonSidesChange={(value) => {
        setPolygonSides(value);
        polygonSidesRef.current = value;
      }}
      onSetArcToolMode={onSetArcToolMode}
      onSetCircleToolMode={onSetCircleToolMode}
      onSetPolygonToolMode={onSetPolygonToolMode}
      onSetRectangleToolMode={onSetRectangleToolMode}
      onSetDimensionToolMode={onSetDimensionToolMode}
      onSketchToolConstructionChange={(checked) => {
        sketchToolConstructionRef.current = checked;
        setSketchToolConstruction(checked);
      }}
      onSubmitDimensionEdit={handleSubmitDimensionEdit}
      onToggleGrid={() => {
        toggleGridVisibility(isSketchMode ? "sketch" : "viewport");
      }}
      onToggleConstraints={toggleConstraintsVisibility}
    />
  );
}

function constraintPreviewEquals(
  a: ConstraintPreviewState | null,
  b: ConstraintPreviewState | null,
) {
  if (a === b) {
    return true;
  }
  if (!a || !b) {
    return false;
  }
  return a.kind === b.kind && a.x === b.x && a.y === b.y;
}

function screenPointEquals(
  a: { x: number; y: number } | null,
  b: { x: number; y: number } | null,
) {
  if (a === b) {
    return true;
  }
  if (!a || !b) {
    return false;
  }
  return a.x === b.x && a.y === b.y;
}
