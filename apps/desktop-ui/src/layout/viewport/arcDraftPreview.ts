import * as THREE from "three";
import type { SketchPlaneFrame } from "@/types";
import {
  buildSketchArcObject,
  buildSketchCircleObject,
  toWorldPoint,
} from "@/utils";
import { buildDraftChordHint } from "./draftChordHint";

export type ArcToolMode = "three_point" | "center_start_end";

interface ArcDraftPreviewOptions {
  mode: ArcToolMode;
  start: [number, number];
  current: [number, number];
  secondPoint: [number, number] | null;
  planeId: string;
  planeFrame: SketchPlaneFrame | null;
  isConstruction: boolean;
}

function buildArcPreview({
  centerLocal,
  radius,
  startLocal,
  endLocal,
  ccw,
  planeId,
  planeFrame,
  isConstruction,
}: {
  centerLocal: [number, number];
  radius: number;
  startLocal: [number, number];
  endLocal: [number, number];
  ccw: boolean;
  planeId: string;
  planeFrame: SketchPlaneFrame | null;
  isConstruction: boolean;
}) {
  if (radius < 1e-3) {
    return null;
  }
  return buildSketchArcObject(
    {
      arcId: "preview-arc",
      startPointId: "preview-arc-start",
      endPointId: "preview-arc-end",
      planeId,
      planeFrame,
      center: toWorldPoint(planeId, centerLocal, planeFrame),
      radius,
      start: toWorldPoint(planeId, startLocal, planeFrame),
      end: toWorldPoint(planeId, endLocal, planeFrame),
      ccw,
      isSelected: false,
      isConstruction,
      isPreview: true,
      isProjected: false,
      generatedBy: null,
    },
    planeFrame,
  );
}

function buildArcRadiusCircle({
  start,
  current,
  planeId,
  planeFrame,
  isConstruction,
}: Pick<
  ArcDraftPreviewOptions,
  "start" | "current" | "planeId" | "planeFrame" | "isConstruction"
>) {
  const radius = Math.hypot(current[0] - start[0], current[1] - start[1]);
  if (radius < 1e-3) {
    return null;
  }
  return buildSketchCircleObject(
    {
      circleId: "preview-arc-circle",
      planeId,
      planeFrame,
      center: toWorldPoint(planeId, start, planeFrame),
      radius,
      isSelected: false,
      isConstruction,
      isPreview: true,
      isProjected: false,
      generatedBy: null,
    },
    planeFrame,
  ) as unknown as THREE.Line;
}

function buildThreePointArcPreview({
  start,
  current,
  secondPoint,
  planeId,
  planeFrame,
  isConstruction,
}: Omit<ArcDraftPreviewOptions, "mode">) {
  if (!secondPoint) {
    return buildDraftChordHint({ start, current, planeId, planeFrame });
  }

  const [sx, sy] = start;
  const [ex, ey] = secondPoint;
  const [ax, ay] = current;
  const d = 2 * (sx * (ey - ay) + ex * (ay - sy) + ax * (sy - ey));
  if (Math.abs(d) <= 1e-9) {
    return null;
  }

  const s2 = sx * sx + sy * sy;
  const e2 = ex * ex + ey * ey;
  const a2 = ax * ax + ay * ay;
  const cx = (s2 * (ey - ay) + e2 * (ay - sy) + a2 * (sy - ey)) / d;
  const cy = (s2 * (ax - ex) + e2 * (sx - ax) + a2 * (ex - sx)) / d;
  const radius = Math.hypot(sx - cx, sy - cy);
  const cross = (ax - sx) * (ey - sy) - (ay - sy) * (ex - sx);

  return buildArcPreview({
    centerLocal: [cx, cy],
    radius,
    startLocal: [sx, sy],
    endLocal: [ex, ey],
    ccw: cross > 0,
    planeId,
    planeFrame,
    isConstruction,
  });
}

function buildCenterStartEndArcPreview({
  start,
  current,
  secondPoint,
  planeId,
  planeFrame,
  isConstruction,
}: Omit<ArcDraftPreviewOptions, "mode">) {
  if (!secondPoint) {
    return buildArcRadiusCircle({
      start,
      current,
      planeId,
      planeFrame,
      isConstruction,
    });
  }

  const [cx, cy] = start;
  const [sx, sy] = secondPoint;
  const radius = Math.hypot(sx - cx, sy - cy);
  const endDx = current[0] - cx;
  const endDy = current[1] - cy;
  const endLen = Math.hypot(endDx, endDy);
  if (radius < 1e-3 || endLen < 1e-3) {
    return null;
  }

  const finalEx = cx + (endDx * radius) / endLen;
  const finalEy = cy + (endDy * radius) / endLen;
  const cross = (sx - cx) * (finalEy - cy) - (sy - cy) * (finalEx - cx);

  return buildArcPreview({
    centerLocal: [cx, cy],
    radius,
    startLocal: [sx, sy],
    endLocal: [finalEx, finalEy],
    ccw: cross > 0,
    planeId,
    planeFrame,
    isConstruction,
  });
}

export function buildArcDraftPreview(options: ArcDraftPreviewOptions) {
  if (options.mode === "three_point") {
    return buildThreePointArcPreview(options);
  }
  return buildCenterStartEndArcPreview(options);
}
