import * as THREE from "three";

import type { SketchFeatureParameters, SketchPlaneFrame } from "@/types";
import { useCadCoreStore } from "@/state";
import { resolveSketchPlanePoint } from "@/utils";
import { endpointDragDelta, type EndpointDrag } from "./endpointDrag";

/** Id of a standalone Point-tool point sitting exactly at `(x, y)`, excluding
 *  the vertex being dragged. A drop that landed on a point passes this to the
 *  core, which binds and absorbs the point instead of leaving a duplicate
 *  vertex hidden under the endpoint. Coordinates are compared with a 1e-6
 *  tolerance because both sides come from the same snap. */
export function pointIdAtPosition(
  sketch: SketchFeatureParameters | null | undefined,
  x: number,
  y: number,
  excludeVertexId?: string,
  tolerance = 1e-6,
): string | undefined {
  if (!sketch?.points) {
    return undefined;
  }
  for (const point of sketch.points) {
    if (point.point_id === excludeVertexId) {
      continue;
    }
    if (Math.abs(point.x - x) <= tolerance && Math.abs(point.y - y) <= tolerance) {
      return point.point_id;
    }
  }
  return undefined;
}

interface MutableRef<T> {
  current: T;
}

interface EndpointDragPointerUpParams {
  event: PointerEvent;
  renderer: THREE.WebGLRenderer;
  camera: THREE.Camera;
  controls: { enabled: boolean };
  endpointDragRef: MutableRef<EndpointDrag | null>;
  dragSnapResultRef: MutableRef<{ snapX: number; snapY: number } | null>;
  activeSketchPlaneIdRef: MutableRef<string | null>;
  activeSketchPlaneFrameRef: MutableRef<SketchPlaneFrame | null>;
  pendingEndpointCommitRef: MutableRef<boolean>;
  dragCursorRef: MutableRef<{ x: number; y: number } | null>;
  updateSketchPoint: (
    pointId: string,
    x: number,
    y: number,
    bindToVertexId?: string,
  ) => Promise<void> | void;
  setConstraintPreview: (preview: null) => void;
  setSketchSnapLabel: (label: string | null) => void;
  setHoveredSketchEntity: (entityId: string | null) => void;
  setHoveredSketchPoint: (pointId: string | null) => void;
  setPointerDown: (point: { x: number; y: number } | null) => void;
  /** Restores committed geometry after a preview that mutated real scene
   *  objects (drag released without crossing the move threshold). */
  restorePreviewScene: () => void;
}

export function finishEndpointDragPointerUp({
  event,
  renderer,
  camera,
  controls,
  endpointDragRef,
  dragSnapResultRef,
  activeSketchPlaneIdRef,
  activeSketchPlaneFrameRef,
  pendingEndpointCommitRef,
  dragCursorRef,
  updateSketchPoint,
  setConstraintPreview,
  setSketchSnapLabel,
  setHoveredSketchEntity,
  setHoveredSketchPoint,
  setPointerDown,
  restorePreviewScene,
}: EndpointDragPointerUpParams) {
  const drag = endpointDragRef.current;
  if (!drag) {
    return "inactive" as const;
  }

  const { hasMoved } = endpointDragDelta(event, drag);
  // M24: the pointer-down pinned the document revision.  A core command
  // (e.g. an awaited undo) landing mid-drag changed the sketch under the
  // pointer — discard the commit and rebuild the committed scene instead
  // of moving a vertex that may no longer exist or mean the same thing.
  const currentRevision = useCadCoreStore.getState().document?.revision ?? 0;
  if (hasMoved && currentRevision !== drag.startRevision) {
    restorePreviewScene();
    setConstraintPreview(null);
    dragCursorRef.current = null;
    controls.enabled = true;
    (renderer.domElement as HTMLCanvasElement).style.cursor = "";
    setSketchSnapLabel(null);
    setHoveredSketchEntity(null);
    setHoveredSketchPoint(null);
    setPointerDown(null);
    return "consumed" as const;
  }
  if (hasMoved) {
    const committed = commitEndpointDrag({
      event,
      renderer,
      camera,
      drag,
      dragSnapResultRef,
      activeSketchPlaneIdRef,
      activeSketchPlaneFrameRef,
      updateSketchPoint,
    });
    if (committed) {
      pendingEndpointCommitRef.current = true;
    } else {
      // The drop would collapse a connected segment (the core refuses those
      // with "Sketch lines must have non-zero length"). Put the preview back
      // instead of provoking an error.
      restorePreviewScene();
      setConstraintPreview(null);
      dragCursorRef.current = null;
    }
  } else {
    // The preview mutated the real scene objects; force a rebuild from
    // the store state so the committed geometry is shown again.
    restorePreviewScene();
    setConstraintPreview(null);
    dragCursorRef.current = null;
  }

  controls.enabled = true;
  (renderer.domElement as HTMLCanvasElement).style.cursor = "";
  setSketchSnapLabel(null);
  setHoveredSketchEntity(null);
  setHoveredSketchPoint(null);

  if (hasMoved) {
    setPointerDown(null);
    return "consumed" as const;
  }

  setPointerDown({ x: event.clientX, y: event.clientY });
  return "continue" as const;
}

