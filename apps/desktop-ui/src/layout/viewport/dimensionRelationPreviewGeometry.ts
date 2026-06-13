import type {
  SketchCircleEntry,
  SketchDimensionScene,
  SketchLineEntry,
  SketchPlaneFrame,
} from "@/types";
import { toWorldPoint } from "@/utils";
import { clampAngleRadius, formatDraftDimension } from "./draftDimensions";

interface AnglePreviewState {
  isReflex: boolean;
  angle: number;
}

function lineLength(line: SketchLineEntry) {
  return Math.hypot(line.end_x - line.start_x, line.end_y - line.start_y);
}

function lineDirection(line: SketchLineEntry): [number, number] | null {
  const length = lineLength(line);
  if (length <= 1e-9) {
    return null;
  }
  return [
    (line.end_x - line.start_x) / length,
    (line.end_y - line.start_y) / length,
  ];
}

function midpointOfLine(line: SketchLineEntry): [number, number] {
  return [(line.start_x + line.end_x) / 2, (line.start_y + line.end_y) / 2];
}

export function distanceToLineSegment(
  point: [number, number],
  line: SketchLineEntry,
) {
  const dx = line.end_x - line.start_x;
  const dy = line.end_y - line.start_y;
  const lenSq = dx * dx + dy * dy;
  if (lenSq <= 1e-12) {
    return {
      distance: Math.hypot(point[0] - line.start_x, point[1] - line.start_y),
      local: [line.start_x, line.start_y] as [number, number],
      t: 0,
    };
  }

  const t = Math.max(
    0,
    Math.min(
      1,
      ((point[0] - line.start_x) * dx + (point[1] - line.start_y) * dy) /
        lenSq,
    ),
  );
  const local: [number, number] = [
    line.start_x + t * dx,
    line.start_y + t * dy,
  ];
  return {
    distance: Math.hypot(point[0] - local[0], point[1] - local[1]),
    local,
    t,
  };
}

export function distanceToCircleEdge(
  point: [number, number],
  circle: SketchCircleEntry,
) {
  const dx = point[0] - circle.center_x;
  const dy = point[1] - circle.center_y;
  const centerDistance = Math.hypot(dx, dy);
  return Math.abs(centerDistance - circle.radius);
}

export function areLinesParallel(
  first: SketchLineEntry,
  second: SketchLineEntry,
) {
  const firstDir = lineDirection(first);
  const secondDir = lineDirection(second);
  if (!firstDir || !secondDir) {
    return false;
  }
  return (
    Math.abs(firstDir[0] * secondDir[1] - firstDir[1] * secondDir[0]) < 0.03
  );
}

function pointOnSegment(
  px: number,
  py: number,
  ax: number,
  ay: number,
  bx: number,
  by: number,
  tolerance = 0.05,
): boolean {
  const dx = bx - ax;
  const dy = by - ay;
  const len2 = dx * dx + dy * dy;
  if (len2 < 1e-12) {
    return Math.hypot(px - ax, py - ay) <= tolerance;
  }
  const t = Math.max(0, Math.min(1, ((px - ax) * dx + (py - ay) * dy) / len2));
  const projX = ax + t * dx;
  const projY = ay + t * dy;
  return Math.hypot(px - projX, py - projY) <= tolerance;
}

function sharedLineEndpoint(
  first: SketchLineEntry,
  second: SketchLineEntry,
): [number, number] | null {
  if (first.start_point_id === second.start_point_id) {
    return [first.start_x, first.start_y];
  }
  if (first.start_point_id === second.end_point_id) {
    return [first.start_x, first.start_y];
  }
  if (first.end_point_id === second.start_point_id) {
    return [first.end_x, first.end_y];
  }
  if (first.end_point_id === second.end_point_id) {
    return [first.end_x, first.end_y];
  }

  if (
    pointOnSegment(
      first.start_x,
      first.start_y,
      second.start_x,
      second.start_y,
      second.end_x,
      second.end_y,
    )
  ) {
    return [first.start_x, first.start_y];
  }
  if (
    pointOnSegment(
      first.end_x,
      first.end_y,
      second.start_x,
      second.start_y,
      second.end_x,
      second.end_y,
    )
  ) {
    return [first.end_x, first.end_y];
  }
  if (
    pointOnSegment(
      second.start_x,
      second.start_y,
      first.start_x,
      first.start_y,
      first.end_x,
      first.end_y,
    )
  ) {
    return [second.start_x, second.start_y];
  }
  if (
    pointOnSegment(
      second.end_x,
      second.end_y,
      first.start_x,
      first.start_y,
      first.end_x,
      first.end_y,
    )
  ) {
    return [second.end_x, second.end_y];
  }

  return null;
}

