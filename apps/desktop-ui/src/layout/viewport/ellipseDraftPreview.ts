import type { SketchPlaneFrame } from "@/types";
import { buildSketchEllipseObject, toWorldPoint } from "@/utils";

// Draft preview for the ellipse tool (3 clicks: center → major-axis
// point → minor-axis point). Stage 1 (no axis point yet) shows a
// circle with the drag radius — the axis direction isn't locked.
// Stage 2 shows the real ellipse: `a` = center→axis-point distance
// with its rotation, `b` = perpendicular distance of the cursor from
// the major axis.
export interface EllipseDraftPreviewOptions {
  start: [number, number];
  current: [number, number];
  axisPoint: [number, number] | null;
  planeId: string;
  planeFrame: SketchPlaneFrame | null;
  isConstruction: boolean;
}

export function buildEllipseDraftPreview({
  start,
  current,
  axisPoint,
  planeId,
  planeFrame,
  isConstruction,
}: EllipseDraftPreviewOptions) {
  const dx = current[0] - start[0];
  const dy = current[1] - start[1];
  if (!axisPoint) {
    const radius = Math.hypot(dx, dy);
    if (radius <= 0.001) {
      return null;
    }
    return buildSketchEllipseObject(
      {
        ellipseId: "preview-ellipse",
        planeId,
        planeFrame,
        center: toWorldPoint(planeId, start, planeFrame),
        a: radius,
        b: radius,
        rotation: 0,
        isSelected: false,
        isConstruction,
        isPreview: true,
        generatedBy: null,
      },
      planeFrame,
    );
  }

  const ax = axisPoint[0] - start[0];
  const ay = axisPoint[1] - start[1];
  const a = Math.hypot(ax, ay);
  if (a <= 0.001) {
    return null;
  }
  const rotation = Math.atan2(ay, ax);
  // Perpendicular distance of the cursor from the major axis.
  const b = Math.abs((ax * dy - ay * dx) / a);
  if (b <= 0.001) {
    return null;
  }
  return buildSketchEllipseObject(
    {
      ellipseId: "preview-ellipse",
      planeId,
      planeFrame,
      center: toWorldPoint(planeId, start, planeFrame),
      a,
      b,
      rotation,
      isSelected: false,
      isConstruction,
      isPreview: true,
      generatedBy: null,
    },
    planeFrame,
  );
}
