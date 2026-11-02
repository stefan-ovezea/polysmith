import type { SketchPreviewPoint } from "@/types";

export type ConstraintPreviewKind =
  | "midpoint"
  | "perpendicular"
  | "on_line"
  | "on_circle"
  | "horizontal"
  | "vertical"
  | "tangent"
  | "endpoint"
  | "parallel";

export interface ConstraintPreviewState {
  kind: ConstraintPreviewKind;
  x: number;
  y: number;
}

export function constraintPreviewFromSnap(
  sketchPoint: SketchPreviewPoint,
  x: number,
  y: number,
): ConstraintPreviewState | null {
  if (sketchPoint.snapMidpointHostLineId) {
    const isWhole =
      sketchPoint.snapMidpointT !== null &&
      sketchPoint.snapMidpointT !== undefined &&
      Math.abs(sketchPoint.snapMidpointT - 0.5) < 1e-9;
    return { kind: isWhole ? "midpoint" : "on_line", x, y };
  }
  if (sketchPoint.snapEndpointHostLineId) {
    return { kind: "endpoint", x, y };
  }
  if (sketchPoint.snapLineBodyHostLineId) {
    return { kind: "on_line", x, y };
  }
  if (sketchPoint.snapCircleBodyHostCircleId) {
    return { kind: "on_circle", x, y };
  }
  if (sketchPoint.snapPerpendicularHostLineId) {
    return { kind: "perpendicular", x, y };
  }
  if (sketchPoint.snapParallelHostLineId) {
    return { kind: "parallel", x, y };
  }
  if (sketchPoint.snapLabel?.startsWith("On ")) {
    return { kind: "on_line", x, y };
  }
  if (sketchPoint.snapAxisLock) {
    return { kind: sketchPoint.snapAxisLock, x, y };
  }
  if (sketchPoint.snapTangentCircleId) {
    return { kind: "tangent", x, y };
  }
  return null;
}
