import * as THREE from "three";
import type { OrbitControls } from "three/examples/jsm/controls/OrbitControls.js";

import type { ViewportDrawingCurve, ViewportState } from "@/types";
import { themeColor } from "@/utils";

import { ORTHO_FRUSTUM_HEIGHT } from "./viewportPanelTypes";

// ── ISO Drawing sheet scene objects ───────────────────────────────
//
// The drawing workspace renders the active drawing's sheets as flat
// groups in the XY plane (world units = mm).  Projection curves are
// emitted by the core already in sheet-mm; here they become geometry:
// lines as ribbon meshes (WebGL ignores linewidth on Windows), arcs
// as tessellated ribbons.  Line classes map to ISO line styles —
// hidden edges dashed thin (P5 adds the full ISO 128-2 pattern
// vocabulary + dash recalculation).

const ISO_THICK_LINE_MM = 0.5;   // group 0.5 (A3/A4 default)
const ISO_THIN_LINE_MM = 0.25;
const ARC_SEGMENTS = 64;

function sheetColor(token: string, fallback: string): number {
  return new THREE.Color(themeColor(token, fallback)).getHex();
}

/// Tessellates a sheet curve to polyline points (sheet-mm).
export function tessellateSheetCurve(
  curve: ViewportDrawingCurve,
): Array<[number, number]> {
  if (curve.kind === "circle" && curve.center && curve.radius) {
    const points: Array<[number, number]> = [];
    // A full circle (end == start + 2π) must not collapse to a point.
    const sweep = Math.abs(curve.end_angle - curve.start_angle);
    const full = sweep >= Math.PI * 2 - 1e-6;
    const steps = full ? ARC_SEGMENTS : Math.max(2, Math.ceil((sweep / (Math.PI * 2)) * ARC_SEGMENTS));
    for (let i = 0; i <= steps; i += 1) {
      const a = curve.start_angle + (sweep * i) / steps;
      points.push([
        curve.center[0] + curve.radius * Math.cos(a),
        curve.center[1] + curve.radius * Math.sin(a),
      ]);
    }
    return points;
  }
  if (curve.kind === "ellipse" && curve.center && curve.major_dir &&
      curve.major_radius !== undefined && curve.minor_radius !== undefined) {
    // The angular parameter measures from the major axis; the point is
    // center + major * cos(a) + minor_dir * minor * sin(a).
    const [mx, my] = curve.major_dir;
    const [nx, ny] = [-my, mx];
    const points: Array<[number, number]> = [];
    const sweep = Math.abs(curve.end_angle - curve.start_angle);
    const full = sweep >= Math.PI * 2 - 1e-6;
    const steps = full ? ARC_SEGMENTS : Math.max(2, Math.ceil((sweep / (Math.PI * 2)) * ARC_SEGMENTS));
    for (let i = 0; i <= steps; i += 1) {
      const a = curve.start_angle + (sweep * i) / steps;
      points.push([
        curve.center[0] + mx * curve.major_radius * Math.cos(a) +
          nx * curve.minor_radius * Math.sin(a),
        curve.center[1] + my * curve.major_radius * Math.cos(a) +
          ny * curve.minor_radius * Math.sin(a),
      ]);
    }
    return points;
  }
  // Lines and anything else: endpoints.
  return [curve.p0, curve.p1];
}

/// Builds a flat ribbon mesh (a thick line) along a polyline in the
/// XY plane — linewidth is ignored on Windows, so ISO line groups are
/// real geometry.
function buildRibbon(points: Array<[number, number]>, widthMm: number,
                     color: number): THREE.Mesh | null {
  if (points.length < 2) {
    return null;
  }
  const half = widthMm / 2;
  const positions: number[] = [];
  for (let i = 0; i < points.length; i += 1) {
    const [x, y] = points[i];
    // Segment direction; for endpoints use the adjacent segment.
    const prev = points[Math.max(0, i - 1)];
    const next = points[Math.min(points.length - 1, i + 1)];
    let dx = next[0] - prev[0];
    let dy = next[1] - prev[1];
    const len = Math.hypot(dx, dy) || 1;
    dx /= len;
    dy /= len;
    // Perpendicular (in-plane): (-dy, dx).
    positions.push(x - dy * half, y + dx * half, 0);
    positions.push(x + dy * half, y - dx * half, 0);
  }
  const indices: number[] = [];
  for (let i = 0; i < points.length - 1; i += 1) {
    const a = i * 2;
    const b = a + 1;
    const c = a + 2;
    const d = a + 3;
    indices.push(a, b, c, b, d, c);
  }
  const geometry = new THREE.BufferGeometry();
  geometry.setAttribute(
    "position",
    new THREE.Float32BufferAttribute(positions, 3),
  );
  geometry.setIndex(indices);
  const mesh = new THREE.Mesh(
    geometry,
    new THREE.MeshBasicMaterial({ color, toneMapped: false }),
  );
  mesh.renderOrder = 1;
  return mesh;
}

/// The furniture purposes (from the core's flattened stream) draw in
/// the border color instead of the visible-line color.
const FURNITURE_PURPOSES = new Set([
  "frame",
  "centring_mark",
  "grid_ref",
  "projection_symbol",
]);

