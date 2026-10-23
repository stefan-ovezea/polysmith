// @vitest-environment node
// Degenerate endpoint drops. Dropping a vertex onto one of its own neighbours
// makes a line zero-length, which the core refuses with
// "Sketch lines must have non-zero length (moving connected points)" — a
// user-visible error for what is really an accidental slip. The drag commit
// now refuses the drop locally and restores the preview instead.
import { describe, expect, it } from "vitest";

import type { SketchFeatureParameters } from "@/types";

import { endpointDropWouldCollapse } from "./endpointDragPointerUp";

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
