import * as THREE from "three";

import type {
  SketchFeatureParameters,
  SketchPlaneFrame,
  SketchPreviewPoint,
} from "@/types";
import { toWorldPoint } from "@/utils";
import { getBridge } from "@/lib/planegcsSolver";
import type { SketchConstraintData } from "@/lib/planegcsBridge";

export interface EndpointDrag {
  pointId: string;
  startClientX: number;
  startClientY: number;
  startLocalX: number;
  startLocalY: number;
  hasMoved: boolean;
  /** True while an IPC update is in flight; intermediate frames are dropped. */
  inFlight: boolean;
}

export interface EndpointDragDelta {
  dx: number;
  dy: number;
  hasMoved: boolean;
}

export function endpointDragDelta(
  event: PointerEvent,
  drag: EndpointDrag,
  threshold = 4,
): EndpointDragDelta {
  const dx = event.clientX - drag.startClientX;
  const dy = event.clientY - drag.startClientY;
  return {
    dx,
    dy,
    hasMoved: Math.abs(dx) > threshold || Math.abs(dy) > threshold,
  };
}

export function endpointDragHasMoved(
  event: PointerEvent,
  drag: EndpointDrag,
  threshold = 3,
) {
  return endpointDragDelta(event, drag, threshold).hasMoved;
}

export function endpointDragCursorPosition(
  event: PointerEvent,
  canvasRect: DOMRect,
) {
  return {
    x: event.clientX - canvasRect.left,
    y: event.clientY - canvasRect.top,
  };
}

export function endpointDragAnchorLocal(
  params: SketchFeatureParameters | null,
  pointId: string,
): [number, number] | null {
  const anchor = endpointDragAnchors(params, pointId)[0];
  return anchor ? [anchor.x, anchor.y] : null;
}

export interface EndpointDragPreviewSegment {
  start: [number, number];
  end: [number, number];
}

/** Circle center drag metadata — carried alongside preview segments so the
 *  renderer can also draw a dashed circle outline at the preview position. */
export interface CircleCenterDragPreview {
  kind: "circle_center_drag";
  oldCenter: [number, number];
  newCenter: [number, number];
  radius: number;
}

export function endpointDragPreviewSegments(
  params: SketchFeatureParameters | null,
  pointId: string,
  snappedLocal: [number, number],
): EndpointDragPreviewSegment[] {
  if (!params) {
    return [];
  }

  return endpointDragAnchors(params, pointId).map((anchor) => ({
    start: [anchor.x, anchor.y],
    end: snappedLocal,
  }));
}

/** If pointId is a circle center, returns the drag preview metadata.
 *  Returns null for non-circle points. */
export function circleCenterDragPreview(
  params: SketchFeatureParameters | null,
  pointId: string,
  snappedLocal: [number, number],
): CircleCenterDragPreview | null {
  if (!params) return null;

  const match = /^point-circle-(.+)-center$/.exec(pointId);
  if (!match) return null;

  const circleId = match[1];
  const circle = params.circles.find((c) => c.circle_id === circleId);
  if (!circle) return null;

  return {
    kind: "circle_center_drag",
    oldCenter: [circle.center_x, circle.center_y],
    newCenter: [snappedLocal[0], snappedLocal[1]],
    radius: circle.radius,
  };
}

function endpointDragAnchors(
  params: SketchFeatureParameters | null,
  pointId: string,
) {
  if (!params) {
    return [];
  }

  const anchors: Array<{ x: number; y: number }> = [];

  // Line endpoints: anchored to the opposite endpoint.
  for (const line of params.lines) {
    if (line.start_point_id !== pointId && line.end_point_id !== pointId) {
      continue;
    }
    const anchoredId =
      line.start_point_id === pointId ? line.end_point_id : line.start_point_id;
    const anchored = params.points.find((point) => point.point_id === anchoredId);
    if (anchored) {
      anchors.push(anchored);
    }
  }

  // Circle center: anchor is the circle's current center (so we get a
  // line from old center → new center).
  if (anchors.length === 0) {
    const circleMatch = /^point-circle-(.+)-center$/.exec(pointId);
    if (circleMatch) {
      const circle = params.circles.find((c) => c.circle_id === circleMatch[1]);
      if (circle) {
        anchors.push({ x: circle.center_x, y: circle.center_y });
      }
    }
  }

  return anchors;
}

export function buildEndpointDragPreviewLines({
  segments,
  planeId,
  planeFrame,
  color,
}: {
  segments: readonly EndpointDragPreviewSegment[];
  planeId: string;
  planeFrame: SketchPlaneFrame | null;
  /** Hex color for the preview lines. Yellow (0xffe784) for TS-only snap,
   *  cornflower blue (0x6495ed) when constraint-solver was used. */
  color?: number;
}) {
  const lineColor = color ?? 0xffe784;
  return segments.map((segment) => {
    const mat = new THREE.LineDashedMaterial({
      color: lineColor,
      transparent: true,
      opacity: 0.6,
      dashSize: 1.5,
      gapSize: 0.8,
    });
    const geo = new THREE.BufferGeometry().setFromPoints([
      new THREE.Vector3(...toWorldPoint(planeId, segment.start, planeFrame)),
      new THREE.Vector3(...toWorldPoint(planeId, segment.end, planeFrame)),
    ]);
    const preview = new THREE.Line(geo, mat);
    preview.computeLineDistances();
    return preview;
  });
}