/** Adds one sheet (paper, border, projection curves, view frames). */
function addSheetGroup(
  group: THREE.Group,
  sheet: NonNullable<ViewportState["drawing_sheets"]>[number],
) {
  const paper = new THREE.Mesh(
    new THREE.PlaneGeometry(sheet.width_mm, sheet.height_mm),
    new THREE.MeshBasicMaterial({
      color: sheetColor("--cad-drawing-sheet", "#faf9f6"),
      side: THREE.DoubleSide,
      toneMapped: false,
    }),
  );
  paper.renderOrder = 0;
  paper.name = "sheet-paper";
  group.add(paper);

  // Border: thin outline around the paper (the ISO 5457 frame lands
  // in P5).
  const border = buildRibbon(
    [
      [0, 0], [sheet.width_mm, 0],
      [sheet.width_mm, sheet.height_mm],
      [0, sheet.height_mm], [0, 0],
    ],
    ISO_THIN_LINE_MM,
    sheetColor("--cad-drawing-sheet-border", "#3b494c"),
  );
  if (border) {
    group.add(border);
  }

  const visibleColor = sheetColor("--cad-drawing-visible-line", "#1c1b1b");
  const hiddenColor = sheetColor("--cad-drawing-hidden-line", "#6d6d69");
  const borderColor = sheetColor("--cad-drawing-sheet-border", "#3b494c");
  const staleColor = sheetColor("--cad-drawing-stale", "#e08a3c");

  for (const curve of sheet.curves) {
    const points = tessellateSheetCurve(curve);
    const isHidden = curve.line_class === "hidden";
    const isFurniture = FURNITURE_PURPOSES.has(curve.purpose ?? "");
    // The P5 core flatten applies the ISO line styles (dash patterns
    // included) — the renderer draws continuous ribbons of the
    // stream's width only.
    const color = isHidden ? hiddenColor
      : isFurniture ? borderColor
        : visibleColor;
    const widthMm = curve.width_mm
      ?? (isHidden ? ISO_THIN_LINE_MM : ISO_THICK_LINE_MM);
    const ribbon = buildRibbon(points, widthMm, color);
    if (ribbon) {
      group.add(ribbon);
    }
  }

  for (const view of sheet.views) {
    // View frame: a light rectangle around the content bounds plus a
    // label sprite (the P5 flatten turns these into proper view
    // boundary lines).
    const labelColor = view.stale
      ? staleColor
      : sheetColor("--cad-drawing-view-label", "#11505a");
    const frame = buildRibbon(
      [
        [view.min[0], view.min[1]], [view.max[0], view.min[1]],
        [view.max[0], view.max[1]], [view.min[0], view.max[1]],
        [view.min[0], view.min[1]],
      ],
      ISO_THIN_LINE_MM,
      labelColor,
    );
    if (frame) {
      group.add(frame);
    }
    const label = makeLabelSprite(
      view.stale ? `${view.label} — ${view.warning || "stale"}` : view.label,
      labelColor,
      view.min[0],
      view.max[1] + 6,
    );
    if (label) {
      group.add(label);
    }
  }
}

/** Canvas-texture sprite for a sheet label (mm-sized). */
function makeLabelSprite(text: string, color: number, x: number, y: number) {
  const fontPx = 96;
  const padPx = 16;
  const canvas = document.createElement("canvas");
  const ctx = canvas.getContext("2d");
  if (!ctx) {
    return null;
  }
  ctx.font = `${fontPx}px sans-serif`;
  const width = Math.ceil(ctx.measureText(text).width) + padPx * 2;
  canvas.width = width;
  canvas.height = fontPx + padPx * 2;
  ctx.font = `${fontPx}px sans-serif`;
  ctx.fillStyle = `#${new THREE.Color(color).getHexString()}`;
  ctx.textBaseline = "middle";
  ctx.fillText(text, padPx, canvas.height / 2);
  const texture = new THREE.CanvasTexture(canvas);
  const material = new THREE.SpriteMaterial({ map: texture, toneMapped: false });
  const sprite = new THREE.Sprite(material);
  // mm-per-pixel: 3 mm text height at 96px font.
  const mmPerPx = 3 / fontPx;
  sprite.scale.set(width * mmPerPx, canvas.height * mmPerPx, 1);
  sprite.position.set(x, y, 0.5);
  sprite.renderOrder = 2;
  return sprite;
}

/** Adds the drawing sheets to a dedicated group (drawing workspace). */
export function addDrawingSheetObjects({
  viewport,
  drawingGroup,
}: {
  viewport: ViewportState | null;
  drawingGroup: THREE.Group;
}) {
  const sheets = viewport?.drawing_sheets ?? [];
  let offsetX = 0;
  for (const sheet of sheets) {
    const sheetGroup = new THREE.Group();
    sheetGroup.name = `drawing-sheet-${sheet.sheet_id}`;
    sheetGroup.position.set(offsetX, 0, 0);
    addSheetGroup(sheetGroup, sheet);
    drawingGroup.add(sheetGroup);
    offsetX += sheet.width_mm + 24;
  }
}

/** Fits the orthographic camera to a sheet's extents (mm world units). */
export function fitCameraToDrawingSheet({
  camera,
  controls,
  host,
  widthMm,
  heightMm,
  offsetX = 0,
}: {
  camera: THREE.OrthographicCamera;
  controls: OrbitControls;
  host: { clientWidth: number; clientHeight: number } | null;
  widthMm: number;
  heightMm: number;
  offsetX?: number;
}) {
  const aspect = host && host.clientHeight > 0
    ? host.clientWidth / host.clientHeight
    : 1.5;
  const margin = 1.12;
  const zoom = Math.min(
    ORTHO_FRUSTUM_HEIGHT / (heightMm * margin),
    ORTHO_FRUSTUM_HEIGHT / ((widthMm / aspect) * margin),
  );
  camera.zoom = Math.max(1e-4, zoom);
  camera.updateProjectionMatrix();
  controls.target.set(offsetX + widthMm / 2, heightMm / 2, 0);
  controls.update();
}