/** True when `(x, y)` at `vertexId` would sit on top of a vertex that shares
 *  a line or arc with it — the degenerate move the core rejects. The drag
 *  previews such a position happily (the solver just folds), so the commit is
 *  what must refuse it. */
export function endpointDropWouldCollapse(
  sketch: SketchFeatureParameters | null | undefined,
  vertexId: string,
  x: number,
  y: number,
  tolerance = 0.01,
): boolean {
  if (!sketch) {
    return false;
  }
  const neighbours = new Set<string>();
  for (const line of sketch.lines) {
    if (line.start_vertex_id === vertexId) {
      neighbours.add(line.end_vertex_id);
    } else if (line.end_vertex_id === vertexId) {
      neighbours.add(line.start_vertex_id);
    }
  }
  for (const arc of sketch.arcs ?? []) {
    if (arc.start_vertex_id === vertexId) {
      neighbours.add(arc.end_vertex_id);
    } else if (arc.end_vertex_id === vertexId) {
      neighbours.add(arc.start_vertex_id);
    }
  }
  for (const vertex of sketch.vertices) {
    if (!neighbours.has(vertex.vertex_id)) {
      continue;
    }
    if (Math.hypot(vertex.x - x, vertex.y - y) < tolerance) {
      return true;
    }
  }
  return false;
}

function activeSketchParameters(): SketchFeatureParameters | null {
  const document = useCadCoreStore.getState().document;
  if (!document?.active_sketch_feature_id) {
    return null;
  }
  return (
    document.feature_history.find(
      (feature) => feature.feature_id === document.active_sketch_feature_id,
    )?.sketch_parameters ?? null
  );
}

function commitEndpointDrag({
  event,
  renderer,
  camera,
  drag,
  dragSnapResultRef,
  activeSketchPlaneIdRef,
  activeSketchPlaneFrameRef,
  updateSketchPoint,
}: Pick<
  EndpointDragPointerUpParams,
  | "event"
  | "renderer"
  | "camera"
  | "dragSnapResultRef"
  | "activeSketchPlaneIdRef"
  | "activeSketchPlaneFrameRef"
  | "updateSketchPoint"
> & {
  drag: EndpointDrag;
}): boolean {
  const sketch = activeSketchParameters();
  const snapResult = dragSnapResultRef.current;
  if (snapResult) {
    if (
      endpointDropWouldCollapse(
        sketch,
        drag.vertexId,
        snapResult.snapX,
        snapResult.snapY,
      )
    ) {
      return false;
    }
    void updateSketchPoint(
      drag.vertexId,
      snapResult.snapX,
      snapResult.snapY,
      // Dropped on a standalone Point: bind it into the endpoint.
      pointIdAtPosition(
        sketch,
        snapResult.snapX,
        snapResult.snapY,
        drag.vertexId,
      ),
    );
    return true;
  }

  const activeSketchPlaneId = activeSketchPlaneIdRef.current;
  if (!activeSketchPlaneId) {
    return false;
  }
  const rawPoint = resolveSketchPlanePoint(
    event,
    renderer,
    camera,
    activeSketchPlaneId,
    activeSketchPlaneFrameRef.current,
  );
  if (!rawPoint) {
    return false;
  }
  if (
    endpointDropWouldCollapse(
      sketch,
      drag.vertexId,
      rawPoint.local[0],
      rawPoint.local[1],
    )
  ) {
    return false;
  }
  void updateSketchPoint(
    drag.vertexId,
    rawPoint.local[0],
    rawPoint.local[1],
    pointIdAtPosition(
      sketch,
      rawPoint.local[0],
      rawPoint.local[1],
      drag.vertexId,
    ),
  );
  return true;
}
