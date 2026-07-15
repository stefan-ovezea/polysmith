import * as THREE from "three";
import type { OrbitControls } from "three/examples/jsm/controls/OrbitControls.js";

import type {
  ArmedSketchConstraint,
  SketchFeatureParameters,
  SketchPlaneFrame,
  SketchPreviewPoint,
  SketchTool,
  ViewportScene,
} from "@/types";
import { resolveSketchPlanePoint } from "@/utils";
import type { ViewportPickHit } from "./contextMenuState";
import { commitDraftPointerUp } from "./draftCommit";
import type {
  ArcDraftCommitOptions,
  CircleDraftCommitOptions,
  DraftCommitSketchPoint,
  DraftPointerUpCommitOptions,
  LineBodyHost,
  LineDraftCommitOptions,
  Point2d,
  PolygonDraftCommitOptions,
  PolygonToolMode,
  RectangleDraftCommitOptions,
} from "./draftCommit";
import type {
  DraftDimensionSession,
} from "./draftDimensions";
import { finishDraftStartedPointerUp } from "./draftPointerDown";
import {
  finishMoveGizmoPointerUp as finishMoveGizmoPointerUpBase,
  type MoveGizmoDragState,
} from "./moveGizmo";
import {
  handleActiveSketchPointerUpTool,
  type ActiveSketchPointerUpContext,
} from "./pointerUpActiveSketch";
import { handlePointerUpSceneSelection } from "./pointerUpSceneSelection";
import type {
  ActiveSketchSelectHit,
  SharedSketchSelectionHit,
} from "./sketchClickSelection";
import {
  finishRectangleSelectionDrag,
  type SelectionDrag,
  type SelectionRectOverlay,
} from "./selectionGeometry";

interface MutableRef<T> {
  current: T;
}

type PointerDownPosition = { x: number; y: number };
type EndpointDragPointerUpResult = "inactive" | "continue" | "consumed";

