import * as THREE from "three";

import type {
  ArmedSketchConstraint,
  SketchConstraintScene,
  SketchFeatureParameters,
  SketchPlaneFrame,
} from "@/types";
import { resolveSketchPlanePoint, toWorldPoint } from "@/utils";
import { getBridge } from "@/lib/planegcsSolver";
import type { SketchConstraintData } from "@/lib/planegcsBridge";
import { useCadCoreStore } from "@/state";
import type { ViewportPickHit } from "./contextMenuState";
import { updateSketchCircleObject } from "./sceneIncrementalUpdate";
import { applySolvedPointsToSketchScene } from "./sketchPreviewSceneUpdate";

/**
 * Perimeter-resize drag: grabbing a circle's edge resizes it — the centre
 * stays fixed, the radius follows the cursor, and any `point_on_circle`
 * -attached points ride the boundary radially (the same move the core
 * solver produces on commit).
 */

export interface CircleResizeDrag {
  circleId: string;
  centerX: number;
  centerY: number;
  startClientX: number;
  startClientY: number;
  startRadius: number;
  hasMoved: boolean;
  inFlight: boolean;
  /** Document revision at pointer-down; the commit is discarded if it changed. */
  startRevision: number;
}

interface MutableRef<T> {
  current: T;
}

/** Radius implied by the cursor around a fixed centre, clamped to a positive
 *  minimum. Pure — the geometry kernel of the resize gesture. */
export function resolveCircleResizeRadius(
  cursorLocal: [number, number],
  centerX: number,
  centerY: number,
  minRadius = 1e-3,
): number {
  return Math.max(
    Math.hypot(cursorLocal[0] - centerX, cursorLocal[1] - centerY),
    minRadius,
  );
}

export function circleResizeHasMoved(
  event: PointerEvent,
  drag: CircleResizeDrag,
  threshold = 3,
): boolean {
  return (
    Math.abs(event.clientX - drag.startClientX) > threshold ||
    Math.abs(event.clientY - drag.startClientY) > threshold
  );
}

export function beginCircleResizePointerDown({
  event,
  renderer,
  controls,
  hit,
  sketch,
  circleResizeDragRef,
  armedSketchConstraint,
}: {
  event: PointerEvent;
  renderer: THREE.WebGLRenderer;
  controls: { enabled: boolean };
  hit: ViewportPickHit | null;
  sketch: SketchFeatureParameters | null;
  circleResizeDragRef: MutableRef<CircleResizeDrag | null>;
  armedSketchConstraint: ArmedSketchConstraint;
}): boolean {
  if (armedSketchConstraint) return false;
  if (!hit || hit.kind !== "sketch_entity" || hit.entityKind !== "circle") {
    return false;
  }
  if (!sketch) return false;
  const circle = sketch.circles.find((c) => c.circle_id === hit.id);
  if (!circle) return false;

  circleResizeDragRef.current = {
    circleId: circle.circle_id,
    centerX: circle.center_x,
    centerY: circle.center_y,
    startClientX: event.clientX,
    startClientY: event.clientY,
    startRadius: circle.radius,
    hasMoved: false,
    inFlight: false,
    startRevision: useCadCoreStore.getState().document?.revision ?? 0,
  };
  controls.enabled = false;
  renderer.domElement.setPointerCapture(event.pointerId);
  (renderer.domElement as HTMLCanvasElement).style.cursor = "none";
  return true;
}

