// Corner-trim hover preview rendering.
//
// The core's corner_trim_preview_result carries the two resulting
// segments in SKETCH-LOCAL coordinates (same convention as the trim
// preview — the sketch group carries the plane transform, so children
// live in sketch-local space). The renderer samples arc segments and
// hands the polyline points to the viewport's corner-preview actions.

import type { CornerTrimPreviewResultEvent } from "@/types";

type CornerPreviewPayload = NonNullable<
  CornerTrimPreviewResultEvent["payload"]
>;

export interface CornerPreviewActions {
  clearCornerPreview: () => void;
  updateCornerPreview: (
    aPoints: Array<[number, number, number]>,
    bPoints: Array<[number, number, number]>,
    corner: [number, number, number],
  ) => void;
}

export function renderCornerTrimPreview({
  data,
  actions,
}: {
  data: CornerPreviewPayload | null;
  actions: CornerPreviewActions;
}) {
  if (!data || !data.valid || !data.a || !data.b || !data.corner) {
    actions.clearCornerPreview();
    return;
  }
  actions.updateCornerPreview(
    sampleSegment(data.a),
    sampleSegment(data.b),
    [data.corner[0], data.corner[1], 0],
  );
}

function sampleSegment(
  seg: NonNullable<CornerPreviewPayload["a"]>,
): Array<[number, number, number]> {
  if (seg.kind === "line") {
    return [
      [seg.start[0], seg.start[1], 0],
      [seg.end[0], seg.end[1], 0],
    ];
  }
  // Arc: sample along the circle from the start angle to the end
  // angle in the arc's sweep direction.
  const [cx, cy] = seg.center ?? [0, 0];
  const r = seg.radius ?? 1;
  const s = Math.atan2(seg.start[1] - cy, seg.start[0] - cx);
  let e = Math.atan2(seg.end[1] - cy, seg.end[0] - cx);
  if (seg.ccw) {
    while (e <= s) e += 2 * Math.PI;
  } else {
    while (e >= s) e -= 2 * Math.PI;
  }
  const n = 24;
  const points: Array<[number, number, number]> = [];
  for (let i = 0; i <= n; ++i) {
    const t = s + ((e - s) * i) / n;
    points.push([cx + r * Math.cos(t), cy + r * Math.sin(t), 0]);
  }
  return points;
}
