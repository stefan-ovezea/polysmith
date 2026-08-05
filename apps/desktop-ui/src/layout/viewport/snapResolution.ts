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
import { useCadCoreStore } from "@/state/cadCoreStore";
import { useToastStore } from "@/state/toastStore";
import type { SketchConstraintData } from "@/lib/planegcsBridge";

// Diagnostic logger for tangent snap. Each gate fires at most once per
// 5 seconds (cooldown) so you can see retries after moving the cursor.
const _gateCooldowns = new Map<string, number>();
function _snapDebug(gate: string, detail: string) {
  const now = Date.now();
  const last = _gateCooldowns.get(gate) ?? 0;
  if (now - last < 5000) return;
  _gateCooldowns.set(gate, now);
  const msg = `[tangent] ${gate}: ${detail}`;
  try { useCadCoreStore.getState().addMessage(msg); } catch (_) { /* ok */ }
  try { useToastStore.getState().pushToast("info", msg); } catch (_) { /* ok */ }
}

export interface SketchSnapCandidate {
  local: [number, number];
  label: string;
  kind?:
    | "midpoint"
    | "endpoint"
    | "center"
    | "intersection"
    | "nearest"
    | "tangent"
    | "quadrant";
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
  objectSnapLatchKey?: string | null;
  /** When false, inference alignment snaps (H/V alignment with existing
   *  sketch vertices) are not resolved — only used for visual guides.
   *  Defaults to true (inference snaps active). Set to false during
   *  pointer-up commit to prevent inference from pulling committed
   *  coordinates away from the user's intended click position. */
  inferenceSnapsEnabled?: boolean;
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
  objectSnapLatchKey = null,
  inferenceSnapsEnabled = true,
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
  objectSnapLatchKey?: string | null;
  inferenceSnapsEnabled?: boolean;
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
    onCircle: string;
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
    objectSnapLatchKey,
  });
  const prioritySnap = priorityStaticSnapPoint(staticSnap);
  if (prioritySnap) {
    return prioritySnap;
  }

  const resolvedSnap = chooseResolvedSnap({
    staticSnap,
    dynamicSnap: dynamicSnapsEnabled !== false
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
          objectSnapLatchKey,
          inferenceSnapsEnabled,
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
  const nearX = Math.abs(local[0] - nearestX) <= threshold;
  const nearY = Math.abs(local[1] - nearestY) <= threshold;
  // Only snap when BOTH axes are within threshold — snap to the
  // nearest grid intersection, not to a single grid line.
  if (nearX && nearY) {
    return [nearestX, nearestY];
  }
  return [local[0], local[1]];
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
  objectSnapLatchKey,
}: {
  candidates: readonly SketchSnapCandidate[];
  point: RawSketchPoint;
  filter: SelectionFilter;
  sketchSnapDistance: number;
  activeSketchPlaneId: string | null;
  activeSketchPlaneFrame: SketchPlaneFrame | null;
  includeEndpointMetadata: boolean;
  objectSnapLatchKey?: string | null;
}): ResolvedStaticSnap | null {
  const latched = objectSnapLatchKey
    ? staticSnapCandidateByKey(candidates, objectSnapLatchKey)
    : null;
  if (
    latched &&
    isStaticSnapCandidateAllowed(latched, filter) &&
    distanceBetweenPoints(point.local, latched.local) <= sketchSnapDistance
  ) {
    return resolvedStaticSnapFromCandidate({
      candidate: latched,
      point,
      activeSketchPlaneId,
      activeSketchPlaneFrame,
      includeEndpointMetadata,
    });
  }

  const closest = closestStaticSnapCandidate(
    candidates,
    point.local,
    filter,
    distanceBetweenPoints,
  );
  if (!closest || closest.distance > sketchSnapDistance) {
    return null;
  }
  return resolvedStaticSnapFromCandidate({
    candidate: closest.candidate,
    point,
    activeSketchPlaneId,
    activeSketchPlaneFrame,
    includeEndpointMetadata,
  });
}

