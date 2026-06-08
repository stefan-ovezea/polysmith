import type {
  SelectionFilter,
  SketchFeatureParameters,
  SketchPlaneFrame,
  SketchPreviewPoint,
  SnapCandidateEntry,
} from "@/types";
import { distanceBetweenPoints, toWorldPoint } from "@/utils";

export interface SketchSnapCandidate {
  local: [number, number];
  label: string;
  kind?:
    | "midpoint"
    | "endpoint"
    | "center"
    | "intersection"
    | "nearest"
    | "tangent";
  hostLineId?: string;
  tValue?: number;
  endpointHostLineId?: string;
}

export interface ClosestSnapCandidate {
  candidate: SketchSnapCandidate;
  distance: number;
}

export interface RawSketchPoint {
  local: [number, number];
  world: [number, number, number];
}

type TranslateSnapLabel = (
  key: string,
  options?: Record<string, string>,
) => string;

export function resolveSnappedSketchPoint({
  rawPoint,
  draftStartLocal,
  sketchSnapCandidates,
  sketchParameters,
  filter,
  activeSketchPlaneId,
  activeSketchPlaneFrame,
  currentGridSpacing,
  worldUnitsPerPixel,
  gridSnapScreenDistancePx,
  sketchSnapDistance,
  labels,
}: {
  rawPoint: RawSketchPoint;
  draftStartLocal?: [number, number] | null;
  sketchSnapCandidates: readonly SketchSnapCandidate[];
  sketchParameters: SketchFeatureParameters | null | undefined;
  filter: SelectionFilter;
  activeSketchPlaneId: string | null;
  activeSketchPlaneFrame: SketchPlaneFrame | null;
  currentGridSpacing: number;
  worldUnitsPerPixel: number;
  gridSnapScreenDistancePx: number;
  sketchSnapDistance: number;
  labels: {
    grid: string;
    axisLockHorizontal: string;
    axisLockVertical: string;
    onLine: string;
    tangent: string;
    perpendicular: string;
    parallel: string;
  };
}): SketchPreviewPoint {
  const gridResult = snapRawPointToGrid({
    rawPoint,
    currentGridSpacing,
    worldUnitsPerPixel,
    gridSnapScreenDistancePx,
    gridSnapEnabled: filter.snap_grid,
    activeSketchPlaneId,
    activeSketchPlaneFrame,
  });
  const point = gridResult.point;
  const resolvedSnap = chooseResolvedSnap({
    staticSnap: resolveStaticSnap({
      candidates: sketchSnapCandidates,
      point,
      filter,
      sketchSnapDistance,
      activeSketchPlaneId,
      activeSketchPlaneFrame,
      includeEndpointMetadata: true,
    }),
    dynamicSnap: resolveDynamicSnap({
      sketchParameters,
      draftStartLocal,
      point,
      filter,
      sketchSnapDistance,
      labels,
      activeSketchPlaneId,
      activeSketchPlaneFrame,
    }),
  });
  return resolvedSnap ?? unsnappedPreviewPoint(point, gridResult.snapped ? labels.grid : null);
}

function chooseResolvedSnap({
  staticSnap,
  dynamicSnap,
}: {
  staticSnap: ResolvedStaticSnap | null;
  dynamicSnap: ResolvedDynamicSnap | null;
}) {
  const prioritySnap = priorityStaticSnapPoint(staticSnap);
  if (prioritySnap) {
    return prioritySnap;
  }
  if (dynamicSnapOutranksStatic(dynamicSnap, staticSnap)) {
    return dynamicSnap.point;
  }
  return staticSnap?.point ?? null;
}

function priorityStaticSnapPoint(staticSnap: ResolvedStaticSnap | null) {
  return staticSnap?.isPriority ? staticSnap.point : null;
}

