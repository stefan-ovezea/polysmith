import * as THREE from "three";

import type { SketchFeatureParameters, SketchPlaneFrame } from "@/types";
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
}: BeginEndpointDragPointerDownParams) {
  if (!hit || !activeSketchPlaneId || !sketch) {
    return false;
  }

  const point = sketch.points?.find((entry) => entry.point_id === hit.id);
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
    pointId: hit.id,
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