export function buildCircleDragPreviewObject({
  circlePreview,
  planeId,
  planeFrame,
  color,
}: {
  circlePreview: CircleCenterDragPreview;
  planeId: string;
  planeFrame: SketchPlaneFrame | null;
  color?: number;
}): THREE.Line {
  const segments = 64;
  const points: THREE.Vector3[] = [];
  const cx = circlePreview.newCenter[0];
  const cy = circlePreview.newCenter[1];
  const r = circlePreview.radius;

  for (let i = 0; i <= segments; i++) {
    const angle = (i / segments) * Math.PI * 2;
    const world = toWorldPoint(
      planeId,
      [cx + Math.cos(angle) * r, cy + Math.sin(angle) * r],
      planeFrame,
    );
    points.push(new THREE.Vector3(...world));
  }

  const mat = new THREE.LineDashedMaterial({
    color: color ?? 0xffe784,
    transparent: true,
    opacity: 0.5,
    dashSize: 2.0,
    gapSize: 1.0,
  });
  const geo = new THREE.BufferGeometry().setFromPoints(points);
  const preview = new THREE.Line(geo, mat);
  preview.computeLineDistances();
  return preview;
}

export interface PendingEndpointDragFrame {
  pointId: string;
  x: number;
  y: number;
}

/**
 * Compute the 1-hop connected point set from a dragged point for
 * ripple-freeze during constraint-aware drag preview.
 *
 * The active set includes:
 *   - The dragged point itself
 *   - The opposite endpoint of every line connected to the dragged point
 *   - The circle center for any circle whose center is the dragged point
 *
 * All other points are frozen (fixed=true) during the WASM solve, so only
 * the dragged point and its immediate neighbours can move.
 */
export function computeRippleActivePoints(
  sketch: SketchFeatureParameters | null,
  draggedPointId: string,
): string[] {
  if (!sketch) return [draggedPointId];

  const active = new Set<string>();
  active.add(draggedPointId);

  for (const line of sketch.lines) {
    if (line.start_point_id === draggedPointId) {
      active.add(line.end_point_id);
    } else if (line.end_point_id === draggedPointId) {
      active.add(line.start_point_id);
    }
  }

  // Include circle center if this is a center point.
  const centerMatch = /^point-circle-(.+)-center$/.exec(draggedPointId);
  if (centerMatch) {
    active.add(draggedPointId); // already added, just being explicit
  }

  // If the dragged point is a normal point that happens to be a circle
  // center, check if there's a circle whose center id matches.
  for (const circle of sketch.circles) {
    const centerId = `point-circle-${circle.circle_id}-center`;
    if (centerId === draggedPointId) {
      active.add(centerId);
    }
  }

  return Array.from(active);
}

export function resolveEndpointDragFrame({
  next,
  sketch,
  planeId,
  planeFrame,
  resolveSnappedSketchPoint,
  constraints,
}: {
  next: PendingEndpointDragFrame;
  sketch: SketchFeatureParameters | null;
  planeId: string;
  planeFrame: SketchPlaneFrame | null;
  resolveSnappedSketchPoint: (
    rawPoint: {
      local: [number, number];
      world: [number, number, number];
    },
    draftStartLocal?: [number, number] | null,
  ) => SketchPreviewPoint;
  /** planegcs constraint data from the viewport state, for WASM solver. */
  constraints?: SketchConstraintData[];
}) {
  const world = toWorldPoint(planeId, [next.x, next.y], planeFrame);
  const anchorLocal = endpointDragAnchorLocal(sketch, next.pointId);
  const sketchPoint = resolveSnappedSketchPoint(
    {
      local: [next.x, next.y],
      world: [world[0], world[1], world[2]],
    },
    anchorLocal,
  );

  // Run planegcs WASM solver for constraint-aware drag preview.
  // Shallow-clone params with the dragged point at the snap position
  // so the solver can compute constraint effects on connected geometry.
  // Ripple-freeze: only the dragged point and its 1-hop neighbours
  // participate — all other geometry is frozen to prevent ghost movement.
  let finalLocal: [number, number] = [sketchPoint.local[0], sketchPoint.local[1]];
  let solverUsed = false;
  const gcsBridge = getBridge();
  if (gcsBridge && sketch) {
    const paramsCopy: SketchFeatureParameters = {
      ...sketch,
      points: sketch.points.map((p) =>
        p.point_id === next.pointId
          ? { ...p, x: sketchPoint.local[0], y: sketchPoint.local[1] }
          : p,
      ),
    };
    const activePointIds = computeRippleActivePoints(sketch, next.pointId);
    const result = gcsBridge.solve(paramsCopy, constraints ?? [], {
      activePointIds,
    });
    if (result.ok) {
      const solved = result.points.find((p) => p.id === next.pointId);
      if (solved) {
        finalLocal = [solved.x, solved.y];
        solverUsed = true;
      }
    }
  }

  const previewLines: THREE.Line[] = buildEndpointDragPreviewLines({
    segments: endpointDragPreviewSegments(
      sketch,
      next.pointId,
      finalLocal,
    ),
    planeId,
    planeFrame,
    color: solverUsed ? 0x6495ed : undefined,
  });

  // Circle center drag: add a dashed circle outline at the preview position.
  const circlePreview = circleCenterDragPreview(sketch, next.pointId, finalLocal);
  if (circlePreview) {
    previewLines.push(
      buildCircleDragPreviewObject({
        circlePreview,
        planeId,
        planeFrame,
        color: solverUsed ? 0x6495ed : undefined,
      }),
    );
  }

  return {
    sketchPoint: { ...sketchPoint, local: finalLocal },
    previewLines,
  };
}
