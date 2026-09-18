// Tool schematic geometry — pure functions, no React, no rendering.
//
// Builds a to-scale 2D profile of a cutting tool from its parameter
// record (the Fusion "Cutter preview" idea, drawn as declarative SVG
// paths — no per-type artwork).  Origin: tool tip on the center axis,
// +Y up the shank, millimeters.  The renderer flips into SVG space
// (y-down) via a viewBox transform and scales uniformly so a Ø8×60
// and a Ø1×38 tool never look identical.
//
// All rounded corners are quarter circles drawn as cubic Béziers
// (k = 0.5523): mirroring a path then only needs x-negation — no SVG
// arc sweep flags to get backwards when the profile is reversed.

import type { ToolType } from "@/types/geometry/cam";

export interface SchematicParams {
  type: ToolType;
  diameter_mm: number;
  shank_diameter_mm: number;
  flute_length_mm: number;
  /** 0 = unset → falls back to flute_length_mm. */
  shoulder_length_mm: number;
  overall_length_mm: number;
  corner_radius_mm: number;
  point_angle_deg: number;
  tip_diameter_mm: number;
  tip_length_mm: number;
  taper_angle_deg: number;
}

export interface SchematicDimLine {
  from: [number, number];
  to: [number, number];
  label: string;
  /** Which side of the profile the line sits on ("left" = -x). */
  side: "left" | "right";
  /** Extra standoff from the profile edge (mm). */
  offset: number;
}

export interface ToolSchematic {
  /** Closed outline path (SVG path data, math space). */
  outline: string;
  /** The cutting region (tip → flute length), accent-filled. */
  fluteRegion: string;
  /** The shank/body above the cutting region. */
  shankRegion: string;
  dims: SchematicDimLine[];
  extent: { width: number; height: number; lowestY: number };
}

interface Point {
  x: number;
  y: number;
}

interface Line {
  kind: "line";
  from: Point;
  to: Point;
}

interface Curve {
  kind: "curve";
  from: Point;
  c1: Point;
  c2: Point;
  to: Point;
}

type Segment = Line | Curve;

const deg2rad = (degrees: number) => (degrees * Math.PI) / 180;

// Bézier constant for a quarter circle (4/3 · tan(π/8)).
const K = 0.5522847498307936;

function line(from: Point, to: Point): Line {
  return { kind: "line", from, to };
}

/**
 * Quarter-circle arc (≤90°) from the current point to `to` around
 * `center`, drawn counterclockwise in math space.
 */
function quarterArc(from: Point, center: Point, to: Point): Curve {
  const tangentAt = (p: Point): Point => ({
    x: -(p.y - center.y),
    y: p.x - center.x,
  });
  const r = Math.hypot(from.x - center.x, from.y - center.y);
  const t1 = tangentAt(from);
  const t2 = tangentAt(to);
  return {
    kind: "curve",
    from,
    c1: { x: from.x + K * r * t1.x, y: from.y + K * r * t1.y },
    c2: { x: to.x - K * r * t2.x, y: to.y - K * r * t2.y },
    to,
  };
}

// ── Tip profiles ──────────────────────────────────────────────────
//
// Each builder appends the RIGHT half of the profile from the tip up
// to the top of the cutting flanks (y = flute_length) and returns the
// current point.  The tip point (lowest, on the axis side) varies per
// type: flat bottom at y=0, ball bulges below, cones rise from the
// point angle.

function appendFlatBottom(
  segments: Segment[],
  current: Point,
  p: SchematicParams,
): Point {
  const r = Math.min(p.corner_radius_mm, p.diameter_mm / 2);
  const half = p.diameter_mm / 2;
  if (r > 0) {
    const cornerStart: Point = { x: half - r, y: 0 };
    segments.push(line(current, cornerStart));
    segments.push(quarterArc(cornerStart, { x: half - r, y: r }, { x: half, y: r }));
    current = { x: half, y: r };
  } else {
    segments.push(line(current, { x: half, y: 0 }));
    current = { x: half, y: 0 };
  }
  const top: Point = { x: half, y: p.flute_length_mm };
  segments.push(line(current, top));
  return top;
}