function resolvedStaticSnapFromCandidate({
  candidate,
  point,
  activeSketchPlaneId,
  activeSketchPlaneFrame,
  includeEndpointMetadata,
}: {
  candidate: SketchSnapCandidate;
  point: RawSketchPoint;
  activeSketchPlaneId: string | null;
  activeSketchPlaneFrame: SketchPlaneFrame | null;
  includeEndpointMetadata: boolean;
}): ResolvedStaticSnap {
  return {
    point: previewPointFromStaticCandidate({
      candidate,
      world: sketchLocalToWorld(
        activeSketchPlaneId,
        candidate.local,
        activeSketchPlaneFrame,
      ),
      includeEndpointMetadata:
        includeEndpointMetadata && isPriorityStaticSnap(candidate),
    }),
    distance: distanceBetweenPoints(point.local, candidate.local),
    isPriority: isPriorityStaticSnap(candidate),
  };
}

function isPriorityStaticSnap(candidate: SketchSnapCandidate) {
  return candidate.kind === "endpoint" || candidate.kind === "midpoint" || candidate.kind === "quadrant";
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
  objectSnapLatchKey,
  inferenceSnapsEnabled = true,
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
    onCircle: string;
    tangent: string;
    perpendicular: string;
    parallel: string;
    intersection: string;
  };
  activeSketchPlaneId: string | null;
  activeSketchPlaneFrame: SketchPlaneFrame | null;
  objectSnapLatchKey?: string | null;
  inferenceSnapsEnabled?: boolean;
}): ResolvedDynamicSnap | null {
  if (!sketchParameters || !hasEnabledDynamicSnap(filter, draftStartLocal)) {
    return null;
  }
  const bestDynamic = dynamicSnapCandidate({
    lines: sketchParameters.lines,
    circles: sketchParameters.circles,
    arcs: sketchParameters.arcs,
    filter,
    draftStart: draftStartLocal,
    cursor: point.local,
    threshold: sketchSnapDistance,
    parallelAngleThresholdRadians:
      (filter.parallel_angle_degrees ?? 8) * Math.PI / 180,
    labels,
    sketchParameters,
    constraints: sketchConstraints,
    objectSnapLatchKey,
    inferenceSnapsEnabled,
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
  // All dynamic snaps are relative to the draft line (axis-lock
  // constrains direction from start, body snaps constrain the second
  // point onto geometry, tangent/perpendicular/parallel define the
  // line angle from start). Without a draft start — placing the first
  // point of a line — none of these are meaningful. Returning false
  // here prevents body snaps from overriding grid snap for the first
  // click, which would otherwise pull the cursor onto nearby geometry
  // bodies instead of snapping to the grid.
  if (!draftStartLocal) return false;

  return (
    filter.snap_nearest ||
    filter.snap_intersection ||
    Boolean(
      filter.snap_polar ||
      filter.snap_tangent ||
      filter.snap_perpendicular ||
      filter.snap_parallel,
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
  const hasInferenceGuides = snap.inferenceGuideLines.length > 0;
  return {
    local: snap.local,
    world: sketchLocalToWorld(activeSketchPlaneId, snap.local, activeSketchPlaneFrame),
    snapLabel: snap.snapLabel,
    snapFeedbackSource: dynamicSnapFeedbackSource(snap),
    snapTargetKey: dynamicSnapTargetKey(snap),
    snapMidpointHostLineId: null,
    snapMidpointT: null,
    snapPerpendicularHostLineId: snap.snapPerpendicularHostLineId,
    snapEndpointHostLineId: null,
    snapLineBodyHostLineId: snap.snapLineBodyHostLineId,
    snapLineBodyT: snap.snapLineBodyT,
    snapCircleBodyHostCircleId: snap.snapCircleBodyHostCircleId,
    snapCircleBodyAngle: snap.snapCircleBodyAngle,
    snapAxisLock: snap.snapAxisLock,
    snapTangentCircleId: snap.snapTangentCircleId,
    snapParallelHostLineId: snap.snapParallelHostLineId,
    snapIntersectionLineIds: snap.snapIntersectionLineIds,
    snapTangentPoint: snap.snapTangentPoint,
    inferenceLines: hasInferenceGuides ? snap.inferenceGuideLines : undefined,
  };
}

function unsnappedPreviewPoint(
  point: RawSketchPoint,
  snapLabel: string | null,
): SketchPreviewPoint {
  return {
    ...point,
    snapLabel,
    snapFeedbackSource: snapLabel ? "grid" : null,
    snapTargetKey: null,
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
        candidates.push({
          local: [candidate.local_x, candidate.local_y],
          label: candidate.label,
          kind: "center",
        });
        break;
      case "quadrant":
        candidates.push({
          local: [candidate.local_x, candidate.local_y],
          label: candidate.label,
          kind: "quadrant",
        });
        break;
      // Projected vertices (from 3D body geometry projected onto the
      // sketch plane). Treated as priority snaps (same tier as endpoints)
      // so that axis-lock / H-V constraint snaps never override them.
      // Exact coordinates are used so the C++ core's find_coincident_endpoint
      // (kCoincidentTolerance = 0.01 mm) always matches and reuses the
      // existing projected vertex instead of creating a duplicate.
      case "projected":
        candidates.push({
          local: [candidate.local_x, candidate.local_y],
          label: candidate.label,
          kind: "endpoint",
        });
        break;
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
      kind: "center",
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
  snapCircleBodyHostCircleId?: string | null;
  snapCircleBodyAngle?: number | null;
  snapIntersectionLineIds: [string, string] | null;
  distance: number;
  /** When the cursor aligned with an existing sketch vertex via H/V
   *  inference, holds the alignment direction and the source point. */
  snapInferenceKind: "horizontal" | "vertical" | null;
  snapInferenceFrom: [number, number] | null;
  /** Guide lines showing all inference alignments (including weaker
   *  ones that didn't win the snap contest). Rendered as dotted lines. */
  inferenceGuideLines: Array<{
    from: [number, number];
    draft: [number, number];
    axis: "horizontal" | "vertical";
  }>;
  /** When tangent snap fires, the exact contact point on the circle
   *  where the tangent line touches. Used at commit time to set the
   *  next polyline segment's start to the solver-resolved endpoint
   *  rather than the cursor-projected snap position. */
  snapTangentPoint?: [number, number] | null;
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

interface SketchSnapArc {
  arc_id: string;
  center_x: number;
  center_y: number;
  radius: number;
  start_x: number;
  start_y: number;
  end_x: number;
  end_y: number;
  ccw: boolean;
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

/**
 * Collect all sketch vertices that can serve as inference alignment
 * sources: line endpoints and circle centers.
 */
function collectInferenceVertices(
  lines: readonly SketchSnapLine[],
  circles: readonly SketchSnapCircle[],
  arcs: readonly SketchSnapArc[],
): Array<[number, number]> {
  const vertices: Array<[number, number]> = [];
  for (const line of lines) {
    if (line.is_construction) continue;
    vertices.push([line.start_x, line.start_y]);
    vertices.push([line.end_x, line.end_y]);
  }
  for (const circle of circles) {
    if (circle.is_construction) continue;
    vertices.push([circle.center_x, circle.center_y]);
  }
  for (const arc of arcs) {
    if (arc.is_construction) continue;
    vertices.push([arc.center_x, arc.center_y]);
    vertices.push([arc.start_x, arc.start_y]);
    vertices.push([arc.end_x, arc.end_y]);
  }
  return vertices;
}

/**
 * Try alignment inference snap: when the cursor is horizontally or
 * vertically aligned with an existing sketch vertex (not the draft
 * start), snap the cursor to that alignment and emit guide line info.
 * This provides the "inference line" / "tracking" alignment visual.
 */
function speculativeAlignmentInferenceSnap({
  draftStart,
  cursor,
  lines,
  circles,
  arcs,
  threshold,
  labels,
}: {
  draftStart: [number, number];
  cursor: [number, number];
  lines: readonly SketchSnapLine[];
  circles: readonly SketchSnapCircle[];
  arcs: readonly SketchSnapArc[];
  threshold: number;
  labels: {
    axisLockHorizontal: string;
    axisLockVertical: string;
  };
}): DynamicSnapResult | null {
  const vertices = collectInferenceVertices(lines, circles, arcs);
  if (vertices.length === 0) return null;

  const [sx, sy] = draftStart;
  const [cx, cy] = cursor;

  // Find the closest H or V alignment among all existing vertices.
  let bestDist = Infinity;
  let bestLocal: [number, number] | null = null;
  let bestAxis: "horizontal" | "vertical" | null = null;
  let bestFrom: [number, number] | null = null;
  const allGuides: DynamicSnapResult["inferenceGuideLines"] = [];

  for (const [vx, vy] of vertices) {
    // Skip vertices too close to the draft start (that's axis-lock)
    const distToStart = Math.hypot(vx - sx, vy - sy);
    if (distToStart < 1e-6) continue;

    const hDist = Math.abs(cy - vy); // cursor Y aligned with vertex Y
    const vDist = Math.abs(cx - vx); // cursor X aligned with vertex X

    // Check horizontal alignment (cursor.y ≈ vertex.y)
    if (hDist <= threshold) {
      const draft: [number, number] = [cx, vy]; // snap cursor to (cursor.x, vertex.y)
      allGuides.push({ from: [vx, vy], draft, axis: "horizontal" });
      if (hDist < bestDist) {
        bestDist = hDist;
        bestLocal = draft;
        bestAxis = "horizontal";
        bestFrom = [vx, vy];
      }
    }

    // Check vertical alignment (cursor.x ≈ vertex.x)
    if (vDist <= threshold) {
      const draft: [number, number] = [vx, cy]; // snap cursor to (vertex.x, cursor.y)
      allGuides.push({ from: [vx, vy], draft, axis: "vertical" });
      if (vDist < bestDist) {
        bestDist = vDist;
        bestLocal = draft;
        bestAxis = "vertical";
        bestFrom = [vx, vy];
      }
    }
  }

  if (!bestLocal || !bestAxis || !bestFrom) return null;

  return {
    local: bestLocal,
    snapLabel:
      bestAxis === "horizontal"
        ? labels.axisLockHorizontal
        : labels.axisLockVertical,
    snapPerpendicularHostLineId: null,
    // NOT snapAxisLock — inference alignment is placement-only guidance,
    // not a constraint. Setting snapAxisLock would force the new line
    // horizontal/vertical, which is wrong for point-to-point alignment.
    snapAxisLock: null,
    snapTangentCircleId: null,
    snapParallelHostLineId: null,
    snapLineBodyHostLineId: null,
    snapLineBodyT: null,
    snapIntersectionLineIds: null,
    snapInferenceKind: bestAxis,
    snapInferenceFrom: bestFrom,
    inferenceGuideLines: allGuides,
    distance: bestDist,
  };
}

export function dynamicSnapCandidate({
  lines,
  circles,
  arcs,
  filter,
  draftStart,
  cursor,
  threshold,
  parallelAngleThresholdRadians,
  labels,
  sketchParameters,
  constraints,
  objectSnapLatchKey,
  inferenceSnapsEnabled = true,
}: {
  lines: readonly SketchSnapLine[];
  circles: readonly SketchSnapCircle[];
  arcs: readonly SketchSnapArc[];
  filter: SelectionFilter;
  draftStart: [number, number] | null | undefined;
  cursor: [number, number];
  threshold: number;
  parallelAngleThresholdRadians: number;
  labels: {
    axisLockHorizontal: string;
    axisLockVertical: string;
    onLine: string;
    onCircle: string;
    tangent: string;
    perpendicular: string;
    parallel: string;
    intersection: string;
  };
  /** Full sketch params for speculative WASM solver path. */
  sketchParameters?: SketchFeatureParameters | null;
  /** Constraints for speculative WASM solver path. */
  constraints?: import("@/lib/planegcsBridge").SketchConstraintData[];
  objectSnapLatchKey?: string | null;
  inferenceSnapsEnabled?: boolean;
}): DynamicSnapResult | null {
  let best: DynamicSnapResult | null = null;
  // Inference guide lines are collected independently so they survive
  // even when a later snap (parallel, perpendicular, etc.) wins via
  // closerDynamicSnap and replaces `best`.
  let inferenceGuides: DynamicSnapResult["inferenceGuideLines"] = [];

  if (filter.snap_nearest && objectSnapLatchKey?.startsWith("dynamic:line-body:")) {
    const lineId = objectSnapLatchKey.slice("dynamic:line-body:".length);
    const line = lines.find((candidate) => candidate.line_id === lineId);
    if (line) {
      const latchedLineBody = speculativeLineBodySnap({
        lines: [line],
        cursor,
        threshold,
        snapLabel: labels.onLine,
      });
      if (latchedLineBody) {
        best = latchedLineBody;
      }
    }
  }

  if (filter.snap_nearest && filter.snap_circle_body && objectSnapLatchKey?.startsWith("dynamic:circle-body:")) {
    const circleId = objectSnapLatchKey.slice("dynamic:circle-body:".length);
    const circle = circles.find((candidate) => candidate.circle_id === circleId);
    if (circle) {
      const latchedCircleBody = speculativeCircleBodySnap({
        circles: [circle],
        cursor,
        threshold,
        snapLabel: labels.onCircle,
      });
      if (latchedCircleBody) {
        best = latchedCircleBody;
      }
    }
  }

  if (draftStart) {
    const draftDx = cursor[0] - draftStart[0];
    const draftDy = cursor[1] - draftStart[1];
    const hasDraftMovement = Math.hypot(draftDx, draftDy) > 1e-6;

    if (hasDraftMovement && filter.snap_polar) {
      // Axis-lock (H/V from draft start) always takes snap priority over
      // inference. The H/V constraint badge and snap coordinates come from
      // axis-lock; inference only provides the dotted guide lines.
      const axisLockSnap = speculativeAxisLockSnap({
        draftStart,
        cursor,
        threshold,
        labels,
      });

      // Inference alignment — H/V alignment with existing sketch vertices.
      // Only active during pointer move (preview), NOT during pointer-up
      // commit. Inference is a visual guide; it must not pull committed
      // coordinates away from the user's intended click position.
      if (inferenceSnapsEnabled) {
        const inferenceSnap = speculativeAlignmentInferenceSnap({
          draftStart,
          cursor,
          lines,
          circles,
          arcs,
          threshold,
          labels,
        });
        if (inferenceSnap && inferenceSnap.inferenceGuideLines.length > 0) {
          inferenceGuides = inferenceSnap.inferenceGuideLines;
          if (axisLockSnap) {
            // Axis-lock wins the snap; inference provides visual guides.
            best = axisLockSnap;
          } else {
            // No axis-lock — inference still provides useful visual guides.
            best = inferenceSnap;
          }
        } else if (axisLockSnap) {
          best = axisLockSnap;
        }
      } else if (axisLockSnap) {
        best = axisLockSnap;
      }
    }
  }

  if (filter.snap_nearest) {
    const spec = speculativeLineBodySnap({
      lines, cursor, threshold,
      snapLabel: labels.onLine,
    });
    best = closerDynamicSnap(best, spec);

    if (filter.snap_circle_body) {
      const circleSpec = speculativeCircleBodySnap({
        circles, cursor, threshold,
        snapLabel: labels.onCircle,
      });
      best = closerDynamicSnap(best, circleSpec);
    }

    if (filter.snap_arc_body) {
      const arcSpec = speculativeArcBodySnap({
        arcs, cursor, threshold,
        snapLabel: labels.onCircle,
      });
      best = closerDynamicSnap(best, arcSpec);
    }
  }

  if (draftStart) {
    const draftDx = cursor[0] - draftStart[0];
    const draftDy = cursor[1] - draftStart[1];
    const hasDraftMovement = Math.hypot(draftDx, draftDy) > 1e-6;

    // diagnostic: show why tangent gate passes/fails
    _snapDebug("gate", `hasDraftStart=1 hasMovement=${hasDraftMovement} snap_tangent=${filter.snap_tangent} circles=${circles.length}`);

    if (hasDraftMovement && filter.snap_tangent) {
      _snapDebug("enter", `circles=${circles.length}`);
      const spec = speculativeTangentSnap({
        sketchParameters: sketchParameters ?? null,
        constraints: constraints ?? [],
        circles, arcs, draftStart, cursor,
        threshold,
        snapLabel: labels.tangent,
      });
      // Tangent is a deliberate geometric relationship — when it fires
      // during line drafting, it takes priority over the generic
      // "nearest point on circle" snap, regardless of which circle won
      // the circle-body proximity check. For non-circle-body snaps
      // (line-body, axis-lock, null) we fall back to distance comparison.
      if (spec) {
        if (best?.snapCircleBodyHostCircleId != null) {
          best = spec;
        } else {
          best = closerDynamicSnap(best, spec);
        }
      }
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

  // Attach inference guide lines to whichever snap won, so dotted
  // alignment hints display even when a parallel/perpendicular/tangent
  // snap overrides the inference snap position.
  if (best && inferenceGuides.length > 0 && best.inferenceGuideLines.length === 0) {
    best.inferenceGuideLines = inferenceGuides;
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

function staticSnapCandidateByKey(
  candidates: readonly SketchSnapCandidate[],
  key: string,
) {
  return candidates.find((candidate) => staticSnapCandidateKey(candidate) === key) ?? null;
}

function staticSnapCandidateKey(candidate: SketchSnapCandidate) {
  const kind = candidate.kind ?? "point";
  const host =
    candidate.endpointHostLineId ??
    candidate.hostLineId ??
    `${candidate.local[0].toFixed(6)},${candidate.local[1].toFixed(6)}`;
  const t = candidate.tValue === undefined ? "" : `:${candidate.tValue.toFixed(6)}`;
  return `static:${kind}:${host}${t}`;
}

type SketchSnapKind = NonNullable<SketchSnapCandidate["kind"]>;

const SNAP_FILTER_KEY_BY_KIND = {
  endpoint: "snap_endpoint",
  midpoint: "snap_midpoint",
  center: "snap_center",
  intersection: "snap_intersection",
  nearest: "snap_nearest",
  tangent: "snap_tangent",
  quadrant: "snap_quadrant",
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
    snapFeedbackSource: "object",
    snapTargetKey: staticSnapCandidateKey(candidate),
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

function dynamicSnapFeedbackSource(
  snap: DynamicSnapResult,
): SketchPreviewPoint["snapFeedbackSource"] {
  if (
    snap.snapPerpendicularHostLineId ||
    snap.snapTangentCircleId ||
    snap.snapParallelHostLineId ||
    snap.snapLineBodyHostLineId ||
    snap.snapCircleBodyHostCircleId ||
    snap.snapIntersectionLineIds
  ) {
    return "object";
  }
  return null;
}

function dynamicSnapTargetKey(snap: DynamicSnapResult) {
  if (snap.snapLineBodyHostLineId) {
    return `dynamic:line-body:${snap.snapLineBodyHostLineId}`;
  }
  if (snap.snapCircleBodyHostCircleId) {
    return `dynamic:circle-body:${snap.snapCircleBodyHostCircleId}`;
  }
  if (snap.snapPerpendicularHostLineId) {
    return `dynamic:perpendicular:${snap.snapPerpendicularHostLineId}`;
  }
  if (snap.snapParallelHostLineId) {
    return `dynamic:parallel:${snap.snapParallelHostLineId}`;
  }
  if (snap.snapTangentCircleId) {
    return `dynamic:tangent:${snap.snapTangentCircleId}`;
  }
  if (snap.snapIntersectionLineIds) {
    return `dynamic:intersection:${snap.snapIntersectionLineIds.join(":")}`;
  }
  return null;
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
    snapInferenceKind: null,
    snapInferenceFrom: null,
    inferenceGuideLines: [],
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
  arcs,
  draftStart,
  cursor,
  threshold,
  snapLabel,
}: {
  sketchParameters: SketchFeatureParameters | null;
  constraints: SketchConstraintData[];
  circles: readonly SketchSnapCircle[];
  arcs: readonly SketchSnapArc[];
  draftStart: [number, number];
  cursor: [number, number];
  threshold: number;
  snapLabel: string;
}): DynamicSnapResult | null {
  // Pure TS geometry — find tangent points for each circle, pick the one
  // nearest to the cursor. No WASM solver needed; the TS math is exact.
  let bestCircle: (typeof circles)[number] | null = null;
  let bestDist = Infinity;
  let bestTpX = 0;
  let bestTpY = 0;

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
        bestTpX = tpX;
        bestTpY = tpY;
      }
    }
  }

  // Arc tangents — compute tangent points on the full circle containing each
  // arc, then keep only those whose angle falls within the arc's sweep.
  let bestArc: (typeof arcs)[number] | null = null;
  for (const arc of arcs) {
    if (arc.is_construction) continue;
    const pcDx = arc.center_x - draftStart[0];
    const pcDy = arc.center_y - draftStart[1];
    const pcDist = Math.hypot(pcDx, pcDy);
    if (pcDist <= arc.radius + 1e-9) continue;

    const alpha = Math.asin(arc.radius / pcDist);
    const baseAngle = Math.atan2(pcDy, pcDx);
    const tangentLen = Math.sqrt(pcDist * pcDist - arc.radius * arc.radius);
    const startAngle = Math.atan2(arc.start_y - arc.center_y, arc.start_x - arc.center_x);
    const endAngle = Math.atan2(arc.end_y - arc.center_y, arc.end_x - arc.center_x);
    for (const sign of [1, -1]) {
      const tpDir = baseAngle + sign * alpha;
      const tpX = draftStart[0] + tangentLen * Math.cos(tpDir);
      const tpY = draftStart[1] + tangentLen * Math.sin(tpDir);
      // Check whether the tangent point's angle from the circle center
      // lies within the arc sweep.  tpDir is the angle from the *draft
      // start*, not from the center — compute the center-relative angle
      // from the tangent point coordinates.
      const tpAngle = Math.atan2(tpY - arc.center_y, tpX - arc.center_x);
      if (!angleInArcSweep(tpAngle, startAngle, endAngle, arc.ccw)) continue;
      const tpDist = Math.hypot(cursor[0] - tpX, cursor[1] - tpY);
      if (tpDist < bestDist) {
        bestDist = tpDist;
        bestCircle = null; // clear circle winner
        bestArc = arc;
        bestTpX = tpX;
        bestTpY = tpY;
      }
    }
  }

  if (!bestCircle && !bestArc) {
    _snapDebug("no_circle", `circles=${circles.length} arcs=${arcs.length}`);
    return null;
  }

  // Project the cursor onto the tangent line (draftStart → tangent point).
  // The snapped position lies on the exact tangent line.
  const tdx = bestTpX - draftStart[0];
  const tdy = bestTpY - draftStart[1];
  const tLenSq = tdx * tdx + tdy * tdy;
  if (tLenSq < 1e-12) return null;

  const t = ((cursor[0] - draftStart[0]) * tdx + (cursor[1] - draftStart[1]) * tdy) / tLenSq;
  // Clamp t so the snap point stays on the "far side" of the tangent point
  // (the user is drawing away from the draft start, past the tangent point).
  const clampedT = Math.max(t, 1.0);
  const snapX = draftStart[0] + clampedT * tdx;
  const snapY = draftStart[1] + clampedT * tdy;

  const snapDist = Math.hypot(snapX - cursor[0], snapY - cursor[1]);
  if (snapDist > threshold) return null;

  const tangentCircleId = bestCircle?.circle_id ?? bestArc?.arc_id ?? null;
  return {
    local: [snapX, snapY],
    snapLabel,
    snapPerpendicularHostLineId: null,
    snapAxisLock: null,
    snapTangentCircleId: tangentCircleId,
    snapParallelHostLineId: null,
    snapLineBodyHostLineId: null,
    snapLineBodyT: null,
    snapIntersectionLineIds: null,
    snapInferenceKind: null,
    snapInferenceFrom: null,
    inferenceGuideLines: [],
    distance: snapDist,
    // Preserve the exact tangent contact point so the polyline chain
    // starts the next segment from the solver-resolved position.
    snapTangentPoint: tangentCircleId ? [bestTpX, bestTpY] : null,
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
    snapInferenceKind: null,
    snapInferenceFrom: null,
    inferenceGuideLines: [],
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

    // Skip host lines the draft start already lies on — the new line
    // is collinear with the host (shares the same infinite line).
    // A parallel constraint would be redundant.
    const startProj =
      ((draftStart[0] - line.start_x) * ldx +
       (draftStart[1] - line.start_y) * ldy) / lenSq;
    const startOnHostX = line.start_x + startProj * ldx;
    const startOnHostY = line.start_y + startProj * ldy;
    const startDistToHost = Math.hypot(
      draftStart[0] - startOnHostX,
      draftStart[1] - startOnHostY,
    );
    if (startDistToHost <= 1e-3) continue;

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
    snapInferenceKind: null,
    snapInferenceFrom: null,
    inferenceGuideLines: [],
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
    snapInferenceKind: null,
    snapInferenceFrom: null,
    inferenceGuideLines: [],
    distance: bestDist,
  };
}

/**
 * Try circle-body snap: project the cursor onto the nearest point on
 * each circle's perimeter. Pure TS geometry — no solver needed.
 */
function speculativeCircleBodySnap({
  circles,
  cursor,
  threshold,
  snapLabel,
}: {
  circles: readonly SketchSnapCircle[];
  cursor: [number, number];
  threshold: number;
  snapLabel: string;
}): DynamicSnapResult | null {
  let bestCircle: (typeof circles)[number] | null = null;
  let bestDist = Infinity;
  let bestAngle = 0;
  let bestLocal: [number, number] | null = null;

  for (const circle of circles) {
    if (circle.is_construction) continue;
    const dx = cursor[0] - circle.center_x;
    const dy = cursor[1] - circle.center_y;
    const distFromCenter = Math.hypot(dx, dy);
    if (distFromCenter < 1e-12) continue;
    const angle = Math.atan2(dy, dx);
    const x = circle.center_x + circle.radius * Math.cos(angle);
    const y = circle.center_y + circle.radius * Math.sin(angle);
    const dist = Math.hypot(cursor[0] - x, cursor[1] - y);
    if (dist > threshold) continue;
    if (dist < bestDist) {
      bestDist = dist;
      bestCircle = circle;
      bestAngle = angle;
      bestLocal = [x, y];
    }
  }

  if (!bestCircle || !bestLocal) return null;

  return {
    local: bestLocal,
    snapLabel,
    snapPerpendicularHostLineId: null,
    snapAxisLock: null,
    snapTangentCircleId: null,
    snapParallelHostLineId: null,
    snapLineBodyHostLineId: null,
    snapLineBodyT: null,
    snapCircleBodyHostCircleId: bestCircle.circle_id,
    snapCircleBodyAngle: bestAngle,
    snapIntersectionLineIds: null,
    snapInferenceKind: null,
    snapInferenceFrom: null,
    inferenceGuideLines: [],
    distance: bestDist,
  };
}


/** True when angle `a` lies within the arc sweep from start to end,
 *  respecting the ccw flag. All angles are in [-π, π]. */
function angleInArcSweep(
  a: number, startAngle: number, endAngle: number, ccw: boolean,
): boolean {
  // Normalize all angles to [0, 2π)
  const norm = (angle: number) => ((angle % (2 * Math.PI)) + 2 * Math.PI) % (2 * Math.PI);
  const na = norm(a);
  const ns = norm(startAngle);
  const ne = norm(endAngle);
  if (ccw) {
    if (ns <= ne) return na >= ns && na <= ne;
    return na >= ns || na <= ne;
  }
  // CW
  if (ns >= ne) return na <= ns && na >= ne;
  return na <= ns || na >= ne;
}

function speculativeArcBodySnap({
  arcs,
  cursor,
  threshold,
  snapLabel,
}: {
  arcs: readonly SketchSnapArc[];
  cursor: [number, number];
  threshold: number;
  snapLabel: string;
}): DynamicSnapResult | null {
  let bestArc: (typeof arcs)[number] | null = null;
  let bestDist = Infinity;
  let bestAngle = 0;
  let bestLocal: [number, number] | null = null;

  for (const arc of arcs) {
    if (arc.is_construction) continue;
    const dx = cursor[0] - arc.center_x;
    const dy = cursor[1] - arc.center_y;
    const distFromCenter = Math.hypot(dx, dy);
    if (distFromCenter < 1e-12) continue;
    const angle = Math.atan2(dy, dx);
    const x = arc.center_x + arc.radius * Math.cos(angle);
    const y = arc.center_y + arc.radius * Math.sin(angle);
    const startAngle = Math.atan2(arc.start_y - arc.center_y, arc.start_x - arc.center_x);
    const endAngle = Math.atan2(arc.end_y - arc.center_y, arc.end_x - arc.center_x);
    if (!angleInArcSweep(angle, startAngle, endAngle, arc.ccw)) continue;
    const dist = Math.hypot(cursor[0] - x, cursor[1] - y);
    if (dist > threshold) continue;
    if (dist < bestDist) {
      bestDist = dist;
      bestArc = arc;
      bestAngle = angle;
      bestLocal = [x, y];
    }
  }

  if (!bestArc || !bestLocal) return null;

  return {
    local: bestLocal,
    snapLabel,
    snapPerpendicularHostLineId: null,
    snapAxisLock: null,
    snapTangentCircleId: null,
    snapParallelHostLineId: null,
    snapLineBodyHostLineId: null,
    snapLineBodyT: null,
    snapCircleBodyHostCircleId: bestArc.arc_id,
    snapCircleBodyAngle: bestAngle,
    snapIntersectionLineIds: null,
    snapInferenceKind: null,
    snapInferenceFrom: null,
    inferenceGuideLines: [],
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
    snapInferenceKind: null,
    snapInferenceFrom: null,
    inferenceGuideLines: [],
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
    snapInferenceKind: null,
    snapInferenceFrom: null,
    inferenceGuideLines: [],
    distance: result.distance,
  };
}
