import * as THREE from "three";

import type { SketchPlaneFrame } from "@/types";
import { fitSplinePoles, splineEval, splineOpenUniformKnots } from "@/lib/splineCurve";
import { themeColor, toWorldPoint } from "@/utils";

// Draft preview for the spline tool (click to place points, click the first
// point again to commit, Escape to cancel).
//
// The preview draws the ACTUAL clamped open-uniform B-spline the core will
// build, using the shared math in lib/splineCurve.ts (the same De Boor the
// core runs, so the committed curve looks exactly like the preview).
//
// `mode` decides what the placed points mean (T2.1):
//   "control" — the points are the poles; the curve is pulled toward them.
//   "fit"     — the points are fit points; the curve is solved to pass
//               through every one of them, exactly as the core solves it.
// The dashed polygon always follows the placed points.


export interface SplineDraftPreviewOptions {
  poles: [number, number][];
  planeId: string;
  planeFrame: SketchPlaneFrame | null;
  isConstruction: boolean;
  // "control" (default): the points are poles. "fit": the points are fit
  // points and the preview solves the interpolation through them.
  mode?: "control" | "fit";
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
  mode = "control",
  cursor,
}: SplineDraftPreviewOptions): THREE.Group | null {
  if (poles.length < 1) {
    return null;
  }
  const degree = Math.min(3, poles.length - 1);
  // In fit mode the CURVE follows the solved poles (the same solve the core
  // runs); the dashed polygon below always follows the clicked points.
  let curvePoles = poles;
  if (mode === "fit" && poles.length >= 2) {
    try {
      curvePoles = fitSplinePoles(poles, degree);
    } catch {
      // Degenerate point set: keep drawing the polygon; the core reports the
      // refusal on commit.
      curvePoles = poles;
    }
  }
  const knots = splineOpenUniformKnots(curvePoles.length, degree);
  const poleXs = curvePoles.map((p) => p[0]);
  const poleYs = curvePoles.map((p) => p[1]);

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