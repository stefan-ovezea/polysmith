// Regression tests for the rectangle-dimension-hiding scan in
// `createViewportScene`. The scan identifies which sketch lines belong
// to rectangles so redundant line_length / line_angle dimensions can be
// hidden. It runs on every viewport scene build, so it must stay near
// O(n²) — a previous O(n⁴) 4-line-combination scan froze the UI for
// projected sketches (76 lines ≈ 1.3M combos ≈ seconds; 910 lines ≈
// 28B combos ≈ permanent freeze).
import { describe, it, expect } from "vitest";
import { createViewportScene } from "./viewportScene";
import type { ViewportState } from "@/types";

type Line = {
  line_id: string;
  start_vertex_id: string;
  end_vertex_id: string;
  is_construction: boolean;
  plane_id: string;
  start: { x: number; y: number; z: number };
  end: { x: number; y: number; z: number };
  is_selected: boolean;
  constraint: "horizontal" | "vertical" | null;
};

let nextId = 0;
const pid = () => `line-${++nextId}`;

// Deterministic well-distributed RNG for the noise fixtures.
function mulberry32(seed: number): () => number {
  let a = seed >>> 0;
  return () => {
    a = (a + 0x6d2b79f5) | 0;
    let t = Math.imul(a ^ (a >>> 15), 1 | a);
    t = (t + Math.imul(t ^ (t >>> 7), 61 | t)) ^ t;
    return ((t ^ (t >>> 14)) >>> 0) / 4294967296;
  };
}

// Rectangle corners share the exact same double values, so opposite
// sides are bitwise equal-length — the 0.001 tolerance then detects
// rotated rectangles too (the real core emits exact shared coordinates
// for chained lines). Sides come back in [bottom, right, top, left]
// order, wound consistently.
function makeRect(
  cx: number,
  cy: number,
  w: number,
  h: number,
  angle: number,
): Line[] {
  const cos = Math.cos(angle);
  const sin = Math.sin(angle);
  const corner = (dx: number, dy: number): [number, number] => [
    cx + dx * cos - dy * sin,
    cy + dx * sin + dy * cos,
  ];
  const pts = [
    corner(-w / 2, -h / 2),
    corner(w / 2, -h / 2),
    corner(w / 2, h / 2),
    corner(-w / 2, h / 2),
  ];
  const side = (i: number, j: number): Line => {
    const id = pid();
    return {
      line_id: id,
      start_vertex_id: `v-${id}-s`,
      end_vertex_id: `v-${id}-e`,
      is_construction: false,
      plane_id: "p1",
      start: { x: pts[i][0], y: pts[i][1], z: 0 },
      end: { x: pts[j][0], y: pts[j][1], z: 0 },
      is_selected: false,
      constraint: null,
    };
  };
  return [side(0, 1), side(1, 2), side(2, 3), side(3, 0)];
}

function makeNoiseLine(rand: () => number): Line {
  const id = pid();
  const grid = (v: number) => Math.round(v / 0.001) * 0.001;
  return {
    line_id: id,
    start_vertex_id: `v-${id}-s`,
    end_vertex_id: `v-${id}-e`,
    is_construction: false,
    plane_id: "p1",
    start: { x: grid(rand() * 20 - 10), y: grid(rand() * 20 - 10), z: 0 },
    end: { x: grid(rand() * 20 - 10), y: grid(rand() * 20 - 10), z: 0 },
    is_selected: false,
    constraint: null,
  };
}

const EMPTY = [] as never[];

function makeViewport(lines: Line[]): ViewportState {
  const dims = lines.flatMap((line) => [
    {
      dimension_id: `d-len-${line.line_id}`,
      plane_id: "p1",
      kind: "line_length" as const,
      entity_id: line.line_id,
      label: "L",
      is_selected: false,
      anchor_start: { x: 0, y: 0, z: 0 },
      anchor_end: { x: 0, y: 0, z: 0 },
      dimension_start: { x: 0, y: 0, z: 0 },
      dimension_end: { x: 0, y: 0, z: 0 },
      label_position: { x: 0, y: 0, z: 0 },
      driven: false,
      display_as: "",
    },
    {
      dimension_id: `d-ang-${line.line_id}`,
      plane_id: "p1",
      kind: "line_angle" as const,
      entity_id: line.line_id,
      label: "A",
      is_selected: false,
      anchor_start: { x: 0, y: 0, z: 0 },
      anchor_end: { x: 0, y: 0, z: 0 },
      dimension_start: { x: 0, y: 0, z: 0 },
      dimension_end: { x: 0, y: 0, z: 0 },
      label_position: { x: 0, y: 0, z: 0 },
      driven: false,
      display_as: "",
    },
  ]);
  return {
    has_active_document: true,
    boxes: EMPTY,
    cylinders: EMPTY,
    polygon_extrudes: EMPTY,
    meshes: EMPTY,
    solid_faces: EMPTY,
    reference_planes: EMPTY,
    reference_axes: EMPTY,
    reference_points: EMPTY,
    helices: EMPTY,
    sketch_lines: lines,
    sketch_circles: EMPTY,
    sketch_polygons: EMPTY,
    sketch_arcs: EMPTY,
    sketch_vertices: EMPTY,
    sketch_dimensions: dims,
    sketch_constraints: EMPTY,
    sketch_profiles: EMPTY,
    cut_previews: EMPTY,
    toolpaths: EMPTY,
    bodies: EMPTY,
    edges: EMPTY,
    vertices: EMPTY,
    dof_statuses: EMPTY,
    snap_candidates: EMPTY,
    scene_width: 100,
    scene_height: 100,
    scene_depth: 100,
    scene_bounds: {
      center: { x: 0, y: 0, z: 0 },
      size: { x: 100, y: 100, z: 100 },
      max_dimension: 100,
    },
    selection_filter: {
      select_curves: true,
      select_points: true,
      select_construction: true,
      select_constraints: true,
      snap_endpoint: true,
      snap_midpoint: true,
      snap_center: true,
      snap_intersection: true,
      snap_nearest: true,
      snap_circle_body: true,
      snap_arc_body: true,
      snap_quadrant: true,
      snap_perpendicular: true,
      snap_parallel: true,
      snap_tangent: true,
      snap_grid: true,
      snap_grid_line: true,
      snap_polar: true,
      polar_angle_degrees: 15,
      parallel_angle_degrees: 8,
      magnetic_pull: true,
      tolerance_px: 10,
    },
  } as unknown as ViewportState;
}

