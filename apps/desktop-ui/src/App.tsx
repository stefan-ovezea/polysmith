import { useEffect, useMemo, useRef, useState } from "react";

import { useTranslation } from "react-i18next";
import {
  awaitDocumentChange,
  useCadCoreStore,
} from "./state";
import { useCadCore } from "./hooks";
import {
  useAppConfig,
} from "./lib";
import {
  AiAssistantPanel,
  FeatureTimeline,
  LogsWindow,
  MessageLog,
  SettingsModal,
  ToastViewport,
  ViewportPanel,
} from "./layout";
import type { CategoryId } from "./layout";
import { ArmedSketchConstraint } from "./types";
import type {
  ExtrudeAdvancedParameters,
  ExtrudeFeatureParameters,
  ExtrudeMode,
  SketchTool,
} from "./types";
import type { RecentProjectsDocument } from "./lib";
import {
  DEFAULT_ANGLE_PLANE_DEGREES,
  DEFAULT_FILLET_RADIUS,
  DEFAULT_OFFSET_PLANE_DISTANCE,
  DEFAULT_SHELL_THICKNESS,
  EMPTY_RECENT_PROJECTS_DOCUMENT,
  IS_MACOS,
  SHOW_DEBUG_MESSAGE_LOG,
  documentHasSolidBody,
  type AnglePlaneAction,
  type ActiveEdgeOpAction,
  type ActiveExtrudeAction,
  type ActiveLoftAction,
  type ActiveMoveAction,
  type ActiveRevolveAction,
  type ActiveSweepAction,
  type FastenerAction,
  type HelixAction,
  type HoleAction,
  type MidplaneAction,
  type OffsetPlaneAction,
  type PendingUnsavedAction,
  type PendingReferenceAction,
  type SavedDocumentBaseline,
  type SidebarTab,
  type ShellAction,
  type SketchDeleteSelection,
  type ThreadAction,
  type WorkspaceView,
} from "./app/appState";
import { computeActiveFeatureParameters } from "./app/activeFeatureParameters";
import {
  buildCamOperations,
  computeDocumentUiState,
} from "./app/documentUiState";
import { ActiveSketchFilletPanel } from "./app/ActiveSketchFilletPanel";
import { ActiveSketchTextPanel } from "./app/ActiveSketchTextPanel";
import { ActiveBodyOperationPanels } from "./app/ActiveBodyOperationPanels";
import { AppTopBar } from "./app/AppTopBar";
import { ActiveMirrorPanel } from "./app/ActiveMirrorPanel";
import { ActiveMaterialsPanel } from "./app/ActiveMaterialsPanel";
import { ActiveViewPanel } from "./app/ActiveViewPanel";
import { ActiveHolePanel } from "./app/ActiveHolePanel";
import { ActiveProfileFeaturePanels } from "./app/ActiveProfileFeaturePanels";
import { ActiveThreadedFeaturePanels } from "./app/ActiveThreadedFeaturePanels";
import { createDocumentLifecycleActions } from "./app/documentLifecycleActions";
import { ActiveExtrudePreview } from "./app/ActiveExtrudePreview";
import { PendingExtrudePreview } from "./app/PendingExtrudePreview";
import { useAppLifecycleEffects } from "./app/appLifecycleEffects";
import { AppSidebar } from "./app/AppSidebar";
import { useAppHotkeys } from "./app/appHotkeys";
import { createBodyMoveActions } from "./app/bodyMoveActions";
import {
  createBodyModifierActions,
  createHoleParameterHandlers,
} from "./app/bodyModifierActions";
import { CamFloatingPanels } from "./app/CamFloatingPanels";
import { ConstructionPendingPanels } from "./app/ConstructionPendingPanels";
import { PrimitiveFeatureEditPanel } from "./app/PrimitiveFeatureEditPanel";
import {
  confirmAndDeleteFeatureFromContext,
  confirmAndDeleteSketchSelectionFromContext,
  deleteSketchSelectionFromContext,
  type PendingSketchDeleteConfirmation,
} from "./app/deleteConfirmations";
import { useExtrudeFeatureActions } from "./app/extrudeFeatureActions";
import { createConstructionActions } from "./app/constructionActions";
import {
  computeEffectiveHiddenFeatureIds,
  computeHiddenSketchPlaneIds,
  syncDefaultOriginVisibility,
} from "./app/featureVisibility";
import { computeFeatureActionAvailability } from "./app/featureActionAvailability";
import { useProfileFeatureActions } from "./app/profileFeatureActions";
import { createRecentProjectHandlers } from "./app/recentProjectHandlers";
import * as selectionSources from "./app/selectionSources";
import { SketchDeleteConfirmationPanel } from "./app/SketchDeleteConfirmationPanel";
import { SlicerWorkspace } from "./app/SlicerWorkspace";
import {
  createSketchToolActions,
  type SketchConstraintVertexKind,
} from "./app/sketchToolActions";
import {
  useSketchToolLifecycleEffects,
  type SketchFilletAction,
  type SketchTextAction,
} from "./app/sketchToolLifecycleEffects";
import {
  startSketchOnSelectedPlaneOrFace,
  toCorePlaneFrame,
} from "./app/sketchCreationActions";
import { buildSketchSourceLabels } from "./app/sketchSourceLabels";
import { buildSelectionSourceState } from "./app/selectionSourceState";
import { useSlicerWorkspaceActions } from "./app/slicerWorkspaceActions";
import { createThreadedFeatureActions } from "./app/threadedFeatureActions";
import { useTimelineEditSession } from "./app/timelineEditSession";
import { createTimelineFeatureEditHandler } from "./app/timelineFeatureEdit";
import { cancelActiveToolFromContext } from "./app/toolCancellation";
import { UnsavedDocumentDialog } from "./app/UnsavedDocumentDialog";
import { handleViewportEdgeSelection } from "./app/viewportEdgeSelection";
import { handleViewportFaceSelection } from "./app/viewportFaceSelection";
import {
  handleInactiveSketchLineSelection,
  handleSketchEntitySelection,
} from "./app/viewportSketchEntitySelection";
import { handleViewportPrimitiveSelection } from "./app/viewportPrimitiveSelection";
import { handleViewportSketchProfileSelection } from "./app/viewportSketchProfileSelection";
import { computeViewportSelectionState } from "./app/viewportSelectionState";
import { isToolStartBlocked } from "./app/actionAvailability";
import { sendCoreCommand } from "./lib/cadCoreClient";
import { makeGetViewportStateCommand } from "./lib/ipcProtocol";
import { usePluginHost } from "./plugins/PluginProvider";
import type { PluginActiveAction } from "./plugins/sdk";

