import type { SketchPlaneFrame } from "@/types";
import {
  axisAlignedRectangleCorners2d,
  rectangleFromThreePoints2d,
} from "@/utils";
import {
  buildDashedDraftHint,
  buildDraftLinePreview,
} from "./draftLinePreview";

export type RectangleToolMode =
  | "corner_corner"
  | "center_point"
  | "three_point";

interface RectangleDraftPreviewOptions {
  mode: RectangleToolMode;
  start: [number, number];
  current: [number, number];
  secondPoint: [number, number] | null;
  planeId: string;
  planeFrame: SketchPlaneFrame | null;
  isConstruction: boolean;
}

export function buildRectangleDraftPreview({
  mode,
  start,
  current,
  secondPoint,
  planeId,
  planeFrame,
  isConstruction,
}: RectangleDraftPreviewOptions) {
  if (mode === "three_point") {
    if (!secondPoint) {
      return buildDashedDraftHint({
        start,
        end: current,
        planeId,
        planeFrame,
      });
    }
    const rectangle = rectangleFromThreePoints2d(start, secondPoint, current);
    if (!rectangle) {
      return null;
    }
    return buildDraftLinePreview({
      points: rectangle.closedCorners,
      planeId,
      planeFrame,
      isConstruction,
    });
  }

  return buildDraftLinePreview({
    points: axisAlignedRectangleCorners2d(mode, start, current),
    planeId,
    planeFrame,
    isConstruction,
  });
}