interface ViewportPointerUpParams {
  event: PointerEvent;
  renderer: THREE.WebGLRenderer;
  camera: THREE.OrthographicCamera;
  controls: OrbitControls;
  activeSketchPlaneId: string | null;
  activeSketchPlaneFrame: SketchPlaneFrame | null;
  pointerDown: PointerDownPosition | null;
  setPointerDown: (point: PointerDownPosition | null) => void;
  lastPointerEventRef: MutableRef<PointerEvent | null>;
  selectionDragRef: MutableRef<SelectionDrag | null>;
  setSelectionRect: (overlay: SelectionRectOverlay | null) => void;
  performRectangleSelect: (
    drag: SelectionDrag,
    additive: boolean,
  ) => Promise<void>;
  moveGizmoDragRef: MutableRef<MoveGizmoDragState | null>;
  finishDimensionLabelDragPointerUp: () => "inactive" | "continue" | "consumed";
  finishEndpointDragPointerUp: (event: PointerEvent) => EndpointDragPointerUpResult;
  finishViewCubePointerUp: (event: PointerEvent) => "inactive" | "consumed";
  draftStartedOnPointerDownRef: MutableRef<boolean>;
  draftDimensionSessionRef: MutableRef<DraftDimensionSession | null>;
  draftDimensionInputRefs: DraftPointerUpCommitOptions["refs"]["draftDimensionInputs"];
  intersectSceneTargets: (event: PointerEvent) => ViewportPickHit;
  activeSketchToolRef: MutableRef<SketchTool>;
  activeSketchPlaneFrameRef: MutableRef<SketchPlaneFrame | null>;
  sketchLinesRef: MutableRef<SketchFeatureParameters | null>;
  armedSketchConstraintRef: MutableRef<ArmedSketchConstraint>;
  mirrorFocusedSlotRef: MutableRef<"objects" | "axis" | null>;
  inactiveSketchEntityPickEnabledRef: MutableRef<boolean>;
  sketchEntityObjectByIdRef: MutableRef<ReadonlyMap<string, THREE.Line | THREE.LineLoop>>;
  sketchPointObjectsRef: MutableRef<readonly THREE.Mesh[]>;
  resolveSnappedSketchPoint: (
    rawPoint: {
      local: Point2d;
      world: [number, number, number];
    },
    draftStartLocal?: Point2d | null,
    options?: { inferenceSnapsEnabled?: boolean },
  ) => SketchPreviewPoint;
  setSketchSnapLabel: (label: string | null) => void;
  selectSketchProfile: ActiveSketchPointerUpContext["selectSketchProfile"];
  selectVertex: ActiveSketchPointerUpContext["selectVertex"];
  selectEdge: ActiveSketchPointerUpContext["selectEdge"];
  selectFace: ActiveSketchPointerUpContext["selectFace"];
  trimSketchEntity: ActiveSketchPointerUpContext["trimSketchEntity"];
  mirrorEntityPick: ActiveSketchPointerUpContext["mirrorEntityPick"];
  selectSketchEntity: ActiveSketchPointerUpContext["selectSketchEntity"];
  pickSketchPoint: ActiveSketchPointerUpContext["pickSketchPoint"];
  handleDimensionClick: ActiveSketchPointerUpContext["handleDimensionClick"];
  setSelectedConstraint: ActiveSketchPointerUpContext["setSelectedConstraint"];
  setIsDimensionEditorOpen: (open: boolean) => void;
  paintSketchEntityMaterials: ActiveSketchPointerUpContext["paintSketchEntityMaterials"];
  paintSketchPointMaterials: ActiveSketchPointerUpContext["paintSketchPointMaterials"];
  addMessage: ActiveSketchPointerUpContext["addMessage"];
  addSketchFillet: ActiveSketchPointerUpContext["addSketchFillet"];
  pendingDimensionPlacement: ActiveSketchPointerUpContext["pendingDimensionPlacement"];
  pendingDimensionSourceId: ActiveSketchPointerUpContext["pendingDimensionSourceId"];
  pendingDimensionId: ActiveSketchPointerUpContext["pendingDimensionId"];
  getDimensionFirstEntityId: ActiveSketchPointerUpContext["getDimensionFirstEntityId"];
  getDimensionFirstPoint: ActiveSketchPointerUpContext["getDimensionFirstPoint"];
  clearDimensionFirstPick: ActiveSketchPointerUpContext["clearDimensionFirstPick"];
  clearDimensionFirstEntity: ActiveSketchPointerUpContext["clearDimensionFirstEntity"];
  clearPendingDimensionPlacement: ActiveSketchPointerUpContext["clearPendingDimensionPlacement"];
  stageDimensionFirstEntity: ActiveSketchPointerUpContext["stageDimensionFirstEntity"];
  stageDimensionFirstPoint: ActiveSketchPointerUpContext["stageDimensionFirstPoint"];
  deleteSketchDimension: ActiveSketchPointerUpContext["deleteSketchDimension"];
  createDimensionAngleOrDistance: ActiveSketchPointerUpContext["createDimensionAngleOrDistance"];
  createDimensionVertexDistance: ActiveSketchPointerUpContext["createDimensionVertexDistance"];
  createDimensionLineAngle: ActiveSketchPointerUpContext["createDimensionLineAngle"];
  createDimensionLine: ActiveSketchPointerUpContext["createDimensionLine"];
  createDimensionLinear: ActiveSketchPointerUpContext["createDimensionLinear"];
  createDimensionCircle: ActiveSketchPointerUpContext["createDimensionCircle"];
  selectDimensionCircle: ActiveSketchPointerUpContext["selectDimensionCircle"];
  createDimensionArc: ActiveSketchPointerUpContext["createDimensionArc"];
  selectDimensionArc: ActiveSketchPointerUpContext["selectDimensionArc"];
  createDimensionPolygon: ActiveSketchPointerUpContext["createDimensionPolygon"];
  selectDimensionPolygon: ActiveSketchPointerUpContext["selectDimensionPolygon"];
  selectDimensionLine: ActiveSketchPointerUpContext["selectDimensionLine"];
  sketchCircleCount: number;
  lineDraftStartRef: MutableRef<Point2d | null>;
  arcSecondPointRef: MutableRef<Point2d | null>;
  rectSecondPointRef: MutableRef<Point2d | null>;
  circleSecondPointRef: MutableRef<Point2d | null>;
  chainBreakRequestedRef: MutableRef<boolean>;
  previousLineAngleRef: MutableRef<number | null>;
  draftStartMidpointHostRef: MutableRef<string | null>;
  draftStartEndpointHostRef: MutableRef<string | null>;
  draftStartLineBodyHostRef: MutableRef<LineBodyHost | null>;
  draftDimensionInputRefsForCommit: DraftPointerUpCommitOptions["refs"]["draftDimensionInputs"];
  arcToolMode: DraftPointerUpCommitOptions["modes"]["arc"];
  rectangleToolMode: DraftPointerUpCommitOptions["modes"]["rectangle"];
  circleToolMode: DraftPointerUpCommitOptions["modes"]["circle"];
  polygonToolMode: PolygonToolMode;
  dimensionToolMode: import("@/types").DimensionToolMode;
  polygonSides: number;
  isConstruction: boolean;
  clearPreviews: () => void;
  clearDraftDimensionSession: DraftPointerUpCommitOptions["clearDraftDimensionSession"];
  suppressDimensionEditorAfterSketchCommit: DraftPointerUpCommitOptions["suppressDimensionEditorAfterSketchCommit"];
  scheduleDimensionDeletion: DraftPointerUpCommitOptions["scheduleDimensionDeletion"];
  scheduleDraftDimensionExpressionUpdate: DraftPointerUpCommitOptions["scheduleDraftDimensionExpressionUpdate"];
  setPendingCircleDimensionPlacement: DraftPointerUpCommitOptions["setPendingCircleDimensionPlacement"];
  captureLineCommitRelations: DraftPointerUpCommitOptions["captureLineCommitRelations"];
  createLineDraftDimensionSession: DraftPointerUpCommitOptions["createLineDraftDimensionSession"];
  clearDraftDimGroup: DraftPointerUpCommitOptions["clearDraftDimGroup"];
  setDraftDimensionSession: DraftPointerUpCommitOptions["setDraftDimensionSession"];
  focusDraftField: DraftPointerUpCommitOptions["focusDraftField"];
  addSketchArc: ArcDraftCommitOptions["addSketchArc"];
  addSketchRectangle: RectangleDraftCommitOptions["addSketchRectangle"];
  addSketchCircle: CircleDraftCommitOptions["addSketchCircle"];
  addSketchPolygon: PolygonDraftCommitOptions["addSketchPolygon"];
  addSketchLine: LineDraftCommitOptions["addSketchLine"];
  sceneDataRef: MutableRef<ViewportScene | null>;
  pickInactiveSketchLine:
    | ((sketchLineId: string) => void | Promise<void>)
    | null
    | undefined;
  selectReference: (referenceId: string) => Promise<void>;
  selectPrimitive: (primitiveId: string) => Promise<void>;
}

