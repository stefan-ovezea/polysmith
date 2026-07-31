import * as THREE from "three";

import type { ArmedSketchConstraint, SketchFeatureParameters, SketchPlaneFrame } from "@/types";
import { resolveSketchPlanePoint } from "@/utils";
import type { EndpointDrag } from "./endpointDrag";

interface MutableRef<T> {
  current: T;
}

type SketchPointHit = {
  kind: "sketch_point";
  id: string;
};

interface BeginEndpointDragPointerDownParams {
  event: PointerEvent;
  renderer: THREE.WebGLRenderer;
  camera: THREE.Camera;
  controls: { enabled: boolean };
  hit: SketchPointHit | null;
  activeSketchPlaneId: string | null;
  activeSketchPlaneFrame: SketchPlaneFrame | null;
  sketch: SketchFeatureParameters | null;
  endpointDragRef: MutableRef<EndpointDrag | null>;
  armedSketchConstraint: ArmedSketchConstraint;
}

export function beginEndpointDragPointerDown({
  event,
  renderer,
  camera,
  controls,
  hit,
  activeSketchPlaneId,
  activeSketchPlaneFrame,
  sketch,
  endpointDragRef,
  armedSketchConstraint,
}: BeginEndpointDragPointerDownParams) {
  if (!hit || !activeSketchPlaneId || !sketch) {
    return false;
  }

  // When a constraint is armed (fix, coincident, etc.), clicking a
  // point should apply the constraint — not start an endpoint drag.
  if (armedSketchConstraint) {
    return false;
  }

  const point = sketch.vertices?.find((entry) => entry.vertex_id === hit.id);
  if (!point || point.is_fixed) {
    return false;
  }

  const rawPoint = resolveSketchPlanePoint(
    event,
    renderer,
    camera,
    activeSketchPlaneId,
    activeSketchPlaneFrame,
  );
  if (!rawPoint) {
    return false;
  }

  endpointDragRef.current = {
    vertexId: point.vertex_id,
    startClientX: event.clientX,
    startClientY: event.clientY,
    startLocalX: rawPoint.local[0],
    startLocalY: rawPoint.local[1],
    hasMoved: false,
    inFlight: false,
  };
  controls.enabled = false;
  renderer.domElement.setPointerCapture(event.pointerId);
  (renderer.domElement as HTMLCanvasElement).style.cursor = "none";
  return true;
}
