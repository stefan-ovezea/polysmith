import * as THREE from "three";

import { buildDynamicGrid, type GridPlaneFrame } from "@/layout/viewport/grid";
import type { GcodeBounds, GcodeFileInfo, GcodeMove } from "@/lib/grblClient";
import { disposeMaterial } from "@/utils";
import { themeColor } from "@/utils/viewport/themeColor";

// Pure THREE builders for the GRBL toolpath preview.  Everything reads
// theme tokens at build time (themeColor) — no hardcoded colors, no
// React.  Machine coordinates are used directly: PolySmith posts are
// already WCS-shifted, so toolpaths, the bed, and GRBL's MPos all share
// one space and overlay 1:1.

// ── Bed ─────────────────────────────────────────────────────────────

export interface GrblBed {
  widthMm: number;
  heightMm: number;
}

// Matches the LaserGRBL machine seed (430×430); used when the document
// has no machine settings.
export const DEFAULT_GRBL_BED: GrblBed = { widthMm: 430, heightMm: 430 };

// The homed corner is (0,0); the bed spans (0,0) → (width,height).
const GRID_PADDING = 1.0; // extra bed-widths of grid around the bed
const GRID_SPACING_MM = 10;

const XY_FRAME: GridPlaneFrame = {
  origin: new THREE.Vector3(0, 0, 0),
  xAxis: new THREE.Vector3(1, 0, 0),
  yAxis: new THREE.Vector3(0, 1, 0),
  normal: new THREE.Vector3(0, 0, 1),
};

export function buildGrblBedGroup(
  bed: GrblBed,
  toolpathBounds: GcodeBounds | null = null,
): THREE.Group {
  const group = new THREE.Group();

  const minor = new THREE.Color(themeColor("--color-cad-grid", "#5a5a5c"));
  const major = new THREE.Color(
    themeColor("--color-cad-grid-axis", "#7a7a7c"),
  );
  const pad = Math.max(bed.widthMm, bed.heightMm) * GRID_PADDING;
  let minU = -pad;
  let maxU = bed.widthMm + pad;
  let minV = -pad;
  let maxV = bed.heightMm + pad;
  if (toolpathBounds) {
    // Extend the grid under the toolpath when the job exceeds the
    // configured bed (machine settings can trail the real machine —
    // e.g. a 900 mm export against a 430 mm seed).
    const extend =
      Math.max(
        toolpathBounds.maxX - toolpathBounds.minX,
        toolpathBounds.maxY - toolpathBounds.minY,
      ) * 0.1;
    minU = Math.min(minU, toolpathBounds.minX - extend);
    maxU = Math.max(maxU, toolpathBounds.maxX + extend);
    minV = Math.min(minV, toolpathBounds.minY - extend);
    maxV = Math.max(maxV, toolpathBounds.maxY + extend);
  }
  const grid = buildDynamicGrid(
    XY_FRAME,
    GRID_SPACING_MM,
    { minU, maxU, minV, maxV },
    minor,
    major,
    major,
    0.6,
  );
  grid.renderOrder = 1;
  group.add(grid);

  const outline = new THREE.Line(
    new THREE.BufferGeometry().setFromPoints([
      new THREE.Vector3(0, 0, 0),
      new THREE.Vector3(bed.widthMm, 0, 0),
      new THREE.Vector3(bed.widthMm, bed.heightMm, 0),
      new THREE.Vector3(0, bed.heightMm, 0),
    ]),
    new THREE.LineBasicMaterial({
      color: major,
      transparent: true,
      opacity: 0.9,
      depthTest: false,
      depthWrite: false,
    }),
  );
  outline.renderOrder = 2;
  group.add(outline);

  return group;
}

// ── Arc tessellation ────────────────────────────────────────────────

// Chord tolerance matches the core CAM preview convention.
const ARC_CHORD_TOLERANCE_MM = 0.01;
const TWO_PI = Math.PI * 2;

export function tessellateArcMove(
  move: GcodeMove,
  toleranceMm = ARC_CHORD_TOLERANCE_MM,
): [number, number, number][] {
  const center = move.center;
  if (!center) {
    // Defensive: a malformed arc without I/J renders as its chord.
    return [move.start, move.end];
  }
  const radius = Math.hypot(move.start[0] - center[0], move.start[1] - center[1]);
  if (radius < 1e-6) {
    return [move.start, move.end];
  }

  const startAngle = Math.atan2(move.start[1] - center[1], move.start[0] - center[0]);
  const endAngle = Math.atan2(move.end[1] - center[1], move.end[0] - center[0]);
  const direction = move.kind === "arcCw" ? -1 : 1;
  const fullCircle =
    Math.hypot(
      move.end[0] - move.start[0],
      move.end[1] - move.start[1],
    ) < 1e-6;

  let sweep = endAngle - startAngle;
  if (fullCircle || Math.abs(sweep) < 1e-9) {
    sweep = TWO_PI * direction;
  } else if (direction > 0) {
    // Wrap the raw delta into the commanded rotation direction so the
    // tessellation travels the same way the machine does.
    while (sweep <= 0) sweep += TWO_PI;
  } else {
    while (sweep >= 0) sweep -= TWO_PI;
  }

  const maxStep = 2 * Math.acos(Math.min(1, 1 - toleranceMm / radius));
  const segments = Math.max(1, Math.ceil(Math.abs(sweep) / maxStep));
  const points: [number, number, number][] = [];
  for (let i = 0; i <= segments; i++) {
    const t = i / segments;
    const angle = startAngle + sweep * t;
    points.push([
      center[0] + radius * Math.cos(angle),
      center[1] + radius * Math.sin(angle),
      center[2] + (move.end[2] - center[2]) * t,
    ]);
  }
  return points;
}

