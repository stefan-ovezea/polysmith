import type { ViewportScene } from "@/types";
import { lineCircleIntersectionTrim, lineLineIntersectionTrim } from "@/utils";

export interface TrimPlaneFrame {
  origin: { x: number; y: number; z: number };
  x_axis: { x: number; y: number; z: number };
  y_axis: { x: number; y: number; z: number };
}

export interface TrimHoverTarget {
  id: string;
  entityKind: "line" | "circle" | "arc";
}

export interface TrimLineHighlightSegment {
  sx: number;
  sy: number;
  sz: number;
  ex: number;
  ey: number;
  ez: number;
}

export type TrimHoverPreview =
  | {
      kind: "line";
      lineId: string;
      segments: TrimLineHighlightSegment[];
      hoveredSegmentIndex: number;
    }
  | {
      kind: "arc";
      points: Array<[number, number, number]>;
    };

function wrapAngle(angle: number): number {
  let result = angle;
  while (result < 0) {
    result += 2 * Math.PI;
  }
  while (result >= 2 * Math.PI) {
    result -= 2 * Math.PI;
  }
  return result;
}

export function trimWorldPointToLocal(
  point: [number, number, number],
  planeId: string,
  planeFrame: TrimPlaneFrame | null,
): [number, number] {
  const [px, py, pz] = point;
  if (planeFrame) {
    const dx = px - planeFrame.origin.x;
    const dy = py - planeFrame.origin.y;
    const dz = pz - planeFrame.origin.z;
    return [
      dx * planeFrame.x_axis.x + dy * planeFrame.x_axis.y + dz * planeFrame.x_axis.z,
      dx * planeFrame.y_axis.x + dy * planeFrame.y_axis.y + dz * planeFrame.y_axis.z,
    ];
  }
  if (planeId === "ref-plane-xy") {
    return [px, py];
  }
  if (planeId === "ref-plane-yz") {
    return [py, pz];
  }
  return [px, pz];
}

export function handleSketchTrimClick({
  hit,
  planeId,
  planeFrame,
  trimSketchEntity,
}: {
  hit:
    | {
        kind: "sketch_entity";
        id: string;
        entityKind: string | null;
        worldPoint: readonly [number, number, number];
      }
    | null
    | undefined;
  planeId: string;
  planeFrame: TrimPlaneFrame | null;
  trimSketchEntity:
    | ((entityId: string, localX: number, localY: number) => Promise<void>)
    | null
    | undefined;
}) {
  if (
    hit?.kind !== "sketch_entity" ||
    (hit.entityKind !== "line" &&
      hit.entityKind !== "circle" &&
      hit.entityKind !== "arc")
  ) {
    return true;
  }

  const [localX, localY] = trimWorldPointToLocal(
    [hit.worldPoint[0], hit.worldPoint[1], hit.worldPoint[2]],
    planeId,
    planeFrame,
  );
  void trimSketchEntity?.(hit.id, localX, localY);
  return true;
}

function toWorldPoint(
  point: [number, number],
  planeId: string,
  planeFrame: TrimPlaneFrame | null,
): [number, number, number] {
  const [ux, uy] = point;
  if (planeFrame) {
    return [
      planeFrame.origin.x + ux * planeFrame.x_axis.x + uy * planeFrame.y_axis.x,
      planeFrame.origin.y + ux * planeFrame.x_axis.y + uy * planeFrame.y_axis.y,
      planeFrame.origin.z + ux * planeFrame.x_axis.z + uy * planeFrame.y_axis.z,
    ];
  }
  if (planeId === "ref-plane-xy") {
    return [ux, uy, 0];
  }
  if (planeId === "ref-plane-yz") {
    return [0, ux, uy];
  }
  return [ux, 0, uy];
}

