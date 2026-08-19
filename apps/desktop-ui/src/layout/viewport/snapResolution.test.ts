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

vi.mock("@/lib/planegcsSolver", () => ({
  getBridge: () => ({ wrapper: {} }),
  ensureBridge: vi.fn(),
}));

const labels = {
  grid: "Grid",
  axisLockHorizontal: "Horizontal",
  axisLockVertical: "Vertical",
  onLine: "On line",
  onCircle: "On circle",
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
      { vertex_id: "p1", x: -10, y: 0, is_fixed: false },
      { vertex_id: "p2", x: 10, y: 0, is_fixed: false },
    ],
    lines: [
      {
        line_id: "l1",
        start_vertex_id: "p1",
        end_vertex_id: "p2",
        start_x: -10,
        start_y: 0,
        end_x: 10,
        end_y: 0,
        constraint: null,
        is_construction: false,
        generated_by: null,
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
    texts: [],
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

  it("resolves tangent-to-circle snap via pure TS geometry", () => {
    const withCircle = {
      ...sketchParameters(),
      circles: [
        {
          circle_id: "c1",
          center_x: 5,
          center_y: 0,
          radius: 3,
          is_construction: false,
        },
      ],
    };
    // Tangent points from [0,0] to circle center [5,0] radius 3:
    //   tangentLen = sqrt(5²-3²) = 4
    //   alpha = asin(3/5) ≈ 0.6435
    //   tp1 = [4*cos(0.6435), 4*sin(0.6435)] ≈ [3.2, 2.4]
    //   tp2 = [4*cos(-0.6435), 4*sin(-0.6435)] ≈ [3.2, -2.4]
    // Cursor at [3.3, 2.3] is closest to tp1 → snap to that tangent line.

    const result = resolveSnappedSketchPoint({
      rawPoint: { local: [3.3, 2.3], world: [3.3, 0, 2.3] },
      draftStartLocal: [0, 0],
      sketchSnapCandidates: [],
      sketchParameters: withCircle,
      sketchConstraints: [],
      dynamicSnapsEnabled: true,
      filter: {
        ...defaultSelectionFilter,
        snap_nearest: false,
        snap_polar: false,
        snap_perpendicular: false,
        snap_parallel: false,
        snap_intersection: false,
      },
      activeSketchPlaneId: "ref-plane-xy",
      activeSketchPlaneFrame: null,
      currentGridSpacing: 10,
      worldUnitsPerPixel: 1,
      gridSnapScreenDistancePx: 0,
      sketchSnapDistance: 5,
      labels,
    });

    expect(result.snapLabel).toBe("Tangent");
    expect(result.snapTangentCircleId).toBe("c1");
    // Snapped position should be on the tangent line from draftStart through tp1
    expect(result.local[0]).toBeCloseTo(3.216, 1);
    expect(result.local[1]).toBeCloseTo(2.412, 1);
  });

  it("resolves perpendicular-to-line snap via speculative solver", () => {
    // draftStart at [0, 0], cursor at [0, 5], line from [-10, 0] to [10, 0]
    // draftStart IS on the line (y=0), cursor foot is [0,0] on the segment
    speculativeSolveMock.mockReturnValue({
      converged: true,
      position: [0, 5],
      distance: 0.1,
      solverStatus: 0,
    });

    const result = resolveSnappedSketchPoint({
      rawPoint: { local: [0.1, 4.9], world: [0.1, 0, 4.9] },
      draftStartLocal: [0, 0],
      sketchSnapCandidates: [],
      sketchParameters: sketchParameters(),
      sketchConstraints: [],
      dynamicSnapsEnabled: true,
      filter: {
        ...defaultSelectionFilter,
        snap_nearest: false,
        snap_polar: false,
        snap_parallel: false,
        snap_tangent: false,
        snap_intersection: false,
      },
      activeSketchPlaneId: "ref-plane-xy",
      activeSketchPlaneFrame: null,
      currentGridSpacing: 10,
      worldUnitsPerPixel: 1,
      gridSnapScreenDistancePx: 0,
      sketchSnapDistance: 5,
      labels,
    });

    expect(result.snapLabel).toBe("Perpendicular");
    expect(result.snapPerpendicularHostLineId).toBe("l1");
    expect(speculativeSolveMock).toHaveBeenCalled();
  });
});