export function sketchLinesShareEndpoint(
  first: SketchLineEntry,
  second: SketchLineEntry,
) {
  return sharedLineEndpoint(first, second) !== null;
}

function lineDirectionAwayFromPoint(
  line: SketchLineEntry,
  point: [number, number],
): [number, number] | null {
  const startDistance = Math.hypot(
    line.start_x - point[0],
    line.start_y - point[1],
  );
  const endDistance = Math.hypot(line.end_x - point[0], line.end_y - point[1]);
  const target =
    startDistance > endDistance
      ? [line.start_x, line.start_y]
      : [line.end_x, line.end_y];
  const dx = target[0] - point[0];
  const dy = target[1] - point[1];
  const length = Math.hypot(dx, dy);
  if (length <= 1e-9) {
    return null;
  }
  return [dx / length, dy / length];
}

function lineHasEndpointAt(
  line: SketchLineEntry,
  point: [number, number],
): boolean {
  return (
    Math.hypot(line.start_x - point[0], line.start_y - point[1]) <= 1e-6 ||
    Math.hypot(line.end_x - point[0], line.end_y - point[1]) <= 1e-6
  );
}

function lineDirectionTowardSmallerAngle(
  line: SketchLineEntry,
  point: [number, number],
  otherDirection: [number, number],
): [number, number] | null {
  const candidates: [number, number][] = [
    [line.start_x - point[0], line.start_y - point[1]],
    [line.end_x - point[0], line.end_y - point[1]],
  ];
  let best: [number, number] | null = null;
  let bestAngle = Number.POSITIVE_INFINITY;
  for (const candidate of candidates) {
    const length = Math.hypot(candidate[0], candidate[1]);
    if (length <= 1e-9) {
      continue;
    }
    const unit: [number, number] = [
      candidate[0] / length,
      candidate[1] / length,
    ];
    const angle = Math.abs(
      Math.atan2(
        unit[0] * otherDirection[1] - unit[1] * otherDirection[0],
        unit[0] * otherDirection[0] + unit[1] * otherDirection[1],
      ),
    );
    if (angle < bestAngle) {
      bestAngle = angle;
      best = unit;
    }
  }
  return best;
}

function formatAngleLabel(radians: number) {
  const degrees = Math.abs((radians * 180) / Math.PI);
  return `${formatDraftDimension(degrees)}°`;
}

export function createParallelLineDistancePreview({
  first,
  second,
  cursor,
  planeId,
  planeFrame,
}: {
  first: SketchLineEntry;
  second: SketchLineEntry;
  cursor: [number, number];
  planeId: string;
  planeFrame: SketchPlaneFrame | null;
}): SketchDimensionScene | null {
  const dir = lineDirection(first);
  if (!dir) {
    return null;
  }

  const normal: [number, number] = [-dir[1], dir[0]];
  const firstMid = midpointOfLine(first);
  const secondMid = midpointOfLine(second);
  const signedDistance =
    (secondMid[0] - firstMid[0]) * normal[0] +
    (secondMid[1] - firstMid[1]) * normal[1];
  if (Math.abs(signedDistance) <= 1e-6) {
    return null;
  }
  if (signedDistance < 0) {
    normal[0] *= -1;
    normal[1] *= -1;
  }

  const projection =
    (cursor[0] - firstMid[0]) * dir[0] + (cursor[1] - firstMid[1]) * dir[1];
  const anchorStart: [number, number] = [
    firstMid[0] + projection * dir[0],
    firstMid[1] + projection * dir[1],
  ];
  const distance = Math.abs(signedDistance);
  const anchorEnd: [number, number] = [
    anchorStart[0] + normal[0] * distance,
    anchorStart[1] + normal[1] * distance,
  ];
  const labelLocal: [number, number] = [
    (anchorStart[0] + anchorEnd[0]) / 2,
    (anchorStart[1] + anchorEnd[1]) / 2,
  ];

  return {
    dimensionId: "preview-dim-line-line-distance",
    planeId,
    kind: "line_line_distance",
    entityId: first.line_id,
    label: `${formatDraftDimension(distance)} mm`,
    rawValue: distance,
    unitSuffix: "mm",
    isSelected: false,
    anchorStart: toWorldPoint(planeId, anchorStart, planeFrame),
    anchorEnd: toWorldPoint(planeId, anchorEnd, planeFrame),
    dimensionStart: toWorldPoint(planeId, anchorStart, planeFrame),
    dimensionEnd: toWorldPoint(planeId, anchorEnd, planeFrame),
    labelPosition: toWorldPoint(planeId, labelLocal, planeFrame),
  };
}