export function handleCircleResizePointerMove({
  event,
  renderer,
  camera,
  circleResizeDragRef,
  activeSketchPlaneIdRef,
  activeSketchPlaneFrameRef,
  sketchLinesRef,
  sketchConstraintsRef,
  sceneConstraintsRef,
  sketchEntityObjectByIdRef,
  sketchPointObjectByIdRef,
  sketchConstraintObjectsRef,
  sketchProfileObjectsRef,
  dragPreviewMutatingRef,
  setSketchSnapLabel,
  requestRender,
}: {
  event: PointerEvent;
  renderer: THREE.WebGLRenderer;
  camera: THREE.Camera;
  circleResizeDragRef: MutableRef<CircleResizeDrag | null>;
  activeSketchPlaneIdRef: MutableRef<string | null>;
  activeSketchPlaneFrameRef: MutableRef<SketchPlaneFrame | null>;
  sketchLinesRef: MutableRef<SketchFeatureParameters | null>;
  sketchConstraintsRef: MutableRef<SketchConstraintData[]>;
  sceneConstraintsRef: MutableRef<readonly SketchConstraintScene[]>;
  sketchEntityObjectByIdRef: MutableRef<Map<string, THREE.Line | THREE.LineLoop>>;
  sketchPointObjectByIdRef: MutableRef<Map<string, THREE.Mesh>>;
  sketchConstraintObjectsRef: MutableRef<THREE.Object3D[]>;
  sketchProfileObjectsRef: MutableRef<THREE.Group[]>;
  dragPreviewMutatingRef: MutableRef<boolean>;
  setSketchSnapLabel: (label: string | null) => void;
  requestRender: () => void;
}): boolean {
  const drag = circleResizeDragRef.current;
  const activeSketchPlaneId = activeSketchPlaneIdRef.current;
  if (!drag || !activeSketchPlaneId) return false;

  const rawPoint = resolveSketchPlanePoint(
    event,
    renderer,
    camera,
    activeSketchPlaneId,
    activeSketchPlaneFrameRef.current,
  );
  if (!rawPoint) return true;

  if (circleResizeHasMoved(event, drag)) drag.hasMoved = true;

  const radius = resolveCircleResizeRadius(
    rawPoint.local,
    drag.centerX,
    drag.centerY,
  );
  const sketch = sketchLinesRef.current;

  // Solve the attached points against the new radius (centre frozen) so they
  // ride the circle live, exactly as the core commit will.
  const solvedPoints = new Map<string, [number, number]>();
  const gcsBridge = getBridge();
  if (gcsBridge && sketch) {
    const paramsCopy: SketchFeatureParameters = {
      ...sketch,
      circles: sketch.circles.map((c) =>
        c.circle_id === drag.circleId ? { ...c, radius } : c,
      ),
    };
    const attachedIds = (sketchConstraintsRef.current ?? [])
      .filter(
        (c) =>
          c.kind === "point_on_circle" &&
          c.target_ids.length >= 2 &&
          c.target_ids[1] === drag.circleId,
      )
      .map((c) => c.target_ids[0]);
    const result = gcsBridge.solve(
      paramsCopy,
      sketchConstraintsRef.current ?? [],
      { activePointIds: attachedIds },
    );
    if (result.ok) {
      for (const p of result.points) solvedPoints.set(p.id, [p.x, p.y]);
    }
  }

  dragPreviewMutatingRef.current = true;

  // Badge follow: each point_on_circle badge sits on its attached point, which
  // rides radially during a resize. Move the badge by the point's displacement
  // (base → solved) so the "O" glyph tracks the endpoint instead of lagging.
  const constraintDeltas = new Map<string, [number, number, number]>();
  const sceneConstraints = sceneConstraintsRef.current;
  if (sceneConstraints && sketch) {
    const planeFrame = activeSketchPlaneFrameRef.current;
    for (const constraint of sceneConstraints) {
      if (
        constraint.kind !== "point_on_circle" ||
        constraint.relatedEntityId !== drag.circleId
      ) {
        continue;
      }
      const base = sketch.vertices.find(
        (v) => v.vertex_id === constraint.entityId,
      );
      const solved = solvedPoints.get(constraint.entityId);
      if (!base || !solved) continue;
      const baseWorld = toWorldPoint(
        activeSketchPlaneId,
        [base.x, base.y],
        planeFrame,
      );
      const solvedWorld = toWorldPoint(activeSketchPlaneId, solved, planeFrame);
      constraintDeltas.set(constraint.constraintId, [
        solvedWorld[0] - baseWorld[0],
        solvedWorld[1] - baseWorld[1],
        solvedWorld[2] - baseWorld[2],
      ]);
    }
  }

  applySolvedPointsToSketchScene({
    solvedPoints,
    sketch,
    planeId: activeSketchPlaneId,
    planeFrame: activeSketchPlaneFrameRef.current,
    sketchEntityObjectById: sketchEntityObjectByIdRef.current,
    sketchPointObjectById: sketchPointObjectByIdRef.current,
    sketchConstraintObjects: sketchConstraintObjectsRef.current,
    sketchProfileObjects: sketchProfileObjectsRef.current,
    constraintDeltas,
  });
  // Override the circle's radius (the shared preview pass re-samples it at the
  // committed radius); the centre is fixed at its pointer-down position.
  const centerWorld = toWorldPoint(
    activeSketchPlaneId,
    [drag.centerX, drag.centerY],
    activeSketchPlaneFrameRef.current,
  );
  updateSketchCircleObject({
    circleObject: sketchEntityObjectByIdRef.current.get(drag.circleId),
    center: centerWorld,
    radius,
  });
  setSketchSnapLabel(null);
  requestRender();
  return true;
}