function dynamicSnapOutranksStatic(
  dynamicSnap: ResolvedDynamicSnap | null,
  staticSnap: ResolvedStaticSnap | null,
) {
  if (!dynamicSnap) {
    return false;
  }
  return !staticSnap || dynamicSnap.distance < staticSnap.distance;
}

function snapRawPointToGrid({
  rawPoint,
  currentGridSpacing,
  worldUnitsPerPixel,
  gridSnapScreenDistancePx,
  gridSnapEnabled,
  activeSketchPlaneId,
  activeSketchPlaneFrame,
}: {
  rawPoint: RawSketchPoint;
  currentGridSpacing: number;
  worldUnitsPerPixel: number;
  gridSnapScreenDistancePx: number;
  gridSnapEnabled: boolean;
  activeSketchPlaneId: string | null;
  activeSketchPlaneFrame: SketchPlaneFrame | null;
}): { point: RawSketchPoint; snapped: boolean } {
  if (!gridSnapEnabled || !Number.isFinite(currentGridSpacing) || currentGridSpacing <= 0) {
    return { point: rawPoint, snapped: false };
  }
  const threshold = worldUnitsPerPixel * gridSnapScreenDistancePx;
  const local = gridSnappedLocalPoint(rawPoint.local, currentGridSpacing, threshold);
  if (local[0] === rawPoint.local[0] && local[1] === rawPoint.local[1]) {
    return { point: rawPoint, snapped: false };
  }
  return {
    point: {
      local,
      world: sketchLocalToWorld(activeSketchPlaneId, local, activeSketchPlaneFrame),
    },
    snapped: true,
  };
}

function gridSnappedLocalPoint(
  local: [number, number],
  spacing: number,
  threshold: number,
): [number, number] {
  const nearestX = Math.round(local[0] / spacing) * spacing;
  const nearestY = Math.round(local[1] / spacing) * spacing;
  return [
    Math.abs(local[0] - nearestX) <= threshold ? nearestX : local[0],
    Math.abs(local[1] - nearestY) <= threshold ? nearestY : local[1],
  ];
}

type ResolvedStaticSnap = {
  point: SketchPreviewPoint;
  distance: number;
  isPriority: boolean;
};

function resolveStaticSnap({
  candidates,
  point,
  filter,
  sketchSnapDistance,
  activeSketchPlaneId,
  activeSketchPlaneFrame,
  includeEndpointMetadata,
}: {
  candidates: readonly SketchSnapCandidate[];
  point: RawSketchPoint;
  filter: SelectionFilter;
  sketchSnapDistance: number;
  activeSketchPlaneId: string | null;
  activeSketchPlaneFrame: SketchPlaneFrame | null;
  includeEndpointMetadata: boolean;
}): ResolvedStaticSnap | null {
  const closest = closestStaticSnapCandidate(
    candidates,
    point.local,
    filter,
    distanceBetweenPoints,
  );
  if (!closest || closest.distance > sketchSnapDistance) {
    return null;
  }
  return {
    point: previewPointFromStaticCandidate({
      candidate: closest.candidate,
      world: sketchLocalToWorld(
        activeSketchPlaneId,
        closest.candidate.local,
        activeSketchPlaneFrame,
      ),
      includeEndpointMetadata:
        includeEndpointMetadata && isPriorityStaticSnap(closest.candidate),
    }),
    distance: closest.distance,
    isPriority: isPriorityStaticSnap(closest.candidate),
  };
}

function isPriorityStaticSnap(candidate: SketchSnapCandidate) {
  return candidate.kind === "endpoint" || candidate.kind === "midpoint";
}

type ResolvedDynamicSnap = {
  point: SketchPreviewPoint;
  distance: number;
};

