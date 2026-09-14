import * as THREE from "three";

import type { ArmedSketchConstraint, SketchFeatureParameters, SketchPlaneFrame } from "@/types";
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
  armedSketchConstraint: ArmedSketchConstraint;
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
  armedSketchConstraint,
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
      armedSketchConstraint,
    })
  ) {
    return { handled: true, clearPointerDown: true };
  }

  // Fusion-style: a click-drag anywhere on the sketch starts the
  // marquee selection — including drags that begin on a profile
  // surface, on an entity, or on a FIXED vertex (projected endpoints
  // can't be dragged, so a drag from them must marquee instead of
  // doing nothing). Non-fixed vertices still start the endpoint drag
  // above. A plain click (no movement) falls through:
  // finishRectangleSelectionDrag only consumes real drags, so the
  // pointer-up click handler still selects the hit entity / surface.
  // On an ACTIVE sketch, drags over BODY geometry (face/edge/vertex
  // hits) marquee too — sketch-on-face puts the whole sketch plane
  // on top of the model, and refusing to marquee there made window
  // selection impossible over the body. 3D mode (no active sketch)
  // keeps its own rules: marquee from empty space only.
  const startsMarquee =
    !hit ||
    hit.kind === "sketch_profile" ||
    hit.kind === "sketch_entity" ||
    hit.kind === "sketch_point" ||
    (activeSketchPlaneId !== null &&
      (hit.kind === "face" || hit.kind === "edge" || hit.kind === "vertex"));
  if (startsMarquee) {
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
