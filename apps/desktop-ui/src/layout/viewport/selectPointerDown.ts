import * as THREE from "three";

import type { SketchFeatureParameters, SketchPlaneFrame } from "@/types";
import type { ViewportPickHit } from "./contextMenuState";
import type { EndpointDrag } from "./endpointDrag";
import { beginEndpointDragPointerDown } from "./endpointDragPointerDown";
import type { SelectionDrag } from "./selectionGeometry";

interface MutableRef<T> {
  current: T;
}

interface BeginSelectPointerDownParams {
  event: PointerEvent;
  renderer: THREE.WebGLRenderer;
  camera: THREE.Camera;
  controls: { enabled: boolean };
  activeSketchPlaneId: string | null;
  activeSketchPlaneFrame: SketchPlaneFrame | null;
  sketch: SketchFeatureParameters | null;
  endpointDragRef: MutableRef<EndpointDrag | null>;
  selectionDragRef: MutableRef<SelectionDrag | null>;
  intersectSceneTargets: (event: PointerEvent) => ViewportPickHit | null;
}

export interface SelectPointerDownResult {
  handled: boolean;
  clearPointerDown: boolean;
}

export function beginSelectPointerDown({
  event,
  renderer,
  camera,
  controls,
  activeSketchPlaneId,
  activeSketchPlaneFrame,
  sketch,
  endpointDragRef,
  selectionDragRef,
  intersectSceneTargets,
}: BeginSelectPointerDownParams): SelectPointerDownResult {
  const hit = intersectSceneTargets(event);

  if (
    hit?.kind === "sketch_point" &&
    beginEndpointDragPointerDown({
      event,
      renderer,
      camera,
      controls,
      hit,
      activeSketchPlaneId,
      activeSketchPlaneFrame,
      sketch,
      endpointDragRef,
    })
  ) {
    return { handled: true, clearPointerDown: true };
  }

  if (!hit) {
    selectionDragRef.current = {
      startX: event.clientX,
      startY: event.clientY,
      currentX: event.clientX,
      currentY: event.clientY,
      active: true,
    };
    controls.enabled = false;
    return { handled: true, clearPointerDown: false };
  }

  return { handled: false, clearPointerDown: false };
}