function sampleTrimArcPoints({
  center,
  radius,
  startAngle,
  endAngle,
  planeId,
  planeFrame,
}: {
  center: [number, number];
  radius: number;
  startAngle: number;
  endAngle: number;
  planeId: string;
  planeFrame: TrimPlaneFrame | null;
}) {
  const points: Array<[number, number, number]> = [];
  for (let index = 0; index <= 48; index += 1) {
    const angle = startAngle + (endAngle - startAngle) * (index / 48);
    points.push(
      toWorldPoint(
        [
          center[0] + radius * Math.cos(angle),
          center[1] + radius * Math.sin(angle),
        ],
        planeId,
        planeFrame,
      ),
    );
  }
  return points;
}

function collectLineCircleIntersectionAngles({
  sceneData,
  center,
  radius,
  planeId,
  planeFrame,
  onAngle,
}: {
  sceneData: ViewportScene;
  center: [number, number];
  radius: number;
  planeId: string;
  planeFrame: TrimPlaneFrame | null;
  onAngle: (angle: number) => void;
}) {
  const [clx, cly] = center;
  for (const other of sceneData.sketchLines) {
    if (other.isConstruction) {
      continue;
    }
    const [ox1, oy1] = trimWorldPointToLocal(other.start, planeId, planeFrame);
    const [ox2, oy2] = trimWorldPointToLocal(other.end, planeId, planeFrame);
    const ts = lineCircleIntersectionTrim(
      ox1,
      oy1,
      ox2,
      oy2,
      clx,
      cly,
      radius,
    );
    for (const t of ts) {
      const ix = ox1 + t * (ox2 - ox1);
      const iy = oy1 + t * (oy2 - oy1);
      onAngle(Math.atan2(iy - cly, ix - clx));
    }
  }
}

function circleCircleIntersectionAngles({
  center,
  radius,
  otherCenter,
  otherRadius,
}: {
  center: [number, number];
  radius: number;
  otherCenter: [number, number];
  otherRadius: number;
}): number[] {
  const [clx, cly] = center;
  const [ocx, ocy] = otherCenter;
  const dx = ocx - clx;
  const dy = ocy - cly;
  const distance = Math.hypot(dx, dy);
  if (
    distance > radius + otherRadius + 0.01 ||
    distance < Math.abs(radius - otherRadius) - 0.01
  ) {
    return [];
  }

  const a = (radius * radius - otherRadius * otherRadius + distance * distance) / (2 * distance);
  const hSq = radius * radius - a * a;
  const h = hSq <= 0 ? 0 : Math.sqrt(hSq);
  const px = clx + (a * dx) / distance;
  const py = cly + (a * dy) / distance;
  const hx = (-dy * h) / distance;
  const hy = (dx * h) / distance;
  const angles = [wrapAngle(Math.atan2(py + hy - cly, px + hx - clx))];
  if (hSq > 1e-12) {
    angles.push(wrapAngle(Math.atan2(py - hy - cly, px - hx - clx)));
  }
  return angles;
}

function dedupeSortedNumbers(values: number[], tolerance = 0.01): number[] {
  if (values.length === 0) {
    return [];
  }
  const deduped = [values[0]];
  for (let index = 1; index < values.length; index++) {
    if (Math.abs(values[index] - deduped[deduped.length - 1]) > tolerance) {
      deduped.push(values[index]);
    }
  }
  return deduped;
}

