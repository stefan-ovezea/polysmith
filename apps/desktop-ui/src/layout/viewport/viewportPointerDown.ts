import * as THREE from "three";
import type { OrbitControls } from "three/examples/jsm/controls/OrbitControls.js";

import type {
  ArmedSketchConstraint,
  SketchDimensionScene,
  SketchFeatureParameters,
  SketchPlaneFrame,
  SketchPreviewPoint,
  SketchTool,
  ViewportContextMenuState,
} from "@/types";
import {
  beginMoveGizmoPointerDown,
  type MoveGizmoDescriptor,
  type MoveGizmoDragState,
} from "./moveGizmo";
import { beginDimensionLabelDragPointerDown } from "./dimensionLabelDrag";
import type { DimensionLabelDragState } from "./draftDimensions";
import {
  beginDraftPointerDown,
  type BeginDraftPointerDownParams,
  type MutableRef,
  type PointerDownPosition,
  type UpdateDraftChainBreakParams,
  updateDraftChainBreakRequest,
} from "./draftPointerDown";
import type {
  DraftDimensionSession,
} from "./draftDimensions";
import { beginSelectPointerDown } from "./selectPointerDown";
import type { EndpointDrag } from "./endpointDrag";
import type { SelectionDrag } from "./selectionGeometry";
import type { ActiveSketchGridPlaneFrame } from "./grid";
import type {
  SelectedConstraintState,
  ViewportPickHit,
} from "./contextMenuState";

interface ViewportPointerDownParams {
  event: PointerEvent;
  renderer: THREE.WebGLRenderer;
  camera: THREE.Camera;
  controls: OrbitControls;
  raycaster: THREE.Raycaster;
  pointer: THREE.Vector2;
  setSelectedConstraint: (state: SelectedConstraintState | null) => void;
  setContextMenu: (state: ViewportContextMenuState | null) => void;
  lastPointerEventRef: MutableRef<PointerEvent | null>;
  setPointerDown: (point: PointerDownPosition | null) => void;
  dimensionLabelDragRef: MutableRef<DimensionLabelDragState | null>;
  activeSketchToolRef: MutableRef<SketchTool>;
  activeSketchPlaneIdRef: MutableRef<string | null>;
  activeSketchPlaneFrameRef: MutableRef<SketchPlaneFrame | null>;
  armedSketchConstraintRef: MutableRef<ArmedSketchConstraint>;
  lineDraftStartRef: BeginDraftPointerDownParams["draftStartRef"];
  lastPointerDownTimeRef: UpdateDraftChainBreakParams["lastPointerDownTimeRef"];
  lastPointerDownPosRef: UpdateDraftChainBreakParams["lastPointerDownPosRef"];
  chainBreakRequestedRef: UpdateDraftChainBreakParams["chainBreakRequestedRef"];
  isPointerInCubeArea: (
    event: PointerEvent,
    canvasRect: DOMRect,
    pixelRatio: number,
  ) => boolean;
  viewCubeDraggingRef: MutableRef<boolean>;
  viewCubeDragStartRef: MutableRef<PointerDownPosition | null>;
  moveGizmoRef: MutableRef<MoveGizmoDescriptor | null>;
  moveGizmoObjectsRef: MutableRef<THREE.Object3D[]>;
  moveGizmoDragRef: MutableRef<MoveGizmoDragState | null>;
  sketchLinesRef: MutableRef<SketchFeatureParameters | null>;
  endpointDragRef: MutableRef<EndpointDrag | null>;
  selectionDragRef: MutableRef<SelectionDrag | null>;
  intersectSceneTargets: (event: PointerEvent) => ViewportPickHit | null;
  displayedSketchDimensionsRef: MutableRef<readonly SketchDimensionScene[]>;
  suppressNextDimensionEditorOpenRef: MutableRef<boolean>;
  setIsDimensionEditorOpen: (open: boolean) => void;
  selectSketchDimension: (dimensionId: string) => Promise<void>;
  setAngleDimensionDragRadius: (
    dimension: SketchDimensionScene,
    dimensionId: string,
    worldPoint: readonly [number, number, number],
  ) => void;
  getDimensionPlacementAxis: (
    dimension: SketchDimensionScene,
  ) => THREE.Vector3 | null;
  draftStartedOnPointerDownRef: BeginDraftPointerDownParams["draftStartedOnPointerDownRef"];
  draftDimensionSessionRef: BeginDraftPointerDownParams["draftDimensionSessionRef"];
  resolveSnappedSketchPoint: BeginDraftPointerDownParams["resolveSnappedSketchPoint"];
  createDraftDimensionSession: BeginDraftPointerDownParams["createDraftDimensionSession"];
  setDraftDimensionSession: BeginDraftPointerDownParams["setDraftDimensionSession"];
  focusDraftField: BeginDraftPointerDownParams["focusDraftField"];
}

export function handleViewportPointerDown(params: ViewportPointerDownParams) {
  params.setSelectedConstraint(null);
  params.lastPointerEventRef.current = params.event;
  params.setContextMenu(null);

  if (captureDimensionPlacementPointer(params)) {
    return;
  }
  if (handleNonPrimaryButton(params)) {
    return;
  }

  handlePrimaryButtonPointerDown(params);
}

