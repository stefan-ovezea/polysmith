// Pure math for the Fusion-style drawing tools (R1) — frame
// construction, projected-child derivation, sector classification and
// slot placement. No React, no IPC: these functions mirror the CORE's
// frame conventions exactly (native/cad-core/src/core/drawing/
// drawing_projection.cpp — standard_view_frame, resolve_view_frame),
// so a child definition built here projects identically to one the
// core would resolve itself.

import type {
  DrawingView,
  DrawingViewFrame,
  IsoViewName,
  SectionDefinition,
  StandardViewName,
  ViewBasis,
} from "@/types";

export const ISO_SCALES = ["0.1", "0.2", "0.5", "1", "2", "5"];

export const STANDARD_VIEWS = [
  "front",
  "right",
  "left",
  "top",
  "bottom",
  "back",
] as const;

// Cutting-plane directions: the plane normal points along the picked
// axis, through the referenced bodies' center.
export const SECTION_NORMALS: Array<{
  key: string;
  vector: [number, number, number];
}> = [
  { key: "+X", vector: [1, 0, 0] },
  { key: "−X", vector: [-1, 0, 0] },
  { key: "+Y", vector: [0, 1, 0] },
  { key: "−Y", vector: [0, -1, 0] },
  { key: "+Z", vector: [0, 0, 1] },
  { key: "−Z", vector: [0, 0, -1] },
];

export const HATCH_ANGLES = ["30", "45", "60"];

/** Gap between adjacent views on the sheet (mm). */
export const VIEW_GAP_MM = 20;

// ── Vector helpers ────────────────────────────────────────────────

export type Vec3 = [number, number, number];

const EPS = 1e-6;

export function dot3(a: Vec3, b: Vec3): number {
  return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}

export function cross3(a: Vec3, b: Vec3): Vec3 {
  return [
    a[1] * b[2] - a[2] * b[1],
    a[2] * b[0] - a[0] * b[2],
    a[0] * b[1] - a[1] * b[0],
  ];
}

export function normalize3(v: Vec3): Vec3 | null {
  const length = Math.hypot(v[0], v[1], v[2]);
  if (length < EPS) {
    return null;
  }
  return [v[0] / length, v[1] / length, v[2] / length];
}

/** Projects v onto the plane perpendicular to the unit normal. */
function projectOntoPlane(v: Vec3, normal: Vec3): Vec3 | null {
  const projected: Vec3 = [
    v[0] - dot3(v, normal) * normal[0],
    v[1] - dot3(v, normal) * normal[1],
    v[2] - dot3(v, normal) * normal[2],
  ];
  return normalize3(projected);
}

/** The shared x_direction policy: horizontal on the sheet, i.e. the
 *  view-Y (= normal × x) keeps as much world +Z as possible.
 *  x = ẑ × normal (degenerate iff normal ∥ ẑ) → ŷ × normal → null. */
export function horizontalXFor(normal: Vec3): Vec3 | null {
  const fromZ = normalize3(cross3([0, 0, 1], normal));
  if (fromZ) {
    return fromZ;
  }
  const fromY = normalize3(cross3([0, 1, 0], normal));
  if (fromY) {
    return fromY;
  }
  return null;
}

// ── Standard frames (mirror the core's standard_view_frame) ───────

/** The core's six standard frames: world Z is up, "front" is the +X
 *  face, side views keep view-Y = +Z, top/bottom keep view-Y = −X
 *  (ISO 128-3 first-angle layout). */
export function standardViewFrame(name: StandardViewName): ViewBasis {
  let normal: Vec3;
  let xDir: Vec3;
  switch (name) {
    case "front":
      normal = [1, 0, 0];
      xDir = [0, 1, 0];
      break;
    case "right":
      normal = [0, 1, 0];
      xDir = [-1, 0, 0];
      break;
    case "left":
      normal = [0, -1, 0];
      xDir = [1, 0, 0];
      break;
    case "top":
      normal = [0, 0, 1];
      xDir = [0, 1, 0];
      break;
    case "bottom":
      normal = [0, 0, -1];
      xDir = [0, -1, 0];
      break;
    case "back":
      normal = [-1, 0, 0];
      xDir = [0, -1, 0];
      break;
  }
  const yDir = cross3(normal, xDir);
  return { normal, xDir, yDir };
}

/** The standard-view name whose normal matches (null = custom frame). */
export function standardViewNameForNormal(
  normal: Vec3,
  eps = 1e-6,
): StandardViewName | null {
  for (const name of STANDARD_VIEWS) {
    const basis = standardViewFrame(name);
    if (
      Math.abs(basis.normal[0] - normal[0]) < eps &&
      Math.abs(basis.normal[1] - normal[1]) < eps &&
      Math.abs(basis.normal[2] - normal[2]) < eps
    ) {
      return name;
    }
  }
  return null;
}

// ── Frame builders ────────────────────────────────────────────────

const ISO_SIGNS: Record<IsoViewName, { sx: number; sy: number }> = {
  ne: { sx: 1, sy: 1 },
  nw: { sx: -1, sy: 1 },
  se: { sx: 1, sy: -1 },
  sw: { sx: -1, sy: -1 },
};

