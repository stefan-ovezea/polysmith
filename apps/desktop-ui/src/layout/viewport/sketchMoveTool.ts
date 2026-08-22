import * as THREE from "three";

import type {
  SketchConstraintScene,
  SketchFeatureParameters,
  SketchPlaneFrame,
  SketchPreviewPoint,
  ViewportScene,
} from "@/types";
import { projectWorldPointToViewport, themeColor, toWorldPoint } from "@/utils";
import { getBridge } from "@/lib/planegcsSolver";
import type { SketchConstraintData } from "@/lib/planegcsBridge";
import type { ResolveSnapOptions } from "./snapResolution";
import { computeRippleActivePointsForVertices } from "./endpointDrag";

// Sketch Move tool logic (Fusion-style manipulator).
//
// Dragging the clicked entity (or the current selection when the clicked
// entity is selected) translates it.  A persistent rotation ring is shown
// at the selection centroid whenever the Move tool is armed and something
// is selected — grabbing the ring itself rotates the selection around the
// centroid.  The preview runs the same WASM 1-hop ripple solve as
// endpoint drag and writes into the real scene objects; the commit sends
// one `move_sketch_entities` command with the full rigid transform.

export interface SketchMoveDrag {
  /** Entity ids moved by the drag (line/circle/arc ids; vertex ids for
   *  standalone points). */
  entityIds: string[];
  /** Every vertex belonging to the moved entities. */
  vertexIds: string[];
  mode: "translate" | "rotate";
  startClientX: number;
  startClientY: number;
  /** Sketch-local pointer position at drag start (translate anchor). */
  startLocal: [number, number];
  /** Snapped sketch-local position of the previous frame — deltas are
   *  per-frame increments, not totals since drag start (accumulating a
   *  total would multiply the movement every frame). */
  lastLocal: [number, number];
  /** Screen angle from the rotation center to the pointer at drag start. */
  startAngle: number;
  /** Screen angle of the previous frame (see lastLocal). */
  lastAngle: number;
  /** Rotation center in sketch-local coords (selection centroid). */
  centerLocal: [number, number];
  /** Base (committed) sketch-local position of every moved vertex. */
  baseVertexPositions: Map<string, [number, number]>;
  hasMoved: boolean;
}

export interface SketchMoveFrameResult {
  dx: number;
  dy: number;
  angleDeg: number;
  /** vertex id → solved sketch-local position (full solved point set). */
  solvedPoints: Map<string, [number, number]>;
  snapLabel: string | null;
}

/** Move/Copy dialog state: the target set captured when the dialog
 *  opened plus the accumulated rigid transform, always applied from the
 *  ORIGINAL base positions.  The manipulator ring stays at the original
 *  centroid for the dialog's lifetime (Fusion-style), and OK commits the
 *  total transform as a single undo step. */
export interface PendingSketchMove {
  entityIds: string[];
  vertexIds: string[];
  baseVertexPositions: Map<string, [number, number]>;
  centerLocal: [number, number];
  dx: number;
  dy: number;
  angleDeg: number;
}

/** Creates the dialog state for the given entity set — null when nothing
 *  movable is in it. */
export function createPendingSketchMove(
  sketch: SketchFeatureParameters | null,
  entityIds: readonly string[],
): PendingSketchMove | null {
  if (!sketch) {
    return null;
  }
  const vertexIds = sketchMoveEntityVertices(sketch, entityIds).filter(
    (id) => !sketch.vertices.find((v) => v.vertex_id === id)?.is_fixed,
  );
  if (vertexIds.length === 0) {
    return null;
  }
  const baseVertexPositions = new Map<string, [number, number]>();
  for (const id of vertexIds) {
    const vertex = sketch.vertices.find((v) => v.vertex_id === id);
    if (vertex) {
      baseVertexPositions.set(id, [vertex.x, vertex.y]);
    }
  }
  return {
    entityIds: [...entityIds],
    vertexIds,
    baseVertexPositions,
    centerLocal: sketchMoveCentroid(sketch, vertexIds),
    dx: 0,
    dy: 0,
    angleDeg: 0,
  };
}

/** Solves the pending transform from base positions — same WASM 1-hop
 *  ripple solve as the drag preview; falls back to the raw rigid
 *  transform when the solve fails. */
