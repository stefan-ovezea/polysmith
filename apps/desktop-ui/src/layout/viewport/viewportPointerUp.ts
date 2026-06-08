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
import type { ArcToolMode } from "./arcDraftPreview";
import type { CircleToolMode } from "./circleDraftPreview";
import type { ViewportPickHit } from "./contextMenuState";
import { commitDraftPointerUp } from "./draftCommit";
import type {
  DraftDimensionField,
  DraftDimensionSession,
} from "./draftDimensions";
import { finishDraftStartedPointerUp } from "./draftPointerDown";
import {
  finishMoveGizmoPointerUp as finishMoveGizmoPointerUpBase,
  type MoveGizmoDragState,
} from "./moveGizmo";
import {
  handleActiveSketchPointerUpTool,
} from "./pointerUpActiveSketch";
import { handlePointerUpSceneSelection } from "./pointerUpSceneSelection";
import type {
  ActiveSketchSelectHit,
  SharedSketchSelectionHit,
} from "./sketchClickSelection";
import type { RectangleToolMode } from "./rectangleDraftPreview";
import {
  finishRectangleSelectionDrag,
  type SelectionDrag,
  type SelectionRectOverlay,
} from "./selectionGeometry";

interface MutableRef<T> {
  current: T;
}

type PointerDownPosition = { x: number; y: number };
type Point2d = [number, number];
type LineBodyHost = { lineId: string; t: number };
type PolygonToolMode = "circumscribed" | "inscribed" | "edge";
type EndpointDragPointerUpResult = "inactive" | "continue" | "consumed";

interface DraftCommitSketchPoint {
  local: Point2d;
  snapMidpointHostLineId?: string | null;
  snapMidpointT?: number | null;
  snapPerpendicularHostLineId?: string | null;
  snapEndpointHostLineId?: string | null;
  snapLineBodyHostLineId?: string | null;
  snapLineBodyT?: number | null;
  snapAxisLock?: "horizontal" | "vertical" | null;
  snapTangentCircleId?: string | null;
  snapParallelHostLineId?: string | null;
}

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
  draftDimensionInputRefs: MutableRef<
    Partial<Record<DraftDimensionField, HTMLInputElement | null>>
  >;
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
  ) => SketchPreviewPoint;
  setSketchSnapLabel: (label: string | null) => void;
  selectSketchProfile: (profileId: string, additive: boolean) => Promise<void>;
  selectVertex: (vertexId: string, additive: boolean) => Promise<void>;
  selectEdge: (edgeId: string, additive: boolean) => Promise<void>;
  selectFace: (faceId: string) => Promise<void>;
  trimSketchEntity:
    | ((entityId: string, localX: number, localY: number) => Promise<void>)
    | null
    | undefined;
  mirrorEntityPick: (
    entityId: string,
    entityKind: "line" | "circle",
  ) => Promise<void>;
  selectSketchEntity: (entityId: string, additive: boolean) => Promise<void>;
  pickSketchPoint: (
    pointId: string,
    pointKind: "endpoint" | "center" | "quadrant",
    additive: boolean,
  ) => Promise<void>;
  handleDimensionClick: (dimensionId: string) => void;
  setSelectedConstraint: Parameters<typeof handleActiveSketchPointerUpTool>[0]["setSelectedConstraint"];
  paintSketchEntityMaterials: () => void;
  paintSketchPointMaterials: () => void;
  addMessage: (message: string) => void;
  addSketchFillet: (
    cornerPointId: string,
    lineAId: string,
    lineBId: string,
  ) => Promise<void>;
  pendingDimensionPlacement: boolean;
  pendingDimensionSourceId: string | null;
  pendingDimensionId: string | null;
  getDimensionFirstEntityId: () => string | null;
  getDimensionFirstPoint: () => { id: string; x: number; y: number } | null;
  clearDimensionFirstPick: () => void;
  clearDimensionFirstEntity: () => void;
  clearPendingDimensionPlacement: () => void;
  stageDimensionFirstEntity: (entityId: string) => void;
  stageDimensionFirstPoint: (point: { id: string; x: number; y: number }) => void;
  deleteSketchDimension: (dimensionId: string) => void;
  createDimensionAngleOrDistance: (
    firstEntityId: string,
    secondEntityId: string,
  ) => void;
  createDimensionPointDistance: (
    firstPointId: string,
    secondPointId: string,
  ) => void;
  createDimensionLine: (lineId: string) => void;
  createDimensionCircle: (circleId: string, label: string) => void;
  selectDimensionCircle: (circleId: string) => void;
  createDimensionPolygon: (polygonId: string) => void;
  selectDimensionPolygon: (polygonId: string) => void;
  selectDimensionLine: (lineId: string) => void;
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
  draftDimensionInputRefsForCommit: MutableRef<
    Partial<Record<DraftDimensionField, HTMLInputElement | null>>
  >;
  arcToolMode: ArcToolMode;
  rectangleToolMode: RectangleToolMode;
  circleToolMode: CircleToolMode;
  polygonToolMode: PolygonToolMode;
  polygonSides: number;
  isConstruction: boolean;
  clearPreviews: () => void;
  clearDraftDimensionSession: () => void;
  suppressDimensionEditorAfterSketchCommit: () => void;
  scheduleDimensionDeletion: (
    tool: "line" | "rectangle" | "circle" | "polygon",
    preCapturedSession?: DraftDimensionSession | null,
  ) => void;
  scheduleDraftDimensionExpressionUpdate: (
    tool: "line" | "rectangle" | "circle" | "polygon",
  ) => void;
  setPendingCircleDimensionPlacement: (placement: {
    fromCircleCount: number;
    center: Point2d;
    end: Point2d;
  }) => void;
  captureLineCommitRelations: (sketchPoint: DraftCommitSketchPoint) => {
    endHostLineId: string | null;
    endLineBodyHost: LineBodyHost | null;
  };
  createLineDraftDimensionSession: (
    start: Point2d,
    current: Point2d,
  ) => DraftDimensionSession;
  clearDraftDimGroup: () => void;
  setDraftDimensionSession: (session: DraftDimensionSession) => void;
  focusDraftField: (field: DraftDimensionSession["activeField"]) => void;
  addSketchArc: (
    startX: number,
    startY: number,
    endX: number,
    endY: number,
    anchorX: number,
    anchorY: number,
    mode: ArcToolMode,
    isConstruction: boolean,
  ) => Promise<void> | void;
  addSketchRectangle: (
    startX: number,
    startY: number,
    endX: number,
    endY: number,
    isConstruction: boolean,
  ) => Promise<void> | void;
  addSketchCircle: (
    centerX: number,
    centerY: number,
    radius: number,
    isConstruction: boolean,
  ) => Promise<void> | void;
  addSketchPolygon: (
    sides: number,
    mode: PolygonToolMode,
    centerX: number,
    centerY: number,
    edgeX: number,
    edgeY: number,
    isConstruction: boolean,
  ) => Promise<void> | void;
  addSketchLine: (
    startX: number,
    startY: number,
    endX: number,
    endY: number,
    isConstruction: boolean,
  ) => Promise<void> | void;
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
    createDimensionPointDistance: params.createDimensionPointDistance,
    createDimensionLine: params.createDimensionLine,
    createDimensionCircle: params.createDimensionCircle,
    selectDimensionCircle: params.selectDimensionCircle,
    createDimensionPolygon: params.createDimensionPolygon,
    selectDimensionPolygon: params.selectDimensionPolygon,
    selectDimensionLine: params.selectDimensionLine,
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
