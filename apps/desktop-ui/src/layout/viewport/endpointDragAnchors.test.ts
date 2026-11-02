import { describe, expect, it } from "vitest";

import type { SketchFeatureParameters } from "@/types";

import { rideAnchoredPoints } from "./endpointDrag";

// A minimal sketch: two independent lines, a point anchored along line-1 and a
// midpoint anchor on line-2.
function sketch(): SketchFeatureParameters {
  return {
    lines: [
      {
        line_id: "line-1",
        start_vertex_id: "v1",
        end_vertex_id: "v2",
      },
      {
        line_id: "line-2",
        start_vertex_id: "v3",
        end_vertex_id: "v4",
      },
    ],
    point_line_anchors: [
      { anchor_id: "a1", vertex_id: "v9", line_id: "line-1", t: 0.25 },
    ],
    midpoint_anchors: [{ anchor_id: "a2", vertex_id: "v8", line_id: "line-2" }],
  } as unknown as SketchFeatureParameters;
}

describe("rideAnchoredPoints", () => {
  it("un-freezes a point anchored along the line being reshaped", () => {
    const active = rideAnchoredPoints(sketch(), "v1", ["v1"]);
    expect(active).toContain("v9");
  });

  it("un-freezes a midpoint-anchored point on the reshaped line", () => {
    const active = rideAnchoredPoints(sketch(), "v3", ["v3"]);
    expect(active).toContain("v8");
  });

  it("leaves anchors of untouched lines alone", () => {
    const active = rideAnchoredPoints(sketch(), "v1", ["v1"]);
    expect(active).not.toContain("v8");
    const other = rideAnchoredPoints(sketch(), "v3", ["v3"]);
    expect(other).not.toContain("v9");
  });

  it("keeps the ripple set it was given", () => {
    const active = rideAnchoredPoints(sketch(), "v1", ["v1", "neighbour"]);
    expect(active).toContain("v1");
    expect(active).toContain("neighbour");
  });

  it("returns the active set untouched when no line is reshaped", () => {
    expect(rideAnchoredPoints(sketch(), "lonely", ["lonely"])).toEqual([
      "lonely",
    ]);
  });
});
