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
  /** Document revision at pointer-down (M24). The pointer-up commit is
   *  discarded when the document changed mid-gesture (e.g. an awaited
   *  undo landed while dragging) — the point under the cursor no longer
   *  means what it meant at drag start. */
  startRevision: number;
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

  // Dragging a circle centre: every point_on_circle badge on that circle sits on
  // an attached point that translates with the centre by the full delta, so the
  // "O" glyph must ride along instead of snapping only when the drag stops.
  const draggedCircle = sketch.circles.find(
    (c) =>
      c.center_vertex_id === draggedVertexId ||
      `point-circle-${c.circle_id}-center` === draggedVertexId,
  );
  if (draggedCircle) {
    for (const constraint of constraints) {
      if (
        constraint.kind === "point_on_circle" &&
        constraint.relatedEntityId === draggedCircle.circle_id
      ) {
        deltas.set(constraint.constraintId, fullDelta);
      }
      // The "T" tangent badge is mounted at the LINE's midpoint, not on a
      // vertex. The line's attached end rides the circle by the full delta, so
      // its midpoint moves by half — without this the badge stayed behind
      // while the geometry moved. (`kind` is a line-relation here, which the
      // scene's ConstraintType union does not enumerate.)
      if (
        (constraint.kind as string) === "tangent_line_circle" &&
        constraint.relatedEntityId === draggedCircle.circle_id
      ) {
        deltas.set(constraint.constraintId, halfDelta);
      }
    }
  }

  // Badges mounted on a LINE (the "H" / "V" glyphs sit at a line's midpoint) move
  // by half the vertex delta — but only on the line(s) that actually contain the
  // dragged vertex. The old test was `lineContainsVertex && sketch.lines.some(...)`,
  // which is true for EVERY line in the sketch, so dragging one vertex slid the
  // "H" badge of every other horizontal line by half the delta: the user saw "a
  // totally unrelated line steals the H constraint during preview and jumps back
  // to its own line during commit".
  const draggedLineIds = new Set(
    sketch.lines
      .filter(
        (line) =>
          line.start_vertex_id === draggedVertexId ||
          line.end_vertex_id === draggedVertexId,
      )
      .map((line) => line.line_id),
  );

  // How far the FAR end moves, following the same rule the core commits and the
  // preview applies: a dimensioned line translates (far end by the full delta), an
  // H/V line carries only the off-axis part. A badge sits at its line's midpoint,
  // so it moves by the average, and a point_on_circle badge on the far end moves
  // by the far end's own delta. Using the old flat "half delta" left the "O" badge
  // behind after a translation and put the "H" badge in the wrong place.
  let farDelta: [number, number, number] = [0, 0, 0];
  const draggedLine = sketch.lines.find((line) =>
    draggedLineIds.has(line.line_id),
  );
  if (draggedLine) {
    const isStart = draggedLine.start_vertex_id === draggedVertexId;
    const farId = isStart
      ? draggedLine.end_vertex_id
      : draggedLine.start_vertex_id;
    const dimensioned = (sketch.dimensions ?? []).some((dimension) => {
      if (dimension.driven || dimension.is_auto) return false;
      if (dimension.kind === "line_length") {
        return dimension.entity_id === draggedLine.line_id;
      }
      if (dimension.kind !== "point_distance") return false;
      return (
        (dimension.entity_id === draggedLine.start_vertex_id &&
          dimension.secondary_entity_id === draggedLine.end_vertex_id) ||
        (dimension.entity_id === draggedLine.end_vertex_id &&
          dimension.secondary_entity_id === draggedLine.start_vertex_id)
      );
    });
    if (dimensioned) {
      farDelta = fullDelta;
    } else if (draggedLine.constraint === "horizontal") {
      farDelta = [0, fullDelta[1], 0];
    } else if (draggedLine.constraint === "vertical") {
      farDelta = [fullDelta[0], 0, 0];
    }
    for (const constraint of constraints) {
      if (
        constraint.entityId === farId &&
        (farDelta[0] !== 0 || farDelta[1] !== 0 || farDelta[2] !== 0)
      ) {
        deltas.set(constraint.constraintId, farDelta);
      }
    }
  }
  const lineMidDelta: [number, number, number] = [
    (fullDelta[0] + farDelta[0]) / 2,
    (fullDelta[1] + farDelta[1]) / 2,
    (fullDelta[2] + farDelta[2]) / 2,
  ];

  for (const constraint of constraints) {
    if (constraint.entityId === draggedVertexId) {
      deltas.set(constraint.constraintId, fullDelta);
    } else if (draggedLineIds.has(constraint.entityId)) {
      deltas.set(constraint.constraintId, lineMidDelta);
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

/**
 * Add the points ANCHORED to a line this drag reshapes to the solve's active
 * set, so they ride the moving line instead of staying frozen.
 *
 * `computeRippleActivePoints` un-freezes only the dragged point and its 1-hop
 * neighbours, so a point anchored somewhere along the host line was locked: it
 * held its position while the line moved under it and only snapped back on at
 * commit — "the point on the line does not stay". This is the line-anchor
 * counterpart of the circle's `point_on_circle` ride in the live drag preview.
 *
 * Pure: the caller keeps passing the result as `activePointIds`.
 */
export function rideAnchoredPoints(
  sketch: SketchFeatureParameters | null,
  draggedVertexId: string,
  activePointIds: readonly string[],
): string[] {
  const reshapedLineIds = (sketch?.lines ?? [])
    .filter(
      (line) =>
        line.start_vertex_id === draggedVertexId ||
        line.end_vertex_id === draggedVertexId,
    )
    .map((line) => line.line_id);
  if (reshapedLineIds.length === 0) {
    return [...activePointIds];
  }
  const riding = new Set(activePointIds);
  for (const anchor of sketch?.point_line_anchors ?? []) {
    if (reshapedLineIds.includes(anchor.line_id)) {
      riding.add(anchor.vertex_id);
    }
  }
  for (const anchor of sketch?.midpoint_anchors ?? []) {
    if (reshapedLineIds.includes(anchor.line_id)) {
      riding.add(anchor.vertex_id);
    }
  }
  return [...riding];
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
  let rigidPreviewApplied = false;
  const gcsBridge = getBridge();
  if (gcsBridge && sketch) {
    // Keep the H/V constraint on the line the user is actually dragging, so the
    // preview shows what the commit will produce. Stripping it let the line
    // rotate away from horizontal during the drag and then snap back on commit
    // — the user's "the line can be dragged out of horizontality but snaps
    // back, and the length changes with the projection of the committed point",
    // which is exactly the mismatch Fusion does not have: Fusion keeps the line
    // H (or V) throughout the drag. The other lines are still stripped, so the
    // ripple-freeze keeps everything else untouched.
    const draggedLineIds = new Set(
      sketch.lines
        .filter(
          (line) =>
            line.start_vertex_id === next.vertexId ||
            line.end_vertex_id === next.vertexId,
        )
        .map((line) => line.line_id),
    );
    const unconstrainedLines = sketch.lines.map((l) => ({
      ...l,
      constraint: draggedLineIds.has(l.line_id)
        ? l.constraint
        : (null as "horizontal" | "vertical" | null),
    }));
    // Dragging the CONTACT point of a tangency is REFUSED by the core (it keeps
    // the relation: with the far end fixed there is one tangent point only), so
    // the preview must refuse it too or the point would animate to the cursor and
    // then snap back on commit. Keep the dragged vertex where the sketch has it.
    const draggedContactLineIds = new Set(
      sketch.lines
        .filter((line) => line.end_vertex_id === next.vertexId)
        .map((line) => line.line_id),
    );
    const refusesDrag = (sketch.line_relations ?? []).some(
      (relation) =>
        relation.kind === "tangent_line_circle" &&
        draggedContactLineIds.has(relation.first_line_id),
    );
    const paramsCopy: SketchFeatureParameters = {
      ...sketch,
      lines: unconstrainedLines,
      line_relations: sketch.line_relations,
      vertices: sketch.vertices.map((p) =>
        p.vertex_id === next.vertexId && !refusesDrag
          ? { ...p, x: sketchPoint.local[0], y: sketchPoint.local[1] }
          : p,
      ),
    };
    let activePointIds = computeRippleActivePoints(sketch, next.vertexId);

    activePointIds = rideAnchoredPoints(sketch, next.vertexId, activePointIds);

    // The drag always WINS over the line's own constraints, in the preview as in
    // the core: dragging an endpoint of an H/V line or of a dimensioned line
    // translates that line (the far end follows) instead of the dragged point
    // being clamped back or the length being re-imposed. Freezing the dragged
    // point at the cursor is what makes the solver move the FAR end; without it
    // the preview clamped the point onto the axis (no motion in the other
    // direction) or failed the solve and fell back to moving the point alone —
    // the line appeared to rotate and then snapped back on commit.
    const draggedLine = sketch.lines.find(
      (line) =>
        line.start_vertex_id === next.vertexId ||
        line.end_vertex_id === next.vertexId,
    );
    const lineIsDimensioned =
      draggedLine !== undefined &&
      (sketch.dimensions ?? []).some((dimension) => {
        if (dimension.driven || dimension.is_auto) return false;
        if (dimension.kind === "line_length") {
          return dimension.entity_id === draggedLine.line_id;
        }
        if (dimension.kind !== "point_distance") return false;
        return (
          (dimension.entity_id === draggedLine.start_vertex_id &&
            dimension.secondary_entity_id === draggedLine.end_vertex_id) ||
          (dimension.entity_id === draggedLine.end_vertex_id &&
            dimension.secondary_entity_id === draggedLine.start_vertex_id)
        );
      });
    const lineIsAxisLocked =
      draggedLine !== undefined &&
      (draggedLine.constraint === "horizontal" ||
        draggedLine.constraint === "vertical");
    if (lineIsDimensioned || lineIsAxisLocked) {
      activePointIds = activePointIds.filter((id) => id !== next.vertexId);
    }

    // A rigid line is previewed DETERMINISTICALLY, exactly as the core commits it,
    // instead of leaving it to the solver: `update_sketch_vertex` carries the far
    // end by the drag delta (a translation) for a dimensioned line, or by the
    // off-axis part only for an H/V line, and translates any circle bound to it.
    //
    // The generic solve could not reproduce that: with the centre frozen it
    // compromised the length (42.715 → 40.197), and with the centre free the
    // system is underdetermined and DogLeg eventually jumped to a degenerate
    // solution — the user's "at some point it explodes during the drag, then jumps
    // back on commit", seen as a 0.00 mm dimension.
    if (draggedLine !== undefined && (lineIsDimensioned || lineIsAxisLocked)) {
      const baseVertex = sketch.vertices.find(
        (vertex) => vertex.vertex_id === next.vertexId,
      );
      const base = baseVertex
        ? [baseVertex.x, baseVertex.y]
        : [sketchPoint.local[0], sketchPoint.local[1]];
      const delta: [number, number] = [
        sketchPoint.local[0] - base[0],
        sketchPoint.local[1] - base[1],
      ];
      const isStart = draggedLine.start_vertex_id === next.vertexId;
      const farId = isStart
        ? draggedLine.end_vertex_id
        : draggedLine.start_vertex_id;
      const farX = isStart ? draggedLine.end_x : draggedLine.start_x;
      const farY = isStart ? draggedLine.end_y : draggedLine.start_y;

      // A tangency OWNS the line's end: `enforce_tangent_line_circle_relations`
      // re-projects the END onto the tangent point on every refresh, so dragging
      // the free START has to preview that projection — translating the line
      // would show the tangency breaking until the commit re-projected it (the
      // user: "one tangent disconnects" during the preview). Same formula and the
      // same branch choice (nearest to the current end) as the core.
      const tangentRelation = (sketch.line_relations ?? []).find(
        (relation) =>
          relation.kind === "tangent_line_circle" &&
          relation.first_line_id === draggedLine.line_id,
      );
      const tangentCircle = tangentRelation
        ? sketch.circles?.find(
            (circle) => circle.circle_id === tangentRelation.second_line_id,
          )
        : undefined;
      let farTarget: [number, number];
      let tangentProjected = false;
      // The relation's contract: the line's END is the contact point, so only a
      // drag on the START re-projects it.
      if (tangentCircle && isStart) {
        const startX = sketchPoint.local[0];
        const startY = sketchPoint.local[1];
        const toCenterX = tangentCircle.center_x - startX;
        const toCenterY = tangentCircle.center_y - startY;
        const distance = Math.hypot(toCenterX, toCenterY);
        const radius = tangentCircle.radius;
        if (distance > radius + 1e-6) {
          const tangentLength = Math.sqrt(distance * distance - radius * radius);
          const ux = toCenterX / distance;
          const uy = toCenterY / distance;
          const vx = -uy;
          const vy = ux;
          const along = (tangentLength * tangentLength) / distance;
          const perp = (tangentLength * radius) / distance;
          const candidateA: [number, number] = [
            startX + along * ux + perp * vx,
            startY + along * uy + perp * vy,
          ];
          const candidateB: [number, number] = [
            startX + along * ux - perp * vx,
            startY + along * uy - perp * vy,
          ];
          const distA = Math.hypot(candidateA[0] - farX, candidateA[1] - farY);
          const distB = Math.hypot(candidateB[0] - farX, candidateB[1] - farY);
          farTarget = distA <= distB ? candidateA : candidateB;
          tangentProjected = true;
        } else {
          farTarget = [farX, farY];
          tangentProjected = true;
        }
      } else {
        farTarget = lineIsDimensioned
          ? [farX + delta[0], farY + delta[1]]
          : draggedLine.constraint === "horizontal"
            ? [farX, sketchPoint.local[1]]
            : [sketchPoint.local[0], farY];
      }

      paramsCopy.vertices = paramsCopy.vertices.map((vertex) => {
        if (vertex.vertex_id === farId) {
          return { ...vertex, x: farTarget[0], y: farTarget[1] };
        }
        return vertex;
      });
      // A circle bound to the far end travels with it, radius untouched (the
      // core translates the centre; the radius has its own dimension).
      const farDelta: [number, number] = [
        farTarget[0] - farX,
        farTarget[1] - farY,
      ];
      for (const constraint of constraints ?? []) {
        // A tangency projection already decided where the end is; the circle must
        // stay put (the core does not move it for a tangent relation), otherwise
        // the preview would carry the circle along a line that is pivoting.
        if (tangentProjected) break;
        if (
          constraint.kind !== "point_on_circle" ||
          constraint.target_ids.length < 2 ||
          constraint.target_ids[0] !== farId
        ) {
          continue;
        }
        const boundCircle = sketch.circles?.find(
          (circle) => circle.circle_id === constraint.target_ids[1],
        );
        if (!boundCircle) continue;
        paramsCopy.vertices = paramsCopy.vertices.map((vertex) =>
          vertex.vertex_id === boundCircle.center_vertex_id
            ? {
                ...vertex,
                x: vertex.x + farDelta[0],
                y: vertex.y + farDelta[1],
              }
            : vertex,
        );
      }
      // Nothing else may move: the preview is this translation.
      activePointIds = [];
      rigidPreviewApplied = true;
    }

    // Dragging a circle CENTRE: the centre is the dragged (fixed) party and the
    // point_on_circle-attached points must ride it. The raw WASM solve would
    // instead relax the centre back toward the frozen point, so translate the
    // attached points by the centre's delta and freeze the centre for the solve.
    const draggedCircle = sketch?.circles.find(
      (c) => c.center_vertex_id === next.vertexId,
    );
    if (draggedCircle && sketch) {
      const base = sketch.vertices.find((v) => v.vertex_id === next.vertexId);
      const dx = sketchPoint.local[0] - (base?.x ?? sketchPoint.local[0]);
      const dy = sketchPoint.local[1] - (base?.y ?? sketchPoint.local[1]);
      const attachedIds = (constraints ?? [])
        .filter(
          (c) =>
            c.kind === "point_on_circle" &&
            c.target_ids.length >= 2 &&
            c.target_ids[1] === draggedCircle.circle_id,
        )
        .map((c) => c.target_ids[0]);
      // A line drawn onto the circle is in this set like any other point: its
      // endpoint no longer adopts the circle's derived quadrant id, so it carries
      // a real point_on_circle (see find_coincident_endpoint in the core).
      const rideIds = [...new Set(attachedIds)];
      paramsCopy.vertices = paramsCopy.vertices.map((p) =>
        rideIds.includes(p.vertex_id)
          ? { ...p, x: p.x + dx, y: p.y + dy }
          : p,
      );
      // Freeze the centre, unfreeze the attached points.
      activePointIds = rideIds;

      // A tangency on this circle OWNS its line's contact END: the core
      // re-projects it onto the new tangent point on every refresh
      // (`enforce_tangent_line_circle_relations`), so dragging the circle must do
      // the same in the preview or the line visibly detaches until the commit
      // snaps it back — the user's "dragging the circle disconnects from the
      // tangent line" (2026-11-04). Same formula, same nearest-branch choice.
      const centreX = sketchPoint.local[0];
      const centreY = sketchPoint.local[1];
      for (const relation of sketch.line_relations ?? []) {
        if (
          relation.kind !== "tangent_line_circle" ||
          relation.second_line_id !== draggedCircle.circle_id
        ) {
          continue;
        }
        const tangentLine = sketch.lines.find(
          (line) => line.line_id === relation.first_line_id,
        );
        if (!tangentLine) continue;
        const startVertex = paramsCopy.vertices.find(
          (vertex) => vertex.vertex_id === tangentLine.start_vertex_id,
        );
        if (!startVertex) continue;
        const sx = startVertex.x;
        const sy = startVertex.y;
        const toCenterX = centreX - sx;
        const toCenterY = centreY - sy;
        const distance = Math.hypot(toCenterX, toCenterY);
        const radius = draggedCircle.radius;
        if (distance <= radius + 1e-6) continue;
        const tangentLength = Math.sqrt(distance * distance - radius * radius);
        const ux = toCenterX / distance;
        const uy = toCenterY / distance;
        const vx = -uy;
        const vy = ux;
        const along = (tangentLength * tangentLength) / distance;
        const perp = (tangentLength * radius) / distance;
        const candidateA: [number, number] = [
          sx + along * ux + perp * vx,
          sy + along * uy + perp * vy,
        ];
        const candidateB: [number, number] = [
          sx + along * ux - perp * vx,
          sy + along * uy - perp * vy,
        ];
        const currentEnd = paramsCopy.vertices.find(
          (vertex) => vertex.vertex_id === tangentLine.end_vertex_id,
        );
        const endX = currentEnd?.x ?? tangentLine.end_x;
        const endY = currentEnd?.y ?? tangentLine.end_y;
        const projected =
          Math.hypot(candidateA[0] - endX, candidateA[1] - endY) <=
          Math.hypot(candidateB[0] - endX, candidateB[1] - endY)
            ? candidateA
            : candidateB;
        paramsCopy.vertices = paramsCopy.vertices.map((vertex) =>
          vertex.vertex_id === tangentLine.end_vertex_id
            ? { ...vertex, x: projected[0], y: projected[1] }
            : vertex,
        );
        if (!activePointIds.includes(tangentLine.end_vertex_id)) {
          activePointIds = [...activePointIds, tangentLine.end_vertex_id];
        }
      }
    }

    // Dragging an ARC CENTRE: mirror the circle path — translate the
    // point_on_arc-attached points by the centre's delta so they ride the arc
    // at the same relative spot instead of sliding around it.
    // `arcs` is optional on the parameters; a circle-only sketch has no
    // `arcs` key at all. Guard it — an unguarded `.find` here threw and
    // aborted the whole preview solve, so a line attached to a CIRCLE never
    // followed the circle during the drag (it only moved on commit).
    const draggedArc = (sketch?.arcs ?? []).find(
      (a) => a.center_vertex_id === next.vertexId,
    );
    if (draggedArc && sketch) {
      const base = sketch.vertices.find((v) => v.vertex_id === next.vertexId);
      const dx = sketchPoint.local[0] - (base?.x ?? sketchPoint.local[0]);
      const dy = sketchPoint.local[1] - (base?.y ?? sketchPoint.local[1]);
      const attachedIds = (constraints ?? [])
        .filter(
          (c) =>
            c.kind === "point_on_arc" &&
            c.target_ids.length >= 2 &&
            c.target_ids[1] === draggedArc.arc_id,
        )
        .map((c) => c.target_ids[0]);
      // Both arc endpoints must translate with the centre — otherwise the
      // start rides but the end stays put and the preview's sweep flips to the
      // complementary (deleted) segment mid-drag.
      const rideIds = [
        ...new Set([
          ...attachedIds,
          draggedArc.start_vertex_id,
          draggedArc.end_vertex_id,
        ]),
      ];
      paramsCopy.vertices = paramsCopy.vertices.map((p) =>
        rideIds.includes(p.vertex_id)
          ? { ...p, x: p.x + dx, y: p.y + dy }
          : p,
      );
      // Freeze the whole junction for the solve — do NOT un-freeze it.
      //
      // The translate above already puts the arc and the lines welded to it
      // where the drag wants them. Un-freezing those points let the solver pull
      // the shared endpoints back: the preview pushes the sketch's driving
      // dimensions unchanged (planegcsBridge step 4f), and a line-length
      // dimension is measured against the PRE-drag geometry, so after the
      // shared end moved the dimension is violated and the solver "repairs" it
      // by dragging the endpoint back. That rotates the arc's endpoints, and
      // since the render derives the sweep from their angles, the preview's
      // sweep shrinks until it passes 180° and the arc flips to its
      // complement ("sweep shrinks, then renders in reverse"). The core
      // freezes the centre and every riding point exactly this way
      // (sketch_line_vertex_update_commands.inc), so freezing here is also
      // what makes the preview match the commit.
      const rideSet = new Set(rideIds);
      activePointIds = activePointIds.filter((id) => !rideSet.has(id));
    }

    const result = gcsBridge.solve(paramsCopy, constraints ?? [], {
      activePointIds,
    });
    if (rigidPreviewApplied) {
      // The translation IS the preview: take it straight from the (already
      // translated) parameters. Running the solver here is what produced the
      // degenerate 0.00 mm jump — with everything frozen it has no freedom and
      // any "repair" it invents is wrong.
      for (const vertex of paramsCopy.vertices) {
        solvedPoints.set(vertex.vertex_id, [vertex.x, vertex.y]);
      }
      const solved = solvedPoints.get(next.vertexId);
      if (solved) finalLocal = solved;
    } else if (result.ok) {
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