function resolveDynamicSnap({
  sketchParameters,
  draftStartLocal,
  point,
  filter,
  sketchSnapDistance,
  labels,
  activeSketchPlaneId,
  activeSketchPlaneFrame,
}: {
  sketchParameters: SketchFeatureParameters | null | undefined;
  draftStartLocal?: [number, number] | null;
  point: RawSketchPoint;
  filter: SelectionFilter;
  sketchSnapDistance: number;
  labels: {
    axisLockHorizontal: string;
    axisLockVertical: string;
    onLine: string;
    tangent: string;
    perpendicular: string;
    parallel: string;
  };
  activeSketchPlaneId: string | null;
  activeSketchPlaneFrame: SketchPlaneFrame | null;
}): ResolvedDynamicSnap | null {
  if (!sketchParameters) {
    return null;
  }
  const bestDynamic = dynamicSnapCandidate({
    lines: sketchParameters.lines,
    circles: sketchParameters.circles,
    filter,
    draftStart: draftStartLocal,
    cursor: point.local,
    threshold: sketchSnapDistance,
    axisAngleThresholdRadians: 5 * Math.PI / 180,
    parallelAngleThresholdRadians: 8 * Math.PI / 180,
    labels,
  });
  if (!bestDynamic || bestDynamic.distance > sketchSnapDistance) {
    return null;
  }
  return {
    point: previewPointFromDynamicSnap({
      snap: bestDynamic,
      activeSketchPlaneId,
      activeSketchPlaneFrame,
    }),
    distance: bestDynamic.distance,
  };
}

function previewPointFromDynamicSnap({
  snap,
  activeSketchPlaneId,
  activeSketchPlaneFrame,
}: {
  snap: DynamicSnapResult;
  activeSketchPlaneId: string | null;
  activeSketchPlaneFrame: SketchPlaneFrame | null;
}): SketchPreviewPoint {
  return {
    local: snap.local,
    world: sketchLocalToWorld(activeSketchPlaneId, snap.local, activeSketchPlaneFrame),
    snapLabel: snap.snapLabel,
    snapMidpointHostLineId: null,
    snapMidpointT: null,
    snapPerpendicularHostLineId: snap.snapPerpendicularHostLineId,
    snapEndpointHostLineId: null,
    snapLineBodyHostLineId: snap.snapLineBodyHostLineId,
    snapLineBodyT: snap.snapLineBodyT,
    snapAxisLock: snap.snapAxisLock,
    snapTangentCircleId: snap.snapTangentCircleId,
    snapParallelHostLineId: snap.snapParallelHostLineId,
  };
}

function unsnappedPreviewPoint(
  point: RawSketchPoint,
  snapLabel: string | null,
): SketchPreviewPoint {
  return {
    ...point,
    snapLabel,
    snapMidpointHostLineId: null,
    snapPerpendicularHostLineId: null,
    snapEndpointHostLineId: null,
    snapLineBodyHostLineId: null,
    snapLineBodyT: null,
    snapAxisLock: null,
    snapTangentCircleId: null,
    snapParallelHostLineId: null,
  };
}

function sketchLocalToWorld(
  activeSketchPlaneId: string | null,
  local: [number, number],
  activeSketchPlaneFrame: SketchPlaneFrame | null,
) {
  return toWorldPoint(
    activeSketchPlaneId ?? "ref-plane-xy",
    local,
    activeSketchPlaneFrame,
  );
}

export function buildSketchSnapCandidates({
  sketchParameters,
  coreCandidates,
  translate,
}: {
  sketchParameters: SketchFeatureParameters | null | undefined;
  coreCandidates: readonly SnapCandidateEntry[] | null | undefined;
  translate: TranslateSnapLabel;
}): SketchSnapCandidate[] {
  if (!sketchParameters) {
    return [];
  }

  const candidates: SketchSnapCandidate[] = [
    { local: [0, 0], label: translate("snap.origin") },
  ];

  if (coreCandidates && coreCandidates.length > 0) {
    appendCoreSnapCandidates(candidates, coreCandidates);
    return candidates;
  }

  appendLegacyLineSnapCandidates(candidates, sketchParameters, translate);
  appendLegacyCircleSnapCandidates(candidates, sketchParameters, translate);
  return candidates;
}

