import {
  useEffect,
  useMemo,
  useRef,
  useState,
} from "react";
import { useTranslation } from "react-i18next";
import * as THREE from "three";
import { OrbitControls } from "three/examples/jsm/controls/OrbitControls.js";
import {
  applyTheme,
  useAppConfig,
} from "@/config";
import type {
  DocumentState,
  SketchTool,
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
} from "@/utils";
import type { ViewCubeHit, CubeBlitScene } from "@/utils";
import { makeGetViewportStateCommand } from "@/lib/ipcProtocol";
import { useCadCoreStore } from "@/state/cadCoreStore";
import {
  disposeDynamicGrid,
  getOrthographicViewHeight,
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
import { usePendingLineCommitRelations } from "./viewport/lineCommitRelationEffects";
import {
  collectRectangleSelectionIds,
  selectionRectOverlayFromDrag,
  type SelectionRectOverlay,
  type SelectionDrag,
} from "./viewport/selectionGeometry";
import { type EndpointDrag } from "./viewport/endpointDrag";
import { handleEndpointDragPointerMove } from "./viewport/endpointDragPointerMove";
import { finishEndpointDragPointerUp } from "./viewport/endpointDragPointerUp";
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
import { createDimensionRelationPreviewActions } from "./viewport/dimensionRelationPreviewActions";
import { createLineAnglePreview } from "./viewport/dimensionRelationPreviewGeometry";
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
import { updateDynamicGrids } from "./viewport/dynamicGridUpdate";
import { bindSketchHotkeys } from "./viewport/sketchHotkeys";
import { syncViewportScene } from "./viewport/sceneSync";
import {
  renderViewCubeFrame,
  rotateCameraAroundCurrentView,
} from "./viewport/viewCubeRender";
import { createViewportPreviewActions } from "./viewport/previewObjectCleanup";
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
  onAddSketchLineAngleDimension,
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
  dimensionToolMode,
  onSetDimensionToolMode,
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
  const addLogEntry = useCadCoreStore((state) => state.addLogEntry);
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
  const pendingDraftPointerMoveEventRef = useRef<PointerEvent | null>(null);
  const pendingDraftPointerMoveFrameRef = useRef<number | null>(null);
  const objectSnapLatchRef = useRef<string | null>(null);
  const lastGeometryKeyRef = useRef("");
  const lastSceneBuildKeyRef = useRef("");
  const requestViewportRenderRef = useRef<(() => void) | null>(null);
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
  const dimensionToolModeRef = useRef(dimensionToolMode);
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
  const addSketchLineAngleDimensionRef = useRef(
    onAddSketchLineAngleDimension,
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
  // Post-commit dimension deletion for drag-only shapes that have no
  // typed value (Fusion 360 behavior). When the user commits a shape
  // by dragging without typing into a draft dimension field, the core
  // still creates an auto-dimension — we delete it here.
  useEffect(() => {
    deletePendingAutoDimensions({
      pendingDimensionDeletionRef,
      sketch: sketchFeature?.sketch_parameters,
      deleteSketchDimension: deleteSketchDimensionRef.current,
    });
  }, [sketchFeature]);
  useEffect(() => {
    applyPendingDraftDimensionExpressions({
      pendingDraftDimensionExpressionsRef,
      sketch: sketchFeature?.sketch_parameters,
      updateSketchDimension: updateSketchDimensionRef.current,
    });
  }, [sketchFeature]);
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
    dimensionToolModeRef.current = dimensionToolMode;
  }, [dimensionToolMode]);
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
    clearDragPreviewLines,
    clearPreviewArc,
    clearPreviewCircle,
    clearPreviewDimension,
    clearPreviewLine,
    clearTrimArcHighlight,
    clearTrimSegmentHighlight,
    updateTrimArcHighlight,
    updateTrimSegmentHighlight,
  } = createViewportPreviewActions({
    sketchGroupRef,
    dragPreviewLinesRef,
    previewLineRef,
    previewCircleRef,
    previewArcRef,
    trimSegmentHighlightRef,
    trimArcHighlightRef,
    previewDimensionRef,
    dimensionRelationPreviewRef,
    dimensionRelationPreviewLabelRef,
    restoreRelationPreviewHiddenDimensions,
  });

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
    createDimensionPointDistance: dimCreatePointDistance,
    createDimensionPolygon: dimCreatePolygon,
    selectDimensionCircle: dimSelectCircle,
    selectDimensionLine: dimSelectLine,
    selectDimensionPolygon: dimSelectPolygon,
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
    addSketchLineLengthDimensionRef,
    addSketchLineAngleDimensionRef,
    addSketchPolygonRadiusDimensionRef,
    addSketchAngleDimensionRef,
    addSketchDistanceDimensionRef,
    addSketchPointDistanceDimensionRef,
    updateSketchDimensionRef,
    setDimensionToolFirstLine,
  });

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
      previewDimensionRef,
      clearPreviewLine,
      clearPreviewCircle,
      clearPreviewArc,
      clearPreviewDimension,
    });
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
      addSketchArcRef,
      addSketchAngleDimensionRef,
      addSketchDistanceDimensionRef,
      addSketchLineLengthDimensionRef,
      addSketchLineAngleDimensionRef,
      addSketchCircleRadiusDimensionRef,
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
      selectSketchEntityRef,
      pickInactiveSketchLineRef,
      inactiveSketchEntityPickEnabledRef,
      pickSketchPointRef,
      updateSketchPointRef,
      selectSketchDimensionRef,
      updateSketchDimensionRef,
      updateSketchDimensionLabelPositionRef,
      addSketchPointDistanceDimensionRef,
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
      unlinkBodyCopyRef,
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
      onAddSketchArc,
      onAddSketchAngleDimension,
      onAddSketchDistanceDimension,
      onAddSketchLineLengthDimension,
      onAddSketchLineAngleDimension,
      onAddSketchCircleRadiusDimension,
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
      onSelectSketchEntity,
      onPickInactiveSketchLine,
      inactiveSketchEntityPickEnabled,
      onPickSketchPoint,
      onUpdateSketchPoint,
      onSelectSketchDimension,
      onUpdateSketchDimension,
      onUpdateSketchDimensionLabelPosition,
      onAddSketchPointDistanceDimension,
      onUpdateSketchDimensionDisplay,
      onSelectSketchProfile,
      onTrimSketchEntity,
      onDeleteSketchSelection,
      onSetSketchTool,
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
    const controls = new OrbitControls(camera, renderer.domElement);
    const contentGroup = new THREE.Group();
    const referenceGroup = new THREE.Group();
    const sketchGroup = new THREE.Group();
    const raycaster = new THREE.Raycaster();
    const pointer = new THREE.Vector2();
    let pointerDown: { x: number; y: number } | null = null;
    let frameId: number | null = null;
    let renderBurstUntil = 0;

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
    controls.addEventListener("change", requestRenderOnControlsChange);

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
        arcSecondPoint: arcSecondPointRef.current,
        circleSecondPoint: circleSecondPointRef.current,
        rectSecondPoint: rectSecondPointRef.current,
        isConstruction: sketchToolConstructionRef.current,
        previewLineRef,
        previewCircleRef,
        previewArcRef,
        previewDimensionRef,
        clearPreviewLine,
        clearPreviewCircle,
        clearPreviewArc,
        clearPreviewDimension,
        clearTrimSegmentHighlight,
        clearTrimArcHighlight,
        updateTrimSegmentHighlight,
        updateTrimArcHighlight,
      });
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

    function handlePointerDown(event: PointerEvent) {
      cancelPendingDraftPointerMoveFrame();
      objectSnapLatchRef.current = null;
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
        activeSketchPlaneIdRef,
        activeSketchPlaneFrameRef,
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
        createDraftDimensionSession,
        setDraftDimensionSession,
        focusDraftField,
      });
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
          pendingDragRef,
          pendingDragFrameRef,
          dragSnapResultRef,
          dragCursorRef,
          dragPreviewLinesRef,
          sketchGroupRef,
          resolveSnappedSketchPoint,
          setSketchSnapLabel,
          clearDragPreviewLines,
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

      const hit = intersectSceneTargets(event);
      applySceneHover(hit, hoverActions());
    }

    function handlePointerLeave() {
      cancelPendingDraftPointerMoveFrame();
      objectSnapLatchRef.current = null;
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
      }

      dimensionLabelDragRef.current = null;
      controls.enabled = true;
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
      cancelPendingDraftPointerMoveFrame();
      objectSnapLatchRef.current = null;
      handleViewportPointerUp({
        event,
        renderer,
        camera,
        controls,
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
	        createDimensionPointDistance: dimCreatePointDistance,
	        createDimensionLine: dimCreateLine,
		createDimensionLineAngle: dimCreateLineAngle,
	        createDimensionCircle: dimCreateCircle,
	        selectDimensionCircle: dimSelectCircle,
	        createDimensionPolygon: dimCreatePolygon,
	        selectDimensionPolygon: dimSelectPolygon,
	        selectDimensionLine: dimSelectLine,
	        sketchCircleCount: sketchFeature?.sketch_parameters?.circles.length ?? 0,
	        lineDraftStartRef,
	        arcSecondPointRef,
	        rectSecondPointRef,
	        circleSecondPointRef,
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
      trimPreviewResultRef.current = (e as CustomEvent).detail;
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
      lastSceneBuildKeyRef.current = "";
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
        lastSceneBuildKey: lastSceneBuildKeyRef,
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
    requestViewportRenderRef.current?.();
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
    unlinkBodyCopyRef,
    deleteSketchSelectionRef,
    deleteSketchDimensionRef,
    clearSketchConstraintRef,
    updateSketchDimensionDisplayRef,
  });

  const lineCount = sketchFeature?.sketch_parameters?.lines.length ?? 0;
  const circleCount = sketchFeature?.sketch_parameters?.circles.length ?? 0;
  const pointCount = sketchFeature?.sketch_parameters?.points.length ?? 0;
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
    usesCrosshairGuide,
  } = computeViewportCrosshairState({
    activeSketchPlaneId,
    activeSketchTool,
    crosshairMode: config.viewport.crosshair,
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
      contextMenuActions={contextMenuActions}
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
