import type {
  SketchConstraintScene,
  SketchFeatureParameters,
  SketchPlaneFrame,
  SketchPreviewPoint,
} from "@/types";
import { toWorldPoint } from "@/utils";
import { getBridge } from "@/lib/planegcsSolver";
import type { SketchConstraintData } from "@/lib/planegcsBridge";
import type { ResolveSnapOptions } from "./snapResolution";

export interface EndpointDrag {
  vertexId: string;
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

/**
 * World-space deltas for constraint badges that annotate geometry touched
 * by the dragged vertex.  Vertex badges (coincident/fixed) follow the
 * vertex; badges on a line containing the vertex sit at the line midpoint,
 * which moves by half the vertex delta.  Exact positions re-sync on commit.
 */
export function endpointDragConstraintDeltas(
  constraints: readonly SketchConstraintScene[],
  sketch: SketchFeatureParameters | null,
  draggedVertexId: string,
  deltaLocal: [number, number],
  planeId: string,
  planeFrame: SketchPlaneFrame | null,
): Map<string, [number, number, number]> {
  const deltas = new Map<string, [number, number, number]>();
  if (!sketch || (deltaLocal[0] === 0 && deltaLocal[1] === 0)) {
    return deltas;
  }

  const base = sketch.vertices.find((v) => v.vertex_id === draggedVertexId);
  const baseLocal: [number, number] = base ? [base.x, base.y] : [0, 0];
  const worldStart = toWorldPoint(planeId, baseLocal, planeFrame);
  const worldEnd = toWorldPoint(
    planeId,
    [baseLocal[0] + deltaLocal[0], baseLocal[1] + deltaLocal[1]],
    planeFrame,
  );
  const fullDelta: [number, number, number] = [
    worldEnd[0] - worldStart[0],
    worldEnd[1] - worldStart[1],
    worldEnd[2] - worldStart[2],
  ];
  const halfDelta: [number, number, number] = [
    fullDelta[0] / 2,
    fullDelta[1] / 2,
    fullDelta[2] / 2,
  ];

  const lineContainsVertex = sketch.lines.some(
    (line) =>
      line.start_vertex_id === draggedVertexId ||
      line.end_vertex_id === draggedVertexId,
  );

  for (const constraint of constraints) {
    if (constraint.entityId === draggedVertexId) {
      deltas.set(constraint.constraintId, fullDelta);
    } else if (
      lineContainsVertex &&
      sketch.lines.some((line) => line.line_id === constraint.entityId)
    ) {
      deltas.set(constraint.constraintId, halfDelta);
    }
  }
  return deltas;
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
    const isStart = line.start_vertex_id === pointId;
    const isEnd   = line.end_vertex_id === pointId;
    if (!isStart && !isEnd) {
      continue;
    }
    const anchoredId = isStart
      ? line.end_vertex_id
      : line.start_vertex_id;
    const anchored = params.vertices.find((point) => point.vertex_id === anchoredId);
    if (anchored) {
      anchors.push(anchored);
    }
  }

  // Arc endpoints: anchored to the opposite arc endpoint.
  if (params.arcs) {
    for (const arc of params.arcs) {
      const isStart = arc.start_vertex_id === pointId;
      const isEnd   = arc.end_vertex_id === pointId;
      if (!isStart && !isEnd) continue;
      anchors.push({
        x: isStart ? arc.end_x : arc.start_x,
        y: isStart ? arc.end_y : arc.start_y,
      });
    }
  }

  // Circle center: anchor is the circle's current center (so we get a
  // line from old center → new center).
  if (anchors.length === 0) {
    const circle = params.circles.find((c) =>
      c.center_vertex_id === pointId ||
      `point-circle-${c.circle_id}-center` === pointId,
    );
    if (circle) {
      anchors.push({ x: circle.center_x, y: circle.center_y });
    }
  }

  // Arc center: anchor is the arc's current center.
  if (anchors.length === 0) {
    const arc = params.arcs?.find((a) =>
      a.center_vertex_id === pointId ||
      `point-arc-${a.arc_id}-center` === pointId,
    );
    if (arc) {
      anchors.push({ x: arc.center_x, y: arc.center_y });
    }
  }

  return anchors;
}

