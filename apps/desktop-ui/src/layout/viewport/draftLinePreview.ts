import * as THREE from "three";
import type { SketchPlaneFrame } from "@/types";
import { themeColor, toWorldPoint } from "@/utils";

export function makeDraftLineMaterial(isConstruction: boolean) {
  if (isConstruction) {
    return new THREE.LineDashedMaterial({
      color: themeColor("--color-tertiary-plane-edge", "#ffe784"),
      transparent: true,
      opacity: 0.72,
      dashSize: 1,
      gapSize: 0.6,
    });
  }
  return new THREE.LineBasicMaterial({
    color: themeColor("--color-tertiary-plane-edge", "#ffe784"),
    transparent: true,
    opacity: 0.88,
  });
}

export function buildDraftLinePreview({
  points,
  planeId,
  planeFrame,
  isConstruction,
}: {
  points: Array<[number, number]>;
  planeId: string;
  planeFrame: SketchPlaneFrame | null;
  isConstruction: boolean;
}) {
  const preview = new THREE.Line(
    new THREE.BufferGeometry().setFromPoints(
      points.map(
        (point) => new THREE.Vector3(...toWorldPoint(planeId, point, planeFrame)),
      ),
    ),
    makeDraftLineMaterial(isConstruction),
  );
  if (isConstruction) {
    preview.computeLineDistances();
  }
  return preview;
}

export function buildDashedDraftHint({
  start,
  end,
  planeId,
  planeFrame,
}: {
  start: [number, number];
  end: [number, number];
  planeId: string;
  planeFrame: SketchPlaneFrame | null;
}) {
  const preview = new THREE.Line(
    new THREE.BufferGeometry().setFromPoints([
      new THREE.Vector3(...toWorldPoint(planeId, start, planeFrame)),
      new THREE.Vector3(...toWorldPoint(planeId, end, planeFrame)),
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
  return preview;
}