function appendCoreSnapCandidates(
  candidates: SketchSnapCandidate[],
  coreCandidates: readonly SnapCandidateEntry[],
) {
  for (const candidate of coreCandidates) {
    switch (candidate.kind) {
      case "endpoint":
        candidates.push({
          local: [candidate.local_x, candidate.local_y],
          label: candidate.label,
          kind: "endpoint",
          endpointHostLineId: candidate.entity_id || undefined,
        });
        break;
      case "midpoint":
        candidates.push({
          local: [candidate.local_x, candidate.local_y],
          label: candidate.label,
          kind: "midpoint",
          hostLineId: candidate.entity_id || undefined,
          tValue: 0.5,
        });
        break;
      case "center":
      default:
        candidates.push({
          local: [candidate.local_x, candidate.local_y],
          label: candidate.label,
        });
        break;
    }
  }
}

function appendLegacyLineSnapCandidates(
  candidates: SketchSnapCandidate[],
  params: SketchFeatureParameters,
  translate: TranslateSnapLabel,
) {
  for (const line of params.lines) {
    candidates.push({
      local: [line.start_x, line.start_y],
      label:
        line.constraint === "horizontal" || line.constraint === "vertical"
          ? translate("snap.constrainedLine", {
              constraint:
                line.constraint === "horizontal"
                  ? translate("toolbar.horizontal")
                  : translate("toolbar.vertical"),
            })
          : translate("snap.lineEndpoint"),
      kind: "endpoint",
      endpointHostLineId: line.line_id,
    });
    candidates.push({
      local: [line.end_x, line.end_y],
      label: translate("snap.lineEndpoint"),
      kind: "endpoint",
      endpointHostLineId: line.line_id,
    });

    appendLegacyLineMidpointCandidates(candidates, params, line, translate);
  }
}

function appendLegacyLineMidpointCandidates(
  candidates: SketchSnapCandidate[],
  params: SketchFeatureParameters,
  line: SketchFeatureParameters["lines"][number],
  translate: TranslateSnapLabel,
) {
  const splitTs: number[] = [];
  for (const anchor of params.midpoint_anchors) {
    if (anchor.line_id === line.line_id) {
      splitTs.push(0.5);
    }
  }
  for (const anchor of params.point_line_anchors ?? []) {
    if (anchor.line_id === line.line_id) {
      splitTs.push(anchor.t);
    }
  }

  const uniqueTs = Array.from(new Set([0, ...splitTs, 1])).sort(
    (left, right) => left - right,
  );
  for (let index = 0; index < uniqueTs.length - 1; index++) {
    const startT = uniqueTs[index];
    const endT = uniqueTs[index + 1];
    if (endT - startT < 1e-9) {
      continue;
    }
    const tMid = (startT + endT) / 2;
    const isWholeLine = uniqueTs.length === 2;
    const dx = line.end_x - line.start_x;
    const dy = line.end_y - line.start_y;
    candidates.push({
      local: [line.start_x + tMid * dx, line.start_y + tMid * dy],
      label: isWholeLine
        ? translate("snap.midpoint")
        : translate("snap.subSegmentMidpoint"),
      kind: "midpoint",
      hostLineId: line.line_id,
      tValue: tMid,
    });
  }
}

function appendLegacyCircleSnapCandidates(
  candidates: SketchSnapCandidate[],
  params: SketchFeatureParameters,
  translate: TranslateSnapLabel,
) {
  for (const circle of params.circles) {
    candidates.push({
      local: [circle.center_x, circle.center_y],
      label: translate("snap.circleCenter"),
    });
  }
}

interface DynamicSnapResult {
  local: [number, number];
  snapLabel: string;
  snapPerpendicularHostLineId: string | null;
  snapAxisLock: "horizontal" | "vertical" | null;
  snapTangentCircleId: string | null;
  snapParallelHostLineId: string | null;
  snapLineBodyHostLineId: string | null;
  snapLineBodyT: number | null;
  distance: number;
}