function appendBallTip(
  segments: Segment[],
  current: Point,
  p: SchematicParams,
): Point {
  const half = p.diameter_mm / 2;
  // Quarter of the ball nose: from the lowest point (0, −half) up the
  // right side to the flank (half, 0).
  segments.push(quarterArc(current, { x: 0, y: 0 }, { x: half, y: 0 }));
  const top: Point = { x: half, y: p.flute_length_mm };
  segments.push(line({ x: half, y: 0 }, top));
  return top;
}

function appendBullTip(
  segments: Segment[],
  current: Point,
  p: SchematicParams,
): Point {
  const half = p.diameter_mm / 2;
  const r = Math.min(p.corner_radius_mm, half);
  const cornerStart: Point = { x: half - r, y: 0 };
  segments.push(line(current, cornerStart));
  segments.push(quarterArc(cornerStart, { x: half - r, y: r }, { x: half, y: r }));
  const top: Point = { x: half, y: p.flute_length_mm };
  segments.push(line({ x: half, y: r }, top));
  return top;
}

/**
 * Cone tip (chamfer, V-bit, spot drill, drill).  `current` IS the tip
 * (lowest point); `tipHalf` = half-width of the flat at the very tip
 * (0 = sharp point).  The cone widens from the tip up to the full
 * diameter, truncated at the flute length.
 */
function appendConeTip(
  segments: Segment[],
  current: Point,
  p: SchematicParams,
  tipHalf: number,
): Point {
  const half = p.diameter_mm / 2;
  const halfAngle = deg2rad(p.point_angle_deg / 2);
  const tangent = Math.tan(halfAngle > 0 ? halfAngle : deg2rad(45));
  // Cone height: from the tip up to the full diameter.
  const coneHeight = (half - tipHalf) / tangent;
  let at = current;
  if (tipHalf > 0) {
    const flat: Point = { x: tipHalf, y: current.y };
    segments.push(line(current, flat));
    at = flat;
  }
  const coneTopY = Math.min(current.y + coneHeight, p.flute_length_mm);
  segments.push(line(at, { x: half, y: coneTopY }));
  const top: Point = { x: half, y: p.flute_length_mm };
  segments.push(line({ x: half, y: coneTopY }, top));
  return top;
}

// ── Shank ─────────────────────────────────────────────────────────

function appendShank(
  segments: Segment[],
  current: Point,
  p: SchematicParams,
): Point {
  const shoulder = Math.max(p.shoulder_length_mm, p.flute_length_mm);
  const half = p.diameter_mm / 2;
  const shankHalf = Math.max(p.shank_diameter_mm / 2, 0.01);
  // Straight at full diameter up to the shoulder, then step in to the
  // shank diameter, then vertical to the overall length.
  const shoulderTop: Point = { x: half, y: shoulder };
  segments.push(line(current, shoulderTop));
  const step: Point = { x: shankHalf, y: shoulder };
  segments.push(line(shoulderTop, step));
  const top: Point = { x: shankHalf, y: p.overall_length_mm };
  segments.push(line(step, top));
  return top;
}

function rightProfile(p: SchematicParams): { segments: Segment[]; bottom: number } {
  const segments: Segment[] = [];
  const half = p.diameter_mm / 2;
  let bottom = 0;
  let current: Point = { x: 0, y: 0 };
  switch (p.type) {
    case "endmill_ball":
      bottom = -half;
      current = { x: 0, y: bottom };
      current = appendBallTip(segments, current, p);
      break;
    case "endmill_bull":
      current = appendBullTip(segments, current, p);
      break;
    case "chamfer":
    case "v_bit":
    case "spot_drill":
      current = appendConeTip(segments, current, p, Math.max(p.tip_diameter_mm / 2, 0));
      break;
    case "drill":
      // Sharp point below the baseline at −tip_length.
      bottom = -p.tip_length_mm;
      current = { x: 0, y: bottom };
      current = appendConeTip(segments, current, p, 0);
      break;
    default:
      current = appendFlatBottom(segments, current, p);
      break;
  }
  appendShank(segments, current, p);
  return { segments, bottom };
}

// ── Path assembly ─────────────────────────────────────────────────

function fmt(value: number): string {
  const rounded = Math.round(value * 1000) / 1000;
  return String(rounded);
}