export function solvePendingSketchMove({
  pending,
  sketch,
  constraints,
}: {
  pending: PendingSketchMove;
  sketch: SketchFeatureParameters | null;
  constraints?: SketchConstraintData[];
}): Map<string, [number, number]> {
  const solvedPoints = new Map<string, [number, number]>();
  const angleRad = (pending.angleDeg * Math.PI) / 180;
  const gcsBridge = getBridge();
  if (gcsBridge && sketch) {
    const paramsCopy = sketchMoveApplyToParams({
      sketch,
      vertexIds: pending.vertexIds,
      baseVertexPositions: pending.baseVertexPositions,
      dx: pending.dx,
      dy: pending.dy,
      center: pending.centerLocal,
      angleRad,
    });
    const activePointIds = computeRippleActivePointsForVertices(
      sketch,
      pending.vertexIds,
    );
    const result = gcsBridge.solve(paramsCopy, constraints ?? [], {
      activePointIds,
    });
    if (result.ok) {
      for (const point of result.points) {
        solvedPoints.set(point.id, [point.x, point.y]);
      }
    }
  }
  if (solvedPoints.size === 0 && sketch) {
    for (const vertex of sketch.vertices) {
      solvedPoints.set(vertex.vertex_id, [vertex.x, vertex.y]);
    }
    for (const id of pending.vertexIds) {
      const base = pending.baseVertexPositions.get(id);
      if (base) {
        solvedPoints.set(
          id,
          sketchMoveTransformPoint(
            base,
            pending.centerLocal,
            pending.dx,
            pending.dy,
            angleRad,
          ),
        );
      }
    }
  }
  return solvedPoints;
}

function sketchMoveVertexPosition(
  sketch: SketchFeatureParameters,
  vertexId: string,
): [number, number] | null {
  const vertex = sketch.vertices.find((v) => v.vertex_id === vertexId);
  if (vertex) {
    return [vertex.x, vertex.y];
  }
  // Legacy circle/arc centers may not be in the vertex table.
  const circle = sketch.circles.find(
    (c) =>
      (c.center_vertex_id ?? `point-circle-${c.circle_id}-center`) ===
      vertexId,
  );
  if (circle) {
    return [circle.center_x, circle.center_y];
  }
  const arc = sketch.arcs?.find(
    (a) => (a.center_vertex_id ?? `point-arc-${a.arc_id}-center`) === vertexId,
  );
  if (arc) {
    return [arc.center_x, arc.center_y];
  }
  return null;
}

/** Selected entity ids carried by the viewport scene data (entity ids
 *  plus ids of selected standalone points). */
export function sketchMoveSelectedEntityIds(
  sceneData: ViewportScene | null,
): string[] {
  const selected: string[] = [];
  if (!sceneData) {
    return selected;
  }
  for (const line of sceneData.sketchLines) {
    if (line.isSelected) selected.push(line.lineId);
  }
  for (const circle of sceneData.sketchCircles) {
    if (circle.isSelected) selected.push(circle.circleId);
  }
  for (const ellipse of sceneData.sketchEllipses) {
    if (ellipse.isSelected) selected.push(ellipse.ellipseId);
  }
  for (const arc of sceneData.sketchArcs) {
    if (arc.isSelected) selected.push(arc.arcId);
  }
  for (const point of sceneData.sketchPoints) {
    if (point.isSelected) selected.push(point.id);
  }
  return selected;
}

/** Persistent manipulator state for the current selection: the entity
 *  set, its movable vertices, and the rotation centroid. */
export interface SketchMoveRingState {
  entityIds: string[];
  vertexIds: string[];
  centerLocal: [number, number];
}

/** Builds the manipulator state for the current selection — null when
 *  nothing movable is selected. */
export function sketchMoveRingStateForSelection(
  sketch: SketchFeatureParameters | null,
  sceneData: ViewportScene | null,
): SketchMoveRingState | null {
  if (!sketch) {
    return null;
  }
  const entityIds = sketchMoveSelectedEntityIds(sceneData);
  const vertexIds = sketchMoveEntityVertices(sketch, entityIds).filter(
    (id) => !sketch.vertices.find((v) => v.vertex_id === id)?.is_fixed,
  );
  if (vertexIds.length === 0) {
    return null;
  }
  return {
    entityIds,
    vertexIds,
    centerLocal: sketchMoveCentroid(sketch, vertexIds),
  };
}

/** Resolves entity ids to the vertices a rigid move must transform:
 *  line endpoints, circle centers, arc centers + endpoints, standalone
 *  points.  Unknown ids are dropped (the commit core command mirrors
 *  this and treats them as vertex ids). */
