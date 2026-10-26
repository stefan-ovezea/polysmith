// @vitest-environment node
//
// The snap matrix: every target a user can draw onto, measured on one sketch.
//
// Written after reading SolidCraft, whose snapping is an *intent string* the core
// command consumes ("mid:<line>", "on:<curve>:x,y", "vertex:x,y,z") and which the
// core turns into a point AND its constraint atomically - which is why a line drawn
// onto geometry there simply stays. PolySmith splits it differently: the core emits
// candidate points (endpoint, midpoint, center, quadrant, intersection, projected),
// the UI decides which one wins and runs its own dynamic geometry (line body, circle
// body, tangent, perpendicular, parallel, alignment), and the UI then *asks* the core
// to attach - through snap bookkeeping that crosses draft -> commit -> effect.
//
// So this matrix records TWO things per cell, because they fail independently:
//   snap    what the resolver chose and where it put the point
//   binding what it recorded that a later attachment can be built from
//
// A cell with a correct position and an empty binding is exactly the reported bug:
// the geometry looks attached and nothing holds it.
//
// `contract` cells are asserted - they are the core-supplied kinds that already
// work, and they must keep working. `measured` cells are printed only: they are the
// honesty of this file, not an endorsement. As each is fixed it moves into
// `contract`. Nothing here changes behaviour.

import { describe, expect, it } from "vitest";

import { defaultSelectionFilter } from "../selectionFilterState";
import {
  resolveSnappedSketchPoint,
  type RawSketchPoint,
  type SketchSnapCandidate,
} from "./snapResolution";

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

