import type { SketchFeatureParameters } from "@/types";

export interface SketchChamferCornerPick {
  cornerPointId: string;
  lineAId: string;
  lineBId: string;
}

// Chamfer picking mirrors the fillet's corner rule: the click must
// land on a vertex shared by exactly two non-construction lines, and
// the corner must hold neither an existing chamfer nor an existing
// fillet (the core rejects both combinations).
export function pickSketchChamferCorner({
  sketch,
  localPoint,
  tolerance = 0.05,
}: {
  sketch: SketchFeatureParameters;
  localPoint: readonly [number, number];
  tolerance?: number;
}): SketchChamferCornerPick | null {
  const cornerPoint = sketch.vertices.find(
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

  const sharesCorner = (lineAId: string, lineBId: string, trimAId: string, trimBId: string) =>
    incidentLines.some(
      (line) =>
        (lineAId === line.line_id || lineBId === line.line_id) &&
        (trimAId === cornerPoint.vertex_id ||
          trimBId === cornerPoint.vertex_id),
    );
  const alreadyChamfered = (sketch.chamfers ?? []).some((chamfer) =>
    sharesCorner(
      chamfer.line_a_id,
      chamfer.line_b_id,
      chamfer.trim_a_vertex_id,
      chamfer.trim_b_vertex_id,
    ),
  );
  if (alreadyChamfered) {
    return null;
  }
  const alreadyFilleted = (sketch.fillets ?? []).some((fillet) =>
    sharesCorner(
      fillet.line_a_id,
      fillet.line_b_id,
      fillet.trim_a_vertex_id,
      fillet.trim_b_vertex_id,
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

export function handleSketchChamferClick({
  sketch,
  localPoint,
  addSketchChamfer,
}: {
  sketch: SketchFeatureParameters | null;
  localPoint: readonly [number, number] | null;
  // The distances live in the App-side chamfer session (mirrors the
  // fillet flow where the radius lives in the session too).
  addSketchChamfer: (
    cornerPointId: string,
    lineAId: string,
    lineBId: string,
  ) => Promise<void>;
}) {
  if (!sketch || !localPoint) {
    return true;
  }

  const pick = pickSketchChamferCorner({
    sketch,
    localPoint,
  });
  if (!pick) {
    return true;
  }

  void addSketchChamfer(
    pick.cornerPointId,
    pick.lineAId,
    pick.lineBId,
  );
  return true;
}
