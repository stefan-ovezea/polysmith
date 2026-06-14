// @vitest-environment node
// @ts-nocheck
import { beforeEach, describe, expect, it, vi } from "vitest";

import { defaultSelectionFilter } from "../selectionFilterState";
import {
  resolveSnappedSketchPoint,
  type RawSketchPoint,
  type SketchSnapCandidate,
} from "./snapResolution";

const { speculativeSolveMock, speculativeMultiSolveMock } = vi.hoisted(() => ({
  speculativeSolveMock: vi.fn(),
  speculativeMultiSolveMock: vi.fn(),
}));

vi.mock("@/lib/speculativeSolve", () => ({
  speculativeSolve: speculativeSolveMock,
  speculativeMultiSolve: speculativeMultiSolveMock,
}));

const labels = {
  grid: "Grid",
  axisLockHorizontal: "Horizontal",
  axisLockVertical: "Vertical",
  onLine: "On line",
  tangent: "Tangent",
  perpendicular: "Perpendicular",
  parallel: "Parallel",
  intersection: "Intersection",
};

function sketchParameters() {
  return {
    plane_id: "ref-plane-xy",
    plane_frame: null,
    points: [
      { point_id: "p1", x: -10, y: 0, is_fixed: false },
      { point_id: "p2", x: 10, y: 0, is_fixed: false },
    ],
    lines: [
      {
        line_id: "l1",
        start_point_id: "p1",
        end_point_id: "p2",
        start_x: -10,
        start_y: 0,
        end_x: 10,
        end_y: 0,
        constraint: null,
        is_construction: false,
      },
    ],
    circles: [],
    arcs: [],
    polygons: [],
    dimensions: [],
    line_relations: [],
    midpoint_anchors: [],
    point_line_anchors: [],
    projections: [],
    profiles: [],
  };
}

function resolvePoint({
  rawPoint,
  candidates = [],
  dynamicSnapsEnabled = true,
  filter = defaultSelectionFilter,
}: {
  rawPoint: RawSketchPoint;
  candidates?: SketchSnapCandidate[];
  dynamicSnapsEnabled?: boolean;
  filter?: typeof defaultSelectionFilter;
}) {
  return resolveSnappedSketchPoint({
    rawPoint,
    draftStartLocal: [0, 0],
    sketchSnapCandidates: candidates,
    sketchParameters: sketchParameters(),
    sketchConstraints: [],
    dynamicSnapsEnabled,
    filter,
    activeSketchPlaneId: "ref-plane-xy",
    activeSketchPlaneFrame: null,
    currentGridSpacing: 10,
    worldUnitsPerPixel: 1,
    gridSnapScreenDistancePx: 0,
    sketchSnapDistance: 5,
    labels,
  });
}

describe("resolveSnappedSketchPoint performance guards", () => {
  beforeEach(() => {
    speculativeSolveMock.mockReset();
    speculativeMultiSolveMock.mockReset();
    speculativeSolveMock.mockReturnValue({
      converged: true,
      position: [1, 0],
      distance: 1,
      solverStatus: 0,
    });
  });

  it("does not run dynamic solver snaps when a priority endpoint snap wins", () => {
    const result = resolvePoint({
      rawPoint: { local: [1, 1], world: [1, 1, 0] },
      candidates: [
        {
          local: [1, 1],
          world: [1, 1, 0],
          label: "Endpoint",
          kind: "endpoint",
          endpointHostLineId: "l1",
        } as SketchSnapCandidate,
      ],
    });

    expect(result.snapLabel).toBe("Endpoint");
    expect(speculativeSolveMock).not.toHaveBeenCalled();
    expect(speculativeMultiSolveMock).not.toHaveBeenCalled();
  });

  it("honors the dynamic snap opt-out used by endpoint drag", () => {
    const result = resolvePoint({
      rawPoint: { local: [1, 1], world: [1, 1, 0] },
      dynamicSnapsEnabled: false,
    });

    expect(result.snapLabel).toBeNull();
    expect(speculativeSolveMock).not.toHaveBeenCalled();
    expect(speculativeMultiSolveMock).not.toHaveBeenCalled();
  });

  it("resolves nearest line-body snaps without invoking the solver", () => {
    const result = resolvePoint({
      rawPoint: { local: [1, 1], world: [1, 1, 0] },
      filter: {
        ...defaultSelectionFilter,
        snap_polar: false,
        snap_perpendicular: false,
        snap_parallel: false,
        snap_intersection: false,
        snap_tangent: false,
      },
    });

    expect(result.snapLabel).toBe("On line");
    expect(result.local).toEqual([1, 0]);
    expect(result.snapLineBodyHostLineId).toBe("l1");
    expect(speculativeSolveMock).not.toHaveBeenCalled();
    expect(speculativeMultiSolveMock).not.toHaveBeenCalled();
  });
});