// ── Toolpath ────────────────────────────────────────────────────────

export interface GrblToolpathMaterials {
  rapid: THREE.LineDashedMaterial;
  feed: THREE.LineBasicMaterial;
  executed: THREE.LineBasicMaterial;
  dwellOn: THREE.PointsMaterial;
  dwellOff: THREE.PointsMaterial;
}

export interface GrblToolpathObjects {
  group: THREE.Group;
  /** One line per non-dwell move, indexed by move.index (dwells absent). */
  lineObjects: (THREE.Line | undefined)[];
  materials: GrblToolpathMaterials;
}

function buildGrblToolpathMaterials(): GrblToolpathMaterials {
  const rapidColor = themeColor("--cad-toolpath-rapid", "#ff6b7a");
  const feedColor = themeColor("--cad-toolpath-feed", "#2bd978");
  return {
    rapid: new THREE.LineDashedMaterial({
      color: rapidColor,
      dashSize: 1,
      gapSize: 0.6,
      transparent: true,
      opacity: 0.95,
      depthTest: false,
      depthWrite: false,
    }),
    feed: new THREE.LineBasicMaterial({
      color: feedColor,
      transparent: true,
      opacity: 0.95,
      depthTest: false,
      depthWrite: false,
    }),
    executed: new THREE.LineBasicMaterial({
      color: themeColor("--cad-toolpath-executed", "#ffe784"),
      transparent: true,
      opacity: 1,
      depthTest: false,
      depthWrite: false,
    }),
    dwellOn: new THREE.PointsMaterial({
      color: feedColor,
      size: 5,
      sizeAttenuation: false,
      depthTest: false,
      depthWrite: false,
    }),
    dwellOff: new THREE.PointsMaterial({
      color: rapidColor,
      size: 5,
      sizeAttenuation: false,
      depthTest: false,
      depthWrite: false,
    }),
  };
}

// Single-color materials for utility programs (framing box, focus
// pulse) so they read as an overlay on the real toolpath; executed
// lines still swap to the standard executed color.
function buildGrblOverlayMaterials(): GrblToolpathMaterials {
  const color = themeColor("--cad-framing-overlay", "#ff9f43");
  const solid = new THREE.LineBasicMaterial({
    color,
    transparent: true,
    opacity: 0.95,
    depthTest: false,
    depthWrite: false,
  });
  const dashed = new THREE.LineDashedMaterial({
    color,
    dashSize: 1,
    gapSize: 0.6,
    transparent: true,
    opacity: 0.95,
    depthTest: false,
    depthWrite: false,
  });
  return {
    rapid: dashed,
    feed: solid,
    executed: new THREE.LineBasicMaterial({
      color: themeColor("--cad-toolpath-executed", "#ffe784"),
      transparent: true,
      opacity: 1,
      depthTest: false,
      depthWrite: false,
    }),
    dwellOn: new THREE.PointsMaterial({
      color,
      size: 5,
      sizeAttenuation: false,
      depthTest: false,
      depthWrite: false,
    }),
    dwellOff: new THREE.PointsMaterial({
      color,
      size: 5,
      sizeAttenuation: false,
      depthTest: false,
      depthWrite: false,
    }),
  };
}