export function handleViewportPointerUp(params: ViewportPointerUpParams) {
  if (finishRectangleSelection(params)) {
    return;
  }

  params.lastPointerEventRef.current = params.event;
  if (handleNonPrimaryPointerUp(params)) {
    return;
  }
  releasePointerCapture(params);

  if (finishTransientPointerUp(params)) {
    return;
  }
  finishClickPointerUp(params);
}

function finishRectangleSelection(params: ViewportPointerUpParams) {
  return finishRectangleSelectionDrag({
    event: params.event,
    selectionDragRef: params.selectionDragRef,
    setSelectionRect: params.setSelectionRect,
    controls: params.controls,
    performRectangleSelect: params.performRectangleSelect,
  });
}

function handleNonPrimaryPointerUp(params: ViewportPointerUpParams) {
  if (params.event.button === 1) {
    params.controls.mouseButtons.MIDDLE = THREE.MOUSE.PAN;
    params.setPointerDown(null);
    return true;
  }
  if (params.event.button !== 0) {
    params.setPointerDown(null);
    return true;
  }
  return false;
}

function releasePointerCapture({ renderer, event }: ViewportPointerUpParams) {
  if (renderer.domElement.hasPointerCapture(event.pointerId)) {
    renderer.domElement.releasePointerCapture(event.pointerId);
  }
}