function computeLineTrimPreview({
  sceneData,
  targetId,
  cursorLocal,
  planeId,
  planeFrame,
}: {
  sceneData: ViewportScene;
  targetId: string;
  cursorLocal: [number, number];
  planeId: string;
  planeFrame: TrimPlaneFrame | null;
}): TrimHoverPreview | null {
  const lineData = sceneData.sketchLines.find((line) => line.lineId === targetId);
  if (!lineData) {
    return null;
  }

  const [ax, ay] = trimWorldPointToLocal(lineData.start, planeId, planeFrame);
  const [bx, by] = trimWorldPointToLocal(lineData.end, planeId, planeFrame);
  const ts: number[] = [];

  for (const other of sceneData.sketchLines) {
    if (other.lineId === lineData.lineId || other.isConstruction) {
      continue;
    }
    const [ox1, oy1] = trimWorldPointToLocal(other.start, planeId, planeFrame);
    const [ox2, oy2] = trimWorldPointToLocal(other.end, planeId, planeFrame);
    const t = lineLineIntersectionTrim(ax, ay, bx, by, ox1, oy1, ox2, oy2);
    if (t !== null) {
      ts.push(t);
    }
  }

  for (const circle of sceneData.sketchCircles) {
    if (circle.isConstruction) {
      continue;
    }
    const [cx, cy] = trimWorldPointToLocal(circle.center, planeId, planeFrame);
    ts.push(...lineCircleIntersectionTrim(ax, ay, bx, by, cx, cy, circle.radius));
  }

  if (ts.length === 0) {
    const [sx, sy, sz] = toWorldPoint([ax, ay], planeId, planeFrame);
    const [ex, ey, ez] = toWorldPoint([bx, by], planeId, planeFrame);
    return {
      kind: "line",
      lineId: lineData.lineId,
      segments: [{ sx, sy, sz, ex, ey, ez }],
      hoveredSegmentIndex: 0,
    };
  }

  ts.sort((left, right) => left - right);
  const deduped = dedupeSortedNumbers(ts);
  const segments: TrimLineHighlightSegment[] = [];
  const pushSegment = (startT: number, endT: number) => {
    const startLocal: [number, number] = [
      ax + startT * (bx - ax),
      ay + startT * (by - ay),
    ];
    const endLocal: [number, number] = [
      ax + endT * (bx - ax),
      ay + endT * (by - ay),
    ];
    const [sx, sy, sz] = toWorldPoint(startLocal, planeId, planeFrame);
    const [ex, ey, ez] = toWorldPoint(endLocal, planeId, planeFrame);
    segments.push({ sx, sy, sz, ex, ey, ez });
  };

  pushSegment(0, deduped[0]);
  for (let index = 0; index + 1 < deduped.length; index++) {
    pushSegment(deduped[index], deduped[index + 1]);
  }
  pushSegment(deduped[deduped.length - 1], 1);

  const [mx, my] = cursorLocal;
  const abx = bx - ax;
  const aby = by - ay;
  const abLenSq = abx * abx + aby * aby;
  let clickT = 0;
  if (abLenSq > 1e-6) {
    clickT = ((mx - ax) * abx + (my - ay) * aby) / abLenSq;
    clickT = Math.max(0, Math.min(1, clickT));
  }

  let hoveredSegmentIndex = -1;
  if (clickT >= -1e-10 && clickT <= deduped[0] + 1e-10) {
    hoveredSegmentIndex = 0;
  } else {
    for (let index = 0; index + 1 < deduped.length; index++) {
      if (
        clickT >= deduped[index] - 1e-10 &&
        clickT <= deduped[index + 1] + 1e-10
      ) {
        hoveredSegmentIndex = index + 1;
        break;
      }
    }
    if (
      hoveredSegmentIndex < 0 &&
      clickT >= deduped[deduped.length - 1] - 1e-10
    ) {
      hoveredSegmentIndex = deduped.length;
    }
  }

  return {
    kind: "line",
    lineId: lineData.lineId,
    segments,
    hoveredSegmentIndex,
  };
}

