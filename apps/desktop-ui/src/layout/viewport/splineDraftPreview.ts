import * as THREE from "three";

import type { SketchPlaneFrame } from "@/types";
import { themeColor, toWorldPoint } from "@/utils";

// Draft preview for the control-point spline tool (click to place
// poles, click the first pole again to commit, Escape to cancel).
//
// The preview draws the ACTUAL clamped open-uniform B-spline of the
// placed poles — the same de Boor math the core uses (spline_math.h),
// so the committed curve looks exactly like the preview. The control
// polygon is drawn as a dashed strip through the poles.

function splineOpenUniformKnots(nPoles: number, degree: number): number[] {
  const interior = nPoles - degree - 1;
  const total = nPoles + degree + 1;
  const knots = new Array<number>(total).fill(0);
  for (let i = degree + 1; i < nPoles; i++) {
    knots[i] = (i - degree) / (interior + 1);
  }
  for (let i = nPoles; i < total; i++) {
    knots[i] = 1;
  }
  return knots;
}

// De Boor evaluation of a clamped B-spline, u in [0, 1].
function splineEval(
  degree: number,
  knots: number[],
  poleXs: number[],
  poleYs: number[],
  u: number,
): [number, number] {
  const n = poleXs.length;
  if (n < 2) {
    return [poleXs[0] ?? 0, poleYs[0] ?? 0];
  }
  if (u <= 0) {
    return [poleXs[0], poleYs[0]];
  }
  if (u >= 1) {
    return [poleXs[n - 1], poleYs[n - 1]];
  }
  let span = n - 1;
  for (let i = 0; i + 1 < knots.length; i++) {
    if (u >= knots[i] && u < knots[i + 1]) {
      span = i;
      break;
    }
  }
  const dx = poleXs.slice();
  const dy = poleYs.slice();
  for (let r = 1; r <= degree; r++) {
    for (let i = span - degree + r; i <= span; i++) {
      const denom = knots[i + degree - r + 1] - knots[i];
      const alpha = denom > 1e-15 ? (u - knots[i]) / denom : 0;
      dx[i] = (1 - alpha) * dx[i - 1] + alpha * dx[i];
      dy[i] = (1 - alpha) * dy[i - 1] + alpha * dy[i];
    }
  }
  return [dx[span], dy[span]];
}

export interface SplineDraftPreviewOptions {
  poles: [number, number][];
  planeId: string;
  planeFrame: SketchPlaneFrame | null;
  isConstruction: boolean;
  // Live cursor position: a dashed rubber segment connects the last
  // placed pole to the mouse until the next click lands.
  cursor: [number, number] | null;
}

// Builds (or refreshes) the preview group. Returns null when fewer
// than 2 poles are placed.
export function buildSplineDraftPreview({
  poles,
  planeId,
  planeFrame,
  isConstruction,
  cursor,
}: SplineDraftPreviewOptions): THREE.Group | null {
  if (poles.length < 1) {
    return null;
  }
  const degree = Math.min(3, poles.length - 1);
  const knots = splineOpenUniformKnots(poles.length, degree);
  const poleXs = poles.map((p) => p[0]);
  const poleYs = poles.map((p) => p[1]);

  const baseColor = themeColor("--color-tertiary-plane-fill", "#fff7c0");
  const group = new THREE.Group();

  // Rubber segment: last pole -> live cursor (dashed, faint).
  if (cursor) {
    const rubberMaterial = new THREE.LineDashedMaterial({
      color: baseColor,
      transparent: true,
      opacity: 0.35,
      dashSize: 0.8,
      gapSize: 0.8,
    });
    rubberMaterial.depthTest = false;
    rubberMaterial.depthWrite = false;
    const last = poles[poles.length - 1];
    const rubberLine = new THREE.Line(
      new THREE.BufferGeometry().setFromPoints([
        new THREE.Vector3(...toWorldPoint(planeId, last, planeFrame)),
        new THREE.Vector3(...toWorldPoint(planeId, cursor, planeFrame)),
      ]),
      rubberMaterial,
    );
    rubberLine.renderOrder = 5;
    rubberLine.computeLineDistances();
    group.add(rubberLine);
  }

  const curveMaterial = isConstruction
    ? new THREE.LineDashedMaterial({
        color: baseColor,
        transparent: true,
        opacity: 0.72,
        dashSize: 1,
        gapSize: 0.6,
      })
    : new THREE.LineBasicMaterial({
        color: baseColor,
        transparent: true,
        opacity: 0.98,
      });
  curveMaterial.depthTest = false;
  curveMaterial.depthWrite = false;
  const samples = 16 * Math.max(1, poles.length - 1);
  const curvePoints: THREE.Vector3[] = [];
  for (let i = 0; i <= samples; i++) {
    const u = i / samples;
    const [x, y] = splineEval(degree, knots, poleXs, poleYs, u);
    curvePoints.push(
      new THREE.Vector3(...toWorldPoint(planeId, [x, y], planeFrame)),
    );
  }
  const curveLine = new THREE.Line(
    new THREE.BufferGeometry().setFromPoints(curvePoints),
    curveMaterial,
  );
  curveLine.renderOrder = 7;
  if (isConstruction) {
    curveLine.computeLineDistances();
  }
  if (poles.length >= 2) {
    group.add(curveLine);
  }

  const poleMaterial = new THREE.LineDashedMaterial({
    color: baseColor,
    transparent: true,
    opacity: 0.5,
    dashSize: 0.8,
    gapSize: 0.8,
  });
  poleMaterial.depthTest = false;
  poleMaterial.depthWrite = false;
  const poleLine = new THREE.Line(
    new THREE.BufferGeometry().setFromPoints(
      poles.map(
        (p) => new THREE.Vector3(...toWorldPoint(planeId, p, planeFrame)),
      ),
    ),
    poleMaterial,
  );
  poleLine.renderOrder = 6;
  poleLine.computeLineDistances();
  group.add(poleLine);
  return group;
}