export function createLineAnglePreview({
  first,
  second,
  cursor,
  planeId,
  planeFrame,
}: {
  first: SketchLineEntry;
  second: SketchLineEntry;
  cursor: [number, number];
  planeId: string;
  planeFrame: SketchPlaneFrame | null;
}): { dimension: SketchDimensionScene; anglePreview: AnglePreviewState } | null {
  const pivot = sharedLineEndpoint(first, second);
  if (!pivot) {
    return null;
  }
  const firstDir = lineDirectionAwayFromPoint(first, pivot);
  const secondDir = lineDirectionAwayFromPoint(second, pivot);
  if (!firstDir || !secondDir) {
    return null;
  }
  const firstEndpointAtPivot = lineHasEndpointAt(first, pivot);
  const secondEndpointAtPivot = lineHasEndpointAt(second, pivot);
  const resolvedFirstDir = firstEndpointAtPivot
    ? firstDir
    : (lineDirectionTowardSmallerAngle(first, pivot, secondDir) ?? firstDir);
  const resolvedSecondDir = secondEndpointAtPivot
    ? secondDir
    : (lineDirectionTowardSmallerAngle(second, pivot, firstDir) ?? secondDir);

  const dot = Math.max(
    -1,
    Math.min(
      1,
      resolvedFirstDir[0] * resolvedSecondDir[0] +
        resolvedFirstDir[1] * resolvedSecondDir[1],
    ),
  );
  const acuteAngle = Math.acos(dot);
  const sharedEndpoint =
    first.start_point_id === second.start_point_id ||
    first.start_point_id === second.end_point_id ||
    first.end_point_id === second.start_point_id ||
    first.end_point_id === second.end_point_id;
  const reflexThreshold = sharedEndpoint ? -0.3 : 0;

  let useReflex = false;
  const cursorVec = [cursor[0] - pivot[0], cursor[1] - pivot[1]];
  const cursorLen = Math.hypot(cursorVec[0], cursorVec[1]);
  if (cursorLen > 1e-9) {
    const cx = cursorVec[0] / cursorLen;
    const cy = cursorVec[1] / cursorLen;
    const bx = resolvedFirstDir[0] + resolvedSecondDir[0];
    const by = resolvedFirstDir[1] + resolvedSecondDir[1];
    const blen = Math.hypot(bx, by);
    if (blen > 1e-9) {
      useReflex = (cx * bx + cy * by) / blen < reflexThreshold;
    }
  }

  const angle = useReflex ? 2 * Math.PI - acuteAngle : acuteAngle;
  const cursorRadius = Math.hypot(cursor[0] - pivot[0], cursor[1] - pivot[1]);
  const radius = clampAngleRadius(cursorRadius);
  const dimensionStart: [number, number] = [
    pivot[0] + resolvedFirstDir[0] * radius,
    pivot[1] + resolvedFirstDir[1] * radius,
  ];
  const dimensionEnd: [number, number] = [
    pivot[0] + resolvedSecondDir[0] * radius,
    pivot[1] + resolvedSecondDir[1] * radius,
  ];
  const anchorRadius = Math.min(lineLength(first), lineLength(second), radius + 2);
  const anchorStart: [number, number] = [
    pivot[0] + resolvedFirstDir[0] * anchorRadius,
    pivot[1] + resolvedFirstDir[1] * anchorRadius,
  ];
  const anchorEnd: [number, number] = [
    pivot[0] + resolvedSecondDir[0] * anchorRadius,
    pivot[1] + resolvedSecondDir[1] * anchorRadius,
  ];
  const bisector: [number, number] = [
    resolvedFirstDir[0] + resolvedSecondDir[0],
    resolvedFirstDir[1] + resolvedSecondDir[1],
  ];
  const bisectorLength = Math.hypot(bisector[0], bisector[1]);
  if (bisectorLength <= 1e-9) {
    return null;
  }
  bisector[0] /= bisectorLength;
  bisector[1] /= bisectorLength;
  const labelLocal: [number, number] = [
    pivot[0] + bisector[0] * radius,
    pivot[1] + bisector[1] * radius,
  ];

  return {
    anglePreview: { isReflex: useReflex, angle },
    dimension: {
      dimensionId: "preview-dim-line-angle",
      planeId,
      kind: "line_angle",
      entityId: first.line_id,
      label: formatAngleLabel(angle),
      rawValue: angle,
      unitSuffix: "°",
      isSelected: false,
      anchorStart: toWorldPoint(planeId, anchorStart, planeFrame),
      anchorEnd: toWorldPoint(planeId, anchorEnd, planeFrame),
      dimensionStart: toWorldPoint(planeId, dimensionStart, planeFrame),
      dimensionEnd: toWorldPoint(planeId, dimensionEnd, planeFrame),
      labelPosition: toWorldPoint(planeId, labelLocal, planeFrame),
      arcCenter: toWorldPoint(planeId, pivot, planeFrame),
      arcRadius: radius,
      arcStartAngle: Math.atan2(firstDir[1], firstDir[0]),
      arcEndAngle: Math.atan2(secondDir[1], secondDir[0]),
      arcCcw: true,
    },
  };
}