function finishTransientPointerUp(params: ViewportPointerUpParams) {
  if (finishMoveGizmoPointerUp(params)) {
    return true;
  }
  if (params.finishDimensionLabelDragPointerUp() === "consumed") {
    return true;
  }
  if (params.finishEndpointDragPointerUp(params.event) === "consumed") {
    return true;
  }
  return params.finishViewCubePointerUp(params.event) === "consumed";
}

function finishMoveGizmoPointerUp(params: ViewportPointerUpParams) {
  if (
    !finishMoveGizmoPointerUpBase({
      renderer: params.renderer,
      controls: params.controls,
      moveGizmoDragRef: params.moveGizmoDragRef,
    })
  ) {
    return false;
  }
  params.setPointerDown(null);
  return true;
}

function finishClickPointerUp(params: ViewportPointerUpParams) {
  const pointerDown = params.pointerDown;
  if (!pointerDown) {
    return;
  }

  const deltaX = Math.abs(params.event.clientX - pointerDown.x);
  const deltaY = Math.abs(params.event.clientY - pointerDown.y);
  params.setPointerDown(null);

  if (finishDraftStarted(params, deltaX, deltaY)) {
    return;
  }
  if (deltaX > 4 || deltaY > 4) {
    return;
  }

  if (params.activeSketchPlaneId) {
    finishActiveSketchPointerUp(params);
    // Commit any remaining dimension label placement that wasn't
    // consumed by entity handling (e.g. two-click regroup workflows
    // clear the drag ref via clearPendingDimensionPlacement so this
    // is a no-op for those; single-click empty-space commits fall
    // through to here).
    params.finishDimensionLabelDragPointerUp();
    return;
  }
  finishScenePointerUp(params);
}

function finishDraftStarted(
  params: ViewportPointerUpParams,
  deltaX: number,
  deltaY: number,
) {
  return finishDraftStartedPointerUp({
    deltaX,
    deltaY,
    draftStartedOnPointerDownRef: params.draftStartedOnPointerDownRef,
    draftDimensionSessionRef: params.draftDimensionSessionRef,
    draftDimensionInputRefs: params.draftDimensionInputRefs,
  });
}

function finishActiveSketchPointerUp(params: ViewportPointerUpParams) {
  const hit = params.intersectSceneTargets(params.event);
  const additiveSelection =
    params.event.shiftKey || params.event.ctrlKey || params.event.metaKey;

  // Click on empty canvas with select tool → close the dimension editor.
  if (!hit && params.activeSketchToolRef.current === "select" && !additiveSelection) {
    params.setIsDimensionEditorOpen(false);
  }

  if (
    handleActiveSketchToolPointerUp(
      params,
      hit as ActiveSketchSelectHit,
      additiveSelection,
    )
  ) {
    return;
  }
  commitActiveSketchDraft(params);
}