export function finishCircleResizePointerUp({
  event,
  renderer,
  camera,
  controls,
  circleResizeDragRef,
  activeSketchPlaneIdRef,
  activeSketchPlaneFrameRef,
  dragPreviewMutatingRef,
  updateSketchCircle,
  restorePreviewScene,
  setSketchSnapLabel,
  setHoveredSketchEntity,
  setHoveredSketchPoint,
  setPointerDown,
}: {
  event: PointerEvent;
  renderer: THREE.WebGLRenderer;
  camera: THREE.Camera;
  controls: { enabled: boolean };
  circleResizeDragRef: MutableRef<CircleResizeDrag | null>;
  activeSketchPlaneIdRef: MutableRef<string | null>;
  activeSketchPlaneFrameRef: MutableRef<SketchPlaneFrame | null>;
  dragPreviewMutatingRef: MutableRef<boolean>;
  updateSketchCircle: (
    circleId: string,
    centerX: number,
    centerY: number,
    radius: number,
  ) => Promise<void> | void;
  restorePreviewScene: () => void;
  setSketchSnapLabel: (label: string | null) => void;
  setHoveredSketchEntity: (entityId: string | null) => void;
  setHoveredSketchPoint: (pointId: string | null) => void;
  setPointerDown: (point: { x: number; y: number } | null) => void;
}): "inactive" | "continue" | "consumed" {
  const drag = circleResizeDragRef.current;
  if (!drag) return "inactive";

  const hasMoved = circleResizeHasMoved(event, drag, 4);
  const currentRevision = useCadCoreStore.getState().document?.revision ?? 0;
  if (hasMoved && currentRevision !== drag.startRevision) {
    circleResizeDragRef.current = null;
    restorePreviewScene();
    controls.enabled = true;
    (renderer.domElement as HTMLCanvasElement).style.cursor = "";
    setSketchSnapLabel(null);
    setHoveredSketchEntity(null);
    setHoveredSketchPoint(null);
    setPointerDown(null);
    return "consumed";
  }

  if (hasMoved) {
    const rawPoint = resolveSketchPlanePoint(
      event,
      renderer,
      camera,
      activeSketchPlaneIdRef.current,
      activeSketchPlaneFrameRef.current,
    );
    if (rawPoint) {
      const radius = resolveCircleResizeRadius(
        rawPoint.local,
        drag.centerX,
        drag.centerY,
      );
      void updateSketchCircle(drag.circleId, drag.centerX, drag.centerY, radius);
    }
  } else {
    restorePreviewScene();
  }

  circleResizeDragRef.current = null;
  dragPreviewMutatingRef.current = false;
  controls.enabled = true;
  (renderer.domElement as HTMLCanvasElement).style.cursor = "";
  setSketchSnapLabel(null);
  setHoveredSketchEntity(null);
  setHoveredSketchPoint(null);

  if (hasMoved) {
    setPointerDown(null);
    return "consumed";
  }
  setPointerDown({ x: event.clientX, y: event.clientY });
  return "continue";
}