function computeCircleTrimPreview({
  sceneData,
  targetId,
  cursorLocal,
  planeId,
  planeFrame,
}: {
  sceneData: ViewportScene;
  targetId: string;
  cursorLocal: [number, number];
  planeId: string;
  planeFrame: TrimPlaneFrame | null;
}): TrimHoverPreview | null {
  const circleData = sceneData.sketchCircles.find(
    (circle) => circle.circleId === targetId,
  );
  if (!circleData) {
    return null;
  }

  const [clx, cly] = trimWorldPointToLocal(circleData.center, planeId, planeFrame);
  const [mx, my] = cursorLocal;
  const cursorAngle = wrapAngle(Math.atan2(my - cly, mx - clx));
  const angles: number[] = [];

  collectLineCircleIntersectionAngles({
    sceneData,
    center: [clx, cly],
    radius: circleData.radius,
    planeId,
    planeFrame,
    onAngle: (angle) => angles.push(wrapAngle(angle)),
  });

  for (const other of sceneData.sketchCircles) {
    if (other.circleId === circleData.circleId || other.isConstruction) {
      continue;
    }
    angles.push(
      ...circleCircleIntersectionAngles({
        center: [clx, cly],
        radius: circleData.radius,
        otherCenter: trimWorldPointToLocal(other.center, planeId, planeFrame),
        otherRadius: other.radius,
      }),
    );
  }

  if (angles.length === 0) {
    return {
      kind: "arc",
      points: sampleTrimArcPoints({
        center: [clx, cly],
        radius: circleData.radius,
        startAngle: 0,
        endAngle: 2 * Math.PI,
        planeId,
        planeFrame,
      }),
    };
  }

  angles.sort((left, right) => left - right);
  const deduped = dedupeSortedNumbers(angles);
  const full = 2 * Math.PI;
  let hoveredSegment = -1;
  for (let index = 0; index < deduped.length; index++) {
    const start = deduped[index];
    const end = deduped[(index + 1) % deduped.length];
    const adjustedEnd = end <= start ? end + full : end;
    let adjustedCursor = cursorAngle;
    if (end <= start && adjustedCursor < start) {
      adjustedCursor += full;
    }
    if (
      adjustedCursor >= start - 1e-10 &&
      adjustedCursor <= adjustedEnd + 1e-10
    ) {
      hoveredSegment = index;
      break;
    }
  }
  if (hoveredSegment < 0) {
    return null;
  }

  const start = deduped[hoveredSegment];
  const end = deduped[(hoveredSegment + 1) % deduped.length];
  const adjustedEnd = end <= start ? end + full : end;
  return {
    kind: "arc",
    points: sampleTrimArcPoints({
      center: [clx, cly],
      radius: circleData.radius,
      startAngle: start,
      endAngle: adjustedEnd,
      planeId,
      planeFrame,
    }),
  };
}

