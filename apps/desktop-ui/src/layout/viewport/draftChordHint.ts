import * as THREE from "three";

import type { SketchPlaneFrame } from "@/types";
import { themeColor, toWorldPoint } from "@/utils";

export interface DraftChordHintOptions {
  start: [number, number];
  current: [number, number];
  planeId: string;
  planeFrame: SketchPlaneFrame | null;
}

export function buildDraftChordHint({
  start,
  current,
  planeId,
  planeFrame,
}: DraftChordHintOptions) {
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
  return preview;
}