interface SketchSnapLine {
  line_id: string;
  start_x: number;
  start_y: number;
  end_x: number;
  end_y: number;
  is_construction: boolean;
}

interface SketchSnapCircle {
  circle_id: string;
  center_x: number;
  center_y: number;
  radius: number;
  is_construction: boolean;
}

interface ClosestPointOnSegmentResult {
  x: number;
  y: number;
  t: number;
  dist: number;
}

function closestPointOnSegment2d(
  sx: number,
  sy: number,
  ex: number,
  ey: number,
  px: number,
  py: number,
): ClosestPointOnSegmentResult {
  const segDx = ex - sx;
  const segDy = ey - sy;
  const lenSq = segDx * segDx + segDy * segDy;
  if (lenSq < 1e-12) {
    return { x: sx, y: sy, t: 0, dist: Math.hypot(px - sx, py - sy) };
  }
  const t = Math.max(
    0,
    Math.min(1, ((px - sx) * segDx + (py - sy) * segDy) / lenSq),
  );
  const x = sx + t * segDx;
  const y = sy + t * segDy;
  return { x, y, t, dist: Math.hypot(px - x, py - y) };
}

function angleDiffBetween2d(
  dx1: number,
  dy1: number,
  dx2: number,
  dy2: number,
): number {
  const mag1 = Math.hypot(dx1, dy1);
  const mag2 = Math.hypot(dx2, dy2);
  if (mag1 < 1e-12 || mag2 < 1e-12) {
    return Math.PI / 2;
  }
  const dot = (dx1 * dx2 + dy1 * dy2) / (mag1 * mag2);
  const clamped = Math.max(-1, Math.min(1, dot));
  return Math.acos(clamped);
}

function axisLockSnapCandidate({
  draftStart,
  cursor,
  thresholdRadians,
  horizontalLabel,
  verticalLabel,
}: {
  draftStart: [number, number];
  cursor: [number, number];
  thresholdRadians: number;
  horizontalLabel: string;
  verticalLabel: string;
}): DynamicSnapResult | null {
  const [sx, sy] = draftStart;
  const [cx, cy] = cursor;
  const draftDx = cx - sx;
  const draftDy = cy - sy;
  if (Math.hypot(draftDx, draftDy) <= 1e-6) {
    return null;
  }

  const draftAngle = Math.atan2(draftDy, draftDx);
  let best: DynamicSnapResult | null = null;
  for (const [targetAngle, axisKind, snapLabel] of [
    [0, "horizontal" as const, horizontalLabel],
    [Math.PI / 2, "vertical" as const, verticalLabel],
    [Math.PI, "horizontal" as const, horizontalLabel],
    [-Math.PI / 2, "vertical" as const, verticalLabel],
  ] as const) {
    let diff = draftAngle - targetAngle;
    while (diff > Math.PI) {
      diff -= 2 * Math.PI;
    }
    while (diff < -Math.PI) {
      diff += 2 * Math.PI;
    }
    if (Math.abs(diff) >= thresholdRadians) {
      continue;
    }
    const lockX = axisKind === "horizontal" ? cx : sx;
    const lockY = axisKind === "vertical" ? cy : sy;
    const distance = Math.hypot(cx - lockX, cy - lockY);
    if (!best || distance < best.distance) {
      best = {
        local: [lockX, lockY],
        snapLabel,
        snapPerpendicularHostLineId: null,
        snapAxisLock: axisKind,
        snapTangentCircleId: null,
        snapParallelHostLineId: null,
        snapLineBodyHostLineId: null,
        snapLineBodyT: null,
        distance,
      };
    }
  }

  return best;
}