function computeArcTrimPreview({
  sceneData,
  targetId,
  cursorLocal,
  planeId,
  planeFrame,
}: {
  sceneData: ViewportScene;
  targetId: string;
  cursorLocal: [number, number];
  planeId: string;
  planeFrame: TrimPlaneFrame | null;
}): TrimHoverPreview | null {
  const arcData = sceneData.sketchArcs.find((arc) => arc.arcId === targetId);
  if (!arcData) {
    return null;
  }

  const [clx, cly] = trimWorldPointToLocal(arcData.center, planeId, planeFrame);
  const [asx, asy] = trimWorldPointToLocal(arcData.start, planeId, planeFrame);
  const [aex, aey] = trimWorldPointToLocal(arcData.end, planeId, planeFrame);
  const startAngle = Math.atan2(asy - cly, asx - clx);
  const endAngle = Math.atan2(aey - cly, aex - clx);
  const ccw = arcData.ccw;
  const full = 2 * Math.PI;
  const wrappedStart = wrapAngle(startAngle);
  const wrappedEnd = wrapAngle(endAngle);
  const sweepEnd = ccw
    ? wrappedEnd <= wrappedStart
      ? wrappedEnd + full
      : wrappedEnd
    : wrappedEnd >= wrappedStart
      ? wrappedEnd - full
      : wrappedEnd;

  const normalizeToArcSweep = (angle: number) => {
    let normalized = wrapAngle(angle);
    if (ccw) {
      if (normalized < wrappedStart) {
        normalized += full;
      }
    } else if (normalized > wrappedStart) {
      normalized -= full;
    }
    return normalized;
  };

  const isOnSweep = (angle: number) =>
    ccw
      ? angle >= wrappedStart - 1e-10 && angle <= sweepEnd + 1e-10
      : angle <= wrappedStart + 1e-10 && angle >= sweepEnd - 1e-10;

  const [mx, my] = cursorLocal;
  const cursorAngle = normalizeToArcSweep(Math.atan2(my - cly, mx - clx));
  const angles: number[] = [];

  collectLineCircleIntersectionAngles({
    sceneData,
    center: [clx, cly],
    radius: arcData.radius,
    planeId,
    planeFrame,
    onAngle: (angle) => {
      const normalized = normalizeToArcSweep(angle);
      if (isOnSweep(normalized)) {
        angles.push(normalized);
      }
    },
  });

  for (const other of sceneData.sketchCircles) {
    if (other.isConstruction) {
      continue;
    }
    const otherCenter = trimWorldPointToLocal(other.center, planeId, planeFrame);
    const intersectionAngles = circleCircleIntersectionAngles({
      center: [clx, cly],
      radius: arcData.radius,
      otherCenter,
      otherRadius: other.radius,
    });
    for (const angle of intersectionAngles) {
      const normalized = normalizeToArcSweep(angle);
      if (isOnSweep(normalized)) {
        angles.push(normalized);
      }
    }
  }

  if (angles.length === 0) {
    return {
      kind: "arc",
      points: sampleTrimArcPoints({
        center: [clx, cly],
        radius: arcData.radius,
        startAngle,
        endAngle:
          startAngle + (ccw ? 1 : -1) * Math.abs(sweepEnd - wrappedStart),
        planeId,
        planeFrame,
      }),
    };
  }

  angles.sort((left, right) => (ccw ? left - right : right - left));
  const deduped = dedupeSortedNumbers(angles);
  const segments = [wrappedStart, ...deduped, sweepEnd];
  let hoveredSegment = -1;
  for (let index = 0; index + 1 < segments.length; index++) {
    const start = segments[index];
    const end = segments[index + 1];
    let adjustedCursor = cursorAngle;
    if (ccw) {
      if (adjustedCursor < start) {
        adjustedCursor += full;
      }
      if (adjustedCursor >= start - 1e-10 && adjustedCursor <= end + 1e-10) {
        hoveredSegment = index;
        break;
      }
    } else {
      // CW segments are descending (end < start); the ascending
      // interval test is empty for them.
      if (adjustedCursor > start) {
        adjustedCursor -= full;
      }
      if (adjustedCursor <= start + 1e-10 && adjustedCursor >= end - 1e-10) {
        hoveredSegment = index;
        break;
      }
    }
  }
  if (hoveredSegment < 0) {
    return null;
  }

  const start = segments[hoveredSegment];
  const end = segments[hoveredSegment + 1];
  return {
    kind: "arc",
    points: sampleTrimArcPoints({
      center: [clx, cly],
      radius: arcData.radius,
      startAngle: start,
      endAngle: end,
      planeId,
      planeFrame,
    }),
  };
}

export function computeTrimHoverPreview({
  sceneData,
  target,
  cursorLocal,
  planeId,
  planeFrame,
}: {
  sceneData: ViewportScene;
  target: TrimHoverTarget;
  cursorLocal: [number, number];
  planeId: string;
  planeFrame: TrimPlaneFrame | null;
}): TrimHoverPreview | null {
  if (target.entityKind === "line") {
    return computeLineTrimPreview({
      sceneData,
      targetId: target.id,
      cursorLocal,
      planeId,
      planeFrame,
    });
  }
  if (target.entityKind === "circle") {
    return computeCircleTrimPreview({
      sceneData,
      targetId: target.id,
      cursorLocal,
      planeId,
      planeFrame,
    });
  }
  return computeArcTrimPreview({
    sceneData,
    targetId: target.id,
    cursorLocal,
    planeId,
    planeFrame,
  });
}