function handlePrimaryButtonPointerDown(params: ViewportPointerDownParams) {
  updateDraftChainBreakRequest({
    event: params.event,
    activeSketchTool: params.activeSketchToolRef.current,
    draftStartRef: params.lineDraftStartRef,
    lastPointerDownTimeRef: params.lastPointerDownTimeRef,
    lastPointerDownPosRef: params.lastPointerDownPosRef,
    chainBreakRequestedRef: params.chainBreakRequestedRef,
  });

  if (beginViewCubePointerDown(params)) {
    return;
  }
  if (beginMoveGizmoDrag(params)) {
    return;
  }

  params.setPointerDown(pointerDownPosition(params.event));
  if (beginSelectToolPointerDown(params)) {
    return;
  }

  params.renderer.domElement.setPointerCapture(params.event.pointerId);
  if (beginDimensionLabelDrag(params)) {
    return;
  }
  beginDraftPointerDown({
    event: params.event,
    renderer: params.renderer,
    camera: params.camera,
    activeSketchPlaneId: params.activeSketchPlaneIdRef.current,
    activeSketchPlaneFrame: params.activeSketchPlaneFrameRef.current,
    activeSketchTool: params.activeSketchToolRef.current,
    draftStartRef: params.lineDraftStartRef,
    draftStartedOnPointerDownRef: params.draftStartedOnPointerDownRef,
    draftDimensionSessionRef: params.draftDimensionSessionRef,
    resolveSnappedSketchPoint: params.resolveSnappedSketchPoint,
    createDraftDimensionSession: params.createDraftDimensionSession,
    setDraftDimensionSession: params.setDraftDimensionSession,
    focusDraftField: params.focusDraftField,
  });
}

function captureDimensionPlacementPointer({
  dimensionLabelDragRef,
  renderer,
  event,
  setPointerDown,
}: ViewportPointerDownParams) {
  if (!dimensionLabelDragRef.current?.isPlacement) {
    return false;
  }
  renderer.domElement.setPointerCapture(event.pointerId);
  setPointerDown(pointerDownPosition(event));
  return true;
}

function handleNonPrimaryButton({
  event,
  controls,
  setPointerDown,
}: ViewportPointerDownParams) {
  if (event.button === 1) {
    controls.mouseButtons.MIDDLE =
      event.ctrlKey || event.metaKey ? THREE.MOUSE.ROTATE : THREE.MOUSE.PAN;
    return true;
  }
  if (event.button !== 0) {
    setPointerDown(null);
    return true;
  }
  return false;
}

function beginViewCubePointerDown({
  event,
  renderer,
  controls,
  isPointerInCubeArea,
  viewCubeDraggingRef,
  viewCubeDragStartRef,
  setPointerDown,
}: ViewportPointerDownParams) {
  if (
    !isPointerInCubeArea(
      event,
      renderer.domElement.getBoundingClientRect(),
      renderer.getPixelRatio(),
    )
  ) {
    return false;
  }

  viewCubeDraggingRef.current = true;
  viewCubeDragStartRef.current = pointerDownPosition(event);
  controls.enabled = false;
  setPointerDown(null);
  return true;
}

function beginMoveGizmoDrag(params: ViewportPointerDownParams) {
  if (
    !beginMoveGizmoPointerDown({
      event: params.event,
      renderer: params.renderer,
      camera: params.camera,
      controls: params.controls,
      raycaster: params.raycaster,
      pointer: params.pointer,
      moveGizmo: params.moveGizmoRef.current,
      moveGizmoObjects: params.moveGizmoObjectsRef.current,
      moveGizmoDragRef: params.moveGizmoDragRef,
    })
  ) {
    return false;
  }

  params.setPointerDown(null);
  return true;
}

function beginSelectToolPointerDown(params: ViewportPointerDownParams) {
  if (params.activeSketchToolRef.current !== "select") {
    return false;
  }

  const result = beginSelectPointerDown({
    event: params.event,
    renderer: params.renderer,
    camera: params.camera,
    controls: params.controls,
    activeSketchPlaneId: params.activeSketchPlaneIdRef.current,
    activeSketchPlaneFrame: params.activeSketchPlaneFrameRef.current,
    sketch: params.sketchLinesRef.current,
    endpointDragRef: params.endpointDragRef,
    selectionDragRef: params.selectionDragRef,
    intersectSceneTargets: params.intersectSceneTargets,
    armedSketchConstraint: params.armedSketchConstraintRef.current,
  });
  if (result.clearPointerDown) {
    params.setPointerDown(null);
  }
  return result.handled;
}

function beginDimensionLabelDrag(params: ViewportPointerDownParams) {
  const activeSketchPlaneId = params.activeSketchPlaneIdRef.current;
  if (
    !activeSketchPlaneId ||
    (params.activeSketchToolRef.current !== "select" &&
      params.activeSketchToolRef.current !== "dimension")
  ) {
    return false;
  }

  const hit = params.intersectSceneTargets(params.event);
  if (hit?.kind !== "sketch_dimension") {
    return false;
  }

  return beginDimensionLabelDragPointerDown({
    event: params.event,
    renderer: params.renderer,
    camera: params.camera,
    controls: params.controls,
    hit,
    activeSketchPlaneId,
    activeSketchPlaneFrame: params.activeSketchPlaneFrameRef.current as
      | ActiveSketchGridPlaneFrame
      | null,
    dimensions: params.displayedSketchDimensionsRef.current,
    suppressNextDimensionEditorOpenRef: params.suppressNextDimensionEditorOpenRef,
    dimensionLabelDragRef: params.dimensionLabelDragRef,
    setIsDimensionEditorOpen: params.setIsDimensionEditorOpen,
    selectSketchDimension: params.selectSketchDimension,
    setAngleDimensionDragRadius: params.setAngleDimensionDragRadius,
    getDimensionPlacementAxis: params.getDimensionPlacementAxis,
  });
}

function pointerDownPosition(event: PointerEvent): PointerDownPosition {
  return { x: event.clientX, y: event.clientY };
}