export function createCircleLineDistancePreview({
  line,
  circle,
  planeId,
  planeFrame,
}: {
  line: SketchLineEntry;
  circle: SketchCircleEntry;
  planeId: string;
  planeFrame: SketchPlaneFrame | null;
}): SketchDimensionScene | null {
  const closest = distanceToLineSegment([circle.center_x, circle.center_y], line);
  const dx = circle.center_x - closest.local[0];
  const dy = circle.center_y - closest.local[1];
  const centerDistance = Math.hypot(dx, dy);
  if (centerDistance <= 1e-9) {
    return null;
  }
  const nx = dx / centerDistance;
  const ny = dy / centerDistance;
  const circleEdge: [number, number] = [
    circle.center_x - nx * circle.radius,
    circle.center_y - ny * circle.radius,
  ];
  const distance = Math.max(0, centerDistance - circle.radius);
  const labelLocal: [number, number] = [
    (closest.local[0] + circleEdge[0]) / 2,
    (closest.local[1] + circleEdge[1]) / 2,
  ];

  return {
    dimensionId: "preview-dim-circle-line-distance",
    planeId,
    kind: "circle_line_distance",
    entityId: line.line_id,
    label: `${formatDraftDimension(distance)} mm`,
    rawValue: distance,
    unitSuffix: "mm",
    isSelected: false,
    anchorStart: toWorldPoint(planeId, closest.local, planeFrame),
    anchorEnd: toWorldPoint(planeId, circleEdge, planeFrame),
    dimensionStart: toWorldPoint(planeId, closest.local, planeFrame),
    dimensionEnd: toWorldPoint(planeId, circleEdge, planeFrame),
    labelPosition: toWorldPoint(planeId, labelLocal, planeFrame),
  };
}

export function createCircleCenterDistancePreview({
  first,
  second,
  planeId,
  planeFrame,
}: {
  first: SketchCircleEntry;
  second: SketchCircleEntry;
  planeId: string;
  planeFrame: SketchPlaneFrame | null;
}): SketchDimensionScene | null {
  const dx = second.center_x - first.center_x;
  const dy = second.center_y - first.center_y;
  const distance = Math.hypot(dx, dy);
  if (distance <= 1e-9) {
    return null;
  }
  const start: [number, number] = [first.center_x, first.center_y];
  const end: [number, number] = [second.center_x, second.center_y];
  const labelLocal: [number, number] = [
    (first.center_x + second.center_x) / 2,
    (first.center_y + second.center_y) / 2,
  ];

  return {
    dimensionId: "preview-dim-circle-center-distance",
    planeId,
    kind: "circle_center_distance",
    entityId: first.circle_id,
    label: `${formatDraftDimension(distance)} mm`,
    rawValue: distance,
    unitSuffix: "mm",
    isSelected: false,
    anchorStart: toWorldPoint(planeId, start, planeFrame),
    anchorEnd: toWorldPoint(planeId, end, planeFrame),
    dimensionStart: toWorldPoint(planeId, start, planeFrame),
    dimensionEnd: toWorldPoint(planeId, end, planeFrame),
    labelPosition: toWorldPoint(planeId, labelLocal, planeFrame),
  };
}
