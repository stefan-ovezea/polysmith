// @vitest-environment node
// Degenerate endpoint drops. Dropping a vertex onto one of its own neighbours
// makes a line zero-length, which the core refuses with
// "Sketch lines must have non-zero length (moving connected points)" — a
// user-visible error for what is really an accidental slip. The drag commit
// now refuses the drop locally and restores the preview instead.
import { describe, expect, it } from "vitest";

import type { SketchFeatureParameters } from "@/types";

import {
  endpointDropWouldCollapse,
  pointIdAtPosition,
} from "./endpointDragPointerUp";

function sketch(): SketchFeatureParameters {
  return {
    plane_id: "ref-plane-xy",
    lines: [
      {
        line_id: "line-1",
        start_vertex_id: "v-1",
        end_vertex_id: "v-2",
        start_x: 0,
        start_y: 0,
        end_x: 40,
        end_y: 0,
        constraint: null,
        is_construction: false,
        generated_by: null,
      },
      {
        line_id: "line-2",
        start_vertex_id: "v-2",
        end_vertex_id: "v-3",
        start_x: 40,
        start_y: 0,
        end_x: 40,
        end_y: 20,
        constraint: null,
        is_construction: false,
        generated_by: null,
      },
    ],
    arcs: [],
    vertices: [
      { vertex_id: "v-1", x: 0, y: 0 },
      { vertex_id: "v-2", x: 40, y: 0 },
      { vertex_id: "v-3", x: 40, y: 20 },
    ],
  } as unknown as SketchFeatureParameters;
}

describe("endpointDropWouldCollapse", () => {
  it("refuses a drop onto a directly connected endpoint", () => {
    expect(endpointDropWouldCollapse(sketch(), "v-1", 40, 0)).toBe(true);
    expect(endpointDropWouldCollapse(sketch(), "v-2", 40, 20)).toBe(true);
  });

  it("allows a drop anywhere else, including near an unconnected vertex", () => {
    expect(endpointDropWouldCollapse(sketch(), "v-1", 40, 20)).toBe(false);
    expect(endpointDropWouldCollapse(sketch(), "v-1", 10, 5)).toBe(false);
  });

  it("treats a missing sketch as movable", () => {
    expect(endpointDropWouldCollapse(null, "v-1", 40, 0)).toBe(false);
  });
});

describe("pointIdAtPosition", () => {
  const withPoints = (points: Array<{ point_id: string; x: number; y: number }>) =>
    ({ ...sketch(), points } as unknown as SketchFeatureParameters);

  it("finds the standalone point the drop landed on", () => {
    const s = withPoints([{ point_id: "vertex-9", x: 12, y: 8 }]);
    expect(pointIdAtPosition(s, 12, 8, "v-1")).toBe("vertex-9");
  });

  it("never binds a vertex to itself", () => {
    const s = withPoints([{ point_id: "vertex-9", x: 12, y: 8 }]);
    expect(pointIdAtPosition(s, 12, 8, "vertex-9")).toBeUndefined();
  });

  it("ignores a drop that is not exactly on a point", () => {
    const s = withPoints([{ point_id: "vertex-9", x: 12, y: 8 }]);
    expect(pointIdAtPosition(s, 12.5, 8, "v-1")).toBeUndefined();
  });

  it("is safe on a sketch without a points array (older documents)", () => {
    expect(pointIdAtPosition(sketch(), 12, 8, "v-1")).toBeUndefined();
    expect(pointIdAtPosition(null, 12, 8, "v-1")).toBeUndefined();
  });
});
