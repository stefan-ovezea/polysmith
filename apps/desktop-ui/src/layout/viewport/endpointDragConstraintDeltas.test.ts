// @vitest-environment node
import { describe, expect, it } from "vitest";

import { endpointDragConstraintDeltas } from "./endpointDrag";
import type { SketchConstraintScene, SketchFeatureParameters } from "@/types";

function sketch(): SketchFeatureParameters {
  return {
    plane_id: "ref-plane-xy",
    plane_frame: null,
    vertices: [
      { vertex_id: "vertex-1", kind: "center", x: 0, y: 0, is_fixed: false },
      { vertex_id: "vertex-2", kind: "endpoint", x: 10, y: 0, is_fixed: false },
      { vertex_id: "vertex-3", kind: "endpoint", x: 20, y: 0, is_fixed: false },
    ],
    lines: [
      {
        line_id: "line-1",
        start_vertex_id: "vertex-2",
        end_vertex_id: "vertex-3",
        start_x: 10,
        start_y: 0,
        end_x: 20,
        end_y: 0,
        constraint: null,
        is_construction: false,
        generated_by: null,
      },
    ],
    circles: [
      {
        circle_id: "circle-1",
        center_vertex_id: "vertex-1",
        center_x: 0,
        center_y: 0,
        radius: 10,
        is_construction: false,
        generated_by: null,
      },
    ],
  } as unknown as SketchFeatureParameters;
}

function pocBadge(): SketchConstraintScene {
  return {
    constraintId: "constraint-point-on-circle-1",
    planeId: "ref-plane-xy",
    kind: "point_on_circle",
    entityId: "vertex-2",
    relatedEntityId: "circle-1",
    label: "O",
    isSelected: false,
    position: [0, 0, 0],
  };
}

describe("endpointDragConstraintDeltas", () => {
  it("moves a point_on_circle badge with the dragged circle centre", () => {
    const deltas = endpointDragConstraintDeltas(
      [pocBadge()],
      sketch(),
      "vertex-1", // dragging the centre
      [5, 0], // centre delta
      "ref-plane-xy",
      null,
    );
    expect(deltas.get("constraint-point-on-circle-1")).toEqual([5, 0, 0]);
  });

  it("does not move a point_on_circle badge when dragging an unrelated vertex", () => {
    const deltas = endpointDragConstraintDeltas(
      [pocBadge()],
      sketch(),
      "vertex-3", // dragging the free line end, not the centre
      [5, 0],
      "ref-plane-xy",
      null,
    );
    expect(deltas.get("constraint-point-on-circle-1")).toBeUndefined();
  });
});