// One sketch holding one of everything a snap can target.
//
//   line-1      (0,0)->(100,0)     horizontal host, midpoint (50,0)
//   line-2      (50,-60)->(50,-20) vertical stem, clear of line-1
//   circle-1    centre (140,0) r 20 -> perimeter at (160,0)
//   arc-1       centre (0,100) r 20, quadrant at (20,100)
//   line-3/4    cross at (20,40): (0,40)->(40,40) and (20,20)->(20,60)
function matrixSketch() {
  const line = (
    id: string,
    x1: number,
    y1: number,
    x2: number,
    y2: number,
  ) => ({
    line_id: id,
    start_vertex_id: `${id}-a`,
    end_vertex_id: `${id}-b`,
    start_x: x1,
    start_y: y1,
    end_x: x2,
    end_y: y2,
    constraint: null,
    is_construction: false,
    generated_by: null,
  });
  return {
    plane_id: "ref-plane-xy",
    plane_frame: null,
    points: [],
    lines: [
      line("line-1", 0, 0, 100, 0),
      line("line-2", 50, -60, 50, -20),
      line("line-3", 0, 40, 40, 40),
      line("line-4", 20, 20, 20, 60),
    ],
    circles: [
      {
        circle_id: "circle-1",
        center_vertex_id: "circle-1-c",
        center_x: 140,
        center_y: 0,
        radius: 20,
        is_construction: false,
        generated_by: null,
      },
    ],
    arcs: [
      {
        arc_id: "arc-1",
        start_vertex_id: "arc-1-a",
        end_vertex_id: "arc-1-b",
        center_vertex_id: "arc-1-c",
        center_x: 0,
        center_y: 100,
        radius: 20,
        start_x: 20,
        start_y: 100,
        end_x: 0,
        end_y: 120,
        ccw: true,
        is_construction: false,
        generated_by: null,
      },
    ],
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

// What the core emits for that sketch: the static candidates. Hand-written here
// because this file measures the UI side; the core-side emission is `endpoint`,
// `midpoint`, `center`, `quadrant`, `intersection`, `projected`.
function coreCandidates(): SketchSnapCandidate[] {
  // Faithful to what appendCoreSnapCandidates() builds: a midpoint carries its host
  // line id (t = 0.5), an endpoint carries endpointHostLineId. Center and quadrant
  // carry NO host in the product mapping - noted below, because that means a snap to
  // a centre or a quadrant has nothing to attach to.
  const candidate = (
    local: [number, number],
    label: string,
    kind: SketchSnapCandidate["kind"],
    extra: Partial<SketchSnapCandidate> = {},
  ) =>
    ({
      local,
      world: [local[0], local[1], 0],
      label,
      kind,
      ...extra,
    }) as SketchSnapCandidate;
  return [
    candidate([0, 0], "Endpoint", "endpoint", { endpointHostLineId: "line-1" }),
    candidate([100, 0], "Endpoint", "endpoint", { endpointHostLineId: "line-1" }),
    candidate([50, 0], "Midpoint", "midpoint", {
      hostLineId: "line-1",
      tValue: 0.5,
    }),
    candidate([50, -60], "Endpoint", "endpoint", { endpointHostLineId: "line-2" }),
    candidate([50, -20], "Endpoint", "endpoint", { endpointHostLineId: "line-2" }),
    candidate([140, 0], "Center", "center"),
    candidate([160, 0], "Quadrant", "quadrant"),
    candidate([20, 100], "Quadrant", "quadrant"),
    candidate([20, 40], "Intersection", "intersection"),
  ];
}

type Cell = {
  name: string;
  cursor: [number, number];
  draftStart: [number, number];
};

const cells: Cell[] = [
  // Targets with a core candidate behind them.
  { name: "line endpoint", cursor: [100.6, 0.5], draftStart: [-40, -40] },
  { name: "line midpoint", cursor: [50.4, 0.5], draftStart: [-40, -40] },
  { name: "circle centre", cursor: [140.5, 0.6], draftStart: [-40, -40] },
  { name: "arc quadrant", cursor: [20.5, 100.6], draftStart: [-40, -40] },
  { name: "line x line intersection", cursor: [20.4, 40.5], draftStart: [-40, -40] },
  // Targets that are UI-side dynamic geometry.
  { name: "line body (t != 0.5)", cursor: [30.5, 0.6], draftStart: [-40, -40] },
  { name: "circle perimeter", cursor: [160.6, 0.6], draftStart: [-40, -40] },
  { name: "arc perimeter", cursor: [14.5, 114.5], draftStart: [-40, -40] },
  { name: "vertical stem body", cursor: [50.6, -40.5], draftStart: [-140, -140] },
  // Direction inference, needs a previous point.
  { name: "horizontal alignment", cursor: [70.5, 0.6], draftStart: [-40, -40] },
  { name: "parallel host", cursor: [30.5, 6.0], draftStart: [30, 40] },
  { name: "perpendicular host", cursor: [30.5, 6.0], draftStart: [30, 40] },
];

function resolveCell(cell: Cell) {
  const raw: RawSketchPoint = {
    local: cell.cursor,
    world: [cell.cursor[0], cell.cursor[1], 0],
  };
  return resolveSnappedSketchPoint({
    rawPoint: raw,
    draftStartLocal: cell.draftStart,
    sketchSnapCandidates: coreCandidates(),
    sketchParameters: matrixSketch() as never,
    sketchConstraints: [],
    dynamicSnapsEnabled: true,
    filter: defaultSelectionFilter,
    activeSketchPlaneId: "ref-plane-xy",
    activeSketchPlaneFrame: null,
    currentGridSpacing: 10,
    worldUnitsPerPixel: 1,
    gridSnapScreenDistancePx: 0,
    sketchSnapDistance: 5,
    labels,
  });
}

// The binding is what a later attachment can be built from. If this is empty, the
// point can look perfectly placed and still be attached to nothing.
function bindingOf(result: ReturnType<typeof resolveCell>): string {
  const parts: string[] = [];
  if (result.snapMidpointHostLineId) {
    parts.push(`midpoint:${result.snapMidpointHostLineId}`);
  }
  if (result.snapEndpointHostLineId) {
    parts.push(`endpoint:${result.snapEndpointHostLineId}`);
  }
  if (result.snapLineBodyHostLineId) {
    parts.push(`lineBody:${result.snapLineBodyHostLineId}`);
  }
  if (result.snapCircleBodyHostCircleId) {
    parts.push(`circleBody:${result.snapCircleBodyHostCircleId}`);
  }
  if (result.snapIntersectionLineIds) {
    parts.push(`intersection:${result.snapIntersectionLineIds.join("+")}`);
  }
  if (result.snapTangentCircleId) {
    parts.push(`tangent:${result.snapTangentCircleId}`);
  }
  if (result.snapPerpendicularHostLineId) {
    parts.push(`perp:${result.snapPerpendicularHostLineId}`);
  }
  if (result.snapParallelHostLineId) {
    parts.push(`parallel:${result.snapParallelHostLineId}`);
  }
  if (result.snapAxisLock) {
    parts.push(`axis:${result.snapAxisLock}`);
  }
  return parts.length > 0 ? parts.join(" ") : "—";
}

describe("snap matrix", () => {
  it("contract: the core-supplied kinds resolve and carry a binding", () => {
    // Only what is proven to work today. Everything else is in `knownGaps` below and
    // is printed, not asserted - a matrix that lies about the broken cells is worse
    // than no matrix.
    const contract = ["line endpoint", "line midpoint"];
    for (const name of contract) {
      const cell = cells.find((entry) => entry.name === name);
      expect(cell, `cell ${name} exists`).toBeDefined();
      const result = resolveCell(cell as Cell);
      expect(result.snapLabel, `${name}: resolved a label`).not.toBe(labels.grid);
      expect(bindingOf(result), `${name}: carries a binding`).not.toBe("—");
    }
  });

  // Measured on 2026-10-26 with the fixture above. Each of these loses the binding,
  // so nothing can attach: the label may even be a feature ("Quadrant") while the
  // binding is empty, or the axis lock / alignment steals the label and the position
  // still comes from the candidate.
  const knownGaps = [
    "circle centre", // centre snap carries no host id, so no attachment exists
    "arc quadrant", // quadrant snap carries no host id either
    "line x line intersection", // core emits the pair; the mapping drops it
    "line body (t != 0.5)", // body snaps lose to the alignment/axis label
    "circle perimeter", // quadrant wins the label, no body binding recorded
    "vertical stem body",
    "horizontal alignment",
    "parallel host", // axis lock wins before the parallel inference can
    "perpendicular host",
  ];

  it("prints the whole matrix, including the cells that are known to be broken", () => {
    const rows: string[] = [
      "| target | snap label | placed at | binding |",
      "|---|---|---|---|",
    ];
    for (const cell of cells) {
      const result = resolveCell(cell);
      const placed = `${result.local[0].toFixed(2)}, ${result.local[1].toFixed(2)}`;
      rows.push(
        `| ${cell.name} | ${result.snapLabel ?? "(none)"} | ${placed} | ${bindingOf(result)} |`,
      );
    }
    // Printed so the real state is visible in the test output; the table is the
    // deliverable of this file, not an assertion about it.
    process.stdout.write(`\n${rows.join("\n")}\n`);
    expect(rows.length).toBe(cells.length + 2);
  });
});