function handleActiveSketchToolPointerUp(
  params: ViewportPointerUpParams,
  hit: ActiveSketchSelectHit,
  additiveSelection: boolean,
) {
  // Resolve cursor to sketch-local coords for dimension axis detection.
  let cursorLocal: [number, number] | undefined;
  if (params.activeSketchPlaneId) {
    const resolved = resolveSketchPlanePoint(
      params.event,
      params.renderer,
      params.camera,
      params.activeSketchPlaneId,
      params.activeSketchPlaneFrameRef.current,
    );
    cursorLocal = resolved?.local;
  }

  return handleActiveSketchPointerUpTool({
    activeSketchTool: params.activeSketchToolRef.current,
    hit,
    additiveSelection,
    planeId: params.activeSketchPlaneId ?? "",
    planeFrame: params.activeSketchPlaneFrameRef.current,
    sketch: params.sketchLinesRef.current,
    armedSketchConstraint: params.armedSketchConstraintRef.current,
    mirrorFocusedSlot: params.mirrorFocusedSlotRef.current,
    inactiveSketchEntityPickEnabled: params.inactiveSketchEntityPickEnabledRef.current,
    sketchEntityObjectById: params.sketchEntityObjectByIdRef.current,
    sketchPointObjects: params.sketchPointObjectsRef.current,
    resolveFilletPoint: () => resolveFilletPoint(params),
    selectSketchProfile: params.selectSketchProfile,
    selectVertex: params.selectVertex,
    selectEdge: params.selectEdge,
    selectFace: params.selectFace,
    trimSketchEntity: params.trimSketchEntity,
    mirrorEntityPick: params.mirrorEntityPick,
    selectSketchEntity: params.selectSketchEntity,
    pickSketchPoint: params.pickSketchPoint,
    handleDimensionClick: params.handleDimensionClick,
    setSelectedConstraint: params.setSelectedConstraint,
    setIsDimensionEditorOpen: params.setIsDimensionEditorOpen,
    paintSketchEntityMaterials: params.paintSketchEntityMaterials,
    paintSketchPointMaterials: params.paintSketchPointMaterials,
    addMessage: params.addMessage,
    addSketchFillet: params.addSketchFillet,
    pendingDimensionPlacement: params.pendingDimensionPlacement,
    pendingDimensionSourceId: params.pendingDimensionSourceId,
    pendingDimensionId: params.pendingDimensionId,
    getDimensionFirstEntityId: params.getDimensionFirstEntityId,
    getDimensionFirstPoint: params.getDimensionFirstPoint,
    clearDimensionFirstPick: params.clearDimensionFirstPick,
    clearDimensionFirstEntity: params.clearDimensionFirstEntity,
    clearPendingDimensionPlacement: params.clearPendingDimensionPlacement,
    stageDimensionFirstEntity: params.stageDimensionFirstEntity,
    stageDimensionFirstPoint: params.stageDimensionFirstPoint,
    deleteSketchDimension: params.deleteSketchDimension,
    createDimensionAngleOrDistance: params.createDimensionAngleOrDistance,
    createDimensionVertexDistance: params.createDimensionVertexDistance,
    createDimensionLine: params.createDimensionLine,
    createDimensionLinear: params.createDimensionLinear,
    createDimensionLineAngle: params.createDimensionLineAngle,
    createDimensionCircle: params.createDimensionCircle,
    selectDimensionCircle: params.selectDimensionCircle,
    createDimensionArc: params.createDimensionArc,
    selectDimensionArc: params.selectDimensionArc,
    createDimensionPolygon: params.createDimensionPolygon,
    selectDimensionPolygon: params.selectDimensionPolygon,
    selectDimensionLine: params.selectDimensionLine,
    dimensionToolMode: params.dimensionToolMode,
    cursorLocal,
  });
}

function resolveFilletPoint(params: ViewportPointerUpParams) {
  if (!params.activeSketchPlaneId) {
    return null;
  }
  const rawPoint = resolveSketchPlanePoint(
    params.event,
    params.renderer,
    params.camera,
    params.activeSketchPlaneId,
    params.activeSketchPlaneFrame,
  );
  return rawPoint ? params.resolveSnappedSketchPoint(rawPoint) : null;
}

