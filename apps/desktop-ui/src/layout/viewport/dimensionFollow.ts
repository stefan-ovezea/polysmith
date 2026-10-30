import type { SketchDimensionScene } from "@/types";

export type Point3 = [number, number, number];

/** One sketch point that moved during a drag preview. */
export interface SketchPointMove {
  /** Position the point had in the committed scene (the anchor to match). */
  from: Point3;
  /** Position the drag preview has moved it to. */
  to: Point3;
}

const DEFAULT_TOLERANCE = 1e-6;

function subtraction(a: Point3, b: Point3): Point3 {
  return [a[0] - b[0], a[1] - b[1], a[2] - b[2]];
}

function addition(a: Point3, b: Point3): Point3 {
  return [a[0] + b[0], a[1] + b[1], a[2] + b[2]];
}

function midpoint(a: Point3, b: Point3): Point3 {
  return [(a[0] + b[0]) / 2, (a[1] + b[1]) / 2, (a[2] + b[2]) / 2];
}

function samePoint(a: Point3, b: Point3, tolerance: number) {
  return (
    Math.abs(a[0] - b[0]) <= tolerance &&
    Math.abs(a[1] - b[1]) <= tolerance &&
    Math.abs(a[2] - b[2]) <= tolerance
  );
}

/**
 * Follow a point through the move list: when `anchor` still sits on a point
 * that the drag has moved, return the same anchor translated by that move.
 */
function followPoint(
  anchor: Point3 | undefined,
  moves: readonly SketchPointMove[],
  tolerance: number,
): { position: Point3 | undefined; delta: Point3 | null } {
  if (anchor === undefined) return { position: undefined, delta: null };
  for (const move of moves) {
    if (samePoint(anchor, move.from, tolerance)) {
      return { position: move.to, delta: subtraction(move.to, move.from) };
    }
  }
  return { position: anchor, delta: null };
}

/**
 * The rotation that carries the old measured pair onto the new one. The returned
 * function maps a point to its ROTATED OFFSET from a pivot, so callers can rebase
 * that offset onto the moved anchors instead of onto a fixed origin. Returns null
 * when either pair has no direction, which leaves the caller to fall back to a
 * plain translation.
 */
function rotationWith(
  oldStart: Point3,
  oldEnd: Point3,
  newStart: Point3,
  newEnd: Point3,
): ((point: Point3, pivot: Point3) => Point3) | null {
  const oldX = oldEnd[0] - oldStart[0];
  const oldY = oldEnd[1] - oldStart[1];
  const newX = newEnd[0] - newStart[0];
  const newY = newEnd[1] - newStart[1];
  if (Math.hypot(oldX, oldY) <= 1e-9 || Math.hypot(newX, newY) <= 1e-9) {
    return null;
  }
  const angle = Math.atan2(newY, newX) - Math.atan2(oldY, oldX);
  const cos = Math.cos(angle);
  const sin = Math.sin(angle);
  return (point, pivot) => {
    const dx = point[0] - pivot[0];
    const dy = point[1] - pivot[1];
    return [
      dx * cos - dy * sin,
      dx * sin + dy * cos,
      point[2] - pivot[2],
    ];
  };
}

/** Kinds whose value is NOT the distance between the two anchors. */
const UNMEASURABLE_KINDS = new Set<SketchDimensionScene["kind"]>([
  "angle",
  "line_angle",
  "arc_angle",
  "arc_length",
]);

function anchorDistance(start: Point3, end: Point3) {
  return Math.hypot(end[0] - start[0], end[1] - start[1], end[2] - start[2]);
}

/**
 * Rewrite the label's number, keeping the core's prefix and suffix ("D " and
 * " mm", "°"). The committed label is formatted with two decimals, so matching
 * that keeps the text identical when nothing has moved.
 */
function relabel(dimension: SketchDimensionScene, value: number) {
  const text = value.toFixed(2);
  return dimension.label.replace(/-?\d+(?:\.\d+)?/, text);
}

