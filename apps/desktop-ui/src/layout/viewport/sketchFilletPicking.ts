import type { SketchFeatureParameters } from "@/types";

export interface SketchFilletCornerPick {
  cornerPointId: string;
  lineAId: string;
  lineBId: string;
}

export function pickSketchFilletCorner({
  sketch,
  localPoint,
  tolerance = 0.05,
}: {
  sketch: SketchFeatureParameters;
  localPoint: readonly [number, number];
  tolerance?: number;
}): SketchFilletCornerPick | null {
  const cornerPoint = sketch.points.find(
    (point) =>
      Math.hypot(point.x - localPoint[0], point.y - localPoint[1]) <= tolerance,
  );
  if (!cornerPoint) {
    return null;
  }

  const incidentLines = sketch.lines.filter(
    (line) =>
      !line.is_construction &&
      (line.start_vertex_id === cornerPoint.vertex_id ||
        line.end_vertex_id === cornerPoint.vertex_id),
  );
  if (incidentLines.length !== 2) {
    return null;
  }

  const alreadyFilleted = (sketch.fillets ?? []).some((fillet) =>
    incidentLines.some(
      (line) =>
        (fillet.line_a_id === line.line_id ||
          fillet.line_b_id === line.line_id) &&
        (fillet.trim_a_point_id === cornerPoint.vertex_id ||
          fillet.trim_b_point_id === cornerPoint.vertex_id),
    ),
  );
  if (alreadyFilleted) {
    return null;
  }

  return {
    cornerPointId: cornerPoint.vertex_id,
    lineAId: incidentLines[0].line_id,
    lineBId: incidentLines[1].line_id,
  };
}

export function handleSketchFilletClick({
  sketch,
  localPoint,
  addSketchFillet,
}: {
  sketch: SketchFeatureParameters | null;
  localPoint: readonly [number, number] | null;
  addSketchFillet: (
    cornerPointId: string,
    lineAId: string,
    lineBId: string,
  ) => Promise<void>;
}) {
  if (!sketch || !localPoint) {
    return true;
  }

  const pick = pickSketchFilletCorner({
    sketch,
    localPoint,
  });
  if (!pick) {
    return true;
  }

  void addSketchFillet(
    pick.cornerPointId,
    pick.lineAId,
    pick.lineBId,
  );
  return true;
}
