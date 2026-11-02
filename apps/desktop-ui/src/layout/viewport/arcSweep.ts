// Which of the two arcs through a pair of endpoints to draw.
//
// Kept dependency-free so it can be unit-tested: the rule is the whole fix for
// the "trimmed arc renders its complement mid-drag" defect, and it is worth
// pinning independently of THREE and the scene graph.

/**
 * Pick the sweep to draw from an arc's two endpoint angles.
 *
 * Two points always admit two arcs, and `ccw` alone does not choose between
 * them: normalizing the angle gap in the `ccw` direction yields whichever of
 * the two happens to be shorter in that direction. During a drag the endpoints
 * move, so when their angular order swaps the normalized sweep silently
 * becomes the complement — the "the deleted arc comes back while dragging"
 * report on trimmed arcs.
 *
 * `previousSweep` (the span the arc was last drawn with) resolves it: the
 * candidate closest to it in *magnitude* is the same shape, whichever way
 * round it is traversed. Magnitude rather than sign, because the complement is
 * the same point set walked the other way.
 *
 * @param startAngle angle of the start endpoint, radians
 * @param endAngle   angle of the end endpoint, radians
 * @param ccw        the arc's stored direction
 * @param previousSweep signed span the arc was last drawn with, if known
 */
export function chooseArcSweep({
  startAngle,
  endAngle,
  ccw,
  previousSweep,
}: {
  startAngle: number;
  endAngle: number;
  ccw: boolean;
  previousSweep?: number;
}): number {
  let sweep = endAngle - startAngle;
  if (ccw) {
    while (sweep <= 0) sweep += Math.PI * 2;
  } else {
    while (sweep >= 0) sweep -= Math.PI * 2;
  }
  if (previousSweep === undefined) return sweep;
  // The other arc through the same endpoints: the same points walked the
  // other way round, i.e. the gap shifted by a full turn in that direction.
  const other = ccw ? sweep - Math.PI * 2 : sweep + Math.PI * 2;
  return Math.abs(Math.abs(other) - Math.abs(previousSweep)) <
    Math.abs(Math.abs(sweep) - Math.abs(previousSweep))
    ? other
    : sweep;
}
