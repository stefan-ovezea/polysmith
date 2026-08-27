import type {
  SketchArcScene,
  SketchCircleScene,
  SketchEllipseScene,
  TrimPreviewResultEvent,
  ViewportScene,
} from "@/types";
import type { TrimLineHighlightSegment } from "./trimHoverPreview";

type TrimPreviewPayload = NonNullable<TrimPreviewResultEvent["payload"]>;

export interface TrimPreviewHighlightActions {
  clearTrimSegmentHighlight: () => void;
  clearTrimArcHighlight: () => void;
  updateTrimSegmentHighlight: (
    lineId: string,
    segments: TrimLineHighlightSegment[],
    hoveredSegmentIndex: number,
  ) => void;
  updateTrimArcHighlight: (worldPoints: Array<[number, number, number]>) => void;
}

export function renderTrimPreviewHighlight({
  data,
  sceneData,
  actions,
}: {
  data: TrimPreviewPayload | null;
  sceneData: ViewportScene | null;
  actions: TrimPreviewHighlightActions;
}) {
  if (!data) {
    clearTrimHighlights(actions);
    return;
  }

  const hoveredIndex = data.hovered_index;
  if (hoveredIndex == null || hoveredIndex < 0) {
    clearTrimHighlights(actions);
    return;
  }

  if (data.entity_kind === "line") {
    renderLineTrimPreview(data, hoveredIndex, actions);
    return;
  }

  if (!sceneData) {
    return;
  }
  renderCurveTrimPreview(data, hoveredIndex, sceneData, actions);
}

function renderLineTrimPreview(
  data: TrimPreviewPayload,
  hoveredIndex: number,
  actions: TrimPreviewHighlightActions,
) {
  const segment = data.segments?.[hoveredIndex];
  if (!segment?.start || !segment.end) {
    actions.clearTrimSegmentHighlight();
    return;
  }

  actions.clearTrimArcHighlight();
  actions.updateTrimSegmentHighlight(
    data.entity_id,
    [lineHighlightSegmentFromPreview(segment.start, segment.end)],
    0,
  );
}

function lineHighlightSegmentFromPreview(
  start: [number, number],
  end: [number, number],
): TrimLineHighlightSegment {
  return {
    sx: start[0],
    sy: start[1],
    sz: 0,
    ex: end[0],
    ey: end[1],
    ez: 0,
  };
}

function renderCurveTrimPreview(
  data: TrimPreviewPayload,
  hoveredIndex: number,
  sceneData: ViewportScene,
  actions: TrimPreviewHighlightActions,
) {
  if (data.entity_kind === "spline") {
    // Splines highlight as a sub-range of the sampled world polyline.
    const spline = sceneData.sketchSplines.find(
      (s) => s.splineId === data.entity_id,
    );
    if (!spline) {
      actions.clearTrimArcHighlight();
      return;
    }
    const segment = data.segments?.[hoveredIndex];
    let points: Array<[number, number, number]>;
    if (data.full_spline || !segment) {
      points = spline.curvePoints;
    } else {
      const n = spline.curvePoints.length;
      const i0 = Math.max(0, Math.floor((segment.param_start ?? 0) * (n - 1)));
      const i1 = Math.min(n - 1, Math.ceil((segment.param_end ?? 1) * (n - 1)));
      points = spline.curvePoints.slice(i0, i1 + 1);
    }
    if (points.length < 2) {
      actions.clearTrimArcHighlight();
      return;
    }
    actions.clearTrimSegmentHighlight();
    actions.updateTrimArcHighlight(points);
    return;
  }

  const curve = findTrimPreviewCurve(data, sceneData);
  if (!curve) {
    actions.clearTrimArcHighlight();
    return;
  }

  const points = data.full_circle || data.full_arc || data.full_ellipse
    ? sampleFullCurve(curve)
    : sampleCurveSegment(data, hoveredIndex, curve);
  if (!points) {
    actions.clearTrimArcHighlight();
    return;
  }

  actions.clearTrimSegmentHighlight();
  actions.updateTrimArcHighlight(points);
}

type TrimPreviewCurve = SketchCircleScene | SketchArcScene | SketchEllipseScene;

function findTrimPreviewCurve(data: TrimPreviewPayload, sceneData: ViewportScene) {
  if (data.entity_kind === "circle") {
    return sceneData.sketchCircles.find((circle) => circle.circleId === data.entity_id) ?? null;
  }
  if (data.entity_kind === "ellipse") {
    return sceneData.sketchEllipses.find((ellipse) => ellipse.ellipseId === data.entity_id) ?? null;
  }
  return sceneData.sketchArcs.find((arc) => arc.arcId === data.entity_id) ?? null;
}

function sampleFullCurve(curve: TrimPreviewCurve): Array<[number, number, number]> {
  return sampleTrimCurve(curve, 0, 2 * Math.PI);
}

function sampleCurveSegment(
  data: TrimPreviewPayload,
  hoveredIndex: number,
  curve: TrimPreviewCurve,
): Array<[number, number, number]> | null {
  const segment = data.segments?.[hoveredIndex];
  if (segment?.param_start == null || segment.param_end == null) {
    return null;
  }

  // Circles and CCW arcs carry ascending parameter ranges (a wrap
  // segment stores param_end < param_start and renders through +2π).
  // CW arcs carry descending ranges — sampling them ascending draws
  // the complement (the "long arc"), so keep the stored direction.
  const ccw = "ccw" in curve && curve.ccw !== undefined ? curve.ccw : true;
  let end = segment.param_end;
  if (ccw) {
    if (end <= segment.param_start) end += 2 * Math.PI;
  } else if (end >= segment.param_start) {
    end -= 2 * Math.PI;
  }
  return sampleTrimCurve(curve, segment.param_start, end);
}

function sampleTrimCurve(
  curve: TrimPreviewCurve,
  startAngle: number,
  endAngle: number,
): Array<[number, number, number]> {
  const [cx, cy, cz] = curve.center;
  const points: Array<[number, number, number]> = [];
  const isEllipse = "a" in curve;
  const a = isEllipse ? curve.a : curve.radius;
  const b = isEllipse ? curve.b : curve.radius;
  const rotation = isEllipse ? curve.rotation : 0;
  const cu = Math.cos(rotation);
  const su = Math.sin(rotation);
  for (let index = 0; index <= 48; index++) {
    const angle = startAngle + (endAngle - startAngle) * (index / 48);
    points.push([
      cx + a * Math.cos(angle) * cu - b * Math.sin(angle) * su,
      cy + a * Math.cos(angle) * su + b * Math.sin(angle) * cu,
      cz,
    ]);
  }
  return points;
}

function clearTrimHighlights(actions: TrimPreviewHighlightActions) {
  actions.clearTrimSegmentHighlight();
  actions.clearTrimArcHighlight();
}
