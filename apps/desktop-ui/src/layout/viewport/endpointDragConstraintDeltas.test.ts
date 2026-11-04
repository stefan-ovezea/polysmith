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

  // Reported 2026-11-04: "a totally unrelated line steals the H constraint during
  // preview and jumps back to its own line during commit". The line-mounted badge
  // pass used to move EVERY line's badge, because its guard only asked whether the
  // dragged vertex belonged to *some* line.
  it("moves only the dragged line's H badge, never another line's", () => {
    const twoLines = sketch();
    twoLines.lines = [
      twoLines.lines![0],
      {
        ...twoLines.lines![0],
        line_id: "line-2",
        start_vertex_id: "vertex-4",
        end_vertex_id: "vertex-5",
        start_x: 0,
        start_y: 40,
        end_x: 50,
        end_y: 40,
      } as (typeof twoLines.lines)[number],
    ];
    const draggedBadge: SketchConstraintScene = {
      constraintId: "constraint-horizontal-line-1",
      planeId: "ref-plane-xy",
      kind: "horizontal",
      entityId: "line-1",
      relatedEntityId: null,
      label: "H",
      isSelected: false,
      position: [15, 0, 0],
    };
    const untouchedBadge: SketchConstraintScene = {
      ...draggedBadge,
      constraintId: "constraint-horizontal-line-2",
      entityId: "line-2",
      position: [25, 40, 0],
    };
    const deltas = endpointDragConstraintDeltas(
      [draggedBadge, untouchedBadge],
      twoLines,
      "vertex-3", // an endpoint of line-1 only
      [10, 0],
      "ref-plane-xy",
      null,
    );
    expect(deltas.get("constraint-horizontal-line-1")).toEqual([5, 0, 0]);
    expect(deltas.get("constraint-horizontal-line-2")).toBeUndefined();
  });
});