/** An isometric frame derived from the CURRENT orientation's basis:
 *  normal = normalize(n ± x ± y), x_direction keeps view-Y horizontal.
 *  Front-relative NE → normal (1,1,1)/√3, x = (−1,1,0)/√2 — the
 *  classic 120° isometric. */
export function buildIsoFrame(
  basis: ViewBasis,
  which: IsoViewName,
): DrawingViewFrame | null {
  const { sx, sy } = ISO_SIGNS[which];
  const normal = normalize3([
    basis.normal[0] + sx * basis.xDir[0] + sy * basis.yDir[0],
    basis.normal[1] + sx * basis.xDir[1] + sy * basis.yDir[1],
    basis.normal[2] + sx * basis.xDir[2] + sy * basis.yDir[2],
  ]);
  if (!normal) {
    return null;
  }
  const xDir = horizontalXFor(normal);
  if (!xDir) {
    return null;
  }
  return { origin: [0, 0, 0], normal, x_direction: xDir };
}

// ── Body choice ───────────────────────────────────────────────────

/** A body chosen earlier may disappear while the tool is armed
 *  (deleted upstream) — fall back to the assembly view instead of
 *  committing a ghost id. */
export function resolveBodyIds(
  choice: string,
  bodies: Array<{ id: string }>,
): string[] {
  if (choice !== "__all__" && bodies.some((body) => body.id === choice)) {
    return [choice];
  }
  return bodies.map((body) => body.id);
}

/** The union center of the chosen bodies — the default cutting-plane
 *  point and the "Current 3D view" frame origin. */
export function bodyCenterForChoice(
  choice: string,
  bodies: Array<{ id: string; center: { x: number; y: number; z: number } }>,
): Vec3 {
  const chosen = bodies.filter((body) =>
    resolveBodyIds(choice, bodies).includes(body.id),
  );
  if (chosen.length === 0) {
    return [0, 0, 0];
  }
  return [
    chosen.reduce((sum, body) => sum + body.center.x, 0) / chosen.length,
    chosen.reduce((sum, body) => sum + body.center.y, 0) / chosen.length,
    chosen.reduce((sum, body) => sum + body.center.z, 0) / chosen.length,
  ];
}

// ── Committed-view bases ──────────────────────────────────────────

/** A raw custom frame, orthonormalized the way the core's gp_Ax2
 *  would (x re-projected onto the view plane, defaults when
 *  degenerate). */
export function customFrameBasis(frame: DrawingViewFrame): ViewBasis {
  const normal = normalize3(frame.normal) ?? [0, 0, 1];
  const xDir =
    projectOntoPlane(frame.x_direction, normal) ?? horizontalXFor(normal) ?? [
      1, 0, 0,
    ];
  const yDir = cross3(normal, xDir);
  return { normal, xDir, yDir };
}

/** The frame basis of a committed DrawingView, mirroring the core's
 *  resolve_view_frame: section → cutting plane; standard_view →
 *  the standard table; else the custom frame. */
export function viewBasisOf(view: DrawingView): ViewBasis | null {
  if (view.kind === "section" && view.section) {
    return sectionViewBasis(view.section);
  }
  if (view.standard_view) {
    const name = STANDARD_VIEWS.find(
      (candidate) => candidate === view.standard_view,
    );
    if (name) {
      return standardViewFrame(name);
    }
    return null;
  }
  if (!view.custom_frame) {
    return null;
  }
  return customFrameBasis(view.custom_frame);
}

/** Section frames derive from the cutting plane (the core's rule):
 *  normal = plane normal, x = world +X projected onto the plane
 *  (fallbacks +Y, +Z). */
export function sectionViewBasis(section: SectionDefinition): ViewBasis | null {
  const normal = normalize3(section.cutting_plane_normal);
  if (!normal) {
    return null;
  }
  const xDir =
    projectOntoPlane([1, 0, 0], normal) ??
    projectOntoPlane([0, 1, 0], normal) ??
    projectOntoPlane([0, 0, 1], normal);
  if (!xDir) {
    return null;
  }
  const yDir = cross3(normal, xDir);
  return { normal, xDir, yDir };
}

// ── Projected-view derivation ─────────────────────────────────────

/** The 8 sheet sectors in atan2 order: 0=E 1=NE 2=N 3=NW 4=W 5=SW
 *  6=S 7=SE. */
export const SECTOR_DIRS: Array<[number, number]> = [
  [1, 0],
  [1, 1],
  [0, 1],
  [-1, 1],
  [-1, 0],
  [-1, -1],
  [0, -1],
  [1, -1],
];

/** Classifies the cursor offset from the parent center into one of 8
 *  sectors (45° each, centered on the cardinal + diagonal directions);
 *  null = on top of the parent center (no direction). */
export function classifySector(dx: number, dy: number): number | null {
  const radius = Math.hypot(dx, dy);
  if (radius < 1e-6) {
    return null;
  }
  const theta = Math.atan2(dy, dx);
  const index = Math.round(theta / (Math.PI / 4));
  return ((index % 8) + 8) % 8;
}