function commitActiveSketchDraft(params: ViewportPointerUpParams) {
  if (!params.activeSketchPlaneId) {
    return;
  }
  const rawPoint = resolveSketchPlanePoint(
    params.event,
    params.renderer,
    params.camera,
    params.activeSketchPlaneId,
    params.activeSketchPlaneFrame,
  );
  if (!rawPoint) {
    return;
  }

  const sketchPoint = params.resolveSnappedSketchPoint(
    rawPoint,
    params.lineDraftStartRef.current,
    { inferenceSnapsEnabled: false },
  );
  params.setSketchSnapLabel(sketchPoint.snapLabel);
  commitDraftPointerUp({
    activeSketchTool: params.activeSketchToolRef.current,
    sketchPoint,
    draftDimensionSession: params.draftDimensionSessionRef.current,
    sketchCircleCount: params.sketchCircleCount,
    refs: {
      draftStart: params.lineDraftStartRef,
      arcSecondPoint: params.arcSecondPointRef,
      rectSecondPoint: params.rectSecondPointRef,
      circleSecondPoint: params.circleSecondPointRef,
      chainBreakRequested: params.chainBreakRequestedRef,
      previousLineAngle: params.previousLineAngleRef,
      draftStartMidpointHost: params.draftStartMidpointHostRef,
      draftStartEndpointHost: params.draftStartEndpointHostRef,
      draftStartLineBodyHost: params.draftStartLineBodyHostRef,
      draftDimensionSession: params.draftDimensionSessionRef,
      draftDimensionInputs: params.draftDimensionInputRefsForCommit,
    },
    modes: {
      arc: params.arcToolMode,
      rectangle: params.rectangleToolMode,
      circle: params.circleToolMode,
      polygon: params.polygonToolMode,
    },
    polygonSides: params.polygonSides,
    isConstruction: params.isConstruction,
    clearPreviews: params.clearPreviews,
    clearDraftDimensionSession: params.clearDraftDimensionSession,
    suppressDimensionEditorAfterSketchCommit:
      params.suppressDimensionEditorAfterSketchCommit,
    scheduleDimensionDeletion: params.scheduleDimensionDeletion,
    scheduleDraftDimensionExpressionUpdate:
      params.scheduleDraftDimensionExpressionUpdate,
    setPendingCircleDimensionPlacement: params.setPendingCircleDimensionPlacement,
    captureLineCommitRelations: params.captureLineCommitRelations,
    createLineDraftDimensionSession: params.createLineDraftDimensionSession,
    clearDraftDimGroup: params.clearDraftDimGroup,
    setDraftDimensionSession: params.setDraftDimensionSession,
    focusDraftField: params.focusDraftField,
    addSketchArc: params.addSketchArc,
    addSketchRectangle: params.addSketchRectangle,
    addSketchCircle: params.addSketchCircle,
    addSketchPolygon: params.addSketchPolygon,
    addSketchLine: params.addSketchLine,
  });
}

function finishScenePointerUp(params: ViewportPointerUpParams) {
  handlePointerUpSceneSelection({
    event: params.event,
    sceneData: params.sceneDataRef.current,
    camera: params.camera,
    renderer: params.renderer,
    inactiveSketchEntityPickEnabled: params.inactiveSketchEntityPickEnabledRef.current,
    pickInactiveSketchLine: params.pickInactiveSketchLine,
    intersectSceneTargets: (event) =>
      params.intersectSceneTargets(event) as SharedSketchSelectionHit,
    selectSketchEntity: params.selectSketchEntity,
    selectSketchProfile: params.selectSketchProfile,
    selectReference: params.selectReference,
    selectVertex: params.selectVertex,
    selectEdge: params.selectEdge,
    selectFace: params.selectFace,
    selectPrimitive: params.selectPrimitive,
  });
}
