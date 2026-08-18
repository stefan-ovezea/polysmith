import * as THREE from "three";

import type {
  SketchConstraintScene,
  SketchFeatureParameters,
  SketchPlaneFrame,
  SketchPreviewPoint,
} from "@/types";
import { resolveSketchPlanePoint } from "@/utils";
import type { SketchConstraintData } from "@/lib/planegcsBridge";
import { applySolvedPointsToSketchScene } from "./sketchPreviewSceneUpdate";
import {
  endpointDragCursorPosition,
  endpointDragHasMoved,
  resolveEndpointDragFrame,
  type EndpointDrag,
  type PendingEndpointDragFrame,
} from "./endpointDrag";
import type { ResolveSnapOptions } from "./snapResolution";

interface MutableRef<T> {
  current: T;
}

type ResolveSnappedSketchPoint = (
  rawPoint: {
    local: [number, number];
    world: [number, number, number];
  },
  draftStartLocal?: [number, number] | null,
  options?: ResolveSnapOptions,
) => SketchPreviewPoint;

interface HandleEndpointDragPointerMoveParams {
  event: PointerEvent;
  renderer: THREE.WebGLRenderer;
  camera: THREE.Camera;
  endpointDragRef: MutableRef<EndpointDrag | null>;
  activeSketchPlaneIdRef: MutableRef<string | null>;
  activeSketchPlaneFrameRef: MutableRef<SketchPlaneFrame | null>;
  sketchLinesRef: MutableRef<SketchFeatureParameters | null>;
  /** planegcs constraint data from the viewport state. */
  sketchConstraintsRef: MutableRef<SketchConstraintData[]>;
  /** Scene constraint data, for badge-follow deltas during the preview. */
  sceneConstraintsRef: MutableRef<readonly SketchConstraintScene[]>;
  pendingDragRef: MutableRef<PendingEndpointDragFrame | null>;
  pendingDragFrameRef: MutableRef<number | null>;
  dragSnapResultRef: MutableRef<{ snapX: number; snapY: number } | null>;
  dragCursorRef: MutableRef<{ x: number; y: number } | null>;
  /** Set while the preview is mutating committed scene objects — keeps
   *  mid-drag viewport_state syncs from snapping the preview back. */
  dragPreviewMutatingRef: MutableRef<boolean>;
  sketchEntityObjectByIdRef: MutableRef<
    Map<string, THREE.Line | THREE.LineLoop>
  >;
  sketchPointObjectByIdRef: MutableRef<Map<string, THREE.Mesh>>;
  sketchConstraintObjectsRef: MutableRef<THREE.Object3D[]>;
  sketchProfileObjectsRef: MutableRef<THREE.Group[]>;
  resolveSnappedSketchPoint: ResolveSnappedSketchPoint;
  setSketchSnapLabel: (label: string | null) => void;
  requestRender: () => void;
}

export function handleEndpointDragPointerMove(
  params: HandleEndpointDragPointerMoveParams,
) {
  const endpointDrag = params.endpointDragRef.current;
  const activeSketchPlaneId = params.activeSketchPlaneIdRef.current;
  if (!endpointDrag || !activeSketchPlaneId) {
    return false;
  }

  const rawPoint = resolveSketchPlanePoint(
    params.event,
    params.renderer,
    params.camera,
    activeSketchPlaneId,
    params.activeSketchPlaneFrameRef.current,
  );
  if (!rawPoint) {
    return true;
  }

  if (endpointDragHasMoved(params.event, endpointDrag)) {
    endpointDrag.hasMoved = true;
  }

  const canvasRect = params.renderer.domElement.getBoundingClientRect();
  params.dragCursorRef.current = endpointDragCursorPosition(
    params.event,
    canvasRect,
  );

  params.pendingDragRef.current = {
    vertexId: endpointDrag.vertexId,
    x: rawPoint.local[0],
    y: rawPoint.local[1],
  };

  requestEndpointDragFrame(params);
  return true;
}

function requestEndpointDragFrame(params: HandleEndpointDragPointerMoveParams) {
  if (params.pendingDragFrameRef.current !== null) {
    return;
  }

  params.pendingDragFrameRef.current = window.requestAnimationFrame(() => {
    params.pendingDragFrameRef.current = null;
    const next = params.pendingDragRef.current;
    params.pendingDragRef.current = null;
    if (!next) {
      return;
    }

    const result = resolveEndpointDragFrame({
      next,
      sketch: params.sketchLinesRef.current,
      planeId: params.activeSketchPlaneIdRef.current ?? "ref-plane-xy",
      planeFrame: params.activeSketchPlaneFrameRef.current,
      resolveSnappedSketchPoint: params.resolveSnappedSketchPoint,
      constraints: params.sketchConstraintsRef.current,
      sceneConstraints: params.sceneConstraintsRef.current,
    });
    applyEndpointDragFrameResult(params, result);
    params.requestRender();
  });
}

function applyEndpointDragFrameResult(
  params: HandleEndpointDragPointerMoveParams,
  result: ReturnType<typeof resolveEndpointDragFrame>,
) {
  params.dragSnapResultRef.current = {
    snapX: result.sketchPoint.local[0],
    snapY: result.sketchPoint.local[1],
  };
  params.setSketchSnapLabel(result.sketchPoint.snapLabel);

  // Live preview: write the solved positions into the real committed
  // scene objects so the actual geometry moves with the pointer.
  params.dragPreviewMutatingRef.current = true;
  applySolvedPointsToSketchScene({
    solvedPoints: result.solvedPoints,
    sketch: params.sketchLinesRef.current,
    planeId: params.activeSketchPlaneIdRef.current ?? "ref-plane-xy",
    planeFrame: params.activeSketchPlaneFrameRef.current,
    sketchEntityObjectById: params.sketchEntityObjectByIdRef.current,
    sketchPointObjectById: params.sketchPointObjectByIdRef.current,
    sketchConstraintObjects: params.sketchConstraintObjectsRef.current,
    sketchProfileObjects: params.sketchProfileObjectsRef.current,
    constraintDeltas: result.constraintDeltas,
  });
}