/** Builds the uncommitted projected child for a cursor sector around
 *  the parent. Ortho sectors become the adjacent STANDARD view when
 *  the child normal matches the core's table (first angle mirrors the
 *  cursor: first-angle cursor-left of the front = "right" view),
 *  else a custom frame; diagonal sectors always become isometric
 *  custom frames (normal = normalize(n_p + dir)). The child inherits
 *  scale / hidden / bodies from the parent. sheet_position is the
 *  slot origin (already offset-corrected by the caller). */
export function buildProjectedChild(
  parent: ViewBasis,
  sectorIndex: number,
  angle: "first_angle" | "third_angle",
  sheetPosition: [number, number],
  inherit: {
    scale: number;
    showHidden: boolean;
    sourceBodyIds: string[];
  },
): { def: DrawingView; isDiagonal: boolean } | null {
  const dir = SECTOR_DIRS[sectorIndex];
  const isDiagonal = dir[0] !== 0 && dir[1] !== 0;
  // In-plane direction in the parent basis: d = dx·x_p + dy·y_p.
  const d: Vec3 = [
    dir[0] * parent.xDir[0] + dir[1] * parent.yDir[0],
    dir[0] * parent.xDir[1] + dir[1] * parent.yDir[1],
    dir[0] * parent.xDir[2] + dir[1] * parent.yDir[2],
  ];

  let kind: DrawingView["kind"];
  let standardView = "";
  let customFrame: DrawingViewFrame | undefined;

  if (isDiagonal) {
    // n_p + (±x_p ± y_p) → the 45° isometric between the three axes.
    const normal = normalize3([
      parent.normal[0] + d[0],
      parent.normal[1] + d[1],
      parent.normal[2] + d[2],
    ]);
    const xDir = normal ? horizontalXFor(normal) : null;
    if (!normal || !xDir) {
      return null;
    }
    kind = "axonometric";
    customFrame = { origin: [0, 0, 0], normal, x_direction: xDir };
  } else {
    // sign = −cursor for first angle, +cursor for third (ISO 128-3:
    // first angle shows the right side LEFT of the front view).
    const sign = angle === "first_angle" ? -1 : 1;
    const normal = normalize3([sign * d[0], sign * d[1], sign * d[2]]);
    if (!normal) {
      return null;
    }
    const name = standardViewNameForNormal(normal);
    if (name) {
      // A standard child: the core resolves its own frame table
      // (view-Y conventions included) — never send a custom frame.
      kind = "projection";
      standardView = name;
    } else {
      // A custom-frame parent: derive the child frame directly.
      const xDir = horizontalXFor(normal);
      if (!xDir) {
        return null;
      }
      kind = "projection";
      customFrame = { origin: [0, 0, 0], normal, x_direction: xDir };
    }
  }

  return {
    def: {
      view_id: "",
      kind,
      standard_view: standardView,
      ...(customFrame ? { custom_frame: customFrame } : {}),
      source_body_ids: inherit.sourceBodyIds,
      scale: inherit.scale,
      sheet_position: sheetPosition,
      show_hidden: inherit.showHidden,
      warning: "",
    },
    isDiagonal,
  };
}

// ── Slot placement ────────────────────────────────────────────────

/** The sheet_position that makes the content land at targetMin,
 *  given the orientation-dependent origin→min offset. */
export function originFromTargetMin(
  targetMin: [number, number],
  offset: [number, number],
): [number, number] {
  return [targetMin[0] - offset[0], targetMin[1] - offset[1]];
}

/** Clamps a view's content-min so its content stays inside the sheet's
 *  10 mm drawing margin.  When the content CANNOT fit within the
 *  margins (a near-page-size view), the fallback is DIRECTION
 *  PRESERVING: the clamp keeps at least `margin` mm of the view
 *  on-sheet on the side the cursor pushes toward and lets the
 *  opposite side bleed off the page.  A symmetric edge-to-edge
 *  fallback instead pinned near-page-size views to the top-left
 *  corner — they "had a mind of their own" and could never be
 *  lowered below the parent view. */
export function clampSheetPosition(
  position: [number, number],
  viewWidthMm: number,
  viewHeightMm: number,
  sheetWidthMm: number,
  sheetHeightMm: number,
): [number, number] {
  return [
    clampAxis(position[0], viewWidthMm, sheetWidthMm),
    clampAxis(position[1], viewHeightMm, sheetHeightMm),
  ];
}

function clampAxis(
  value: number,
  viewSizeMm: number,
  sheetSizeMm: number,
): number {
  const margin = 10;
  const lo = margin;
  const hi = sheetSizeMm - margin - viewSizeMm;
  if (hi >= lo) {
    return Math.min(Math.max(value, lo), hi);
  }
  // Content wider than the drawing area — the margins cannot both
  // hold.  Keep at least `margin` mm on-sheet on the side the cursor
  // is NOT pushing past (value bounded below by margin − viewSize),
  // and let the cursor's side bleed: the view follows the pointer
  // instead of snapping to a corner.
  return Math.min(Math.max(value, margin - viewSizeMm), sheetSizeMm - margin);
}