function visibleDimensionIds(vp: ViewportState): Set<string> {
  const scene = createViewportScene(vp, {});
  const visible = new Set<string>();
  for (const dim of scene.sketchDimensions) {
    if (dim.kind === "line_length" || dim.kind === "line_angle") {
      visible.add(dim.dimensionId);
    }
  }
  return visible;
}

describe("viewport rectangle dimension hiding", () => {
  it("detects an isolated axis-aligned rectangle and hides the lower sides' dims", () => {
    const rect = makeRect(0, 0, 2, 1, 0);
    const got = visibleDimensionIds(makeViewport(rect));
    const [bottom, right, top, left] = rect.map((line) => line.line_id);

    // Every line in the rectangle hides its line_angle dimension.
    for (const id of rect.map((line) => line.line_id)) {
      expect(got.has(`d-ang-${id}`)).toBe(false);
    }
    // The line_length dims are hidden on the "lower" side of each
    // opposite pair: bottom (lower y) and left (tie on y, lower x).
    expect(got.has(`d-len-${bottom}`)).toBe(false);
    expect(got.has(`d-len-${left}`)).toBe(false);
    expect(got.has(`d-len-${top}`)).toBe(true);
    expect(got.has(`d-len-${right}`)).toBe(true);
  });

  it("detects rotated rectangles", () => {
    const rect = makeRect(3, -2, 2.5, 1.25, 0.7);
    const got = visibleDimensionIds(makeViewport(rect));
    for (const line of rect) {
      expect(got.has(`d-ang-${line.line_id}`)).toBe(false);
    }
    // Exactly two length dims hidden — one per opposite pair.
    const hidden = rect.filter((line) => !got.has(`d-len-${line.line_id}`));
    expect(hidden.length).toBe(2);
  });

  it("leaves every dimension visible when no rectangle exists", () => {
    const rand = mulberry32(7);
    const lines: Line[] = [];
    for (let i = 0; i < 40; i += 1) {
      lines.push(makeNoiseLine(rand));
    }
    const got = visibleDimensionIds(makeViewport(lines));
    expect(got.size).toBe(lines.length * 2);
  });

  it("detects adjacent rectangles sharing a side", () => {
    // First rect spans x in [-1, 1]; the neighbor spans [1, 3] with its
    // left side replaced by the first rect's right side (same
    // endpoints, so the shared line participates in both rectangles).
    const first = makeRect(0, 0, 2, 1, 0);
    const neighbor = makeRect(2, 0, 2, 1, 0);
    const all = [...first, neighbor[0], neighbor[1], neighbor[2], first[1]];
    const got = visibleDimensionIds(makeViewport(all));
    // Both rectangles detected: every line hides its angle dim. The
    // shared side's length dim is hidden too (it is the lower-x side
    // of the neighbor's vertical pair).
    for (const line of all) {
      expect(got.has(`d-ang-${line.line_id}`)).toBe(false);
    }
    expect(got.has(`d-len-${first[1].line_id}`)).toBe(false);
  });

  it("stays fast at fan-panel line counts (regression: was O(n⁴))", () => {
    const rand = mulberry32(11);
    const lines: Line[] = [];
    for (let i = 0; i < 900; i += 1) {
      lines.push(makeNoiseLine(rand));
    }
    lines.push(...makeRect(0, 0, 2, 1, 0));
    const start = performance.now();
    const got = visibleDimensionIds(makeViewport(lines));
    const ms = performance.now() - start;
    expect(ms).toBeLessThan(5000);
    // The rectangle itself is still detected at scale.
    for (const line of lines.slice(-4)) {
      expect(got.has(`d-ang-${line.line_id}`)).toBe(false);
    }
  });
});
