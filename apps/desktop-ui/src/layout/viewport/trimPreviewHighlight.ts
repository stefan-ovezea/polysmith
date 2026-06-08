import type { TrimPreviewResultEvent, ViewportScene } from "@/types";
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
  const curve = findTrimPreviewCurve(data, sceneData);
  if (!curve) {
    actions.clearTrimArcHighlight();
    return;
  }

  const points = data.full_circle || data.full_arc
    ? sampleFullCurve(curve.center, curve.radius)
    : sampleCurveSegment(data, hoveredIndex, curve.center, curve.radius);
  if (!points) {
    actions.clearTrimArcHighlight();
    return;
  }

  actions.clearTrimSegmentHighlight();
  actions.updateTrimArcHighlight(points);
}

function findTrimPreviewCurve(data: TrimPreviewPayload, sceneData: ViewportScene) {
  if (data.entity_kind === "circle") {
    return sceneData.sketchCircles.find((circle) => circle.circleId === data.entity_id) ?? null;
  }
  return sceneData.sketchArcs.find((arc) => arc.arcId === data.entity_id) ?? null;
}

function sampleFullCurve(
  center: [number, number, number],
  radius: number,
): Array<[number, number, number]> {
  return sampleCurveAngles(center, radius, 0, 2 * Math.PI);
}

function sampleCurveSegment(
  data: TrimPreviewPayload,
  hoveredIndex: number,
  center: [number, number, number],
  radius: number,
): Array<[number, number, number]> | null {
  const segment = data.segments?.[hoveredIndex];
  if (segment?.param_start == null || segment.param_end == null) {
    return null;
  }

  const end = segment.param_end <= segment.param_start
    ? segment.param_end + 2 * Math.PI
    : segment.param_end;
  return sampleCurveAngles(center, radius, segment.param_start, end);
}

function sampleCurveAngles(
  center: [number, number, number],
  radius: number,
  startAngle: number,
  endAngle: number,
): Array<[number, number, number]> {
  const [cx, cy, cz] = center;
  const points: Array<[number, number, number]> = [];
  for (let index = 0; index <= 48; index++) {
    const angle = startAngle + (endAngle - startAngle) * (index / 48);
    points.push([
      cx + Math.cos(angle) * radius,
      cy + Math.sin(angle) * radius,
      cz,
    ]);
  }
  return points;
}

function clearTrimHighlights(actions: TrimPreviewHighlightActions) {
  actions.clearTrimSegmentHighlight();
  actions.clearTrimArcHighlight();
}
