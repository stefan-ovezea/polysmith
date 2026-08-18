import * as THREE from "three";

import type { SketchFeatureParameters, SketchPlaneFrame } from "@/types";
import { toWorldPoint } from "@/utils";
import {
  updateSketchArcObject,
  updateSketchCircleObject,
  updateSketchLineObject,
} from "./sceneIncrementalUpdate";

// Shared live-preview mutation for sketch drag interactions.
//
// Endpoint drag and the Move tool both solve a preview sketch locally
// (planegcs WASM, 1-hop ripple freeze) and then write the solved vertex
// positions INTO the real committed scene objects, so the user sees the
// actual geometry moving instead of a dashed overlay.  Commit paths are
// unchanged: the core re-solves natively and the incoming viewport_state
// rebuild reconciles the scene.
//
// Circles and arcs are re-sampled WITHOUT a plane frame: their sampling
// basis is recovered from the objects' own geometry, which is correct
// even when the sketch's plane_frame is null (origin-plane sketches).

export function applySolvedPointsToSketchScene({
  solvedPoints,
  sketch,
  planeId,
  planeFrame,
  sketchEntityObjectById,
  sketchPointObjectById,
  sketchConstraintObjects,
  sketchProfileObjects,
  constraintDeltas,
}: {
  /** vertex id → solved sketch-local position.  Points missing from the
   *  map (or whose scene object is missing) are simply skipped. */
  solvedPoints: ReadonlyMap<string, [number, number]>;
  sketch: SketchFeatureParameters | null;
  planeId: string;
  planeFrame: SketchPlaneFrame | null;
  sketchEntityObjectById: ReadonlyMap<string, THREE.Line | THREE.LineLoop>;
  sketchPointObjectById: ReadonlyMap<string, THREE.Mesh>;
  sketchConstraintObjects: readonly THREE.Object3D[];
  /** Profile region objects — hidden while the preview is mutating
   *  entity geometry, otherwise they ghost at the committed position
   *  (e.g. a selected circle profile leaves a stale circle behind the
   *  moving entity).  The commit/restore rebuild recreates them. */
  sketchProfileObjects?: readonly THREE.Object3D[];
  /** constraint id → world-space delta; badges follow the entities they
   *  annotate during the preview (exact positions re-sync on commit). */
  constraintDeltas?: ReadonlyMap<string, [number, number, number]>;
}) {
  if (!sketch) {
    return;
  }

  for (const profileObject of sketchProfileObjects ?? []) {
    profileObject.visible = false;
  }

  const toWorld = (local: [number, number]): [number, number, number] =>
    toWorldPoint(planeId, local, planeFrame);

  for (const line of sketch.lines) {
    const start = solvedPoints.get(line.start_vertex_id);
    const end = solvedPoints.get(line.end_vertex_id);
    if (!start || !end) {
      continue;
    }
    updateSketchLineObject(
      sketchEntityObjectById.get(line.line_id),
      toWorld(start),
      toWorld(end),
    );
  }

  for (const circle of sketch.circles) {
    const centerId =
      circle.center_vertex_id ?? `point-circle-${circle.circle_id}-center`;
    const center = solvedPoints.get(centerId);
    if (!center) {
      continue;
    }
    updateSketchCircleObject({
      circleObject: sketchEntityObjectById.get(circle.circle_id),
      center: toWorld(center),
      radius: circle.radius,
      // No plane axes: the updater recovers the sampling basis from the
      // circle's own buffer, which cannot flip planes.
    });
  }

  if (sketch.arcs) {
    for (const arc of sketch.arcs) {
      const centerId =
        arc.center_vertex_id ?? `point-arc-${arc.arc_id}-center`;
      const center = solvedPoints.get(centerId);
      const start = solvedPoints.get(arc.start_vertex_id);
      const end = solvedPoints.get(arc.end_vertex_id);
      if (!center || !start || !end) {
        continue;
      }
      const centerWorld = toWorld(center);
      const startWorld = toWorld(start);
      // Radius follows the dragged endpoint so the preview matches the
      // commit (the core recomputes the circumradius from center+endpoints).
      const radius = Math.hypot(
        startWorld[0] - centerWorld[0],
        startWorld[1] - centerWorld[1],
        startWorld[2] - centerWorld[2],
      );
      updateSketchArcObject({
        arcObject: sketchEntityObjectById.get(arc.arc_id),
        center: centerWorld,
        start: startWorld,
        end: toWorld(end),
        radius,
        ccw: arc.ccw,
        // No plane axes: the updater derives the sampling basis from the
        // arc's own center/start/end geometry.
      });
    }
  }

  if (sketch.vertices) {
    for (const vertex of sketch.vertices) {
      const solved = solvedPoints.get(vertex.vertex_id);
      if (!solved) {
        continue;
      }
      const pointObject = sketchPointObjectById.get(vertex.vertex_id);
      if (!pointObject) {
        continue;
      }
      const world = toWorld(solved);
      pointObject.position.set(world[0], world[1], world[2]);
    }
  }

  if (constraintDeltas && constraintDeltas.size > 0) {
    for (const object of sketchConstraintObjects) {
      const constraintId = object.userData.sketchConstraintId as
        | string
        | undefined;
      if (!constraintId) {
        continue;
      }
      const delta = constraintDeltas.get(constraintId);
      if (!delta) {
        continue;
      }
      object.position.x += delta[0];
      object.position.y += delta[1];
      object.position.z += delta[2];
    }
  }
}