export function sketchMoveEntityVertices(
  sketch: SketchFeatureParameters | null,
  entityIds: readonly string[],
): string[] {
  if (!sketch) {
    return [];
  }
  const vertices = new Set<string>();
  for (const id of entityIds) {
    const line = sketch.lines.find((l) => l.line_id === id);
    if (line) {
      vertices.add(line.start_vertex_id);
      vertices.add(line.end_vertex_id);
      continue;
    }
    const circle = sketch.circles.find((c) => c.circle_id === id);
    if (circle) {
      vertices.add(
        circle.center_vertex_id ?? `point-circle-${circle.circle_id}-center`,
      );
      continue;
    }
    // Ellipse moves drag the center only — the axis points are fixed
    // at creation, and the ring builder below filters fixed vertices.
    const ellipse = sketch.ellipses.find((e) => e.ellipse_id === id);
    if (ellipse) {
      vertices.add(ellipse.center_vertex_id);
      continue;
    }
    const arc = sketch.arcs?.find((a) => a.arc_id === id);
    if (arc) {
      vertices.add(arc.start_vertex_id);
      vertices.add(arc.end_vertex_id);
      vertices.add(arc.center_vertex_id ?? `point-arc-${arc.arc_id}-center`);
      continue;
    }
    if (sketch.vertices.some((v) => v.vertex_id === id)) {
      vertices.add(id);
    }
  }
  return Array.from(vertices);
}

export function sketchMoveCentroid(
  sketch: SketchFeatureParameters | null,
  vertexIds: readonly string[],
): [number, number] {
  if (!sketch) {
    return [0, 0];
  }
  let sumX = 0;
  let sumY = 0;
  let count = 0;
  for (const id of vertexIds) {
    const position = sketchMoveVertexPosition(sketch, id);
    if (!position) {
      continue;
    }
    sumX += position[0];
    sumY += position[1];
    count += 1;
  }
  return count > 0 ? [sumX / count, sumY / count] : [0, 0];
}

export function sketchMoveTransformPoint(
  point: [number, number],
  center: [number, number],
  dx: number,
  dy: number,
  angleRad: number,
): [number, number] {
  const cosA = Math.cos(angleRad);
  const sinA = Math.sin(angleRad);
  const rx = point[0] - center[0];
  const ry = point[1] - center[1];
  return [
    rx * cosA - ry * sinA + center[0] + dx,
    rx * sinA + ry * cosA + center[1] + dy,
  ];
}

/** Builds the preview params copy: moved vertices at the transformed
 *  positions; H/V stripped from moved lines when rotating so the solver
 *  allows the rotation (the core strips H/V on rotate commit too). */
export function sketchMoveApplyToParams({
  sketch,
  vertexIds,
  baseVertexPositions,
  dx,
  dy,
  center,
  angleRad,
}: {
  sketch: SketchFeatureParameters;
  vertexIds: readonly string[];
  baseVertexPositions: ReadonlyMap<string, [number, number]>;
  dx: number;
  dy: number;
  center: [number, number];
  angleRad: number;
}): SketchFeatureParameters {
  const rotating = Math.abs(angleRad) > 1e-9;
  const movedVertexIds = new Set(vertexIds);
  const transformed = new Map<string, [number, number]>();
  for (const id of vertexIds) {
    const base = baseVertexPositions.get(id);
    if (!base) {
      continue;
    }
    transformed.set(
      id,
      sketchMoveTransformPoint(base, center, dx, dy, angleRad),
    );
  }
  return {
    ...sketch,
    lines: sketch.lines.map((line) =>
      rotating &&
      (movedVertexIds.has(line.start_vertex_id) ||
        movedVertexIds.has(line.end_vertex_id))
        ? { ...line, constraint: null }
        : line,
    ),
    vertices: sketch.vertices.map((vertex) => {
      const next = transformed.get(vertex.vertex_id);
      return next ? { ...vertex, x: next[0], y: next[1] } : vertex;
    }),
  };
}

/** Constraint-badge deltas: badges annotating a moved entity (or a moved
 *  vertex) follow that entity's representative vertex.  Exact positions
 *  re-sync on commit. */
