// @vitest-environment node
// Where a Point-tool click lands. The pointer move previews the snap marker,
// so the commit must use the SNAPPED position — committing the raw ray point
// made the placed point disagree with the marker (reported as "there is no
// snap for point").
import { describe, expect, it } from "vitest";

import type { ActiveSketchSelectHit } from "./sketchClickSelection";
import { pointPlacementPosition } from "./pointerUpActiveSketch";
import type { SketchFeatureParameters } from "@/types";

const vertexHit: ActiveSketchSelectHit = {
  kind: "sketch_point",
  id: "vertex-7",
  pointKind: "endpoint",
};

function sketchWithVertex(): SketchFeatureParameters {
  return {
    vertices: [
      { vertex_id: "vertex-7", x: 40, y: 20 },
      { vertex_id: "vertex-8", x: 10, y: 5 },
    ],
    lines: [],
    arcs: [],
  } as unknown as SketchFeatureParameters;
}

const noHit = null as ActiveSketchSelectHit;

describe("pointPlacementPosition", () => {
  it("uses the snapped position, not the raw cursor position", () => {
    expect(
      pointPlacementPosition({
        hit: noHit,
        sketch: sketchWithVertex(),
        cursorLocal: [11.7, 4.2],
        cursorSnappedLocal: [12, 5],
      }),
    ).toEqual([12, 5]);
  });

  it("falls back to the raw position when nothing snapped", () => {
    expect(
      pointPlacementPosition({
        hit: noHit,
        sketch: sketchWithVertex(),
        cursorLocal: [11.7, 4.2],
      }),
    ).toEqual([11.7, 4.2]);
  });

  it("prefers an exact vertex hit over both", () => {
    expect(
      pointPlacementPosition({
        hit: vertexHit,
        sketch: sketchWithVertex(),
        cursorLocal: [39.9, 19.9],
        cursorSnappedLocal: [39.95, 19.95],
      }),
    ).toEqual([40, 20]);
  });

  it("returns null when there is no position at all", () => {
    expect(
      pointPlacementPosition({
        hit: noHit,
        sketch: sketchWithVertex(),
      }),
    ).toBeNull();
  });

  it("ignores a vertex hit whose vertex is gone (stale scene id)", () => {
    expect(
      pointPlacementPosition({
        hit: { kind: "sketch_point", id: "vertex-unknown", pointKind: "endpoint" },
        sketch: sketchWithVertex(),
        cursorLocal: [1, 2],
        cursorSnappedLocal: [3, 4],
      }),
    ).toEqual([3, 4]);
  });
});