function lineBodySnapCandidate({
  lines,
  cursor,
  threshold,
  snapLabel,
}: {
  lines: readonly SketchSnapLine[];
  cursor: [number, number];
  threshold: number;
  snapLabel: string;
}): DynamicSnapResult | null {
  const [cx, cy] = cursor;
  let best: DynamicSnapResult | null = null;

  for (const line of lines) {
    if (line.is_construction) {
      continue;
    }
    const closest = closestPointOnSegment2d(
      line.start_x,
      line.start_y,
      line.end_x,
      line.end_y,
      cx,
      cy,
    );
    if (closest.dist > threshold) {
      continue;
    }
    if (!best || closest.dist < best.distance) {
      best = {
        local: [closest.x, closest.y],
        snapLabel,
        snapPerpendicularHostLineId: null,
        snapAxisLock: null,
        snapTangentCircleId: null,
        snapParallelHostLineId: null,
        snapLineBodyHostLineId: line.line_id,
        snapLineBodyT: closest.t,
        distance: closest.dist,
      };
    }
  }

  return best;
}

function tangentSnapCandidate({
  circles,
  draftStart,
  cursor,
  threshold,
  snapLabel,
}: {
  circles: readonly SketchSnapCircle[];
  draftStart: [number, number];
  cursor: [number, number];
  threshold: number;
  snapLabel: string;
}): DynamicSnapResult | null {
  const [sx, sy] = draftStart;
  const [cx, cy] = cursor;
  let best: DynamicSnapResult | null = null;

  for (const circle of circles) {
    if (circle.is_construction) {
      continue;
    }
    const pcDx = circle.center_x - sx;
    const pcDy = circle.center_y - sy;
    const pcDist = Math.hypot(pcDx, pcDy);
    if (pcDist <= circle.radius + 1e-9) {
      continue;
    }

    const alpha = Math.asin(circle.radius / pcDist);
    const baseAngle = Math.atan2(pcDy, pcDx);
    const tangentLen = Math.sqrt(pcDist * pcDist - circle.radius * circle.radius);
    for (const sign of [1, -1]) {
      const tpDir = baseAngle + sign * alpha;
      const tpX = sx + tangentLen * Math.cos(tpDir);
      const tpY = sy + tangentLen * Math.sin(tpDir);
      const tpDist = Math.hypot(cx - tpX, cy - tpY);
      if (tpDist > threshold) {
        continue;
      }
      if (!best || tpDist < best.distance) {
        best = {
          local: [tpX, tpY],
          snapLabel,
          snapPerpendicularHostLineId: null,
          snapAxisLock: null,
          snapTangentCircleId: circle.circle_id,
          snapParallelHostLineId: null,
          snapLineBodyHostLineId: null,
          snapLineBodyT: null,
          distance: tpDist,
        };
      }
    }
  }

  return best;
}

function perpendicularSnapCandidate({
  lines,
  draftStart,
  cursor,
  threshold,
  snapLabel,
}: {
  lines: readonly SketchSnapLine[];
  draftStart: [number, number];
  cursor: [number, number];
  threshold: number;
  snapLabel: string;
}): DynamicSnapResult | null {
  const [sx, sy] = draftStart;
  const [cx, cy] = cursor;
  let best: DynamicSnapResult | null = null;

  for (const line of lines) {
    if (line.is_construction) {
      continue;
    }
    const ldx = line.end_x - line.start_x;
    const ldy = line.end_y - line.start_y;
    const llenSq = ldx * ldx + ldy * ldy;
    if (llenSq < 1e-12) {
      continue;
    }

    const tProj = ((cx - line.start_x) * ldx + (cy - line.start_y) * ldy) / llenSq;
    const footX = line.start_x + tProj * ldx;
    const footY = line.start_y + tProj * ldy;
    const footDist = Math.hypot(cx - footX, cy - footY);
    const segT = Math.max(0, Math.min(1, tProj));
    const segX = line.start_x + segT * ldx;
    const segY = line.start_y + segT * ldy;
    const closestOnSeg = Math.hypot(footX - segX, footY - segY);
    if (footDist > threshold || closestOnSeg > threshold) {
      continue;
    }

    const startOnLine = closestPointOnSegment2d(
      line.start_x,
      line.start_y,
      line.end_x,
      line.end_y,
      sx,
      sy,
    );
    if (startOnLine.dist > threshold) {
      continue;
    }

    if (!best || footDist < best.distance) {
      best = {
        local: [footX, footY],
        snapLabel,
        snapPerpendicularHostLineId: line.line_id,
        snapAxisLock: null,
        snapTangentCircleId: null,
        snapParallelHostLineId: null,
        snapLineBodyHostLineId: null,
        snapLineBodyT: null,
        distance: footDist,
      };
    }
  }

  return best;
}

