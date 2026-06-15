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
  gridSnapScreenDistancePx = 0,
}: {
  rawPoint: RawSketchPoint;
  candidates?: SketchSnapCandidate[];
  dynamicSnapsEnabled?: boolean;
  filter?: typeof defaultSelectionFilter;
  gridSnapScreenDistancePx?: number;
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
    gridSnapScreenDistancePx,
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
    expect(result.snapFeedbackSource).toBe("object");
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
    expect(result.snapFeedbackSource).toBe("object");
    expect(result.local).toEqual([1, 0]);
    expect(result.snapLineBodyHostLineId).toBe("l1");
    expect(speculativeSolveMock).not.toHaveBeenCalled();
    expect(speculativeMultiSolveMock).not.toHaveBeenCalled();
  });

  it("marks grid snaps separately so cursor feedback does not steal to grid", () => {
    const result = resolvePoint({
      rawPoint: { local: [9.5, 9.5], world: [9.5, 0, 9.5] },
      candidates: [],
      dynamicSnapsEnabled: false,
      gridSnapScreenDistancePx: 1,
    });

    expect(result.snapLabel).toBe("Grid");
    expect(result.local).toEqual([10, 10]);
    expect(result.snapFeedbackSource).toBe("grid");
  });

  it("keeps an object snap latched until that target leaves tolerance", () => {
    const first = resolvePoint({
      rawPoint: { local: [0.5, 0], world: [0.5, 0, 0] },
      candidates: [
        {
          local: [0, 0],
          label: "Endpoint A",
          kind: "endpoint",
          endpointHostLineId: "a",
        },
        {
          local: [2, 0],
          label: "Endpoint B",
          kind: "endpoint",
          endpointHostLineId: "b",
        },
      ],
    });
    expect(first.snapTargetKey).toBe("static:endpoint:a");

    const latched = resolveSnappedSketchPoint({
      rawPoint: { local: [1.6, 0], world: [1.6, 0, 0] },
      draftStartLocal: [0, 0],
      sketchSnapCandidates: [
        {
          local: [0, 0],
          label: "Endpoint A",
          kind: "endpoint",
          endpointHostLineId: "a",
        },
        {
          local: [2, 0],
          label: "Endpoint B",
          kind: "endpoint",
          endpointHostLineId: "b",
        },
      ],
      sketchParameters: sketchParameters(),
      sketchConstraints: [],
      dynamicSnapsEnabled: true,
      objectSnapLatchKey: first.snapTargetKey,
      filter: defaultSelectionFilter,
      activeSketchPlaneId: "ref-plane-xy",
      activeSketchPlaneFrame: null,
      currentGridSpacing: 10,
      worldUnitsPerPixel: 1,
      gridSnapScreenDistancePx: 0,
      sketchSnapDistance: 2,
      labels,
    });

    expect(latched.snapLabel).toBe("Endpoint A");
    expect(latched.local).toEqual([0, 0]);
    expect(latched.snapTargetKey).toBe("static:endpoint:a");
  });
});