export function sketchMoveConstraintDeltas({
  sceneConstraints,
  sketch,
  entityIds,
  vertexIds,
  baseVertexPositions,
  dx,
  dy,
  center,
  angleRad,
  planeId,
  planeFrame,
}: {
  sceneConstraints: readonly SketchConstraintScene[];
  sketch: SketchFeatureParameters | null;
  entityIds: readonly string[];
  vertexIds: readonly string[];
  baseVertexPositions: ReadonlyMap<string, [number, number]>;
  dx: number;
  dy: number;
  center: [number, number];
  angleRad: number;
  planeId: string;
  planeFrame: SketchPlaneFrame | null;
}): Map<string, [number, number, number]> {
  const deltas = new Map<string, [number, number, number]>();
  if (!sketch) {
    return deltas;
  }
  const movedEntityIds = new Set(entityIds);
  const movedVertexIds = new Set(vertexIds);

  // Representative vertex per moved entity, for badge-follow.
  const representativeOf = (entityId: string): string | null => {
    const line = sketch.lines.find((l) => l.line_id === entityId);
    if (line) {
      return line.start_vertex_id;
    }
    const circle = sketch.circles.find((c) => c.circle_id === entityId);
    if (circle) {
      return circle.center_vertex_id ?? `point-circle-${circle.circle_id}-center`;
    }
    const arc = sketch.arcs?.find((a) => a.arc_id === entityId);
    if (arc) {
      return arc.center_vertex_id ?? `point-arc-${arc.arc_id}-center`;
    }
    return movedVertexIds.has(entityId) ? entityId : null;
  };

  const deltaFor = (vertexId: string): [number, number, number] | null => {
    const base = baseVertexPositions.get(vertexId);
    if (!base) {
      return null;
    }
    const transformed = sketchMoveTransformPoint(base, center, dx, dy, angleRad);
    const worldBase = toWorldPoint(planeId, base, planeFrame);
    const worldNext = toWorldPoint(planeId, transformed, planeFrame);
    return [
      worldNext[0] - worldBase[0],
      worldNext[1] - worldBase[1],
      worldNext[2] - worldBase[2],
    ];
  };

  for (const constraint of sceneConstraints) {
    const representative =
      movedEntityIds.has(constraint.entityId)
        ? representativeOf(constraint.entityId)
        : movedVertexIds.has(constraint.entityId)
          ? constraint.entityId
          : null;
    if (!representative) {
      continue;
    }
    const delta = deltaFor(representative);
    if (delta) {
      deltas.set(constraint.constraintId, delta);
    }
  }
  return deltas;
}

export function resolveSketchMoveFrame({
  drag,
  sketch,
  planeId,
  planeFrame,
  resolveSnappedSketchPoint,
  constraints,
  sceneConstraints,
  rawLocal,
  screenAngleRad,
}: {
  drag: SketchMoveDrag;
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
  /** Current raw sketch-local pointer position (translate mode). */
  rawLocal: [number, number];
  /** Current pointer screen angle around the center (rotate mode). */
  screenAngleRad: number;
}): SketchMoveFrameResult {
  let dx = 0;
  let dy = 0;
  let angleDeg = 0;
  let snapLabel: string | null = null;

  if (drag.mode === "translate") {
    // PER-FRAME incremental delta (the dialog accumulates it into the
    // pending transform — using the total since drag start would
    // multiply the movement every frame and run away from the cursor):
    // snapped cursor minus the previous frame's snapped position.  The
    // moved entities' own geometry is excluded from the snap candidates
    // so a selection can't snap onto itself and jump at drag start.
    const world = toWorldPoint(planeId, rawLocal, planeFrame);
    const sketchPoint = resolveSnappedSketchPoint(
      { local: rawLocal, world: [world[0], world[1], world[2]] },
      drag.startLocal,
      {
        dynamicSnapsEnabled: false,
        excludeEntityIds: drag.entityIds,
      },
    );
    dx = sketchPoint.local[0] - drag.lastLocal[0];
    dy = sketchPoint.local[1] - drag.lastLocal[1];
    drag.lastLocal = [sketchPoint.local[0], sketchPoint.local[1]];
    snapLabel = sketchPoint.snapLabel;
  } else {
    // Same per-frame increment for rotation: angle delta from the
    // previous frame's screen angle, wrapped to ±180°.
    let delta = screenAngleRad - drag.lastAngle;
    if (delta > Math.PI) {
      delta -= 2 * Math.PI;
    } else if (delta < -Math.PI) {
      delta += 2 * Math.PI;
    }
    drag.lastAngle = screenAngleRad;
    angleDeg = (delta * 180) / Math.PI;
    // Fusion-style angle readout in the feedback label.
    snapLabel = `${angleDeg >= 0 ? "+" : ""}${angleDeg.toFixed(1)}°`;
  }

  const angleRad = (angleDeg * Math.PI) / 180;

  const solvedPoints = new Map<string, [number, number]>();
  const gcsBridge = getBridge();
  if (gcsBridge && sketch) {
    const paramsCopy = sketchMoveApplyToParams({
      sketch,
      vertexIds: drag.vertexIds,
      baseVertexPositions: drag.baseVertexPositions,
      dx,
      dy,
      center: drag.centerLocal,
      angleRad,
    });
    const activePointIds = computeRippleActivePointsForVertices(
      sketch,
      drag.vertexIds,
    );
    const result = gcsBridge.solve(paramsCopy, constraints ?? [], {
      activePointIds,
    });
    if (result.ok) {
      for (const point of result.points) {
        solvedPoints.set(point.id, [point.x, point.y]);
      }
    }
  }

  // Solve failure (or missing bridge): preview the rigid transform
  // itself; everything else stays at its base position.
  if (solvedPoints.size === 0 && sketch) {
    for (const vertex of sketch.vertices) {
      solvedPoints.set(vertex.vertex_id, [vertex.x, vertex.y]);
    }
    for (const id of drag.vertexIds) {
      const base = drag.baseVertexPositions.get(id);
      if (base) {
        solvedPoints.set(
          id,
          sketchMoveTransformPoint(base, drag.centerLocal, dx, dy, angleRad),
        );
      }
    }
  }

  return { dx, dy, angleDeg, solvedPoints, snapLabel };
}

