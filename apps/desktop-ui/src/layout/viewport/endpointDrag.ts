import * as THREE from "three";

import type {
  SketchFeatureParameters,
  SketchPlaneFrame,
  SketchPreviewPoint,
} from "@/types";
import { toWorldPoint } from "@/utils";

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
}: {
  segments: readonly EndpointDragPreviewSegment[];
  planeId: string;
  planeFrame: SketchPlaneFrame | null;
}) {
  return segments.map((segment) => {
    const mat = new THREE.LineDashedMaterial({
      color: 0xffe784,
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

  return {
    sketchPoint,
    previewLines: buildEndpointDragPreviewLines({
      segments: endpointDragPreviewSegments(
        sketch,
        next.pointId,
        sketchPoint.local,
      ),
      planeId,
      planeFrame,
    }),
  };
}