/**
 * The live value of a dimension the drag is moving.
 *
 * The core only measures on commit, so the label used to keep its pre-drag number
 * until the drag stopped. The UI cannot re-derive every kind's convention (a
 * diameter dimension reads twice its radius, a distance reads its anchor pair), and
 * guessing per kind would print wrong numbers - worse than a stale one. Instead
 * CALIBRATE: the committed label tells us how its number relates to the anchor
 * distance it was drawn from (`rawValue / baseline`), and that ratio is applied to
 * the new distance. The convention is inherited rather than reimplemented.
 *
 * Returns null when the value cannot be measured this way - an angle, an arc
 * length, or a pair with no baseline - and the committed number is left alone.
 */
export function measureFollowedValue(
  dimension: SketchDimensionScene,
  nextAnchorStart: Point3,
  nextAnchorEnd: Point3,
): number | null {
  if (UNMEASURABLE_KINDS.has(dimension.kind)) return null;
  const baseline = anchorDistance(dimension.anchorStart, dimension.anchorEnd);
  if (baseline <= 1e-6) return null;
  if (!Number.isFinite(dimension.rawValue) || dimension.rawValue === 0) return null;
  const current = anchorDistance(nextAnchorStart, nextAnchorEnd);
  return (current / baseline) * dimension.rawValue;
}

/** The label fields to update, or nothing when the value cannot be measured. */
function measuredLabel(
  dimension: SketchDimensionScene,
  nextAnchorStart: Point3,
  nextAnchorEnd: Point3,
): Partial<Pick<SketchDimensionScene, "rawValue" | "label">> {
  const value = measureFollowedValue(dimension, nextAnchorStart, nextAnchorEnd);
  if (value === null) return {};
  return { rawValue: value, label: relabel(dimension, value) };
}

/**
 * Re-anchor one dimension after the geometry it points at has moved.
 * The rule is geometric rather than per-kind. The anchors that still sit on a moved
 * point are carried to that point's new position, and the rest of the dimension —
 * the two dimension-line ends, the label and any reference line or arc centre — is
 * carried by the SAME rotation, pivoting on the anchor pair. So the leader stays
 * parallel to what it measures, its offset distance stays constant, and its own
 * length follows the anchors instead of stuttering as the geometry turns. No
 * per-kind knowledge is needed: a line length, a circle-to-line distance and an
 * angle all re-anchor the same way.
 *
 * Returns `null` when the dimension does not touch any of the moves, so callers
 * can rebuild only the dimensions that actually changed.
 */