/** Ring radius scales with the moved-selection bounding box. */
export function sketchMoveRingRadius(
  sketch: SketchFeatureParameters | null,
  vertexIds: readonly string[],
): number {
  let minX = Infinity;
  let minY = Infinity;
  let maxX = -Infinity;
  let maxY = -Infinity;
  if (sketch) {
    for (const id of vertexIds) {
      const position = sketchMoveVertexPosition(sketch, id);
      if (!position) {
        continue;
      }
      minX = Math.min(minX, position[0]);
      minY = Math.min(minY, position[1]);
      maxX = Math.max(maxX, position[0]);
      maxY = Math.max(maxY, position[1]);
    }
  }
  const diagonal =
    maxX === -Infinity ? 0 : Math.hypot(maxX - minX, maxY - minY);
  return Math.max(diagonal * 0.35, 5);
}

export function buildSketchMoveRingObject({
  centerWorld,
  radius,
  planeFrame,
}: {
  centerWorld: [number, number, number];
  radius: number;
  planeFrame: SketchPlaneFrame | null;
}): { group: THREE.Group; pickables: THREE.Object3D[] } {
  const group = new THREE.Group();
  const material = new THREE.MeshBasicMaterial({
    color: themeColor("--color-primary-glow", "#00e5ff"),
    transparent: true,
    opacity: 0.75,
    depthTest: false,
    depthWrite: false,
  });
  const ring = new THREE.Mesh(
    new THREE.TorusGeometry(radius, Math.max(radius * 0.02, 0.25), 8, 96),
    material,
  );
  ring.renderOrder = 49;
  ring.userData.moveRingHandle = true;
  group.add(ring);

  const centerSphere = new THREE.Mesh(
    new THREE.SphereGeometry(Math.max(radius * 0.04, 0.6), 12, 12),
    material,
  );
  centerSphere.renderOrder = 49;
  group.add(centerSphere);

  // The torus is built in the XY plane; align it to the sketch plane.
  if (planeFrame) {
    const normal = new THREE.Vector3(
      planeFrame.normal.x,
      planeFrame.normal.y,
      planeFrame.normal.z,
    );
    group.quaternion.setFromUnitVectors(new THREE.Vector3(0, 0, 1), normal);
  }
  group.position.set(centerWorld[0], centerWorld[1], centerWorld[2]);
  return { group, pickables: [ring] };
}

export function disposeSketchMoveRingObject(group: THREE.Group | null) {
  if (!group) {
    return;
  }
  group.traverse((child) => {
    if (!(child instanceof THREE.Mesh)) {
      return;
    }
    child.geometry.dispose();
    const material = child.material as THREE.Material | THREE.Material[];
    if (Array.isArray(material)) {
      material.forEach((m) => m.dispose());
    } else {
      material.dispose();
    }
  });
  group.removeFromParent();
}

/** Screen-space angle from a world center point to the pointer — port of
 *  moveGizmoScreenAngle for a sketch-plane rotation center. */
export function sketchMoveScreenAngle(
  clientX: number,
  clientY: number,
  centerWorld: [number, number, number],
  camera: THREE.Camera,
  renderer: THREE.WebGLRenderer,
): number {
  const projectedCenter = projectWorldPointToViewport(
    centerWorld,
    camera,
    renderer,
  );
  if (!projectedCenter) {
    return 0;
  }
  const rect = renderer.domElement.getBoundingClientRect();
  return Math.atan2(
    clientY - rect.top - projectedCenter.y,
    clientX - rect.left - projectedCenter.x,
  );
}