function pathFromSegments(segments: Segment[], start: Point): string {
  let path = `M ${fmt(start.x)} ${fmt(start.y)}`;
  for (const segment of segments) {
    if (segment.kind === "line") {
      path += ` L ${fmt(segment.to.x)} ${fmt(segment.to.y)}`;
    } else {
      path +=
        ` C ${fmt(segment.c1.x)} ${fmt(segment.c1.y)}` +
        ` ${fmt(segment.c2.x)} ${fmt(segment.c2.y)}` +
        ` ${fmt(segment.to.x)} ${fmt(segment.to.y)}`;
    }
  }
  return path;
}

/** Mirrors a profile across the axis (x → −x) and reverses it. */
function mirrored(segments: Segment[]): Segment[] {
  const m = (point: Point): Point => ({ x: -point.x, y: point.y });
  const result: Segment[] = [];
  for (let i = segments.length - 1; i >= 0; --i) {
    const segment = segments[i];
    if (segment.kind === "line") {
      result.push({ kind: "line", from: m(segment.to), to: m(segment.from) });
    } else {
      result.push({
        kind: "curve",
        from: m(segment.to),
        c1: m(segment.c2),
        c2: m(segment.c1),
        to: m(segment.from),
      });
    }
  }
  return result;
}

// ── Public API ────────────────────────────────────────────────────

export function buildToolSchematic(p: SchematicParams): ToolSchematic {
  const { segments, bottom } = rightProfile(p);
  const right = pathFromSegments(segments, { x: 0, y: bottom });
  // Closed outline: right profile up, across the shank top, mirrored
  // profile back down to the tip.  The mirrored path ends exactly at
  // the start point, so Z only joins the tip corner.
  const left = mirrored(segments);
  const outline =
    right +
    ` L ${fmt(left[0].from.x)} ${fmt(p.overall_length_mm)}` +
    " " +
    pathFromSegments(left, left[0].from) +
    " Z";

  // Flute region: right profile from the tip to flute_length, closed
  // back along the axis.
  const fluteSegments = segments.filter(
    (segment) => segment.to.y <= p.flute_length_mm + 1e-9,
  );
  const fluteRegion =
    pathFromSegments(fluteSegments, { x: 0, y: bottom }) +
    ` L 0 ${fmt(p.flute_length_mm)} Z`;

  // Shank region: axis → (half, flute) → profile from flute to the
  // overall length → back down the axis.
  const shankProfile = segments.filter(
    (segment) => segment.from.y >= p.flute_length_mm - 1e-9,
  );
  const shankRegion =
    `M 0 ${fmt(p.flute_length_mm)}` +
    ` L ${fmt(p.diameter_mm / 2)} ${fmt(p.flute_length_mm)}` +
    " " +
    pathFromSegments(shankProfile, { x: p.diameter_mm / 2, y: p.flute_length_mm }) +
    ` L 0 ${fmt(p.overall_length_mm)} Z`;

  // Dimension lines: overall length on the right, flute length on the
  // left, diameter under the tip.
  const rightOffset = Math.max(p.diameter_mm, p.shank_diameter_mm) / 2 + 4;
  const leftOffset = -(p.diameter_mm / 2 + 4);
  const dims: SchematicDimLine[] = [
    {
      from: [leftOffset, 0],
      to: [leftOffset, p.flute_length_mm],
      label: `FL ${fmt(p.flute_length_mm)}`,
      side: "left",
      offset: 4,
    },
    {
      from: [rightOffset, 0],
      to: [rightOffset, p.overall_length_mm],
      label: `OAL ${fmt(p.overall_length_mm)}`,
      side: "right",
      offset: 4,
    },
    {
      from: [-p.diameter_mm / 2, bottom - 4],
      to: [p.diameter_mm / 2, bottom - 4],
      label: `Ø${fmt(p.diameter_mm)}`,
      side: "right",
      offset: 4,
    },
  ];

  const width = Math.max(
    p.diameter_mm,
    p.shank_diameter_mm,
    p.diameter_mm / 2 + 20,
  );
  return {
    outline,
    fluteRegion,
    shankRegion,
    dims,
    extent: {
      width,
      height: p.overall_length_mm - bottom,
      lowestY: bottom,
    },
  };
}
