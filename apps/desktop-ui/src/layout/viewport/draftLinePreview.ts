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

/**
 * Build inference guide lines that show alignment with existing sketch
 * vertices. Each guide is a dotted line from an existing vertex to the
 * projected draft position (inference line / tracking guide).
 */
export function buildInferenceGuideLines({
  guides,
  planeId,
  planeFrame,
}: {
  guides: Array<{ from: [number, number]; draft: [number, number] }>;
  planeId: string;
  planeFrame: SketchPlaneFrame | null;
}): THREE.Line[] {
  // One material for the whole batch: the guides are one visual element, and a
  // material per line meant the cleanup disposed the same material once per
  // guide.
  //
  // The guide lies exactly ON the sketch plane, like the geometry it tracks, so
  // depth testing it against the plane/grid is a z-fight: the dashes win on some
  // frames and lose on others, which reads as "it flickers and is almost
  // invisible". It is a hint overlay, so it draws over the plane instead
  // (depthTest false, renderOrder above the sketch lines at 7).
  const material = new THREE.LineDashedMaterial({
    color: themeColor("--color-tertiary-plane-edge", "#ffe784"),
    transparent: true,
    // 0.42 was too faint to read against the plane on a dark theme.
    opacity: 0.9,
    dashSize: 1.1,
    gapSize: 0.7,
    depthTest: false,
    depthWrite: false,
  });
  return guides.map((guide) => {
    const line = new THREE.Line(
      new THREE.BufferGeometry().setFromPoints([
        new THREE.Vector3(...toWorldPoint(planeId, guide.from, planeFrame)),
        new THREE.Vector3(...toWorldPoint(planeId, guide.draft, planeFrame)),
      ]),
      material,
    );
    line.renderOrder = 8;
    line.computeLineDistances();
    return line;
  });
}
