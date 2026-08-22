import * as THREE from "three";

import type { ViewportScene } from "@/types";
import { projectWorldPointToViewport } from "@/utils";

export interface ScreenRect {
  x1: number;
  y1: number;
  x2: number;
  y2: number;
}

export interface SelectionDrag {
  startX: number;
  startY: number;
  currentX: number;
  currentY: number;
  active: boolean;
}

export interface SelectionRectOverlay {
  left: number;
  top: number;
  width: number;
  height: number;
  visible: boolean;
  direction: "window" | "crossing";
}

interface MutableRef<T> {
  current: T;
}

export function selectionRectOverlayFromDrag(
  drag: SelectionDrag,
): SelectionRectOverlay {
  const left = Math.min(drag.startX, drag.currentX);
  const top = Math.min(drag.startY, drag.currentY);
  const width = Math.abs(drag.currentX - drag.startX);
  const height = Math.abs(drag.currentY - drag.startY);
  return {
    left,
    top,
    width,
    height,
    visible: width > 3 || height > 3,
    direction: drag.currentX >= drag.startX ? "window" : "crossing",
  };
}

export function finishRectangleSelectionDrag({
  event,
  selectionDragRef,
  setSelectionRect,
  controls,
  performRectangleSelect,
}: {
  event: PointerEvent;
  selectionDragRef: MutableRef<SelectionDrag | null>;
  setSelectionRect: (overlay: SelectionRectOverlay | null) => void;
  controls: { enabled: boolean };
  performRectangleSelect: (
    drag: SelectionDrag,
    additive: boolean,
  ) => Promise<void>;
}) {
  const drag = selectionDragRef.current;
  if (!drag?.active) {
    return false;
  }

  const width = Math.abs(drag.currentX - drag.startX);
  const height = Math.abs(drag.currentY - drag.startY);
  const wasRealDrag = width > 3 || height > 3;
  if (wasRealDrag) {
    void performRectangleSelect(drag, event.shiftKey);
  }
  selectionDragRef.current = null;
  setSelectionRect(null);
  controls.enabled = true;
  // Only consume the event if there was an actual drag. A simple click
  // on empty canvas should propagate so the selection flow can deselect.
  return wasRealDrag;
}

function cross2d(
  x1: number,
  y1: number,
  x2: number,
  y2: number,
  x3: number,
  y3: number,
): number {
  return (x2 - x1) * (y3 - y1) - (y2 - y1) * (x3 - x1);
}

function pointOnSegment(
  x1: number,
  y1: number,
  x2: number,
  y2: number,
  px: number,
  py: number,
): boolean {
  return (
    Math.min(x1, x2) <= px &&
    px <= Math.max(x1, x2) &&
    Math.min(y1, y2) <= py &&
    py <= Math.max(y1, y2)
  );
}

function segmentsIntersect(
  ax1: number,
  ay1: number,
  ax2: number,
  ay2: number,
  bx1: number,
  by1: number,
  bx2: number,
  by2: number,
): boolean {
  const d1 = cross2d(ax1, ay1, ax2, ay2, bx1, by1);
  const d2 = cross2d(ax1, ay1, ax2, ay2, bx2, by2);
  const d3 = cross2d(bx1, by1, bx2, by2, ax1, ay1);
  const d4 = cross2d(bx1, by1, bx2, by2, ax2, ay2);
  if (
    ((d1 > 0 && d2 < 0) || (d1 < 0 && d2 > 0)) &&
    ((d3 > 0 && d4 < 0) || (d3 < 0 && d4 > 0))
  ) {
    return true;
  }
  if (d1 === 0 && pointOnSegment(ax1, ay1, ax2, ay2, bx1, by1)) {
    return true;
  }
  if (d2 === 0 && pointOnSegment(ax1, ay1, ax2, ay2, bx2, by2)) {
    return true;
  }
  if (d3 === 0 && pointOnSegment(bx1, by1, bx2, by2, ax1, ay1)) {
    return true;
  }
  if (d4 === 0 && pointOnSegment(bx1, by1, bx2, by2, ax2, ay2)) {
    return true;
  }
  return false;
}

function segmentCrossesRect(
  x1: number,
  y1: number,
  x2: number,
  y2: number,
  rect: ScreenRect,
): boolean {
  return (
    segmentsIntersect(x1, y1, x2, y2, rect.x1, rect.y1, rect.x2, rect.y1) ||
    segmentsIntersect(x1, y1, x2, y2, rect.x2, rect.y1, rect.x2, rect.y2) ||
    segmentsIntersect(x1, y1, x2, y2, rect.x2, rect.y2, rect.x1, rect.y2) ||
    segmentsIntersect(x1, y1, x2, y2, rect.x1, rect.y2, rect.x1, rect.y1)
  );
}

function boxesIntersect(
  ax1: number,
  ay1: number,
  ax2: number,
  ay2: number,
  bx1: number,
  by1: number,
  bx2: number,
  by2: number,
): boolean {
  return !(ax2 < bx1 || bx2 < ax1 || ay2 < by1 || by2 < ay1);
}

