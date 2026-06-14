import type {
  SelectionFilter,
  SketchFeatureParameters,
  SketchPlaneFrame,
  SketchPreviewPoint,
  SnapCandidateEntry,
} from "@/types";
import { distanceBetweenPoints, toWorldPoint } from "@/utils";
import { getBridge } from "@/lib/planegcsSolver";
import { speculativeSolve, speculativeMultiSolve } from "@/lib/speculativeSolve";
import type { SketchConstraintData } from "@/lib/planegcsBridge";

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

export interface ResolveSnapOptions {
  dynamicSnapsEnabled?: boolean;
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
  sketchConstraints,
  dynamicSnapsEnabled = true,
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
  sketchConstraints?: SketchConstraintData[];
  dynamicSnapsEnabled?: boolean;
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
    intersection: string;
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
  const staticSnap = resolveStaticSnap({
    candidates: sketchSnapCandidates,
    point,
    filter,
    sketchSnapDistance,
    activeSketchPlaneId,
    activeSketchPlaneFrame,
    includeEndpointMetadata: true,
  });
  const prioritySnap = priorityStaticSnapPoint(staticSnap);
  if (prioritySnap) {
    return prioritySnap;
  }

  const resolvedSnap = chooseResolvedSnap({
    staticSnap,
    dynamicSnap: dynamicSnapsEnabled
      ? resolveDynamicSnap({
          sketchParameters,
          sketchConstraints,
          draftStartLocal,
          point,
          filter,
          sketchSnapDistance,
          labels,
          activeSketchPlaneId,
          activeSketchPlaneFrame,
        })
      : null,
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
  sketchConstraints,
  draftStartLocal,
  point,
  filter,
  sketchSnapDistance,
  labels,
  activeSketchPlaneId,
  activeSketchPlaneFrame,
}: {
  sketchParameters: SketchFeatureParameters | null | undefined;
  sketchConstraints?: SketchConstraintData[];
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
    intersection: string;
  };
  activeSketchPlaneId: string | null;
  activeSketchPlaneFrame: SketchPlaneFrame | null;
}): ResolvedDynamicSnap | null {
  if (!sketchParameters || !hasEnabledDynamicSnap(filter, draftStartLocal)) {
    return null;
  }
  const bestDynamic = dynamicSnapCandidate({
    lines: sketchParameters.lines,
    circles: sketchParameters.circles,
    filter,
    draftStart: draftStartLocal,
    cursor: point.local,
    threshold: sketchSnapDistance,
    parallelAngleThresholdRadians: 8 * Math.PI / 180,
    labels,
    sketchParameters,
    constraints: sketchConstraints,
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

function hasEnabledDynamicSnap(
  filter: SelectionFilter,
  draftStartLocal?: [number, number] | null,
) {
  return (
    filter.snap_nearest ||
    filter.snap_intersection ||
    Boolean(
      draftStartLocal &&
        (filter.snap_polar ||
          filter.snap_tangent ||
          filter.snap_perpendicular ||
          filter.snap_parallel),
    )
  );
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
    snapIntersectionLineIds: snap.snapIntersectionLineIds,
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
    snapIntersectionLineIds: null,
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
  snapIntersectionLineIds: [string, string] | null;
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
  parallelAngleThresholdRadians,
  labels,
  sketchParameters,
  constraints,
}: {
  lines: readonly SketchSnapLine[];
  circles: readonly SketchSnapCircle[];
  filter: SelectionFilter;
  draftStart: [number, number] | null | undefined;
  cursor: [number, number];
  threshold: number;
  parallelAngleThresholdRadians: number;
  labels: {
    axisLockHorizontal: string;
    axisLockVertical: string;
    onLine: string;
    tangent: string;
    perpendicular: string;
    parallel: string;
    intersection: string;
  };
  /** Full sketch params for speculative WASM solver path. */
  sketchParameters?: SketchFeatureParameters | null;
  /** Constraints for speculative WASM solver path. */
  constraints?: import("@/lib/planegcsBridge").SketchConstraintData[];
}): DynamicSnapResult | null {
  let best: DynamicSnapResult | null = null;

  if (draftStart) {
    const draftDx = cursor[0] - draftStart[0];
    const draftDy = cursor[1] - draftStart[1];
    const hasDraftMovement = Math.hypot(draftDx, draftDy) > 1e-6;

    if (hasDraftMovement && filter.snap_polar) {
      const speculativeResult = speculativeAxisLockSnap({
        draftStart,
        cursor,
        threshold,
        labels,
      });
      if (speculativeResult) {
        best = speculativeResult;
      }
    }
  }

  if (filter.snap_nearest) {
    const spec = speculativeLineBodySnap({
      lines, cursor, threshold,
      snapLabel: labels.onLine,
    });
    best = closerDynamicSnap(best, spec);
  }

  if (draftStart) {
    const draftDx = cursor[0] - draftStart[0];
    const draftDy = cursor[1] - draftStart[1];
    const hasDraftMovement = Math.hypot(draftDx, draftDy) > 1e-6;

    if (hasDraftMovement && filter.snap_tangent) {
      const spec = speculativeTangentSnap({
        sketchParameters: sketchParameters ?? null,
        constraints: constraints ?? [],
        circles, draftStart, cursor,
        threshold: solverSearchThreshold(best, threshold),
        snapLabel: labels.tangent,
      });
      best = closerDynamicSnap(best, spec);
    }
  }

  if (draftStart && filter.snap_perpendicular) {
    const spec = speculativePerpendicularSnap({
      sketchParameters: sketchParameters ?? null,
      constraints: constraints ?? [],
      lines, draftStart, cursor,
      threshold: solverSearchThreshold(best, threshold),
      snapLabel: labels.perpendicular,
    });
    best = closerDynamicSnap(best, spec);
  }

  if (draftStart && filter.snap_parallel) {
    const spec = speculativeParallelSnap({
      sketchParameters: sketchParameters ?? null,
      constraints: constraints ?? [],
      lines, draftStart, cursor,
      threshold: solverSearchThreshold(best, threshold),
      angleThresholdRadians: parallelAngleThresholdRadians,
      snapLabel: labels.parallel,
    });
    best = closerDynamicSnap(best, spec);
  }

  // Multi-constraint intersection snap (P3.3).
  if (filter.snap_intersection) {
    const spec = speculativeIntersectionSnap({
      sketchParameters: sketchParameters ?? null,
      constraints: constraints ?? [],
      lines, cursor,
      threshold: solverSearchThreshold(best, threshold),
      snapLabel: labels.intersection,
    });
    best = closerDynamicSnap(best, spec);
  }

  // Multi-constraint tangent-through-line snap (P3.3).
  if (draftStart && filter.snap_tangent) {
    const spec = speculativeTangentThroughLineSnap({
      sketchParameters: sketchParameters ?? null,
      constraints: constraints ?? [],
      circles, lines, draftStart, cursor,
      threshold: solverSearchThreshold(best, threshold),
      snapLabel: labels.tangent,
    });
    best = closerDynamicSnap(best, spec);
  }

  return best;
}

function solverSearchThreshold(
  best: DynamicSnapResult | null,
  defaultThreshold: number,
) {
  return Math.min(defaultThreshold, best?.distance ?? defaultThreshold);
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
    snapIntersectionLineIds: null,
  };
}

// ---------------------------------------------------------------------------
// Speculative WASM solver snap helpers
// ---------------------------------------------------------------------------

/**
 * Try axis-lock (horizontal / vertical) snap with direct geometry math.
 */
function speculativeAxisLockSnap({
  draftStart,
  cursor,
  threshold,
  labels,
}: {
  draftStart: [number, number];
  cursor: [number, number];
  threshold: number;
  labels: {
    axisLockHorizontal: string;
    axisLockVertical: string;
  };
}): DynamicSnapResult | null {
  const [sx, sy] = draftStart;
  const [cx, cy] = cursor;
  const draftDx = cx - sx;
  const draftDy = cy - sy;

  if (Math.hypot(draftDx, draftDy) <= 1e-6) return null;

  const horizontalDistance = Math.abs(draftDy);
  const verticalDistance = Math.abs(draftDx);
  const useHorizontal = horizontalDistance <= verticalDistance;
  const distance = useHorizontal ? horizontalDistance : verticalDistance;
  if (distance > threshold) return null;

  return {
    local: useHorizontal ? [cx, sy] : [sx, cy],
    snapLabel: useHorizontal
      ? labels.axisLockHorizontal
      : labels.axisLockVertical,
    snapPerpendicularHostLineId: null,
    snapAxisLock: useHorizontal ? "horizontal" : "vertical",
    snapTangentCircleId: null,
    snapParallelHostLineId: null,
    snapLineBodyHostLineId: null,
    snapLineBodyT: null,
    snapIntersectionLineIds: null,
    distance,
  };
}

/**
 * Try tangent snap via speculative WASM solver on the best TS candidate.
 * Uses TS math for fast proximity gating, then refines with the solver.
 */
function speculativeTangentSnap({
  sketchParameters,
  constraints,
  circles,
  draftStart,
  cursor,
  threshold,
  snapLabel,
}: {
  sketchParameters: SketchFeatureParameters | null;
  constraints: SketchConstraintData[];
  circles: readonly SketchSnapCircle[];
  draftStart: [number, number];
  cursor: [number, number];
  threshold: number;
  snapLabel: string;
}): DynamicSnapResult | null {
  const bridge = getBridge();
  if (!bridge || !sketchParameters) return null;

  // Fast TS proximity gating — find the best circle.
  let bestCircle: (typeof circles)[number] | null = null;
  let bestDist = Infinity;

  for (const circle of circles) {
    if (circle.is_construction) continue;
    const pcDx = circle.center_x - draftStart[0];
    const pcDy = circle.center_y - draftStart[1];
    const pcDist = Math.hypot(pcDx, pcDy);
    if (pcDist <= circle.radius + 1e-9) continue;

    const alpha = Math.asin(circle.radius / pcDist);
    const baseAngle = Math.atan2(pcDy, pcDx);
    const tangentLen = Math.sqrt(pcDist * pcDist - circle.radius * circle.radius);
    for (const sign of [1, -1]) {
      const tpDir = baseAngle + sign * alpha;
      const tpX = draftStart[0] + tangentLen * Math.cos(tpDir);
      const tpY = draftStart[1] + tangentLen * Math.sin(tpDir);
      const tpDist = Math.hypot(cursor[0] - tpX, cursor[1] - tpY);
      if (tpDist < bestDist) {
        bestDist = tpDist;
        bestCircle = circle;
      }
    }
  }

  if (!bestCircle || bestDist > threshold) return null;

  // Refine with speculative solver on the best circle only.
  const result = speculativeSolve({
    bridge,
    params: sketchParameters,
    constraints,
    draftStart,
    cursor,
    snapType: "tangent_lc",
    targetEntityId: bestCircle.circle_id,
  });

  if (!result?.converged || result.distance > threshold) return null;

  return {
    local: result.position,
    snapLabel,
    snapPerpendicularHostLineId: null,
    snapAxisLock: null,
    snapTangentCircleId: bestCircle.circle_id,
    snapParallelHostLineId: null,
    snapLineBodyHostLineId: null,
    snapLineBodyT: null,
    snapIntersectionLineIds: null,
    distance: result.distance,
  };
}

/**
 * Try perpendicular snap via speculative WASM solver on the best TS candidate.
 */
function speculativePerpendicularSnap({
  sketchParameters,
  constraints,
  lines,
  draftStart,
  cursor,
  threshold,
  snapLabel,
}: {
  sketchParameters: SketchFeatureParameters | null;
  constraints: SketchConstraintData[];
  lines: readonly SketchSnapLine[];
  draftStart: [number, number];
  cursor: [number, number];
  threshold: number;
  snapLabel: string;
}): DynamicSnapResult | null {
  const bridge = getBridge();
  if (!bridge || !sketchParameters) return null;

  // Fast TS proximity gating — find the best line.
  let bestLine: (typeof lines)[number] | null = null;
  let bestDist = Infinity;

  for (const line of lines) {
    if (line.is_construction) continue;
    const ldx = line.end_x - line.start_x;
    const ldy = line.end_y - line.start_y;
    const llenSq = ldx * ldx + ldy * ldy;
    if (llenSq < 1e-12) continue;

    const tProj = ((cursor[0] - line.start_x) * ldx + (cursor[1] - line.start_y) * ldy) / llenSq;
    const footX = line.start_x + tProj * ldx;
    const footY = line.start_y + tProj * ldy;
    const footDist = Math.hypot(cursor[0] - footX, cursor[1] - footY);
    const segT = Math.max(0, Math.min(1, tProj));
    const segX = line.start_x + segT * ldx;
    const segY = line.start_y + segT * ldy;
    const closestOnSeg = Math.hypot(footX - segX, footY - segY);
    if (footDist > threshold || closestOnSeg > threshold) continue;

    // Check draft start is on line too.
    const startProj = ((draftStart[0] - line.start_x) * ldx + (draftStart[1] - line.start_y) * ldy) / llenSq;
    const startFootX = line.start_x + startProj * ldx;
    const startFootY = line.start_y + startProj * ldy;
    const startDist = Math.hypot(draftStart[0] - startFootX, draftStart[1] - startFootY);
    if (startDist > threshold) continue;

    if (footDist < bestDist) {
      bestDist = footDist;
      bestLine = line;
    }
  }

  if (!bestLine) return null;

  const result = speculativeSolve({
    bridge,
    params: sketchParameters,
    constraints,
    draftStart,
    cursor,
    snapType: "perpendicular_ll",
    targetEntityId: bestLine.line_id,
  });

  if (!result?.converged || result.distance > threshold) return null;

  return {
    local: result.position,
    snapLabel,
    snapPerpendicularHostLineId: bestLine.line_id,
    snapAxisLock: null,
    snapTangentCircleId: null,
    snapParallelHostLineId: null,
    snapLineBodyHostLineId: null,
    snapLineBodyT: null,
    snapIntersectionLineIds: null,
    distance: result.distance,
  };
}

/**
 * Try parallel snap via speculative WASM solver on the best TS candidate.
 */
function speculativeParallelSnap({
  sketchParameters,
  constraints,
  lines,
  draftStart,
  cursor,
  threshold,
  angleThresholdRadians,
  snapLabel,
}: {
  sketchParameters: SketchFeatureParameters | null;
  constraints: SketchConstraintData[];
  lines: readonly SketchSnapLine[];
  draftStart: [number, number];
  cursor: [number, number];
  threshold: number;
  angleThresholdRadians: number;
  snapLabel: string;
}): DynamicSnapResult | null {
  const bridge = getBridge();
  if (!bridge || !sketchParameters) return null;

  const draftDx = cursor[0] - draftStart[0];
  const draftDy = cursor[1] - draftStart[1];
  const draftDistSq = draftDx * draftDx + draftDy * draftDy;
  if (draftDistSq <= 1e-12) return null;

  // Fast TS proximity gating.
  let bestLine: (typeof lines)[number] | null = null;
  let bestDist = Infinity;

  for (const line of lines) {
    if (line.is_construction) continue;
    const ldx = line.end_x - line.start_x;
    const ldy = line.end_y - line.start_y;
    const lenSq = ldx * ldx + ldy * ldy;
    if (lenSq < 1e-12) continue;

    const mag1 = Math.sqrt(draftDistSq);
    const mag2 = Math.sqrt(lenSq);
    const dot = (draftDx * ldx + draftDy * ldy) / (mag1 * mag2);
    const angle = Math.acos(Math.max(-1, Math.min(1, dot)));
    if (Math.min(angle, Math.PI - angle) >= angleThresholdRadians) continue;

    const len = Math.sqrt(lenSq);
    const ux = ldx / len;
    const uy = ldy / len;
    const projLen = (cursor[0] - draftStart[0]) * ux + (cursor[1] - draftStart[1]) * uy;
    const ppx = draftStart[0] + projLen * ux;
    const ppy = draftStart[1] + projLen * uy;
    const dist = Math.hypot(cursor[0] - ppx, cursor[1] - ppy);
    if (dist > threshold) continue;
    if (dist < bestDist) {
      bestDist = dist;
      bestLine = line;
    }
  }

  if (!bestLine) return null;

  const result = speculativeSolve({
    bridge,
    params: sketchParameters,
    constraints,
    draftStart,
    cursor,
    snapType: "parallel",
    targetEntityId: bestLine.line_id,
  });

  if (!result?.converged || result.distance > threshold) return null;

  return {
    local: result.position,
    snapLabel,
    snapPerpendicularHostLineId: null,
    snapAxisLock: null,
    snapTangentCircleId: null,
    snapParallelHostLineId: bestLine.line_id,
    snapLineBodyHostLineId: null,
    snapLineBodyT: null,
    snapIntersectionLineIds: null,
    distance: result.distance,
  };
}

/**
 * Try line-body snap with direct projection onto the nearest sketch segment.
 */
function speculativeLineBodySnap({
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
  let bestLine: (typeof lines)[number] | null = null;
  let bestDist = Infinity;
  let bestT = 0;
  let bestLocal: [number, number] | null = null;

  for (const line of lines) {
    if (line.is_construction) continue;
    const segDx = line.end_x - line.start_x;
    const segDy = line.end_y - line.start_y;
    const lenSq = segDx * segDx + segDy * segDy;
    if (lenSq < 1e-12) continue;
    const t = Math.max(0, Math.min(1,
      ((cursor[0] - line.start_x) * segDx + (cursor[1] - line.start_y) * segDy) / lenSq));
    const x = line.start_x + t * segDx;
    const y = line.start_y + t * segDy;
    const dist = Math.hypot(cursor[0] - x, cursor[1] - y);
    if (dist > threshold) continue;
    if (dist < bestDist) {
      bestDist = dist;
      bestLine = line;
      bestT = t;
      bestLocal = [x, y];
    }
  }

  if (!bestLine || !bestLocal) return null;

  return {
    local: bestLocal,
    snapLabel,
    snapPerpendicularHostLineId: null,
    snapAxisLock: null,
    snapTangentCircleId: null,
    snapParallelHostLineId: null,
    snapLineBodyHostLineId: bestLine.line_id,
    snapLineBodyT: bestT,
    snapIntersectionLineIds: null,
    distance: bestDist,
  };
}


// ---------------------------------------------------------------------------
// Multi-constraint speculative snap helpers (P3.3)
// ---------------------------------------------------------------------------

/**
 * Try line-line intersection snap via speculative WASM solver.
 *
 * Strategy: for every pair of non-parallel lines near the cursor, push
 * point_on_line_pl constraints for both lines simultaneously. The solver
 * finds the intersection point that satisfies both.
 *
 * Uses TS math for fast proximity gating (find best line pair), then
 * runs a single speculativeMultiSolve to refine the intersection.
 */
function speculativeIntersectionSnap({
  sketchParameters,
  constraints,
  lines,
  cursor,
  threshold,
  snapLabel,
}: {
  sketchParameters: SketchFeatureParameters | null;
  constraints: SketchConstraintData[];
  lines: readonly SketchSnapLine[];
  cursor: [number, number];
  threshold: number;
  snapLabel: string;
}): DynamicSnapResult | null {
  const bridge = getBridge();
  if (!bridge || !sketchParameters) return null;

  let bestPair: { line1: (typeof lines)[number]; line2: (typeof lines)[number] } | null = null;
  let bestDist = Infinity;

  for (let i = 0; i < lines.length; i++) {
    const line1 = lines[i];
    if (line1.is_construction) continue;

    const dx1 = line1.end_x - line1.start_x;
    const dy1 = line1.end_y - line1.start_y;
    const lenSq1 = dx1 * dx1 + dy1 * dy1;
    if (lenSq1 < 1e-12) continue;

    for (let j = i + 1; j < lines.length; j++) {
      const line2 = lines[j];
      if (line2.is_construction) continue;

      const dx2 = line2.end_x - line2.start_x;
      const dy2 = line2.end_y - line2.start_y;
      const lenSq2 = dx2 * dx2 + dy2 * dy2;
      if (lenSq2 < 1e-12) continue;

      const cross = dx1 * dy2 - dy1 * dx2;
      if (Math.abs(cross) < 1e-10) continue;

      const x1 = line1.start_x, y1 = line1.start_y;
      const x2 = line2.start_x, y2 = line2.start_y;
      const t = ((x2 - x1) * dy2 - (y2 - y1) * dx2) / cross;
      const ix = x1 + t * dx1;
      const iy = y1 + t * dy1;

      const t1 = ((ix - x1) * dx1 + (iy - y1) * dy1) / lenSq1;
      const t2 = ((ix - x2) * dx2 + (iy - y2) * dy2) / lenSq2;
      if (t1 < -0.05 || t1 > 1.05 || t2 < -0.05 || t2 > 1.05) continue;

      const dist = Math.hypot(cursor[0] - ix, cursor[1] - iy);
      if (dist > threshold) continue;

      if (dist < bestDist) {
        bestDist = dist;
        bestPair = { line1, line2 };
      }
    }
  }

  if (!bestPair) return null;

  const result = speculativeMultiSolve({
    bridge,
    params: sketchParameters,
    constraints,
    draftStart: cursor,
    cursor,
    snapConstraints: [
      { snapType: "point_on_line_pl", targetEntityId: bestPair.line1.line_id },
      { snapType: "point_on_line_pl", targetEntityId: bestPair.line2.line_id },
    ],
  });

  if (!result?.converged || result.distance > threshold) return null;

  return {
    local: result.position,
    snapLabel,
    snapPerpendicularHostLineId: null,
    snapAxisLock: null,
    snapTangentCircleId: null,
    snapParallelHostLineId: null,
    snapLineBodyHostLineId: null,
    snapLineBodyT: null,
    snapIntersectionLineIds: [bestPair.line1.line_id, bestPair.line2.line_id],
    distance: result.distance,
  };
}

/**
 * Try tangent-through-line snap via speculative WASM solver.
 *
 * Strategy: when the draft line is tangent to a circle AND the cursor
 * should lie on a line, push tangent_lc (virtual line + circle) and
 * point_on_line_pl (cursor point + host line) simultaneously.
 */
function speculativeTangentThroughLineSnap({
  sketchParameters,
  constraints,
  circles,
  lines,
  draftStart,
  cursor,
  threshold,
  snapLabel,
}: {
  sketchParameters: SketchFeatureParameters | null;
  constraints: SketchConstraintData[];
  circles: readonly SketchSnapCircle[];
  lines: readonly SketchSnapLine[];
  draftStart: [number, number];
  cursor: [number, number];
  threshold: number;
  snapLabel: string;
}): DynamicSnapResult | null {
  const bridge = getBridge();
  if (!bridge || !sketchParameters) return null;

  let bestCircle: (typeof circles)[number] | null = null;
  let bestLine: (typeof lines)[number] | null = null;
  let bestDist = Infinity;

  for (const circle of circles) {
    if (circle.is_construction) continue;

    const pcDx = circle.center_x - draftStart[0];
    const pcDy = circle.center_y - draftStart[1];
    const pcDist = Math.hypot(pcDx, pcDy);
    if (pcDist <= circle.radius + 1e-9) continue;

    const alpha = Math.asin(circle.radius / pcDist);
    const baseAngle = Math.atan2(pcDy, pcDx);
    const tangentLen = Math.sqrt(pcDist * pcDist - circle.radius * circle.radius);

    for (const sign of [1, -1]) {
      const tpDir = baseAngle + sign * alpha;
      const tpX = draftStart[0] + tangentLen * Math.cos(tpDir);
      const tpY = draftStart[1] + tangentLen * Math.sin(tpDir);

      for (const line of lines) {
        if (line.is_construction) continue;
        const segDx = line.end_x - line.start_x;
        const segDy = line.end_y - line.start_y;
        const segLenSq = segDx * segDx + segDy * segDy;
        if (segLenSq < 1e-12) continue;

        const t = Math.max(0, Math.min(1,
          ((tpX - line.start_x) * segDx + (tpY - line.start_y) * segDy) / segLenSq));
        const lx = line.start_x + t * segDx;
        const ly = line.start_y + t * segDy;
        const dist = Math.hypot(tpX - lx, tpY - ly);
        if (dist > threshold) continue;

        const cursorDist = Math.hypot(cursor[0] - tpX, cursor[1] - tpY);
        if (cursorDist > threshold * 2) continue;

        if (dist < bestDist) {
          bestDist = dist;
          bestCircle = circle;
          bestLine = line;
        }
      }
    }
  }

  if (!bestCircle || !bestLine) return null;

  const result = speculativeMultiSolve({
    bridge,
    params: sketchParameters,
    constraints,
    draftStart,
    cursor,
    snapConstraints: [
      { snapType: "tangent_lc", targetEntityId: bestCircle.circle_id },
      { snapType: "point_on_line_pl", targetEntityId: bestLine.line_id },
    ],
  });

  if (!result?.converged || result.distance > threshold) return null;

  return {
    local: result.position,
    snapLabel,
    snapPerpendicularHostLineId: null,
    snapAxisLock: null,
    snapTangentCircleId: bestCircle.circle_id,
    snapParallelHostLineId: null,
    snapLineBodyHostLineId: bestLine.line_id,
    snapLineBodyT: null,
    snapIntersectionLineIds: null,
    distance: result.distance,
  };
}