export function followDimensionAnchors(
  dimension: SketchDimensionScene,
  moves: readonly SketchPointMove[],
  tolerance: number = DEFAULT_TOLERANCE,
): SketchDimensionScene | null {
  if (moves.length === 0) return null;

  const anchorStart = followPoint(dimension.anchorStart, moves, tolerance);
  const anchorEnd = followPoint(dimension.anchorEnd, moves, tolerance);
  const arcCenter = followPoint(dimension.arcCenter, moves, tolerance);
  const refLineStart = followPoint(dimension.refLineStart, moves, tolerance);
  const refLineEnd = followPoint(dimension.refLineEnd, moves, tolerance);

  const startDelta = anchorStart.delta;
  const endDelta = anchorEnd.delta;
  const touched =
    startDelta !== null ||
    endDelta !== null ||
    arcCenter.delta !== null ||
    refLineStart.delta !== null ||
    refLineEnd.delta !== null;
  if (!touched) return null;

  const nextAnchorStart = anchorStart.position ?? dimension.anchorStart;
  const nextAnchorEnd = anchorEnd.position ?? dimension.anchorEnd;

  // The leader belongs to the measured PAIR, not to world axes: each end sits at a
  // fixed offset from ITS anchor and the dimension line runs parallel to the pair.
  // Translating each end by its own delta preserves neither once the pair rotates —
  // the dimension line stays world-oriented while the geometry turns, so its offset
  // wobbles and its length stutters. Rotate the offsets with the pair instead.
  const rotation =
    startDelta !== null || endDelta !== null
      ? rotationWith(
          dimension.anchorStart,
          dimension.anchorEnd,
          nextAnchorStart,
          nextAnchorEnd,
        )
      : null;

  let dimensionStart: Point3;
  let dimensionEnd: Point3;
  let labelPosition: Point3;
  if (rotation !== null) {
    // Each end keeps its own offset from its own anchor, rotated — so the leader
    // always spans the moved anchors exactly and stays parallel to them.
    dimensionStart = addition(
      nextAnchorStart,
      rotation(dimension.dimensionStart, dimension.anchorStart),
    );
    dimensionEnd = addition(
      nextAnchorEnd,
      rotation(dimension.dimensionEnd, dimension.anchorEnd),
    );
    // The label keeps its offset from the MIDDLE of the leader, so a user-chosen
    // label position survives the drag instead of snapping onto the midpoint.
    const oldMiddle = midpoint(dimension.dimensionStart, dimension.dimensionEnd);
    const newMiddle = midpoint(dimensionStart, dimensionEnd);
    labelPosition = addition(
      newMiddle,
      rotation(dimension.labelPosition, oldMiddle),
    );
  } else {
    // No usable direction (one anchor, or a zero-length pair): plain translation of
    // whichever point moved.
    const move =
      startDelta ?? endDelta ?? arcCenter.delta ?? refLineStart.delta ??
      refLineEnd.delta;
    dimensionStart = addition(dimension.dimensionStart, move as Point3);
    dimensionEnd = addition(dimension.dimensionEnd, move as Point3);
    labelPosition = addition(dimension.labelPosition, move as Point3);
  }

  return {
    ...dimension,
    anchorStart: nextAnchorStart,
    anchorEnd: nextAnchorEnd,
    dimensionStart,
    dimensionEnd,
    labelPosition,
    ...measuredLabel(dimension, nextAnchorStart, nextAnchorEnd),
    ...(dimension.arcCenter !== undefined
      ? { arcCenter: arcCenter.position ?? dimension.arcCenter }
      : {}),
    ...(dimension.refLineStart !== undefined
      ? { refLineStart: refLineStart.position ?? dimension.refLineStart }
      : {}),
    ...(dimension.refLineEnd !== undefined
      ? { refLineEnd: refLineEnd.position ?? dimension.refLineEnd }
      : {}),
  };
}

/** Point positions captured from the last committed scene, by point id. */
export type PointPositionSnapshot = ReadonlyMap<string, Point3>;

/**
 * A drag moves points; the dimensions that point at them keep the anchors they
 * had when the drag began. This is the committed reference they are measured
 * against, so it must be refreshed on every real scene rebuild and NOT while a
 * drag is previewing — otherwise the anchors would follow the drag and there
 * would be nothing left to follow.
 */
export function snapshotPointPositions(
  points: readonly { id: string; position: Point3 }[],
): Map<string, Point3> {
  const snapshot = new Map<string, Point3>();
  for (const point of points) snapshot.set(point.id, point.position);
  return snapshot;
}

/** Every point the drag has moved away from its committed position. */
export function collectDragMoves(
  base: PointPositionSnapshot,
  current: readonly { id: string; position: Point3 }[],
  tolerance: number = DEFAULT_TOLERANCE,
): SketchPointMove[] {
  const moves: SketchPointMove[] = [];
  for (const point of current) {
    const from = base.get(point.id);
    if (from === undefined) continue;
    if (samePoint(from, point.position, tolerance)) continue;
    moves.push({ from, to: point.position });
  }
  return moves;
}

/** Re-anchor every dimension affected by the moves, dropping the untouched. */
export function followDimensionsForMoves(
  dimensions: readonly SketchDimensionScene[],
  moves: readonly SketchPointMove[],
  tolerance: number = DEFAULT_TOLERANCE,
): SketchDimensionScene[] {
  const followed: SketchDimensionScene[] = [];
  for (const dimension of dimensions) {
    const next = followDimensionAnchors(dimension, moves, tolerance);
    if (next !== null) followed.push(next);
  }
  return followed;
}