export function collectRectangleSelectionIds({
  drag,
  sceneData,
  camera,
  renderer,
}: {
  drag: SelectionDrag;
  sceneData: ViewportScene | null;
  camera: THREE.Camera;
  renderer: THREE.WebGLRenderer;
}): string[] {
  if (!sceneData) {
    return [];
  }

  const canvasRect = renderer.domElement.getBoundingClientRect();
  const rect = {
    x1: Math.min(drag.startX, drag.currentX) - canvasRect.left,
    y1: Math.min(drag.startY, drag.currentY) - canvasRect.top,
    x2: Math.max(drag.startX, drag.currentX) - canvasRect.left,
    y2: Math.max(drag.startY, drag.currentY) - canvasRect.top,
  };
  const isWindow = drag.currentX >= drag.startX;
  const insideRect = (px: number, py: number) =>
    px >= rect.x1 && px <= rect.x2 && py >= rect.y1 && py <= rect.y2;
  const selected: string[] = [];

  for (const line of sceneData.sketchLines) {
    if (line.isPreview || line.isConstruction) {
      continue;
    }
    const start = projectWorldPointToViewport(line.start, camera, renderer);
    const end = projectWorldPointToViewport(line.end, camera, renderer);
    if (!start || !end) {
      continue;
    }
    if (isWindow) {
      if (insideRect(start.x, start.y) && insideRect(end.x, end.y)) {
        selected.push(line.lineId);
      }
      continue;
    }
    if (
      insideRect(start.x, start.y) ||
      insideRect(end.x, end.y) ||
      segmentCrossesRect(start.x, start.y, end.x, end.y, rect)
    ) {
      selected.push(line.lineId);
    }
  }

  for (const circle of sceneData.sketchCircles) {
    const center = projectWorldPointToViewport(circle.center, camera, renderer);
    if (!center) {
      continue;
    }
    const right = projectWorldPointToViewport(
      [circle.center[0] + circle.radius, circle.center[1], circle.center[2]],
      camera,
      renderer,
    );
    const approxRadius = right ? Math.abs(right.x - center.x) : 0;
    const bx1 = center.x - approxRadius;
    const by1 = center.y - approxRadius;
    const bx2 = center.x + approxRadius;
    const by2 = center.y + approxRadius;
    if (isWindow) {
      if (bx1 >= rect.x1 && bx2 <= rect.x2 && by1 >= rect.y1 && by2 <= rect.y2) {
        selected.push(circle.circleId);
      }
      continue;
    }
    if (boxesIntersect(bx1, by1, bx2, by2, rect.x1, rect.y1, rect.x2, rect.y2)) {
      selected.push(circle.circleId);
    }
  }

  for (const ellipse of sceneData.sketchEllipses) {
    if (ellipse.isPreview || ellipse.isConstruction) {
      continue;
    }
    const center = projectWorldPointToViewport(ellipse.center, camera, renderer);
    if (!center) {
      continue;
    }
    // Same single-axis screen-radius approximation as circles, sized
    // by the larger radius so the box covers the rotated major axis.
    const maxRadius = Math.max(ellipse.a, ellipse.b);
    const right = projectWorldPointToViewport(
      [ellipse.center[0] + maxRadius, ellipse.center[1], ellipse.center[2]],
      camera,
      renderer,
    );
    const approxRadius = right ? Math.abs(right.x - center.x) : 0;
    const bx1 = center.x - approxRadius;
    const by1 = center.y - approxRadius;
    const bx2 = center.x + approxRadius;
    const by2 = center.y + approxRadius;
    if (isWindow) {
      if (bx1 >= rect.x1 && bx2 <= rect.x2 && by1 >= rect.y1 && by2 <= rect.y2) {
        selected.push(ellipse.ellipseId);
      }
      continue;
    }
    if (boxesIntersect(bx1, by1, bx2, by2, rect.x1, rect.y1, rect.x2, rect.y2)) {
      selected.push(ellipse.ellipseId);
    }
  }

  for (const arc of sceneData.sketchArcs) {
    if (arc.isConstruction) {
      continue;
    }
    const start = projectWorldPointToViewport(arc.start, camera, renderer);
    const end = projectWorldPointToViewport(arc.end, camera, renderer);
    if (!start || !end) {
      continue;
    }

    const centerX = arc.center[0];
    const centerY = arc.center[1];
    const radius = Math.hypot(arc.start[0] - centerX, arc.start[1] - centerY);
    const extremes: Array<[number, number]> = [
      [centerX + radius, centerY],
      [centerX - radius, centerY],
      [centerX, centerY + radius],
      [centerX, centerY - radius],
    ];
    let bx1 = Math.min(start.x, end.x);
    let by1 = Math.min(start.y, end.y);
    let bx2 = Math.max(start.x, end.x);
    let by2 = Math.max(start.y, end.y);
    for (const [worldX, worldY] of extremes) {
      const point = projectWorldPointToViewport(
        [worldX, worldY, arc.start[2]],
        camera,
        renderer,
      );
      if (point) {
        bx1 = Math.min(bx1, point.x);
        by1 = Math.min(by1, point.y);
        bx2 = Math.max(bx2, point.x);
        by2 = Math.max(by2, point.y);
      }
    }

    if (isWindow) {
      if (insideRect(start.x, start.y) && insideRect(end.x, end.y)) {
        selected.push(arc.arcId);
      }
      continue;
    }
    if (
      insideRect(start.x, start.y) ||
      insideRect(end.x, end.y) ||
      boxesIntersect(bx1, by1, bx2, by2, rect.x1, rect.y1, rect.x2, rect.y2)
    ) {
      selected.push(arc.arcId);
    }
  }

  return selected;
}
