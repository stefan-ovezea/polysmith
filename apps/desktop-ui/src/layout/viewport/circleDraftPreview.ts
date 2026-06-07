import * as THREE from "three";
import type { SketchPlaneFrame } from "@/types";
import {
  buildSketchCircleObject,
  circleFromThreePoints2d,
  distanceBetweenPoints,
  themeColor,
  toWorldPoint,
} from "@/utils";

export type CircleToolMode =
  | "center_radius"
  | "two_point"
  | "three_point"
  | "tangent_two_lines"
  | "tangent_three_lines";

export type CircleDraftPreview =
  | {
      kind: "hint";
      object: THREE.Line;
      renderDraftDimension: false;
    }
  | {
      kind: "circle";
      object: THREE.LineLoop;
      renderDraftDimension: boolean;
    };

interface CircleDraftPreviewOptions {
  mode: CircleToolMode;
  start: [number, number];
  current: [number, number];
  secondPoint: [number, number] | null;
  planeId: string;
  planeFrame: SketchPlaneFrame | null;
  isConstruction: boolean;
}

function buildDashedChordHint({
  start,
  current,
  planeId,
  planeFrame,
}: Pick<
  CircleDraftPreviewOptions,
  "start" | "current" | "planeId" | "planeFrame"
>): CircleDraftPreview {
  const preview = new THREE.Line(
    new THREE.BufferGeometry().setFromPoints([
      new THREE.Vector3(...toWorldPoint(planeId, start, planeFrame)),
      new THREE.Vector3(...toWorldPoint(planeId, current, planeFrame)),
    ]),
    new THREE.LineDashedMaterial({
      color: themeColor("--color-tertiary-plane-edge", "#ffe784"),
      transparent: true,
      opacity: 0.65,
      dashSize: 1,
      gapSize: 0.6,
    }),
  );
  preview.computeLineDistances();
  return { kind: "hint", object: preview, renderDraftDimension: false };
}

function buildCircleObject({
  circleId,
  center,
  radius,
  planeId,
  planeFrame,
  isConstruction,
  isPreview,
  renderDraftDimension,
}: {
  circleId: string;
  center: [number, number];
  radius: number;
  planeId: string;
  planeFrame: SketchPlaneFrame | null;
  isConstruction: boolean;
  isPreview: boolean;
  renderDraftDimension: boolean;
}): CircleDraftPreview | null {
  if (radius <= 0.001) {
    return null;
  }
  const object = buildSketchCircleObject(
    {
      circleId,
      planeId,
      planeFrame,
      center: toWorldPoint(planeId, center, planeFrame),
      radius,
      isSelected: false,
      isConstruction,
      isPreview,
      isProjected: false,
    },
    planeFrame,
  );
  return { kind: "circle", object, renderDraftDimension };
}

export function buildCircleDraftPreview({
  mode,
  start,
  current,
  secondPoint,
  planeId,
  planeFrame,
  isConstruction,
}: CircleDraftPreviewOptions): CircleDraftPreview | null {
  if (mode === "two_point") {
    const dist = distanceBetweenPoints(start, current);
    return buildCircleObject({
      circleId: "preview-2pt-circle",
      center: [(start[0] + current[0]) / 2, (start[1] + current[1]) / 2],
      radius: dist / 2,
      planeId,
      planeFrame,
      isConstruction,
      isPreview: false,
      renderDraftDimension: false,
    });
  }

  if (mode === "three_point") {
    if (!secondPoint) {
      return buildDashedChordHint({ start, current, planeId, planeFrame });
    }
    const circle = circleFromThreePoints2d(start, secondPoint, current);
    if (!circle) {
      return null;
    }
    return buildCircleObject({
      circleId: "preview-3pt-circle",
      center: circle.center,
      radius: circle.radius,
      planeId,
      planeFrame,
      isConstruction,
      isPreview: true,
      renderDraftDimension: false,
    });
  }

  if (
    mode === "center_radius" ||
    mode === "tangent_two_lines" ||
    mode === "tangent_three_lines"
  ) {
    return buildCircleObject({
      circleId: "preview-circle",
      center: start,
      radius: distanceBetweenPoints(start, current),
      planeId,
      planeFrame,
      isConstruction,
      isPreview: false,
      renderDraftDimension: true,
    });
  }

  return null;
}
