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
  ViewportScene,
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
import { beginSketchMovePointerDown } from "./sketchMovePointerDown";
import {
  disposeSketchMoveRingObject,
  type PendingSketchMove,
  type SketchMoveDrag,
  type SketchMoveRingState,
} from "./sketchMoveTool";
import { setPointerNdcFromEvent } from "@/utils/viewport/viewportMath";
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
  circleToolMode: "center_radius" | "two_point" | "three_point" | "tangent_two_lines" | "tangent_three_lines";
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
  sketchMoveDragRef: MutableRef<SketchMoveDrag | null>;
  sketchMoveRingGroupRef: MutableRef<THREE.Group | null>;
  /** Move/Copy dialog state — while set, the target is fixed and
   *  pointer-down only starts move drags. */
  pendingSketchMoveRef: MutableRef<PendingSketchMove | null>;
  /** Fusion-style persistent manipulator: rotation ring shown while the
   *  Move tool is armed with a selection.  Grabbing the ring rotates. */
  persistentRingGroupRef: MutableRef<THREE.Group | null>;
  persistentRingPickablesRef: MutableRef<THREE.Object3D[]>;
  persistentRingStateRef: MutableRef<SketchMoveRingState | null>;
  sketchGroupRef: MutableRef<THREE.Group | null>;
  sceneDataRef: MutableRef<ViewportScene | null>;
  restorePreviewScene: () => void;
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
  // Move tool: right-button drag rotates (Alt is unreliable on Windows —
  // the window menu swallows it).  Must run before handleNonPrimaryButton,
  // which consumes every non-left button.
  if (beginSketchMoveRotatePointerDown(params)) {
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

  // Lost pointer-up safety net: restore committed geometry before
  // starting a new interaction.
  if (params.endpointDragRef.current || params.sketchMoveDragRef.current) {
    params.endpointDragRef.current = null;
    params.sketchMoveDragRef.current = null;
    params.restorePreviewScene();
  }

  if (beginSketchMoveToolPointerDown(params)) {
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
  if (
    params.activeSketchToolRef.current === "circle" &&
    (params.circleToolMode === "tangent_two_lines" ||
      params.circleToolMode === "tangent_three_lines")
  ) {
    // Tangent circle modes pick defining lines — the center+radius
    // draft rubber band must never start on these clicks.
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

// Raycasts the persistent manipulator ring; returns its state when hit.
function persistentRingHit(
  params: ViewportPointerDownParams,
): SketchMoveRingState | null {
  if (params.persistentRingPickablesRef.current.length === 0) {
    return null;
  }
  setPointerNdcFromEvent(params.pointer, params.event, params.renderer);
  params.raycaster.setFromCamera(params.pointer, params.camera);
  const [ringHit] = params.raycaster.intersectObjects(
    params.persistentRingPickablesRef.current,
    false,
  );
  return ringHit?.object.userData.moveRingHandle === true
    ? params.persistentRingStateRef.current
    : null;
}

function disposePersistentRing(params: ViewportPointerDownParams) {
  disposeSketchMoveRingObject(params.persistentRingGroupRef.current);
  params.persistentRingGroupRef.current = null;
  params.persistentRingPickablesRef.current = [];
  params.persistentRingStateRef.current = null;
}

// Starts a rotate drag from a persistent-ring grab (Fusion-style).
function beginSketchMoveRingRotate(
  params: ViewportPointerDownParams,
  ringState: SketchMoveRingState,
  activeSketchPlaneId: string,
): boolean {
  disposePersistentRing(params);
  const handled = beginSketchMovePointerDown({
    event: params.event,
    renderer: params.renderer,
    camera: params.camera,
    controls: params.controls,
    activeSketchPlaneId,
    activeSketchPlaneFrame: params.activeSketchPlaneFrameRef.current,
    sketch: params.sketchLinesRef.current,
    sceneData: params.sceneDataRef.current,
    hit: null,
    armedSketchConstraint: params.armedSketchConstraintRef.current,
    sketchMoveDragRef: params.sketchMoveDragRef,
    sketchMoveRingGroupRef: params.sketchMoveRingGroupRef,
    sketchGroupRef: params.sketchGroupRef,
    mode: "rotate",
    entityIdsOverride: ringState.entityIds,
    pendingSketchMoveRef: params.pendingSketchMoveRef,
  });
  if (handled) {
    params.setPointerDown(null);
  }
  return handled;
}

function beginSketchMoveToolPointerDown(params: ViewportPointerDownParams) {
  if (params.activeSketchToolRef.current !== "move") {
    return false;
  }

  const activeSketchPlaneId = params.activeSketchPlaneIdRef.current;
  if (!activeSketchPlaneId) {
    return false;
  }

  // Grabbing the manipulator ring rotates the selection (Fusion-style).
  const ringState = persistentRingHit(params);
  if (ringState) {
    return beginSketchMoveRingRotate(params, ringState, activeSketchPlaneId);
  }

  const hit = params.intersectSceneTargets(params.event);
  const isSketchTarget =
    hit?.kind === "sketch_entity" || hit?.kind === "sketch_point";
  if (isSketchTarget) {
    disposePersistentRing(params);
    const handled = beginSketchMovePointerDown({
      event: params.event,
      renderer: params.renderer,
      camera: params.camera,
      controls: params.controls,
      activeSketchPlaneId,
      activeSketchPlaneFrame: params.activeSketchPlaneFrameRef.current,
      sketch: params.sketchLinesRef.current,
      sceneData: params.sceneDataRef.current,
      hit,
      armedSketchConstraint: params.armedSketchConstraintRef.current,
      sketchMoveDragRef: params.sketchMoveDragRef,
      sketchMoveRingGroupRef: params.sketchMoveRingGroupRef,
      sketchGroupRef: params.sketchGroupRef,
      pendingSketchMoveRef: params.pendingSketchMoveRef,
    });
    if (handled) {
      params.setPointerDown(null);
      return true;
    }
    // Fully-fixed / unmovable hit: fall through to click handling.
    return false;
  }

  // Empty space: rectangle multi-select, exactly like the select tool.
  // (Disabled while the Move/Copy dialog is open — the target set is
  // fixed until OK/Cancel.)
  if (!hit && !params.pendingSketchMoveRef.current) {
    params.selectionDragRef.current = {
      startX: params.event.clientX,
      startY: params.event.clientY,
      currentX: params.event.clientX,
      currentY: params.event.clientY,
      active: true,
    };
    params.controls.enabled = false;
    return true;
  }

  return false;
}

// Move tool rotation via right-button drag — the reliable Windows
// alternative to Alt+drag (the window menu swallows Alt in WebView2).
function beginSketchMoveRotatePointerDown(params: ViewportPointerDownParams) {
  if (params.event.button !== 2) {
    return false;
  }
  if (params.activeSketchToolRef.current !== "move") {
    return false;
  }
  const activeSketchPlaneId = params.activeSketchPlaneIdRef.current;
  if (!activeSketchPlaneId) {
    return false;
  }

  // Right-drag on the manipulator ring also rotates its selection.
  const ringState = persistentRingHit(params);
  if (ringState) {
    return beginSketchMoveRingRotate(params, ringState, activeSketchPlaneId);
  }

  const hit = params.intersectSceneTargets(params.event);
  if (hit?.kind !== "sketch_entity" && hit?.kind !== "sketch_point") {
    return false;
  }

  disposePersistentRing(params);
  const handled = beginSketchMovePointerDown({
    event: params.event,
    renderer: params.renderer,
    camera: params.camera,
    controls: params.controls,
    activeSketchPlaneId,
    activeSketchPlaneFrame: params.activeSketchPlaneFrameRef.current,
    sketch: params.sketchLinesRef.current,
    sceneData: params.sceneDataRef.current,
    hit,
    armedSketchConstraint: params.armedSketchConstraintRef.current,
    sketchMoveDragRef: params.sketchMoveDragRef,
    sketchMoveRingGroupRef: params.sketchMoveRingGroupRef,
    sketchGroupRef: params.sketchGroupRef,
    mode: "rotate",
    pendingSketchMoveRef: params.pendingSketchMoveRef,
  });
  if (handled) {
    params.setPointerDown(null);
  }
  return handled;
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
