import * as THREE from "three";
import type { OrbitControls } from "three/examples/jsm/controls/OrbitControls.js";

import type {
  DrawingDimensionPreviewPayload,
  DrawingViewPreviewPayload,
  ViewportDrawingCurve,
  ViewportDrawingText,
  ViewportState,
} from "@/types";
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
                     color: number, opacity = 1): THREE.Mesh | null {
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
    new THREE.MeshBasicMaterial({
      color,
      toneMapped: false,
      transparent: opacity < 1,
      opacity,
    }),
  );
  mesh.renderOrder = 1;
  return mesh;
}

/// The furniture purposes (from the core's flattened stream) draw in
/// the border color instead of the visible-line color.  Text glyphs
/// (P7) draw in the visible color like the dimension values they
/// represent.
const FURNITURE_PURPOSES = new Set([
  "frame",
  "centring_mark",
  "grid_ref",
  "projection_symbol",
  "title_block",
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
  // PlaneGeometry is centered on its origin (spans −w/2..+w/2); the
  // border, curves and the camera fit all use sheet coordinates
  // (0..w, 0..h) — shift the paper so its extents match.
  paper.position.set(sheet.width_mm / 2, sheet.height_mm / 2, 0);
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
    const isHidden = curve.line_class === "hidden";
    const isFurniture = FURNITURE_PURPOSES.has(curve.purpose ?? "");
    // The P5 core flatten applies the ISO line styles (dash patterns
    // included) — the renderer draws continuous ribbons of the
    // stream's width only.
    const color = isHidden ? hiddenColor
      : isFurniture ? borderColor
        : visibleColor;
    if (curve.kind === "filled_poly") {
      const mesh = buildFilledPoly(curve.points ?? [], color);
      if (mesh) {
        group.add(mesh);
      }
      continue;
    }
    const points = tessellateSheetCurve(curve);
    const widthMm = curve.width_mm
      ?? (isHidden ? ISO_THIN_LINE_MM : ISO_THICK_LINE_MM);
    const ribbon = buildRibbon(points, widthMm, color);
    if (ribbon) {
      group.add(ribbon);
    }
  }

  // Text records stay DATA (the DXF backend emits real DRW_Text from
  // them); the sheet renders the P7 vector glyphs instead — they land
  // in `curves` as "text_glyph" line primitives drawn above.
  for (const view of sheet.views) {
    // View frame: a light rectangle around the content bounds plus a
    // label sprite (the P5 flatten turns these into proper view
    // boundary lines).  Named + tagged so the pointer-down hit test
    // can start a drag (mouse repositioning → drawing_view_move).
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
      frame.name = `view-frame:${view.view_id}`;
      frame.userData.viewFrame = {
        viewId: view.view_id,
        min: [view.min[0], view.min[1]] as [number, number],
        max: [view.max[0], view.max[1]] as [number, number],
      };
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
function makeLabelSprite(
  text: string,
  color: number,
  x: number,
  y: number,
  heightMm = 3,
) {
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
  // mm-per-pixel: the requested text height at 96px font.
  const mmPerPx = heightMm / fontPx;
  sprite.scale.set(width * mmPerPx, canvas.height * mmPerPx, 1);
  sprite.position.set(x, y, 0.5);
  sprite.renderOrder = 2;
  return sprite;
}

/** Filled polygon mesh (dimension arrowheads) — a THREE.Shape in the
 *  XY plane, triangulated. */
function buildFilledPoly(
  points: Array<[number, number]>,
  color: number,
  opacity = 1,
): THREE.Mesh | null {
  if (points.length < 3) {
    return null;
  }
  const shape = new THREE.Shape();
  shape.moveTo(points[0][0], points[0][1]);
  for (let i = 1; i < points.length; i += 1) {
    shape.lineTo(points[i][0], points[i][1]);
  }
  shape.closePath();
  const mesh = new THREE.Mesh(
    new THREE.ShapeGeometry(shape),
    new THREE.MeshBasicMaterial({
      color,
      toneMapped: false,
      transparent: opacity < 1,
      opacity,
    }),
  );
  mesh.renderOrder = 1;
  return mesh;
}

/** Adds one text record (dimension values) as a sprite. */
function addTextObject(group: THREE.Group, text: ViewportDrawingText,
                       color: number, staleColor: number) {
  const sprite = makeLabelSprite(
    text.text,
    text.stale ? staleColor : color,
    text.position[0],
    text.position[1],
    text.height_mm,
  );
  if (sprite) {
    group.add(sprite);
  }
}

/** Draws a set of curves + an optional text record (the dimension
 *  preview payload shape mirrors the sheet curves). */
function addPreviewObjects(
  group: THREE.Group,
  preview: DrawingDimensionPreviewPayload,
) {
  const color = sheetColor("--cad-drawing-visible-line", "#1c1b1b");
  for (const curve of preview.curves ?? []) {
    if (curve.kind === "filled_poly") {
      const mesh = buildFilledPoly(curve.points ?? [], color);
      if (mesh) {
        group.add(mesh);
      }
      continue;
    }
    const points = tessellateSheetCurve(curve as ViewportDrawingCurve);
    const ribbon = buildRibbon(points, curve.width_mm || 0.25, color);
    if (ribbon) {
      group.add(ribbon);
    }
  }
  if (preview.text) {
    addTextObject(group, preview.text, color, color);
  }
}

/** Dashed ribbon — short dashes along a polyline (the Insert View
 *  ghost's placement frame). */
function buildDashedRibbon(
  points: Array<[number, number]>,
  widthMm: number,
  color: number,
  dashLenMm: number,
  gapLenMm: number,
  opacity = 1,
): THREE.Group | null {
  const group = new THREE.Group();
  for (let i = 0; i < points.length - 1; i += 1) {
    const [x0, y0] = points[i];
    const [x1, y1] = points[i + 1];
    const len = Math.hypot(x1 - x0, y1 - y0);
    if (len < 1e-6) {
      continue;
    }
    const steps = Math.max(1, Math.round(len / (dashLenMm + gapLenMm)));
    const seg = len / steps;
    const dash = Math.min(dashLenMm, seg);
    const dx = (x1 - x0) / len;
    const dy = (y1 - y0) / len;
    for (let s = 0; s < steps; s += 1) {
      const t0 = s * seg;
      const t1 = t0 + dash;
      const ribbon = buildRibbon(
        [
          [x0 + dx * t0, y0 + dy * t0],
          [x0 + dx * t1, y0 + dy * t1],
        ],
        widthMm,
        color,
        opacity,
      );
      if (ribbon) {
        group.add(ribbon);
      }
    }
  }
  return group.children.length > 0 ? group : null;
}

/** ISO 5455 scale label: 1 → "1:1", 0.5 → "1:2", 2 → "2:1". */
function scaleLabel(scale: number): string {
  if (scale >= 1) {
    return `${scale}:1`;
  }
  const divisor = 1 / scale;
  return `1:${Number.isInteger(divisor) ? divisor : divisor.toFixed(2).replace(/0+$/, "").replace(/\.$/, "")}`;
}

/** Draws the Insert View ghost (drawing_view_preview_result): the
 *  uncommitted view's projected curves, translucent, plus a dashed
 *  placement frame around its content bounds and a label with the
 *  view name + scale — so position/orientation/scale are all visible
 *  BEFORE the commit.  A degraded preview shows its warning instead. */
function addViewPreviewObjects(
  group: THREE.Group,
  preview: DrawingViewPreviewPayload,
) {
  const color = sheetColor("--cad-drawing-preview", "#7c3aed");
  const staleColor = sheetColor("--cad-drawing-stale", "#e08a3c");
  const { view } = preview;
  const hasGeometry = view.warning === "" &&
    !(view.min[0] === 0 && view.min[1] === 0 &&
      view.max[0] === 0 && view.max[1] === 0);

  // Ghost curves — the committed view's exact geometry, translucent.
  for (const curve of preview.curves) {
    if (curve.kind === "filled_poly") {
      const mesh = buildFilledPoly(curve.points ?? [], color, 0.45);
      if (mesh) {
        group.add(mesh);
      }
      continue;
    }
    const points = tessellateSheetCurve(curve);
    const ribbon = buildRibbon(points, curve.width_mm || 0.25, color, 0.45);
    if (ribbon) {
      group.add(ribbon);
    }
  }

  if (hasGeometry) {
    // Dashed placement frame around the content bounds.
    const frame = buildDashedRibbon(
      [
        [view.min[0], view.min[1]], [view.max[0], view.min[1]],
        [view.max[0], view.max[1]], [view.min[0], view.max[1]],
        [view.min[0], view.min[1]],
      ],
      ISO_THIN_LINE_MM,
      color,
      4,
      3,
      0.8,
    );
    if (frame) {
      group.add(frame);
    }
    const label = makeLabelSprite(
      `${view.label} · ${scaleLabel(view.scale)}`,
      color,
      view.min[0],
      view.max[1] + 6,
      3.5,
    );
    if (label) {
      group.add(label);
    }
  } else {
    const label = makeLabelSprite(
      view.warning || "No preview",
      staleColor,
      view.origin[0],
      view.origin[1] + 6,
      3.5,
    );
    if (label) {
      group.add(label);
    }
  }
}

/** Adds the drawing sheets to a dedicated group (drawing workspace).
 *  `preview` carries the non-mutating dimension preview (P6);
 *  `viewPreview` carries the Insert View ghost — both drawn on top of
 *  the active sheet; the scene rebuilds them every sync so they track
 *  the latest preview reply. */
/** The in-progress drag ghost of a committed view (mouse
 *  repositioning): a dashed frame + label following the cursor. */
function addViewDragGhost(
  group: THREE.Group,
  drag: { min: [number, number]; max: [number, number]; label: string },
) {
  const color = sheetColor("--cad-drawing-preview", "#7c3aed");
  const frame = buildDashedRibbon(
    [
      [drag.min[0], drag.min[1]], [drag.max[0], drag.min[1]],
      [drag.max[0], drag.max[1]], [drag.min[0], drag.max[1]],
      [drag.min[0], drag.min[1]],
    ],
    ISO_THIN_LINE_MM,
    color,
    4,
    3,
    0.8,
  );
  if (frame) {
    group.add(frame);
  }
  const label = makeLabelSprite(
    drag.label,
    color,
    drag.min[0],
    drag.max[1] + 6,
    3.5,
  );
  if (label) {
    group.add(label);
  }
}

export function addDrawingSheetObjects({
  viewport,
  drawingGroup,
  preview,
  viewPreview,
  viewDrag,
}: {
  viewport: ViewportState | null;
  drawingGroup: THREE.Group;
  preview?: DrawingDimensionPreviewPayload | null;
  viewPreview?: DrawingViewPreviewPayload | null;
  viewDrag?: { min: [number, number]; max: [number, number]; label: string } | null;
}) {
  const sheets = viewport?.drawing_sheets ?? [];
  let offsetX = 0;
  for (const sheet of sheets) {
    const sheetGroup = new THREE.Group();
    sheetGroup.name = `drawing-sheet-${sheet.sheet_id}`;
    sheetGroup.position.set(offsetX, 0, 0);
    addSheetGroup(sheetGroup, sheet);
    drawingGroup.add(sheetGroup);
    // The previews + drag ghost belong to the FIRST sheet (the active one).
    if (offsetX === 0) {
      if (preview && !preview.error) {
        addPreviewObjects(sheetGroup, preview);
      }
      if (viewPreview) {
        addViewPreviewObjects(sheetGroup, viewPreview);
      }
      if (viewDrag) {
        addViewDragGhost(sheetGroup, viewDrag);
      }
    }
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
  // Straighten: the camera may arrive tilted from the CAD workspace —
  // a drawing must be read flat.  The view-cube idiom: keep the
  // distance, stand straight above the target (Z is up).
  const distance = Math.max(
    camera.position.distanceTo(controls.target),
    ORTHO_FRUSTUM_HEIGHT,
  );
  camera.position.set(
    controls.target.x,
    controls.target.y,
    controls.target.z + distance,
  );
  camera.up.set(0, 0, 1);
  controls.update();
}
