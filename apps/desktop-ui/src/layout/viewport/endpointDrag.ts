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

function endpointDragAnchors(
  params: SketchFeatureParameters | null,
  pointId: string,
) {
  if (!params) {
    return [];
  }

  const anchors: Array<{ x: number; y: number }> = [];
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

export interface PendingEndpointDragFrame {
  pointId: string;
  x: number;
  y: number;
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
    const result = gcsBridge.solve(paramsCopy, constraints ?? []);
    if (result.ok) {
      const solved = result.points.find((p) => p.id === next.pointId);
      if (solved) {
        finalLocal = [solved.x, solved.y];
        solverUsed = true;
      }
    }
  }

  return {
    sketchPoint: { ...sketchPoint, local: finalLocal },
    previewLines: buildEndpointDragPreviewLines({
      segments: endpointDragPreviewSegments(
        sketch,
        next.pointId,
        finalLocal,
      ),
      planeId,
      planeFrame,
      // Blue when constraint-solver corrected the position,
      // yellow when using raw TS snap (bridge unavailable).
      color: solverUsed ? 0x6495ed : undefined,
    }),
  };
}