function parallelSnapCandidate({
  lines,
  draftStart,
  cursor,
  threshold,
  angleThresholdRadians,
  snapLabel,
}: {
  lines: readonly SketchSnapLine[];
  draftStart: [number, number];
  cursor: [number, number];
  threshold: number;
  angleThresholdRadians: number;
  snapLabel: string;
}): DynamicSnapResult | null {
  const [sx, sy] = draftStart;
  const [cx, cy] = cursor;
  const draftDx = cx - sx;
  const draftDy = cy - sy;
  const draftDistSq = draftDx * draftDx + draftDy * draftDy;
  if (draftDistSq <= 1e-12) {
    return null;
  }

  let best: DynamicSnapResult | null = null;
  for (const line of lines) {
    if (line.is_construction) {
      continue;
    }
    const ldx = line.end_x - line.start_x;
    const ldy = line.end_y - line.start_y;
    const lenSq = ldx * ldx + ldy * ldy;
    if (lenSq < 1e-12) {
      continue;
    }

    const angle = angleDiffBetween2d(draftDx, draftDy, ldx, ldy);
    const parallelAngle = Math.min(angle, Math.PI - angle);
    if (parallelAngle >= angleThresholdRadians) {
      continue;
    }

    const len = Math.sqrt(lenSq);
    const ux = ldx / len;
    const uy = ldy / len;
    const projLen = (cx - sx) * ux + (cy - sy) * uy;
    const ppx = sx + projLen * ux;
    const ppy = sy + projLen * uy;
    const distance = Math.hypot(cx - ppx, cy - ppy);
    if (distance > threshold) {
      continue;
    }
    if (!best || distance < best.distance) {
      best = {
        local: [ppx, ppy],
        snapLabel,
        snapPerpendicularHostLineId: null,
        snapAxisLock: null,
        snapTangentCircleId: null,
        snapParallelHostLineId: line.line_id,
        snapLineBodyHostLineId: null,
        snapLineBodyT: null,
        distance,
      };
    }
  }

  return best;
}

function closerDynamicSnap(
  current: DynamicSnapResult | null,
  candidate: DynamicSnapResult | null,
): DynamicSnapResult | null {
  if (!candidate) {
    return current;
  }
  if (!current || candidate.distance < current.distance) {
    return candidate;
  }
  return current;
}