export function buildGrblToolpathGroup(
  info: GcodeFileInfo,
  materials: GrblToolpathMaterials = buildGrblToolpathMaterials(),
): GrblToolpathObjects {
  const group = new THREE.Group();
  const lineObjects: (THREE.Line | undefined)[] = [];

  for (const move of info.moves) {
    if (move.kind === "dwell") {
      // Pierce marker: a single-point dot at the dwell position,
      // coloured by whether the laser fires there.
      const dot = new THREE.Points(
        new THREE.BufferGeometry().setFromPoints([
          new THREE.Vector3(...move.end),
        ]),
        move.laserOn ? materials.dwellOn : materials.dwellOff,
      );
      dot.renderOrder = 10;
      group.add(dot);
      continue;
    }

    const points =
      move.kind === "arcCw" || move.kind === "arcCcw"
        ? tessellateArcMove(move).map((point) => new THREE.Vector3(...point))
        : [
            new THREE.Vector3(...move.start),
            new THREE.Vector3(...move.end),
          ];
    const material =
      move.kind === "rapid" ? materials.rapid : materials.feed;
    const line = new THREE.Line(
      new THREE.BufferGeometry().setFromPoints(points),
      material,
    );
    // `LineDashedMaterial` requires per-vertex distance data to render
    // the dash pattern; without this call the line renders solid.
    if (material instanceof THREE.LineDashedMaterial) {
      line.computeLineDistances();
    }
    line.renderOrder = 10;
    group.add(line);
    lineObjects[move.index] = line;
  }

  return { group, lineObjects, materials };
}

// Utility-program variant (framing/focus pulse): same geometry
// pipeline, overlay-colored materials.
export function buildGrblOverlayToolpathGroup(
  info: GcodeFileInfo,
): GrblToolpathObjects {
  return buildGrblToolpathGroup(info, buildGrblOverlayMaterials());
}

// Swaps every line whose filtered-line index is at or below
// `executedLines` onto the executed material.  `linesSent` is the
// buffered-not-executed approximation (≤127-byte lead) — the same one
// LaserGRBL shows; completion highlights every line.
export function applyGrblProgress(
  info: GcodeFileInfo,
  lineObjects: (THREE.Line | undefined)[],
  materials: GrblToolpathMaterials,
  executedLines: number,
): void {
  for (const move of info.moves) {
    const line = lineObjects[move.index];
    if (!line) {
      continue;
    }
    const base = move.kind === "rapid" ? materials.rapid : materials.feed;
    const wanted = move.line <= executedLines ? materials.executed : base;
    if (line.material !== wanted) {
      line.material = wanted;
    }
  }
}

export function disposeGrblToolpathGroup(
  objects: GrblToolpathObjects | null,
): void {
  if (!objects) {
    return;
  }
  for (const child of [...objects.group.children]) {
    objects.group.remove(child);
    const disposable = child as THREE.Line & THREE.Points;
    disposable.geometry?.dispose();
  }
  disposeMaterial(objects.materials.rapid);
  disposeMaterial(objects.materials.feed);
  disposeMaterial(objects.materials.executed);
  disposeMaterial(objects.materials.dwellOn);
  disposeMaterial(objects.materials.dwellOff);
}

// ── Live machine position markers ───────────────────────────────────

// Two short axis lines + a center dot.  Both markers live in the same
// machine-coordinate space as the toolpath, so idle jogging and
// in-cut positions overlay the preview directly.  The red pointer
// crosshair (the physical laser dot) sits at MPos + the machine's
// pointer offset and draws after the position marker.
function buildCrosshairMarker(
  color: string,
  renderOrder: number,
): THREE.Group {
  const group = new THREE.Group();
  const lineMaterial = new THREE.LineBasicMaterial({
    color,
    transparent: true,
    opacity: 0.95,
    depthTest: false,
    depthWrite: false,
  });
  const half = 7;
  const crosshair = new THREE.LineSegments(
    new THREE.BufferGeometry().setFromPoints([
      new THREE.Vector3(-half, 0, 0),
      new THREE.Vector3(half, 0, 0),
      new THREE.Vector3(0, -half, 0),
      new THREE.Vector3(0, half, 0),
    ]),
    lineMaterial,
  );
  crosshair.renderOrder = renderOrder;
  group.add(crosshair);

  const dot = new THREE.Mesh(
    new THREE.SphereGeometry(0.8, 12, 12),
    new THREE.MeshBasicMaterial({
      color,
      transparent: true,
      opacity: 0.95,
      depthTest: false,
      depthWrite: false,
    }),
  );
  dot.renderOrder = renderOrder;
  group.add(dot);

  return group;
}

// Machine position (MCS) crosshair — the "head is here" marker.
export function buildGrblPositionMarker(): THREE.Group {
  return buildCrosshairMarker(
    themeColor("--color-primary-edge-active", "#c3f5ff"),
    20,
  );
}

// Red laser pointer crosshair — the physical dot, offset from the
// focal point by the selected machine's pointer offset.
export function buildGrblPointerMarker(): THREE.Group {
  return buildCrosshairMarker(themeColor("--cad-pointer-dot", "#ff3b30"), 21);
}

export function updateGrblPositionMarker(
  marker: THREE.Group,
  mpos: [number, number, number],
): void {
  marker.position.set(mpos[0], mpos[1], mpos[2]);
  marker.visible = true;
}

export function updateGrblPointerMarker(
  marker: THREE.Group,
  position: [number, number, number],
): void {
  marker.position.set(position[0], position[1], position[2]);
  marker.visible = true;
}