function App() {
  const { t } = useTranslation();
  const { config } = useAppConfig();
  const pluginHost = usePluginHost();
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
  const [mirrorPersistent, setMirrorPersistent] = useState(false);
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
  const [activePluginAction, setActivePluginAction] =
    useState<PluginActiveAction | null>(null);
  // Arc tool creation mode. Defaults to three-point (common CAD workflow's default
  // and the most ergonomic for shaping curves on the fly). The
  // SketchToolbar exposes a segmented control to toggle to
  // center+start+end without leaving the tool.
  const [arcToolMode, setArcToolMode] = useState<
    "three_point" | "center_start_end"
  >("three_point");
  // Body-projection mode for the Project tool — applies when the
  // armed tool clicks a mesh body (face clicks route through
  // handleViewportFaceSelection).
  const [bodyProjectionMode, setBodyProjectionMode] = useState<
    "section" | "silhouette"
  >("section");
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
  // Dimension tool mode for the split tool button dropdown.
  // Defaults to "auto" which preserves existing sketch dimension behaviour.
  const [dimensionToolMode, setDimensionToolMode] = useState<
    import("@/types").DimensionToolMode
  >("auto");

  // Sketch fillet panel session. Mirrors `ActiveEdgeOpAction` (the
  // 3D fillet/chamfer flow) shape-for-shape: it opens the moment
  // the user activates the Fillet tool (`pending` phase, no
  // fillets yet) and transitions to `active` as soon as they
  // click their first eligible corner. The panel's `radius`
  // applies to every fillet created in the session and is fanned
  // out across all of them on every debounced numeric change so
  // the user gets the same "select N corners, dial in one
  // radius" experience as 3D fillets give for edges.
  const [sketchFilletAction, setSketchFilletAction] =
    useState<SketchFilletAction | null>(null);
  // Sketch Text panel session. Mirrors `sketchFilletAction`
  // shape-for-shape: pending while the Text tool is armed but no text
  // exists yet; active once a click created a text (or the user
  // picked an existing text's glyph in Select mode) — the panel then
  // edits that text's parameters.
  const [sketchTextAction, setSketchTextAction] =
    useState<SketchTextAction | null>(null);
  const sketchTextActionRef = useRef(sketchTextAction);
  useEffect(() => {
    sketchTextActionRef.current = sketchTextAction;
  }, [sketchTextAction]);
  const [pendingSketchDeleteConfirmation, setPendingSketchDeleteConfirmation] =
    useState<PendingSketchDeleteConfirmation | null>(null);
  // Mirror of `sketchFilletAction.filletIds` for the inline
  // viewport callback. Same trick as `activeEdgeIdsRef` in the 3D
  // edge-op flow: the click handler runs inside a closure that
  // captures the value at panel-open time, so we need a ref to
  // see the live list when each subsequent click lands.
  const sketchFilletIdsRef = useRef<string[]>([]);
  const [edgeOpAction, setEdgeOpAction] = useState<ActiveEdgeOpAction | null>(
    null,
  );
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
  const [offsetPlaneAction, setOffsetPlaneAction] =
    useState<OffsetPlaneAction | null>(null);
  const pendingOffsetRef = useRef<number>(DEFAULT_OFFSET_PLANE_DISTANCE);
  const [midplaneAction, setMidplaneAction] =
    useState<MidplaneAction | null>(null);
  const [tangentPlaneAction, setTangentPlaneAction] =
    useState<PendingReferenceAction | null>(null);
  const [anglePlaneAction, setAnglePlaneAction] =
    useState<AnglePlaneAction | null>(null);
  const pendingAngleRef = useRef<number>(DEFAULT_ANGLE_PLANE_DEGREES);
  const [constructionAxisAction, setConstructionAxisAction] =
    useState<PendingReferenceAction | null>(null);
  const [constructionPointAction, setConstructionPointAction] =
    useState<PendingReferenceAction | null>(null);
  const [helixAction, setHelixAction] = useState<HelixAction | null>(null);
  const [threadAction, setThreadAction] = useState<ThreadAction | null>(null);
  const [fastenerAction, setFastenerAction] = useState<FastenerAction | null>(
    null,
  );
  const [holeAction, setHoleAction] = useState<HoleAction | null>(null);
  // Identifies which feature (if any) is being edited via the floating
  // edit panel. The panel itself reads the feature's parameters
  // directly from `document.feature_history`, so we only need the id
  // here. `null` means the panel is closed. Triggered by a
  // double-click in the timeline (see `onEditFeature` below).
  const [editingFeatureId, setEditingFeatureId] = useState<string | null>(null);
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
  const recentProjectsStore = useMemo(
    () => ({
      read: () => recentProjectsDocumentRef.current,
      write: (nextDocument: RecentProjectsDocument) => {
        recentProjectsDocumentRef.current = nextDocument;
        setRecentProjectsDocument(nextDocument);
      },
    }),
    [],
  );
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
  const [viewPanelOpen, setViewPanelOpen] = useState(false);
  const [workspaceView, setWorkspaceView] = useState<WorkspaceView>("cad");
  const [slicerStatus, setSlicerStatus] = useState<string | null>(null);
  const [hasOrcaEmbedSession, setHasOrcaEmbedSession] = useState(false);
  // CAM workspace state
  const [activeCamOperation, setActiveCamOperation] =
    useState<import("./layout/header/CamToolbar").CamOperationType | null>(null);
  const [selectedCamOperationId, setSelectedCamOperationId] = useState<
    string | null
  >(null);
  const [isCamSetupPanelOpen, setIsCamSetupPanelOpen] = useState(false);
  const [showStock, setShowStock] = useState(true);
  const [wcsOrientation, setWcsOrientation] = useState<string>("model");
  const camOperations = useMemo(
    () => buildCamOperations(document),
    [(document?.cam as any)?.operations],
  );
  const slicerViewportRef = useRef<HTMLDivElement | null>(null);
  const errorLogCount = logs.filter((entry) => entry.level === "error").length;
  const isAiAssistantAvailable =
    config.ai.enabled &&
    config.ai.baseUrl.trim().length > 0 &&
    config.ai.model.trim().length > 0;
  const {
    selectedReference,
    selectedSketchableFace,
    selectedMaterialFace,
    selectedMaterialBodyId,
    selectedSketchProfile,
    selectedSketchProfiles,
    selectedSketchProfileIds,
    selectedSketchEntityIds,
  } = computeViewportSelectionState({ document, viewport });
  const {
    activeHoleFeature,
    activeHoleParameters,
    activeHoleStandards,
    activeHelixFeature,
    activeHelixParameters,
    activeThreadFeature,
    activeThreadParameters,
    activeThreadStandards,
    activeFastenerFeature,
    activeFastenerParameters,
    activeFastenerStandards,
    activeMoveFeature,
    activeMoveParameters,
    selectedMoveBodyId,
  } = computeActiveFeatureParameters({
    document,
    viewport,
    selectedMaterialBodyId,
    holeAction,
    helixAction,
    threadAction,
    fastenerAction,
    moveAction,
  });
  const selectedSketchProfileIdsKey = selectedSketchProfileIds.join("|");
  const {
    sketchProfileLabelById,
    sketchLineLabelById,
    sketchPathEntityLabelById,
  } = buildSketchSourceLabels(document);
  const {
    planeSourceContext,
    axisSourceContext,
    threadTargetContext,
    selectedExtrudableFaceId,
    selectedSweepPathEntityId,
  } = buildSelectionSourceState({
    document,
    viewport,
    selectedSketchProfileIds,
    selectedSketchableFaceId: selectedSketchableFace?.face_id ?? null,
    sketchProfileLabelById,
    sketchLineLabelById,
    sketchPathEntityLabelById,
    translate: t,
  });
  const activeSketchPlaneId = document?.active_sketch_plane_id ?? null;
  const activeSketchTool = document?.active_sketch_tool ?? null;
  // The active sketch's pending mirror state lives in the document.
  // The UI presents the floating panel whenever this is non-null;
  // local React state only tracks which slot has keyboard / pick
  // focus.
  const {
    activeSketchFeature,
    isDocumentDirty,
    currentDocumentName,
    windowDocumentTitle,
    pendingMirror,
    isMirrorToolOpen,
  } = useMemo(
    () =>
      computeDocumentUiState({
        document,
        currentProjectPath,
        savedDocumentBaseline,
        untitledName: t("documentStatus.untitled"),
      }),
    [currentProjectPath, document, savedDocumentBaseline, t],
  );

  async function triggerCreateSketchAction() {
    await startSketchOnSelectedPlaneOrFace({
      activeSketchPlaneId,
      selectedReferenceId: selectedReference?.reference_id ?? null,
      selectedSketchableFace,
      runAction,
      startSketchOnPlane,
      startSketchOnFace,
    });
  }
  const {
    start,
    createDocument,
    exportDocument,
    exportDocumentStl,
    exportDocumentDxf,
    exportBodyStl,
    importStl,
    importDxf,
    convertMeshToBody,
    detachBodyProjections,
    saveDocument,
    loadDocument,
    projectFaceIntoSketch,
    projectProfileIntoSketch,
    projectEdgeIntoSketch,
    projectVertexIntoSketch,
    projectBodyIntoSketch,
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
    updateOffsetPlane,
    updateAnglePlane,
    startSketchOnPlane,
    startSketchOnFace,
    setSketchTool,
    setSketchLineConstraint,
    clearSketchLineConstraints,
    setSketchEqualLengthConstraint,
    setSketchCoincidentConstraint,
    deleteSketchCoincidentConstraint,
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
    moveSketchEntities,
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
    addSketchLineAngleDimension,
    addSketchCircleRadiusDimension,
    addSketchArcRadiusDimension,
    addSketchPolygonRadiusDimension,
    addSketchRectangle,
    addSketchCircle,
    addSketchPolygon,
    addSketchArc,
    addSketchFillet,
    updateSketchFilletRadius,
    deleteSketchFillet,
    addSketchText,
    updateSketchText,
    deleteSketchText,
    deleteSketchDimension,
    toggleSketchDimensionDriven,
    setSketchLineConstruction,
    addSketchVertexDistanceDimension,
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
    camSetupCreate,
    camSetupUpdate,
    camFaceMillingCreate,
    camOperationUpdate,
    camOperationDelete,
  } = useCadCore();

  const {
    timelineEditVisibleFeatureIds,
    setTimelineEditVisibleFeatureIds,
    beginTimelineEditSession,
    restoreTimelineCursorAfterEdit,
  } = useTimelineEditSession({
    document,
    setTimelineCursor,
  });

  useSketchToolLifecycleEffects({
    activeSketchPlaneId,
    activeSketchTool,
    sketchFilletAction,
    sketchFilletIdsRef,
    sketchTextAction,
    setTimelineEditVisibleFeatureIds,
    setArmedSketchConstraint,
    setMirrorFocusedSlot,
    setSketchFilletAction,
    setSketchTextAction,
  });

  useAppLifecycleEffects({
    status,
    document,
    start,
    createDocument,
    addMessage,
    isDocumentDirty,
    windowDocumentTitle,
    recentProjectsDocumentRef,
    allowAppCloseRef,
    isDocumentDirtyRef,
    originVisibilityManuallyChangedRef,
    setRecentProjectsDocument,
    setCurrentProjectPath,
    setSavedDocumentBaseline,
    setPendingUnsavedAction,
    setHiddenFeatureIds,
    setHiddenCategories,
  });

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
    syncDefaultOriginVisibility({
      documentId: document?.document_id ?? null,
      hasSolidBody,
      previousDocumentIdRef,
      originVisibilityManuallyChangedRef,
      setHiddenCategories,
    });
  }, [document?.document_id, hasSolidBody]);

  const effectiveHiddenFeatureIds = useMemo(
    () =>
      computeEffectiveHiddenFeatureIds({
        document,
        hiddenFeatureIds,
        hiddenCategories,
        timelineEditVisibleFeatureIds,
      }),
    [document, hiddenFeatureIds, hiddenCategories, timelineEditVisibleFeatureIds],
  );

  const hiddenSketchPlaneIds = useMemo(
    () => computeHiddenSketchPlaneIds(document, effectiveHiddenFeatureIds),
    [document, effectiveHiddenFeatureIds],
  );

  const activeToolState = {
    activeSketchPlaneId,
    extrudeAction,
    loftAction,
    revolveAction,
    sweepAction,
    edgeOpAction,
    shellAction,
    holeAction,
    offsetPlaneAction,
    midplaneAction,
    tangentPlaneAction,
    anglePlaneAction,
    constructionAxisAction,
    constructionPointAction,
    helixAction,
    threadAction,
    fastenerAction,
    moveAction,
    pluginAction: activePluginAction,
  };

  const {
    createExtrudeFromSelectedFace,
    createExtrudeFromSelectedProfiles,
    createThinExtrudeFromSelectedEntities,
    getDefaultExtrudeSettings,
    recreateNewProfileExtrudePreview,
    triggerExtrudeAction,
    undoUntilExtrudePreviewRemoved,
  } = useExtrudeFeatureActions({
    document,
    viewport,
    activeToolState,
    extrudeAction,
    selectedSketchProfileIds,
    selectedSketchProfileIdsKey,
    selectedExtrudableFaceId,
    selectedSketchEntityIds,
    extrudeCreateInFlightRef,
    lastExtrudeProfileUpdateRef,
    setExtrudeAction,
    extrudeProfile,
    extrudeFace,
    extrudeOpenEntities,
    updateExtrudeProfiles,
    undo,
    runAction,
    addMessage,
  });

  const { triggerLoftAction, triggerRevolveAction, triggerSweepAction } =
    useProfileFeatureActions({
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
    });

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
  const {
    addMidplaneSource,
    createAnglePlaneFeature,
    createConstructionAxisFeature,
    createConstructionPointFeature,
    createOffsetPlaneFeature,
    createTangentPlaneFeature,
    triggerAnglePlaneAction,
    triggerConstructionAxisAction,
    triggerConstructionPointAction,
    triggerMidplaneAction,
    triggerOffsetPlaneAction,
    triggerTangentPlaneAction,
  } = createConstructionActions({
    activeToolState,
    document,
    planeSourceContext,
    axisSourceContext,
    pendingOffsetRef,
    pendingAngleRef,
    midplaneAction,
    setOffsetPlaneAction,
    setMidplaneAction,
    setTangentPlaneAction,
    setAnglePlaneAction,
    setConstructionAxisAction,
    setConstructionPointAction,
    createOffsetPlane,
    createMidplane,
    createTangentPlane,
    createAnglePlane,
    createConstructionAxis,
    createConstructionPoint,
    runAction,
    addMessage,
  });

  const {
    createHelixFeature,
    createThreadFeature,
    triggerFastenerAction,
    triggerHelixAction,
    triggerThreadAction,
    updateActiveFastenerParameters,
    updateActiveHelixParameters,
    updateActiveThreadParameters,
  } = createThreadedFeatureActions({
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
  });

  const {
    bodyContextActions,
    createMoveFeature,
    hideFeatureSourceSketches,
    triggerMoveAction,
    updateActiveMovePreviewParameters,
  } = createBodyMoveActions({
    activeToolState,
    document,
    moveAction,
    selectedMoveBodyId,
    setMoveAction,
    setHiddenFeatureIds,
    createMove,
    createBodyCopy,
    unlinkBodyCopy,
    exportBodyStl,
    convertMeshToBody,
    detachBodyProjections,
    updateMoveParameters,
    runAction,
    addMessage,
    translate: t,
  });

  const {
    createEdgeOpFeature,
    createHoleFeature,
    createShellFeature,
    triggerEdgeOpAction,
    triggerHoleAction,
    triggerShellAction,
  } = createBodyModifierActions({
    activeToolState,
    document,
    viewport,
    planeSourceContext,
    pendingValueRef,
    activeEdgeIdsRef,
    pendingShellThicknessRef,
    setEdgeOpAction,
    setShellAction,
    setHoleAction,
    createFillet,
    createChamfer,
    createShell,
    createHole,
    runAction,
    addMessage,
  });

  const { updateActiveHoleParameters } =
    createHoleParameterHandlers({
      activeHoleParameters,
      holeAction,
      updateHoleParameters,
      runAction,
    });

  async function cancelActiveTool() {
    // Central Escape/Cancel path for app-level tools. Sketch mode is
    // deliberately excluded here; sketch drafting Escape stays owned by
    // ViewportPanel so Esc never exits an active sketch.
    return cancelActiveToolFromContext({
      actions: {
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
        pluginAction: activePluginAction,
        editingFeatureId,
        materialsPanelOpen,
        sketchTextAction,
      },
      setters: {
        setExtrudeAction,
        setLoftAction,
        setRevolveAction,
        setSweepAction,
        setMoveAction,
        setEdgeOpAction,
        setShellAction,
        setHoleAction,
        setOffsetPlaneAction,
        setAnglePlaneAction,
        setMidplaneAction,
        setTangentPlaneAction,
        setConstructionAxisAction,
        setConstructionPointAction,
        setThreadAction,
        setFastenerAction,
        setHelixAction,
        setPluginAction: (value) => {
          if (!value) {
            setActivePluginAction(null);
          }
        },
        setEditingFeatureId,
        setMaterialsPanelOpen,
        setSketchTextAction,
      },
      activeEdgeIdsRef,
      runAction,
      restoreTimelineCursorAfterEdit,
      undo,
      undoUntilExtrudePreviewRemoved,
      setSketchTool,
      deleteSketchText,
      updateExtrudeDepth,
      updateExtrudeMode,
      updateExtrudeTargetBody,
      updateExtrudeParameters,
      updateLoftProfiles,
      updateLoftRuled,
      updateRevolveProfile,
      updateRevolveAxis,
      updateRevolveAngle,
      updateSweepProfile,
      updateSweepPath,
      updateMoveParameters,
      updateThreadParameters,
      updateFastenerParameters,
      deleteFeature,
    });
  }

  const {
    clearArmedSketchConstraint,
    finishActiveSketch,
    handleSketchConstraintLinePick,
    handleSketchConstraintVertexPick,
    setActiveSketchTool,
  } = createSketchToolActions({
    armedSketchConstraint,
    setArmedSketchConstraint,
    runAction,
    finishSketch,
    restoreTimelineCursorAfterEdit,
    setSketchTool,
    selectSketchEntity,
    selectSketchPoint,
    setSketchPointFixed,
    setSketchLineConstraint,
    clearSketchLineConstraints,
    setSketchEqualLengthConstraint,
    setSketchParallelConstraint,
    setSketchPerpendicularConstraint,
    setSketchCoincidentConstraint,
    addMessage,
  });

  const {
    createRecentProjectFolder,
    deleteRecentProject,
    deleteRecentProjectFolder,
    moveRecentProject,
    recordRecentProject,
    renameRecentProjectEntry,
    renameRecentProjectFolder,
    requestOpenRecentProject,
  } = createRecentProjectHandlers({
    recentProjectsStore,
    currentProjectPath,
    setCurrentProjectPath,
    setSavedDocumentBaseline,
    addMessage,
    translate: t,
    requestUnsavedGate,
  });

  const {
    discardThenContinuePendingAction,
    executePendingAction,
    saveCurrentDocument,
    saveThenContinuePendingAction,
  } = createDocumentLifecycleActions({
    document,
    currentProjectPath,
    pendingUnsavedAction,
    translate: t,
    createDocument,
    loadDocument,
    saveDocument,
    recordRecentProject,
    setCurrentProjectPath,
    setSavedDocumentBaseline,
    setHiddenFeatureIds,
    setHiddenCategories,
    setSidebarTab,
    setPendingUnsavedAction,
    originVisibilityManuallyChangedRef,
    snapshotCaptureRef,
    allowAppCloseRef,
    runAction,
    addMessage,
  });

  useAppHotkeys({
    hotkeys: config.hotkeys,
    actions: {
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
      pluginAction: activePluginAction,
      editingFeatureId,
      materialsPanelOpen,
      sketchTextAction,
    },
    state: {
      activeSketchPlaneId,
      activeSketchTool,
      canCreateSketch: Boolean(selectedReference || selectedSketchableFace),
      canUndo: Boolean(session?.can_undo),
      canRedo: Boolean(session?.can_redo),
      document,
    },
    callbacks: {
      cancelActiveTool,
      runAction,
      saveCurrentDocument,
      clearSelection,
      undo,
      redo,
      triggerExtrudeAction,
      triggerEdgeOpAction,
      triggerCreateSketchAction,
      setSketchTool,
    },
  });

  function requestUnsavedGate(action: PendingUnsavedAction) {
    if (isDocumentDirty) {
      setPendingUnsavedAction(action);
      return;
    }
    void executePendingAction(action);
  }

  async function runAction(action: () => Promise<void>) {
    try {
      await action();
    } catch (error) {
      addMessage(`action error: ${String(error)}`);
    }
  }

  async function runActionOrThrow(action: () => Promise<void>) {
    try {
      await action();
    } catch (error) {
      addMessage(`action error: ${String(error)}`);
      throw error;
    }
  }

  const {
    exportToSlicer,
    handleWorkspaceDropdownOpenChange,
    showCadView,
    showCamView,
    showDrawingView,
    showSlicerView,
  } = useSlicerWorkspaceActions({
    workspaceView,
    hasExportableBody,
    hasOrcaEmbedSession,
    orcaSlicer: config.orcaSlicer,
    slicerViewportRef,
    messages: {
      disabled: t("workspace.slicerDisabled"),
      opening: t("workspace.openingSlicer"),
      binaryMissing: t("workspace.slicerBinaryMissing"),
      containerUnavailable: t("workspace.slicerContainerUnavailable"),
      embedFailed: (error) => t("workspace.slicerEmbedFailed", { error }),
      noExportableBody: t("workspace.slicerNoExportableBody"),
      exporting: t("workspace.exportingToSlicer"),
    },
    setWorkspaceView,
    setSlicerStatus,
    setHasOrcaEmbedSession,
    exportDocumentStl,
    addMessage,
  });

  function confirmAndDeleteFeature(featureId: string) {
    confirmAndDeleteFeatureFromContext({
      document,
      featureId,
      activeSketchDeleteBlockedMessage: t("timeline.activeSketchDeleteBlocked"),
      addMessage,
      runAction,
      deleteFeature,
      clearEditingFeature: (deletedFeatureId) => {
        setEditingFeatureId((current) =>
          current === deletedFeatureId ? null : current,
        );
      },
    });
  }

  function deleteSketchSelectionNow(selection: SketchDeleteSelection) {
    deleteSketchSelectionFromContext({
      selection,
      runAction,
      deleteSketchSelection,
    });
  }

  function confirmAndDeleteSketchSelection(selection?: SketchDeleteSelection) {
    confirmAndDeleteSketchSelectionFromContext({
      document,
      activeSketchFeature,
      selection,
      setPendingSketchDeleteConfirmation,
      runAction,
      deleteSketchSelection,
    });
  }

  const {
    canExtrudeFromSelection,
    canStartTimelineFeatureEdit,
    canStartReferencePlaneAction,
    canStartSolidFeatureAction,
    canStartConstructionReferenceAction,
    canStartHelixRibbonAction,
  } = computeFeatureActionAvailability({
    activeSketchPlaneId,
    extrudeAction,
    loftAction,
    revolveAction,
    sweepAction,
    edgeOpAction,
    shellAction,
    holeAction,
    offsetPlaneAction,
    midplaneAction,
    tangentPlaneAction,
    anglePlaneAction,
    constructionAxisAction,
    constructionPointAction,
    helixAction,
    threadAction,
    fastenerAction,
    moveAction,
    pluginAction: activePluginAction,
  });

  const handleTimelineFeatureEdit = createTimelineFeatureEditHandler({
    document,
    viewport,
    canStartTimelineFeatureEdit,
    beginTimelineEditSession,
    runAction,
    reenterSketch,
    setEditingFeatureId,
    setExtrudeAction,
    setLoftAction,
    setRevolveAction,
    setSweepAction,
    setThreadAction,
    setFastenerAction,
    setMoveAction,
    lastLoftProfileUpdateRef,
    lastRevolveInputsRef,
    lastSweepInputsRef,
  });

  const pluginMenuItems = pluginHost.menuItems.map((item) => ({
    ...item,
    label: t(item.labelKey),
  }));

  async function handlePluginCommand(pluginId: string, command: string) {
    if (isToolStartBlocked(activeToolState)) {
      addMessage(t("plugins.actionBlocked"));
      return;
    }

    const pluginEntry = pluginHost.plugins.find(
      (entry) => entry.plugin.manifest.id === pluginId,
    );
    if (!pluginEntry?.enabled) {
      addMessage(t("plugins.disabled"));
      return;
    }
    if (!pluginEntry.runtime.handleCommand) {
      return;
    }
    await runAction(async () => {
      const result = await pluginEntry.runtime.handleCommand?.(command);
      if (result) {
        setActivePluginAction({
          ...result,
          pluginId,
        });
      }
    });
  }

  const activePluginPanel = activePluginAction
    ? pluginHost.plugins
        .find((entry) => entry.plugin.manifest.id === activePluginAction.pluginId)
        ?.runtime.renderAction?.({
          disabled: status !== "connected",
          action: activePluginAction,
          onClose: () => setActivePluginAction(null),
        }) ?? null
    : null;

  return (
    <main className="cad-shell h-screen overflow-x-hidden">
      {/* grid-cols-1 constrains the implicit column to the window width:
          without it the single auto column sizes to the widest child
          (the header toolbar rows), and once that exceeds the window the
          whole grid — canvas included — stretches past the right edge. */}
      <div className="grid h-full min-h-0 grid-cols-1 grid-rows-[auto_minmax(0,1fr)_auto]">
        <AppTopBar
          workspaceView={workspaceView}
          canOpenSlicerView={
            isSlicerConfigured &&
            (config.orcaSlicer.integrationMode === "web" || !IS_MACOS)
          }
          canExportToSlicer={canExportToSlicer}
          showCadView={showCadView}
          showCamView={showCamView}
          showDrawingView={showDrawingView}
          showSlicerView={showSlicerView}
          exportToSlicer={exportToSlicer}
          status={status}
          canUndo={session?.can_undo ?? false}
          canRedo={session?.can_redo ?? false}
          activeSketchPlaneId={activeSketchPlaneId}
          activeSketchTool={activeSketchTool}
          selectedReferenceId={selectedReference?.reference_id ?? null}
          selectedFaceId={selectedSketchableFace?.face_id ?? null}
          armedSketchConstraint={armedSketchConstraint}
          isMirrorToolOpen={isMirrorToolOpen}
          arcToolMode={arcToolMode}
          setArcToolMode={setArcToolMode}
          rectangleToolMode={rectangleToolMode}
          setRectangleToolMode={setRectangleToolMode}
          circleToolMode={circleToolMode}
          setCircleToolMode={setCircleToolMode}
          polygonToolMode={polygonToolMode}
          setPolygonToolMode={setPolygonToolMode}
          dimensionToolMode={dimensionToolMode}
          onSetDimensionToolMode={setDimensionToolMode}
          bodyProjectionMode={bodyProjectionMode}
          setBodyProjectionMode={setBodyProjectionMode}
          runAction={runAction}
          start={start}
          startMirrorPreview={startMirrorPreview}
          setMirrorFocusedSlot={setMirrorFocusedSlot}
          clearArmedSketchConstraint={clearArmedSketchConstraint}
          requestUnsavedGate={requestUnsavedGate}
          translate={t}
          document={document}
          viewport={viewport}
          addMessage={addMessage}
          exportDocument={exportDocument}
          exportDocumentDxf={exportDocumentDxf}
          importStl={importStl}
          importDxf={importDxf}
          saveCurrentDocument={saveCurrentDocument}
          undo={undo}
          redo={redo}
          pluginMenuItems={pluginMenuItems}
          onPluginCommand={handlePluginCommand}
          logCount={logs.length}
          errorLogCount={errorLogCount}
          setIsLogsOpen={setIsLogsOpen}
          setIsSettingsOpen={setIsSettingsOpen}
          showAiAssistant={isAiAssistantAvailable}
          isAiPanelOpen={isAiPanelOpen}
          setIsAiPanelOpen={setIsAiPanelOpen}
          addBoxFeature={addBoxFeature}
          addCylinderFeature={addCylinderFeature}
          canExtrudeFromSelection={canExtrudeFromSelection}
          triggerExtrudeAction={triggerExtrudeAction}
          canStartSolidFeatureAction={canStartSolidFeatureAction}
          triggerLoftAction={triggerLoftAction}
          triggerRevolveAction={triggerRevolveAction}
          triggerSweepAction={triggerSweepAction}
          triggerHoleAction={triggerHoleAction}
          triggerThreadAction={triggerThreadAction}
          triggerFastenerAction={triggerFastenerAction}
          triggerEdgeOpAction={triggerEdgeOpAction}
          triggerMoveAction={triggerMoveAction}
          triggerShellAction={triggerShellAction}
          canStartReferencePlaneAction={canStartReferencePlaneAction}
          triggerOffsetPlaneAction={triggerOffsetPlaneAction}
          triggerMidplaneAction={triggerMidplaneAction}
          triggerTangentPlaneAction={triggerTangentPlaneAction}
          triggerAnglePlaneAction={triggerAnglePlaneAction}
          canStartConstructionReferenceAction={
            canStartConstructionReferenceAction
          }
          triggerConstructionAxisAction={triggerConstructionAxisAction}
          triggerConstructionPointAction={triggerConstructionPointAction}
          canStartHelixRibbonAction={canStartHelixRibbonAction}
          triggerHelixAction={triggerHelixAction}
          triggerCreateSketchAction={triggerCreateSketchAction}
          finishActiveSketch={finishActiveSketch}
          setActiveSketchTool={setActiveSketchTool}
          setArmedSketchConstraint={setArmedSketchConstraint}
          setSketchTool={setSketchTool}
          handleWorkspaceDropdownOpenChange={
            handleWorkspaceDropdownOpenChange
          }
          parametersPanelOpen={parametersPanelOpen}
          setParametersPanelOpen={setParametersPanelOpen}
          filterPanelOpen={filterPanelOpen}
          setFilterPanelOpen={setFilterPanelOpen}
          materialsPanelOpen={materialsPanelOpen}
          setMaterialsPanelOpen={setMaterialsPanelOpen}
          viewPanelOpen={viewPanelOpen}
          setViewPanelOpen={setViewPanelOpen}
          updateSelectionFilter={updateSelectionFilter}
          activeCamOperation={activeCamOperation}
          setActiveCamOperation={setActiveCamOperation}
          setIsCamSetupPanelOpen={setIsCamSetupPanelOpen}
          camFaceMillingCreate={camFaceMillingCreate}
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
            <SlicerWorkspace
              orcaSlicer={config.orcaSlicer}
              slicerViewportRef={slicerViewportRef}
              hasOrcaEmbedSession={hasOrcaEmbedSession}
              slicerStatus={slicerStatus}
              waitingMessage={t("workspace.slicerWaiting")}
              openInBrowserLabel={t("workspace.openInBrowser")}
              addMessage={addMessage}
            />
          ) : (
            <>
              <AppSidebar
                activeProjectPath={currentProjectPath}
                bodyContextActions={bodyContextActions}
                camOperationDelete={camOperationDelete}
                camOperations={camOperations}
                confirmAndDeleteFeature={confirmAndDeleteFeature}
                createRecentProjectFolder={createRecentProjectFolder}
                deleteRecentProject={deleteRecentProject}
                deleteRecentProjectFolder={deleteRecentProjectFolder}
                document={document}
                hiddenCategories={hiddenCategories}
                hiddenFeatureIds={hiddenFeatureIds}
                hierarchyWidth={hierarchyWidth}
                isHierarchyCollapsed={isHierarchyCollapsed}
                markOriginVisibilityChanged={() => {
                  originVisibilityManuallyChangedRef.current = true;
                }}
                moveRecentProject={moveRecentProject}
                recentProjectsDocument={recentProjectsDocument}
                reenterSketch={reenterSketch}
                renameFeature={renameFeature}
                renameRecentProjectEntry={renameRecentProjectEntry}
                renameRecentProjectFolder={renameRecentProjectFolder}
                requestOpenRecentProject={requestOpenRecentProject}
                requestUnsavedGate={requestUnsavedGate}
                runAction={runAction}
                selectedCamOperationId={selectedCamOperationId}
                selectFeature={selectFeature}
                selectReference={selectReference}
                setFeatureSuppressed={setFeatureSuppressed}
                setHiddenCategories={setHiddenCategories}
                setHiddenFeatureIds={setHiddenFeatureIds}
                setHierarchyWidth={setHierarchyWidth}
                setIsHierarchyCollapsed={setIsHierarchyCollapsed}
                setSelectedCamOperationId={setSelectedCamOperationId}
                setSidebarTab={setSidebarTab}
                sidebarTab={sidebarTab}
                workspaceView={workspaceView}
              />

          <section className="relative min-h-0 min-w-0 flex-1">
            <ViewportPanel
              status={status}
              document={document}
              viewport={viewport}
              showStock={showStock && workspaceView === "cam"}
              wcsOrientation={wcsOrientation}
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
                await updateActiveMovePreviewParameters(parameters);
              }}
              {...bodyContextActions}
              inactiveSketchEntityPickEnabled={
                revolveAction !== null ||
                sweepAction !== null ||
                constructionAxisAction !== null ||
                helixAction !== null ||
                threadAction !== null
              }
              onPickInactiveSketchLine={async (lineId) => {
                await handleInactiveSketchLineSelection({
                  lineId,
                  threadAction,
                  helixAction,
                  constructionAxisAction,
                  revolveAction,
                  sweepAction,
                  setThreadAction,
                  setRevolveAction,
                  setSweepAction,
                  createThreadFeature,
                  createHelixFeature,
                  createConstructionAxisFeature,
                });
              }}
              onSnapshotCaptureReady={(capture) => {
                snapshotCaptureRef.current = capture;
              }}
              onSelectPrimitive={async (primitiveId) => {
                await handleViewportPrimitiveSelection({
                  primitiveId,
                  viewport,
                  moveAction,
                  threadAction,
                  setThreadAction,
                  createMoveFeature,
                  createThreadFeature,
                  selectFeature,
                  runAction,
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
                    sourceSummary: selectionSources.describePlaneSource(
                      planeSourceContext,
                      referenceId,
                    ),
                    initialAngle: pendingAngleRef.current,
                  });
                  return;
                }
                await runAction(async () => {
                  await selectReference(referenceId);
                });
              }}
              onSelectFace={async (faceId) => {
                await handleViewportFaceSelection({
                  faceId,
                  viewport,
                  moveAction,
                  threadAction,
                  holeAction,
                  shellAction,
                  offsetPlaneAction,
                  midplaneAction,
                  anglePlaneAction,
                  tangentPlaneAction,
                  activeSketchPlaneId,
                  activeSketchTool,
                  extrudeAction,
                  selectedSketchProfileIds,
                  pendingOffsetRef,
                  pendingShellThicknessRef,
                  pendingAngleRef,
                  setThreadAction,
                  setAnglePlaneAction,
                  setTangentPlaneAction,
                  createMoveFeature,
                  createThreadFeature,
                  createHoleFeature,
                  createShellFeature,
                  createOffsetPlaneFeature,
                  addMidplaneSource,
                  createTangentPlaneFeature,
                  projectFaceIntoSketch,
                  projectBodyIntoSketch,
                  document,
                  bodyProjectionMode,
                  createExtrudeFromSelectedFace,
                  selectFace,
                  getDefaultExtrudeSettings,
                  describePlaneSource: (sourceId) =>
                    selectionSources.describePlaneSource(
                      planeSourceContext,
                      sourceId,
                    ),
                  addMessage,
                  runAction,
                });
              }}
              onSelectEdge={async (edgeId, additive) => {
                await handleViewportEdgeSelection({
                  edgeId,
                  additive,
                  threadAction,
                  helixAction,
                  constructionAxisAction,
                  activeSketchPlaneId,
                  activeSketchTool,
                  anglePlaneAction,
                  edgeOpAction,
                  pendingAngleRef,
                  pendingValueRef,
                  activeEdgeIdsRef,
                  setThreadAction,
                  setEdgeOpAction,
                  createThreadFeature,
                  createHelixFeature,
                  createConstructionAxisFeature,
                  projectEdgeIntoSketch,
                  createAnglePlaneFeature,
                  createEdgeOpFeature,
                  updateFilletEdges,
                  updateChamferEdges,
                  selectEdge,
                  addMessage,
                  runAction,
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
              onSetSketchMidpointAnchor={async (vertexId, hostLineId) => {
                await runAction(async () => {
                  await setSketchMidpointAnchor(vertexId, hostLineId);
                });
              }}
              onSetSketchPointLineAnchor={async (vertexId, hostLineId, t) => {
                await runAction(async () => {
                  await setSketchPointLineAnchor(vertexId, hostLineId, t);
                });
              }}
              onAddSketchAngleDimension={async (firstLineId, secondLineId, value?) => {
                await runActionOrThrow(async () => {
                  await addSketchAngleDimension(firstLineId, secondLineId, value);
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
              onAddSketchLineAngleDimension={async (lineId) => {
                await runAction(async () => {
                  await addSketchLineAngleDimension(lineId);
                });
              }}
              onAddSketchCircleRadiusDimension={async (circleId) => {
                await runAction(async () => {
                  await addSketchCircleRadiusDimension(circleId);
                });
              }}
              onAddSketchArcRadiusDimension={async (arcId) => {
                await runAction(async () => {
                  await addSketchArcRadiusDimension(arcId);
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
              onSetSketchParallelConstraint={async (lineId, otherLineId) => {
                await runAction(async () => {
                  await setSketchParallelConstraint(lineId, otherLineId);
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
              dimensionToolMode={dimensionToolMode}
              onSetDimensionToolMode={setDimensionToolMode}
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
              onAddSketchText={async (anchorX, anchorY) => {
                // Same fire-and-forget IPC trick as the fillet flow:
                // subscribe to the next document update that adds a
                // text on the active sketch so we can pick up the real
                // text id and bind the panel to it.
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
                    const nextTexts =
                      nextSketch?.sketch_parameters?.texts ?? [];
                    const prevTexts =
                      prevSketch?.sketch_parameters?.texts ?? [];
                    return nextTexts.length > prevTexts.length;
                  },
                );

                await runAction(async () => {
                  await addSketchText({ anchorX, anchorY });
                });

                try {
                  const nextDocument = await documentPromise;
                  const nextSketch = nextDocument.feature_history.find(
                    (entry) =>
                      entry.feature_id ===
                      nextDocument.active_sketch_feature_id,
                  );
                  const texts = nextSketch?.sketch_parameters?.texts ?? [];
                  const newText = texts[texts.length - 1];
                  if (!newText) {
                    return;
                  }
                  // Flip pending → active, bound to the new text.
                  setSketchTextAction({
                    phase: "active",
                    textId: newText.text_id,
                    params: newText,
                  });
                } catch {
                  // Document watcher timed out — leave the session
                  // pending. The next click can recover.
                }
              }}
              sketchTextPathPicking={
                sketchTextAction?.phase === "active" &&
                (sketchTextAction.pathPicking ?? false)
              }
              onPickSketchTextPath={(entityId) => {
                const action = sketchTextActionRef.current;
                if (!action || action.phase !== "active") {
                  return;
                }
                // Only user line/arc entities can be paths — generated
                // glyph segments can't. Invalid picks keep the picker
                // armed so the user can click something else.
                const snapshot = useCadCoreStore.getState().document;
                const featureId = snapshot?.active_sketch_feature_id;
                const feature = featureId
                  ? snapshot?.feature_history.find(
                      (entry) => entry.feature_id === featureId,
                    )
                  : undefined;
                const sketch = feature?.sketch_parameters;
                const line = sketch?.lines.find(
                  (entry) => entry.line_id === entityId && !entry.generated_by,
                );
                const arc = sketch?.arcs.find(
                  (entry) => entry.arc_id === entityId && !entry.generated_by,
                );
                if (!line && !arc) {
                  return;
                }
                setSketchTextAction((prev) =>
                  prev && prev.phase === "active"
                    ? {
                        ...prev,
                        params: { ...prev.params, path_entity_id: entityId },
                        pathPicking: false,
                      }
                    : prev,
                );
                void runAction(async () => {
                  await updateSketchText(action.textId, {
                    pathEntityId: entityId,
                  });
                });
              }}
              onPickSketchText={(textId) => {
                // Select-mode glyph pick: the clicked sketch entity is
                // a text glyph segment (`generated_by: "text:<id>"`).
                // Look up the live text entry and open the editor
                // bound to it.
                const snapshot = useCadCoreStore.getState().document;
                const featureId = snapshot?.active_sketch_feature_id;
                const feature = featureId
                  ? snapshot?.feature_history.find(
                      (entry) => entry.feature_id === featureId,
                    )
                  : undefined;
                const entry = feature?.sketch_parameters?.texts.find(
                  (text) => text.text_id === textId,
                );
                if (!entry) {
                  return;
                }
                void (async () => {
                  // The lifecycle effect clears the text action while
                  // the tool is still "select", so wait for the tool
                  // change to land before binding the panel to the
                  // existing text.
                  const toolPromise = awaitDocumentChange(
                    (next) => next.active_sketch_tool === "text",
                  );
                  await runAction(async () => {
                    await setSketchTool("text");
                  });
                  try {
                    await toolPromise;
                  } catch {
                    // Tool change timed out — open the editor anyway.
                  }
                  setSketchTextAction({
                    phase: "active",
                    textId: entry.text_id,
                    params: entry,
                  });
                })();
              }}
              onSelectSketchEntity={async (entityId, additive) => {
                await handleSketchEntitySelection({
                  entityId,
                  additive,
                  threadAction,
                  helixAction,
                  constructionAxisAction,
                  revolveAction,
                  sweepAction,
                  anglePlaneAction,
                  sketchLineLabelById,
                  pendingAngleRef,
                  setThreadAction,
                  setRevolveAction,
                  setSweepAction,
                  createThreadFeature,
                  createHelixFeature,
                  createConstructionAxisFeature,
                  createAnglePlaneFeature,
                  handleSketchConstraintLinePick,
                  runAction,
                });
              }}
              onBatchSelectEntities={async (entityIds, additive) => {
                await runAction(async () => {
                  await batchSelectSketchEntities(entityIds, additive);
                });
              }}
              onPickSketchPoint={async (vertexId, kind, additive) => {
                if (constructionPointAction) {
                  await createConstructionPointFeature(vertexId);
                  return;
                }
                await runAction(async () => {
                  await handleSketchConstraintVertexPick(
                    vertexId,
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

                  if (kind === "coincident") {
                    await deleteSketchCoincidentConstraint(entityId);
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

                  if (kind === "mirror") {
                    await deleteSketchCoincidentConstraint(entityId);
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
                await handleViewportSketchProfileSelection({
                  profileId,
                  additive,
                  offsetPlaneAction,
                  midplaneAction,
                  anglePlaneAction,
                  activeSketchPlaneId,
                  activeSketchTool,
                  loftAction,
                  revolveAction,
                  sweepAction,
                  extrudeAction,
                  pendingOffsetRef,
                  pendingAngleRef,
                  setAnglePlaneAction,
                  setLoftAction,
                  setRevolveAction,
                  setSweepAction,
                  createOffsetPlaneFeature,
                  addMidplaneSource,
                  projectProfileIntoSketch,
                  selectSketchProfile,
                  describePlaneSource: (sourceId) =>
                    selectionSources.describePlaneSource(
                      planeSourceContext,
                      sourceId,
                    ),
                  addMessage,
                  runAction,
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
              onToggleSketchDimensionDriven={async (dimensionId) => {
                await runAction(async () => {
                  await toggleSketchDimensionDriven(dimensionId);
                });
              }}
              onSetSketchLineConstruction={async (
                lineId,
                isConstruction,
              ) => {
                await runAction(async () => {
                  await setSketchLineConstruction(lineId, isConstruction);
                });
              }}
              onAddSketchVertexDistanceDimension={async (
                vertexAId,
                vertexBId,
                axis?,
              ) => {
                await runAction(async () => {
                  await addSketchVertexDistanceDimension(
                    vertexAId,
                    vertexBId,
                    axis,
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
              onFinishSketch={finishActiveSketch}
              onSetSketchTool={setActiveSketchTool}
              onUpdateSketchPoint={async (vertexId, x, y) => {
                await runAction(async () => {
                  await updateSketchPoint(vertexId, x, y);
                });
              }}
              onMoveSketchEntities={async (params) => {
                await runAction(async () => {
                  await moveSketchEntities(params);
                });
              }}
              hiddenFeatureIds={effectiveHiddenFeatureIds}
              hiddenSketchPlaneIds={hiddenSketchPlaneIds}
              hideReferences={hiddenCategories.has("origin")}
            />

            <div className="pointer-events-none absolute bottom-4 right-4 top-4 z-10 flex min-h-0 w-[340px] flex-col gap-3">
              <ActiveMaterialsPanel
                isOpen={materialsPanelOpen}
                selectedBodyId={selectedMaterialBodyId}
                selectedFaceId={selectedMaterialFace?.face_id ?? null}
                runAction={runAction}
                setBodyColor={setBodyColor}
                setFaceColor={setFaceColor}
                clearBodyColor={clearBodyColor}
                clearFaceColor={clearFaceColor}
                clearAppearanceOverrides={clearAppearanceOverrides}
              />
              <ActiveViewPanel isOpen={viewPanelOpen} />
              <PendingExtrudePreview
                disabled={status !== "connected"}
                document={document}
                extrudeAction={extrudeAction}
                selectedExtrudableFaceId={selectedExtrudableFaceId}
                selectedSketchEntityIds={selectedSketchEntityIds}
                selectedSketchProfileIds={selectedSketchProfileIds}
                viewport={viewport}
                createExtrudeFromSelectedFace={createExtrudeFromSelectedFace}
                createExtrudeFromSelectedProfiles={
                  createExtrudeFromSelectedProfiles
                }
                createThinExtrudeFromSelectedEntities={
                  createThinExtrudeFromSelectedEntities
                }
                getDefaultExtrudeSettings={getDefaultExtrudeSettings}
                setExtrudeAction={setExtrudeAction}
                onCancelActiveTool={cancelActiveTool}
              />
              <ActiveExtrudePreview
                extrudeAction={extrudeAction}
                document={document}
                viewport={viewport}
                disabled={status !== "connected"}
                runAction={runAction}
                updateExtrudeDepth={updateExtrudeDepth}
                updateExtrudeMode={updateExtrudeMode}
                updateExtrudeTargetBody={updateExtrudeTargetBody}
                updateExtrudeParameters={updateExtrudeParameters}
                recreateNewProfileExtrudePreview={
                  recreateNewProfileExtrudePreview
                }
                setExtrudeAction={setExtrudeAction}
                setHiddenFeatureIds={setHiddenFeatureIds}
                clearSelection={clearSelection}
                restoreTimelineCursorAfterEdit={restoreTimelineCursorAfterEdit}
                cancelActiveTool={cancelActiveTool}
              />
              <ActiveProfileFeaturePanels
                disabled={status !== "connected"}
                loftAction={loftAction}
                revolveAction={revolveAction}
                sweepAction={sweepAction}
                selectedSketchProfileIds={selectedSketchProfileIds}
                sketchLineLabelById={sketchLineLabelById}
                sketchPathEntityLabelById={sketchPathEntityLabelById}
                sketchProfileLabelById={sketchProfileLabelById}
                hideFeatureSourceSketches={hideFeatureSourceSketches}
                restoreTimelineCursorAfterEdit={restoreTimelineCursorAfterEdit}
                runAction={runAction}
                selectSketchProfile={selectSketchProfile}
                setLoftAction={setLoftAction}
                setRevolveAction={setRevolveAction}
                setSweepAction={setSweepAction}
                updateLoftRuled={updateLoftRuled}
                updateRevolveAngle={updateRevolveAngle}
                onCancelActiveTool={cancelActiveTool}
              />
              <PrimitiveFeatureEditPanel
                disabled={status !== "connected"}
                document={document}
                editingFeatureId={editingFeatureId}
                restoreTimelineCursorAfterEdit={restoreTimelineCursorAfterEdit}
                runAction={runAction}
                setEditingFeatureId={setEditingFeatureId}
                updateBoxFeature={updateBoxFeature}
                updateCylinderFeature={updateCylinderFeature}
              />
              {activePluginPanel}
              <ActiveMirrorPanel
                disabled={status !== "connected"}
                focusedSlot={mirrorFocusedSlot}
                pendingMirror={pendingMirror}
                persistent={mirrorPersistent}
                runAction={runAction}
                setFocusedSlot={setMirrorFocusedSlot}
                setPersistent={setMirrorPersistent}
                cancelMirrorPreview={cancelMirrorPreview}
                commitMirrorPreview={commitMirrorPreview}
                updateMirrorPreviewAxis={updateMirrorPreviewAxis}
                updateMirrorPreviewObjects={updateMirrorPreviewObjects}
              />
              <ActiveBodyOperationPanels
                activeMoveParameters={activeMoveParameters}
                activeEdgeIdsRef={activeEdgeIdsRef}
                anglePlaneAction={anglePlaneAction}
                disabled={status !== "connected"}
                edgeOpAction={edgeOpAction}
                moveAction={moveAction}
                moveBodyLabel={
                  moveAction?.phase === "active"
                    ? (viewport?.bodies.find(
                        (body) => body.id === moveAction.targetBodyId,
                      )?.label ?? null)
                    : null
                }
                offsetPlaneAction={offsetPlaneAction}
                pendingAngleRef={pendingAngleRef}
                pendingOffsetRef={pendingOffsetRef}
                pendingShellThicknessRef={pendingShellThicknessRef}
                pendingValueRef={pendingValueRef}
                shellAction={shellAction}
                clearSelection={clearSelection}
                confirmChamfer={confirmChamfer}
                confirmFillet={confirmFillet}
                confirmMove={confirmMove}
                confirmShell={confirmShell}
                restoreTimelineCursorAfterEdit={restoreTimelineCursorAfterEdit}
                runAction={runAction}
                setAnglePlaneAction={setAnglePlaneAction}
                setEdgeOpAction={setEdgeOpAction}
                setMoveAction={setMoveAction}
                setOffsetPlaneAction={setOffsetPlaneAction}
                setShellAction={setShellAction}
                translate={t}
                updateActiveMovePreviewParameters={updateActiveMovePreviewParameters}
                updateAnglePlane={updateAnglePlane}
                updateChamferDistance={updateChamferDistance}
                updateFilletRadius={updateFilletRadius}
                updateOffsetPlane={updateOffsetPlane}
                updateShellThickness={updateShellThickness}
                onCancelActiveTool={cancelActiveTool}
              />
              {holeAction ? (
                <ActiveHolePanel
                  action={holeAction}
                  disabled={status !== "connected"}
                  parameters={activeHoleParameters}
                  standards={activeHoleStandards}
                  onCancel={() => {
                    void cancelActiveTool();
                  }}
                  onConfirm={() => {
                    void runAction(async () => {
                      if (holeAction.phase === "active") {
                        await confirmHole(holeAction.featureId);
                        await clearSelection();
                      }
                      setHoleAction(null);
                    });
                  }}
                  onUpdateParameters={updateActiveHoleParameters}
                />
              ) : null}
              <ConstructionPendingPanels
                constructionAxisAction={constructionAxisAction}
                constructionPointAction={constructionPointAction}
                disabled={status !== "connected"}
                midplaneAction={midplaneAction}
                tangentPlaneAction={tangentPlaneAction}
                onCancel={() => {
                  void cancelActiveTool();
                }}
              />
              <ActiveThreadedFeaturePanels
                activeFastenerParameters={activeFastenerParameters}
                activeFastenerStandards={activeFastenerStandards}
                activeHelixParameters={activeHelixParameters}
                activeThreadParameters={activeThreadParameters}
                activeThreadStandards={activeThreadStandards}
                axisSourceContext={axisSourceContext}
                disabled={status !== "connected"}
                fastenerAction={fastenerAction}
                helixAction={helixAction}
                threadAction={threadAction}
                threadTargetContext={threadTargetContext}
                cancelActiveTool={cancelActiveTool}
                confirmThread={confirmThread}
                restoreTimelineCursorAfterEdit={restoreTimelineCursorAfterEdit}
                runAction={runAction}
                setFastenerAction={setFastenerAction}
                setHelixAction={setHelixAction}
                setThreadAction={setThreadAction}
                updateActiveFastenerParameters={updateActiveFastenerParameters}
                updateActiveHelixParameters={updateActiveHelixParameters}
                updateActiveThreadParameters={updateActiveThreadParameters}
              />
              {sketchFilletAction ? (
                <ActiveSketchFilletPanel
                  action={sketchFilletAction}
                  disabled={status !== "connected"}
                  sketchFilletIdsRef={sketchFilletIdsRef}
                  setSketchFilletAction={setSketchFilletAction}
                  runAction={runAction}
                  setSketchTool={setSketchTool}
                  updateSketchFilletRadius={updateSketchFilletRadius}
                  deleteSketchFillet={deleteSketchFillet}
                />
              ) : null}
              {sketchTextAction ? (
                <ActiveSketchTextPanel
                  action={sketchTextAction}
                  disabled={status !== "connected"}
                  setSketchTextAction={setSketchTextAction}
                  runAction={runAction}
                  setSketchTool={setSketchTool}
                  updateSketchText={updateSketchText}
                  deleteSketchText={deleteSketchText}
                  pathPicking={
                    sketchTextAction.phase === "active" &&
                    (sketchTextAction.pathPicking ?? false)
                  }
                  onArmPathPick={() => {
                    setSketchTextAction((prev) =>
                      prev && prev.phase === "active"
                        ? { ...prev, pathPicking: true }
                        : prev,
                    );
                  }}
                  onClearPath={() => {
                    const action = sketchTextActionRef.current;
                    if (!action || action.phase !== "active") {
                      return;
                    }
                    setSketchTextAction((prev) =>
                      prev && prev.phase === "active"
                        ? {
                            ...prev,
                            params: { ...prev.params, path_entity_id: null },
                            pathPicking: false,
                          }
                        : prev,
                    );
                    void runAction(async () => {
                      await updateSketchText(action.textId, {
                        pathEntityId: null,
                      });
                    });
                  }}
                />
              ) : null}
              <CamFloatingPanels
                document={document}
                viewport={viewport}
                disabled={status !== "connected"}
                isSetupPanelOpen={isCamSetupPanelOpen}
                selectedOperationId={selectedCamOperationId}
                showStock={showStock}
                wcsOrientation={wcsOrientation}
                setShowStock={setShowStock}
                setWcsOrientation={setWcsOrientation}
                setSetupPanelOpen={setIsCamSetupPanelOpen}
                setSelectedOperationId={setSelectedCamOperationId}
                runAction={runAction}
                camSetupUpdate={camSetupUpdate}
                camOperationUpdate={camOperationUpdate}
                camOperationDelete={camOperationDelete}
              />
              {pendingSketchDeleteConfirmation ? (
                <SketchDeleteConfirmationPanel
                  confirmation={pendingSketchDeleteConfirmation}
                  onConfirm={(selection) => {
                    setPendingSketchDeleteConfirmation(null);
                    deleteSketchSelectionNow(selection);
                  }}
                  onCancel={() => setPendingSketchDeleteConfirmation(null)}
                />
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
          onEditFeature={handleTimelineFeatureEdit}
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
        <UnsavedDocumentDialog
          action={pendingUnsavedAction}
          currentDocumentName={currentDocumentName}
          onCancel={() => setPendingUnsavedAction(null)}
          onDiscard={discardThenContinuePendingAction}
          onSave={() => void saveThenContinuePendingAction()}
        />
      ) : null}
      <ToastViewport />
    </main>
  );
}

export default App;
