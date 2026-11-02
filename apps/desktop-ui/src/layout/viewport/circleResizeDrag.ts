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
import {
  updateSketchArcObject,
  updateSketchCircleObject,
  updateSketchLineObject,
} from "./sceneIncrementalUpdate";
import {
  applySolvedPointsToSketchScene,
  sketchPlaneAxes,
} from "./sketchPreviewSceneUpdate";

/**
 * Perimeter-resize drag: grabbing a circle's edge resizes it — the centre
 * stays fixed, the radius follows the cursor, and any `point_on_circle`
 * -attached points ride the boundary radially (the same move the core
 * solver produces on commit).
 */

export interface CircleResizeDrag {
  /** Which curve the gesture grabbed — a circle and an arc share this drag. */
  kind: "circle" | "arc";
  /** `circle_id` or `arc_id`, matching `kind`. */
  entityId: string;
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
  if (
    !hit ||
    hit.kind !== "sketch_entity" ||
    (hit.entityKind !== "circle" && hit.entityKind !== "arc")
  ) {
    return false;
  }
  if (!sketch) return false;

  // Circles and arcs share this gesture: the centre stays put and only the
  // radius follows the cursor. An arc additionally keeps its angular span, so
  // it grows/shrinks as a shape instead of deforming.
  let kind: "circle" | "arc";
  let centerX: number;
  let centerY: number;
  let startRadius: number;
  if (hit.entityKind === "arc") {
    const arc = (sketch.arcs ?? []).find((a) => a.arc_id === hit.id);
    if (!arc) return false;
    kind = "arc";
    centerX = arc.center_x;
    centerY = arc.center_y;
    startRadius = arc.radius;
  } else {
    const circle = sketch.circles.find((c) => c.circle_id === hit.id);
    if (!circle) return false;
    kind = "circle";
    centerX = circle.center_x;
    centerY = circle.center_y;
    startRadius = circle.radius;
  }

  circleResizeDragRef.current = {
    kind,
    entityId: hit.id,
    centerX,
    centerY,
    startClientX: event.clientX,
    startClientY: event.clientY,
    startRadius,
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
  dragFollowDimensionsRef,
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
  /** Re-anchors the dimensions the drag moved, once the solved positions are in
   *  the scene objects (the sync pass skips those frames). */
  dragFollowDimensionsRef: MutableRef<(() => void) | null>;
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

  // ARC: same gesture, shape preserved. The centre is fixed and both endpoints
  // slide along the rays they already sit on, so the arc grows as an arc
  // instead of deforming. Everything welded to those endpoints (the lines the
  // trim left attached) is scaled radially with them, so the preview shows the
  // junction the commit will produce.
  if (drag.kind === "arc") {
    const arc = (sketch?.arcs ?? []).find((a) => a.arc_id === drag.entityId);
    if (!sketch || !arc) return true;
    const scale = radius / Math.max(drag.startRadius, 1e-9);
    const frame = activeSketchPlaneFrameRef.current;
    const radial = (x: number, y: number): [number, number] => [
      drag.centerX + (x - drag.centerX) * scale,
      drag.centerY + (y - drag.centerY) * scale,
    ];
    const moved = new Map<string, [number, number]>();
    moved.set(arc.start_vertex_id, radial(arc.start_x, arc.start_y));
    moved.set(arc.end_vertex_id, radial(arc.end_x, arc.end_y));
    const startLocal = moved.get(arc.start_vertex_id) as [number, number];
    const endLocal = moved.get(arc.end_vertex_id) as [number, number];
    const { xAxis, yAxis } = sketchPlaneAxes(activeSketchPlaneId, frame);

    dragPreviewMutatingRef.current = true;
    updateSketchArcObject({
      arcObject: sketchEntityObjectByIdRef.current.get(arc.arc_id),
      center: toWorldPoint(
        activeSketchPlaneId,
        [drag.centerX, drag.centerY],
        frame,
      ),
      start: toWorldPoint(activeSketchPlaneId, startLocal, frame),
      end: toWorldPoint(activeSketchPlaneId, endLocal, frame),
      radius,
      ccw: arc.ccw,
      previousSweep: arc.sweep_angle,
      xAxis,
      yAxis,
    });
    for (const [vertexId, position] of moved) {
      const sphere = sketchPointObjectByIdRef.current.get(vertexId);
      if (!sphere) continue;
      const world = toWorldPoint(activeSketchPlaneId, position, frame);
      sphere.position.set(world[0], world[1], world[2]);
    }
    for (const line of sketch.lines) {
      const nextStart = moved.get(line.start_vertex_id);
      const nextEnd = moved.get(line.end_vertex_id);
      if (!nextStart && !nextEnd) continue;
      updateSketchLineObject(
        sketchEntityObjectByIdRef.current.get(line.line_id),
        toWorldPoint(
          activeSketchPlaneId,
          nextStart ?? ([line.start_x, line.start_y] as [number, number]),
          frame,
        ),
        toWorldPoint(
          activeSketchPlaneId,
          nextEnd ?? ([line.end_x, line.end_y] as [number, number]),
          frame,
        ),
      );
    }
    setSketchSnapLabel(null);
    dragFollowDimensionsRef.current?.();
    requestRender();
    return true;
  }

  // Solve the attached points against the new radius (centre frozen) so they
  // ride the circle live, exactly as the core commit will.
  const solvedPoints = new Map<string, [number, number]>();
  const gcsBridge = getBridge();
  if (gcsBridge && sketch) {
    const paramsCopy: SketchFeatureParameters = {
      ...sketch,
      circles: sketch.circles.map((c) =>
        c.circle_id === drag.entityId ? { ...c, radius } : c,
      ),
    };
    const attachedIds = (sketchConstraintsRef.current ?? [])
      .filter(
        (c) =>
          c.kind === "point_on_circle" &&
          c.target_ids.length >= 2 &&
          c.target_ids[1] === drag.entityId,
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
        constraint.relatedEntityId !== drag.entityId
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
    circleObject: sketchEntityObjectByIdRef.current.get(drag.entityId),
    center: centerWorld,
    radius,
  });
  setSketchSnapLabel(null);
  dragFollowDimensionsRef.current?.();
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
  pendingEndpointCommitRef,
  updateSketchCircle,
  updateSketchArcRadius,
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
  /** Set when the resize commits: keeps the mutated preview on-screen across
   *  the async core round-trip, exactly like the endpoint-drag commit. */
  pendingEndpointCommitRef: MutableRef<boolean>;
  updateSketchCircle: (
    circleId: string,
    centerX: number,
    centerY: number,
    radius: number,
  ) => Promise<void> | void;
  /** ARC counterpart: the centre is fixed, only the radius changes. */
  updateSketchArcRadius: (
    arcId: string,
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
      // The commit is fire-and-forget; keep the preview on-screen until the
      // core's viewport state lands, or the next sync repaints the pre-drag
      // circle from the stale viewport (the "old position flashes on commit").
      pendingEndpointCommitRef.current = true;
      if (drag.kind === "arc") {
        void updateSketchArcRadius(drag.entityId, radius);
      } else {
        void updateSketchCircle(
          drag.entityId,
          drag.centerX,
          drag.centerY,
          radius,
        );
      }
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