export function dynamicSnapCandidate({
  lines,
  circles,
  filter,
  draftStart,
  cursor,
  threshold,
  axisAngleThresholdRadians,
  parallelAngleThresholdRadians,
  labels,
}: {
  lines: readonly SketchSnapLine[];
  circles: readonly SketchSnapCircle[];
  filter: SelectionFilter;
  draftStart: [number, number] | null | undefined;
  cursor: [number, number];
  threshold: number;
  axisAngleThresholdRadians: number;
  parallelAngleThresholdRadians: number;
  labels: {
    axisLockHorizontal: string;
    axisLockVertical: string;
    onLine: string;
    tangent: string;
    perpendicular: string;
    parallel: string;
  };
}): DynamicSnapResult | null {
  let best: DynamicSnapResult | null = null;

  if (draftStart) {
    const draftDx = cursor[0] - draftStart[0];
    const draftDy = cursor[1] - draftStart[1];
    const hasDraftMovement = Math.hypot(draftDx, draftDy) > 1e-6;

    if (hasDraftMovement && filter.snap_polar) {
      best = closerDynamicSnap(
        best,
        axisLockSnapCandidate({
          draftStart,
          cursor,
          thresholdRadians: axisAngleThresholdRadians,
          horizontalLabel: labels.axisLockHorizontal,
          verticalLabel: labels.axisLockVertical,
        }),
      );
    }

    if (hasDraftMovement && filter.snap_tangent) {
      best = closerDynamicSnap(
        best,
        tangentSnapCandidate({
          circles,
          draftStart,
          cursor,
          threshold,
          snapLabel: labels.tangent,
        }),
      );
    }
  }

  best = closerDynamicSnap(
    best,
    lineBodySnapCandidate({
      lines,
      cursor,
      threshold,
      snapLabel: labels.onLine,
    }),
  );

  if (draftStart && filter.snap_perpendicular) {
    best = closerDynamicSnap(
      best,
      perpendicularSnapCandidate({
        lines,
        draftStart,
        cursor,
        threshold,
        snapLabel: labels.perpendicular,
      }),
    );
  }

  if (draftStart && filter.snap_parallel) {
    best = closerDynamicSnap(
      best,
      parallelSnapCandidate({
        lines,
        draftStart,
        cursor,
        threshold,
        angleThresholdRadians: parallelAngleThresholdRadians,
        snapLabel: labels.parallel,
      }),
    );
  }

  return best;
}

export function isStaticSnapCandidateAllowed(
  candidate: SketchSnapCandidate,
  filter: SelectionFilter,
) {
  if (!candidate.kind) {
    return true;
  }
  return filter[selectionFilterKeyForSnapKind(candidate.kind)];
}

type SketchSnapKind = NonNullable<SketchSnapCandidate["kind"]>;

const SNAP_FILTER_KEY_BY_KIND = {
  endpoint: "snap_endpoint",
  midpoint: "snap_midpoint",
  center: "snap_center",
  intersection: "snap_intersection",
  nearest: "snap_nearest",
  tangent: "snap_tangent",
} satisfies Record<SketchSnapKind, keyof SelectionFilter>;

function selectionFilterKeyForSnapKind(kind: SketchSnapKind) {
  return SNAP_FILTER_KEY_BY_KIND[kind];
}

export function closestStaticSnapCandidate(
  candidates: readonly SketchSnapCandidate[],
  point: [number, number],
  filter: SelectionFilter,
  distanceBetweenPoints: (
    first: [number, number],
    second: [number, number],
  ) => number,
): ClosestSnapCandidate | null {
  let closest: ClosestSnapCandidate | null = null;

  for (const candidate of candidates) {
    if (!isStaticSnapCandidateAllowed(candidate, filter)) {
      continue;
    }
    const distance = distanceBetweenPoints(point, candidate.local);
    if (!closest || distance < closest.distance) {
      closest = { candidate, distance };
    }
  }

  return closest;
}

export function previewPointFromStaticCandidate({
  candidate,
  world,
  includeEndpointMetadata,
}: {
  candidate: SketchSnapCandidate;
  world: [number, number, number];
  includeEndpointMetadata: boolean;
}): SketchPreviewPoint {
  return {
    local: candidate.local,
    world,
    snapLabel: candidate.label,
    snapMidpointHostLineId:
      candidate.kind === "midpoint" ? (candidate.hostLineId ?? null) : null,
    snapMidpointT:
      candidate.kind === "midpoint" ? (candidate.tValue ?? null) : null,
    snapPerpendicularHostLineId: null,
    snapEndpointHostLineId:
      includeEndpointMetadata && candidate.kind === "endpoint"
        ? (candidate.endpointHostLineId ?? null)
        : null,
    snapLineBodyHostLineId: null,
    snapLineBodyT: null,
    snapAxisLock: null,
    snapTangentCircleId: null,
    snapParallelHostLineId: null,
  };
}