export interface PendingEndpointDragFrame {
  vertexId: string;
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
/** 1-hop ripple set seeded with multiple vertices — used by both endpoint
 *  drag and the Move tool (same freeze semantics, different seed sets). */
export function computeRippleActivePointsForVertices(
  sketch: SketchFeatureParameters | null,
  vertexIds: readonly string[],
): string[] {
  if (!sketch) return [...vertexIds];

  const active = new Set<string>(vertexIds);
  const seeded = new Set(vertexIds);

  const collectNeighbors = (pointId: string) => {
    for (const line of sketch.lines) {
      if (line.start_vertex_id === pointId) {
        active.add(line.end_vertex_id);
      } else if (line.end_vertex_id === pointId) {
        active.add(line.start_vertex_id);
      }
    }
    if (sketch.arcs) {
      for (const arc of sketch.arcs) {
        if (arc.start_vertex_id === pointId) {
          active.add(arc.end_vertex_id);
        } else if (arc.end_vertex_id === pointId) {
          active.add(arc.start_vertex_id);
        }
      }
    }
    const isCircleCenter = sketch.circles.some((c) =>
      c.center_vertex_id === pointId ||
      `point-circle-${c.circle_id}-center` === pointId,
    );
    if (isCircleCenter) {
      active.add(pointId);
    }
  };

  for (const pointId of [...seeded]) {
    collectNeighbors(pointId);
  }

  return Array.from(active);
}

export function computeRippleActivePoints(
  sketch: SketchFeatureParameters | null,
  draggedPointId: string,
): string[] {
  return computeRippleActivePointsForVertices(sketch, [draggedPointId]);
}

export function resolveEndpointDragFrame({
  next,
  sketch,
  planeId,
  planeFrame,
  resolveSnappedSketchPoint,
  constraints,
  sceneConstraints,
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
    options?: ResolveSnapOptions,
  ) => SketchPreviewPoint;
  /** planegcs constraint data from the viewport state, for WASM solver. */
  constraints?: SketchConstraintData[];
  /** Scene constraint data, for badge-follow deltas. */
  sceneConstraints?: readonly SketchConstraintScene[];
}) {
  const world = toWorldPoint(planeId, [next.x, next.y], planeFrame);
  const anchorLocal = endpointDragAnchorLocal(sketch, next.vertexId);
  const sketchPoint = resolveSnappedSketchPoint(
    {
      local: [next.x, next.y],
      world: [world[0], world[1], world[2]],
    },
    anchorLocal,
    { dynamicSnapsEnabled: false },
  );

  // Run planegcs WASM solver for constraint-aware drag preview.
  // Shallow-clone params with the dragged point at the snap position
  // so the solver can compute constraint effects on connected geometry.
  // Ripple-freeze: only the dragged point and its 1-hop neighbours
  // participate — all other geometry is frozen to prevent ghost movement.
  let finalLocal: [number, number] = [sketchPoint.local[0], sketchPoint.local[1]];
  const solvedPoints = new Map<string, [number, number]>();
  const gcsBridge = getBridge();
  if (gcsBridge && sketch) {
    // Strip H/V constraints from lines during drag preview so the
    // solver allows free diagonal movement.  The H/V constraints are
    // re-applied on commit by the C++ core (propagate_connected_point_move
    // → refresh_sketch_derived_state).
    const unconstrainedLines = sketch.lines.map((l) => ({
      ...l,
      constraint: null as "horizontal" | "vertical" | null,
    }));
    const paramsCopy: SketchFeatureParameters = {
      ...sketch,
      lines: unconstrainedLines,
      vertices: sketch.vertices.map((p) =>
        p.vertex_id === next.vertexId
          ? { ...p, x: sketchPoint.local[0], y: sketchPoint.local[1] }
          : p,
      ),
    };
    const activePointIds = computeRippleActivePoints(sketch, next.vertexId);
    const result = gcsBridge.solve(paramsCopy, constraints ?? [], {
      activePointIds,
    });
    if (result.ok) {
      for (const point of result.points) {
        solvedPoints.set(point.id, [point.x, point.y]);
      }
      const solved = solvedPoints.get(next.vertexId);
      if (solved) {
        finalLocal = solved;
      }
    }
  }

  // Solve failure (or missing bridge): preview just the dragged point at
  // the snapped position; everything else stays at its base position.
  if (solvedPoints.size === 0 && sketch) {
    for (const vertex of sketch.vertices) {
      solvedPoints.set(vertex.vertex_id, [vertex.x, vertex.y]);
    }
    solvedPoints.set(next.vertexId, finalLocal);
  }

  const baseVertex = sketch?.vertices.find((v) => v.vertex_id === next.vertexId);
  const baseLocal: [number, number] = baseVertex
    ? [baseVertex.x, baseVertex.y]
    : finalLocal;

  return {
    sketchPoint: { ...sketchPoint, local: finalLocal },
    solvedPoints,
    constraintDeltas: endpointDragConstraintDeltas(
      sceneConstraints ?? [],
      sketch,
      next.vertexId,
      [finalLocal[0] - baseLocal[0], finalLocal[1] - baseLocal[1]],
      planeId,
      planeFrame,
    ),
  };
}