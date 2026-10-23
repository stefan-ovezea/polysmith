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
  | "three_point"
  | "rounded";

// Corner-arc tessellation for the rounded draft outline. A fixed segment
// count per corner reads as a faceted polygon once the radius is large (or
// once the user zooms in), so scale it with the radius from a chord-sagitta
// bound — the same idea the committed profile's boundary sampler uses.
// The draft is only a rubber band (the core builds the real fillet arcs).
const ROUNDED_PREVIEW_SAGITTA_MM = 0.01;
const ROUNDED_PREVIEW_MIN_SEGMENTS = 8;
const ROUNDED_PREVIEW_MAX_SEGMENTS = 64;

/** Segments per 90° corner for the draft outline at `radius` mm. */
export function roundedPreviewCornerSegments(radius: number): number {
  if (!(radius > 0)) {
    return ROUNDED_PREVIEW_MIN_SEGMENTS;
  }
  const ratio = ROUNDED_PREVIEW_SAGITTA_MM / radius;
  if (ratio >= 1) {
    return ROUNDED_PREVIEW_MIN_SEGMENTS;
  }
  // Largest angle whose chord stays within the sagitta bound.
  const maxStep = 2 * Math.acos(1 - ratio);
  const segments = Math.ceil(Math.PI / 2 / maxStep);
  return Math.max(
    ROUNDED_PREVIEW_MIN_SEGMENTS,
    Math.min(ROUNDED_PREVIEW_MAX_SEGMENTS, segments),
  );
}

/**
 * Closed outline of an axis-aligned rectangle whose four corners are
 * rounded with `radius`. The radius is clamped just below half the
 * shorter side so the draft keeps drawing while the user drags out a
 * rectangle too small for the current radius (the core refuses that
 * commit with a message; the preview should not vanish first).
 */
export function roundedRectangleOutline2d(
  start: [number, number],
  current: [number, number],
  radius: number,
): Array<[number, number]> {
  const minX = Math.min(start[0], current[0]);
  const maxX = Math.max(start[0], current[0]);
  const minY = Math.min(start[1], current[1]);
  const maxY = Math.max(start[1], current[1]);
  const clamped = Math.min(
    radius,
    Math.min(maxX - minX, maxY - minY) / 2 - 1e-6,
  );
  if (!(clamped > 1e-6)) {
    return axisAlignedRectangleCorners2d("corner_corner", start, current);
  }

  const cornerSegments = roundedPreviewCornerSegments(clamped);
  const points: Array<[number, number]> = [[minX + clamped, minY]];
  const corner = (
    centerX: number,
    centerY: number,
    fromAngle: number,
    toAngle: number,
  ) => {
    for (let index = 1; index <= cornerSegments; index++) {
      const angle =
        fromAngle +
        ((toAngle - fromAngle) * index) / cornerSegments;
      points.push([
        centerX + Math.cos(angle) * clamped,
        centerY + Math.sin(angle) * clamped,
      ]);
    }
  };
  // Counter-clockwise from the bottom edge: each corner is a quarter arc
  // inside the corner it replaces.
  corner(maxX - clamped, minY + clamped, -Math.PI / 2, 0);
  corner(maxX - clamped, maxY - clamped, 0, Math.PI / 2);
  corner(minX + clamped, maxY - clamped, Math.PI / 2, Math.PI);
  corner(minX + clamped, minY + clamped, Math.PI, (3 * Math.PI) / 2);
  return points;
}

interface RectangleDraftPreviewOptions {
  mode: RectangleToolMode;
  start: [number, number];
  current: [number, number];
  secondPoint: [number, number] | null;
  planeId: string;
  planeFrame: SketchPlaneFrame | null;
  isConstruction: boolean;
  /** Corner radius for mode "rounded" (mm). */
  cornerRadius?: number;
}

export function buildRectangleDraftPreview({
  mode,
  start,
  current,
  secondPoint,
  planeId,
  planeFrame,
  isConstruction,
  cornerRadius = 0,
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

  if (mode === "rounded") {
    return buildDraftLinePreview({
      points: roundedRectangleOutline2d(start, current, cornerRadius),
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
