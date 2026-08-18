import * as THREE from "three";

import type {
  ArmedSketchConstraint,
  SketchFeatureParameters,
  SketchPlaneFrame,
  ViewportScene,
} from "@/types";
import { resolveSketchPlanePoint, toWorldPoint } from "@/utils";
import type { ViewportPickHit } from "./contextMenuState";
import {
  buildSketchMoveRingObject,
  createPendingSketchMove,
  sketchMoveCentroid,
  sketchMoveEntityVertices,
  sketchMoveRingRadius,
  sketchMoveScreenAngle,
  sketchMoveSelectedEntityIds,
  type PendingSketchMove,
  type SketchMoveDrag,
} from "./sketchMoveTool";

interface MutableRef<T> {
  current: T;
}

interface BeginSketchMovePointerDownParams {
  event: PointerEvent;
  renderer: THREE.WebGLRenderer;
  camera: THREE.Camera;
  controls: { enabled: boolean };
  activeSketchPlaneId: string;
  activeSketchPlaneFrame: SketchPlaneFrame | null;
  sketch: SketchFeatureParameters | null;
  sceneData: ViewportScene | null;
  hit: ViewportPickHit | null;
  armedSketchConstraint: ArmedSketchConstraint;
  sketchMoveDragRef: MutableRef<SketchMoveDrag | null>;
  /** Rotation-ring group, live during the drag. */
  sketchMoveRingGroupRef: MutableRef<THREE.Group | null>;
  sketchGroupRef: MutableRef<THREE.Group | null>;
  /** Forced drag mode; defaults to translate (Alt forces rotate). */
  mode?: "translate" | "rotate";
  /** When set (persistent-manipulator ring grab), the hit resolution is
   *  skipped and this entity set is moved. */
  entityIdsOverride?: string[];
  /** Move/Copy dialog state; initialized on the first drag when the
   *  dialog opened without a selection. */
  pendingSketchMoveRef?: MutableRef<PendingSketchMove | null>;
}

// Resolves a pick hit to the entity ids the Move tool should move.
// The Move tool is ENTITY-oriented: a sketch_point hit resolves to the
// entities that own the vertex (geometry_owner_ids), so grabbing near a
// line endpoint moves the line — dragging a single vertex is the select
// tool's endpoint-drag job.  Standalone points (no owners) move as
// points.  Clicked-or-selection: when the clicked entity is part of the
// current selection, the whole selection moves.
export function sketchMoveEntityIdsForHit(
  hit: ViewportPickHit | null,
  sketch: SketchFeatureParameters | null,
  sceneData: ViewportScene | null,
): string[] {
  if (!sketch) {
    return [];
  }
  const selected = new Set(sketchMoveSelectedEntityIds(sceneData));

  if (hit?.kind === "sketch_entity") {
    return selected.has(hit.id) ? Array.from(selected) : [hit.id];
  }

  if (hit?.kind === "sketch_point") {
    const vertex = sketch.vertices.find((v) => v.vertex_id === hit.id);
    const owners = vertex?.geometry_owner_ids ?? [];
    if (owners.length > 0) {
      const ownerInSelection = owners.find((owner) => selected.has(owner));
      return ownerInSelection ? Array.from(selected) : [owners[0]];
    }
    return [hit.id];
  }

  return [];
}

export function beginSketchMovePointerDown({
  event,
  renderer,
  camera,
  controls,
  activeSketchPlaneId,
  activeSketchPlaneFrame,
  sketch,
  sceneData,
  hit,
  armedSketchConstraint,
  sketchMoveDragRef,
  sketchMoveRingGroupRef,
  sketchGroupRef,
  mode,
  entityIdsOverride,
  pendingSketchMoveRef,
}: BeginSketchMovePointerDownParams): boolean {
  if (!sketch) {
    return false;
  }

  // When a constraint is armed, clicks apply the constraint — no move.
  if (armedSketchConstraint) {
    return false;
  }

  const entityIds =
    entityIdsOverride ??
    sketchMoveEntityIdsForHit(hit, sketch, sceneData);
  if (entityIds.length === 0) {
    return false;
  }

  // Fixed vertices never move (mirrors the core's semantics): filter
  // them out so a fully-fixed selection refuses to start a drag.
  const vertexIds = sketchMoveEntityVertices(sketch, entityIds).filter(
    (id) => !sketch.vertices.find((v) => v.vertex_id === id)?.is_fixed,
  );
  if (vertexIds.length === 0) {
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

  const baseVertexPositions = new Map<string, [number, number]>();
  for (const id of vertexIds) {
    const vertex = sketch.vertices.find((v) => v.vertex_id === id);
    if (vertex) {
      baseVertexPositions.set(id, [vertex.x, vertex.y]);
    }
  }

  // With the Move/Copy dialog open, the pending transform exists and
  // the drag accumulates into it; the rotation center stays at the
  // dialog-open centroid.  A dialog opened without a selection gets its
  // pending state from the first drag's target set.
  const pending =
    pendingSketchMoveRef?.current ??
    createPendingSketchMove(sketch, entityIds);
  if (pendingSketchMoveRef && pending && !pendingSketchMoveRef.current) {
    pendingSketchMoveRef.current = pending;
  }

  const centerLocal =
    pending?.centerLocal ?? sketchMoveCentroid(sketch, vertexIds);
  const centerWorld = toWorldPoint(
    activeSketchPlaneId,
    centerLocal,
    activeSketchPlaneFrame,
  );

  // Alt+drag rotates (where the OS doesn't swallow Alt); the caller can
  // force rotate for the right-button drag path.
  const resolvedMode: "translate" | "rotate" =
    mode ?? (event.altKey ? "rotate" : "translate");

  const startAngle = sketchMoveScreenAngle(
    event.clientX,
    event.clientY,
    centerWorld,
    camera,
    renderer,
  );

  sketchMoveDragRef.current = {
    entityIds,
    vertexIds,
    mode: resolvedMode,
    startClientX: event.clientX,
    startClientY: event.clientY,
    startLocal: [rawPoint.local[0], rawPoint.local[1]],
    lastLocal: [rawPoint.local[0], rawPoint.local[1]],
    startAngle,
    lastAngle: startAngle,
    centerLocal,
    baseVertexPositions,
    hasMoved: false,
  };

  // Rotation ring around the centroid — only in rotate mode, where it
  // marks the rotation center.  During translate drags it reads as a
  // spurious extra circle around the selection centroid.
  if (resolvedMode === "rotate") {
    const sketchGroup = sketchGroupRef.current;
    if (sketchGroup) {
      const ring = buildSketchMoveRingObject({
        centerWorld,
        radius: sketchMoveRingRadius(sketch, vertexIds),
        planeFrame: activeSketchPlaneFrame,
      });
      sketchGroup.add(ring.group);
      sketchMoveRingGroupRef.current = ring.group;
    }
  }

  controls.enabled = false;
  renderer.domElement.setPointerCapture(event.pointerId);
  (renderer.domElement as HTMLCanvasElement).style.cursor = "grabbing";
  return true;
}
