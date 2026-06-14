import * as THREE from "three";

import type {
  SketchFeatureParameters,
  SketchPlaneFrame,
  SketchPreviewPoint,
} from "@/types";
import { resolveSketchPlanePoint } from "@/utils";
import type { SketchConstraintData } from "@/lib/planegcsBridge";
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
  pendingDragRef: MutableRef<PendingEndpointDragFrame | null>;
  pendingDragFrameRef: MutableRef<number | null>;
  dragSnapResultRef: MutableRef<{ snapX: number; snapY: number } | null>;
  dragCursorRef: MutableRef<{ x: number; y: number } | null>;
  dragPreviewLinesRef: MutableRef<THREE.Line[]>;
  sketchGroupRef: MutableRef<THREE.Group | null>;
  resolveSnappedSketchPoint: ResolveSnappedSketchPoint;
  setSketchSnapLabel: (label: string | null) => void;
  clearDragPreviewLines: () => void;
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
    pointId: endpointDrag.pointId,
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
    });
    applyEndpointDragFrameResult(params, result);
    params.requestRender();
  });
}

function applyEndpointDragFrameResult(
  {
    dragSnapResultRef,
    dragPreviewLinesRef,
    sketchGroupRef,
    setSketchSnapLabel,
    clearDragPreviewLines,
  }: HandleEndpointDragPointerMoveParams,
  result: ReturnType<typeof resolveEndpointDragFrame>,
) {
  dragSnapResultRef.current = {
    snapX: result.sketchPoint.local[0],
    snapY: result.sketchPoint.local[1],
  };
  setSketchSnapLabel(result.sketchPoint.snapLabel);

  clearDragPreviewLines();
  const sketchGroup = sketchGroupRef.current;
  if (sketchGroup) {
    for (const preview of result.previewLines) {
      sketchGroup.add(preview);
    }
    dragPreviewLinesRef.current = result.previewLines;
  }
}
