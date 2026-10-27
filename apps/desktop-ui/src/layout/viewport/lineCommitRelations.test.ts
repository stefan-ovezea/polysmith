// @vitest-environment node
import { describe, expect, it, vi } from "vitest";

import {
  applyPendingLineCommitRelations,
  lineCommitRelations,
  type PendingLineCommitRelationActions,
  type PendingLineCommitRelationRefs,
} from "./lineCommitRelations";

// A T-junction can be drawn from either end of the new line: start the second line
// ON the first line (the common way), or end it there. Only the end used to be
// dispatched, so a stem drawn from the host's midpoint got NO anchor at all - the
// lines merely touched, dragging the stem detached it and it stayed detached, and
// "Merge coincident points" had nothing to merge because nothing was ever bound.
// This pins both ends, for both anchor kinds.

type AnchorCall = {
  kind: "midpoint" | "pointLine";
  vertexId: string;
  hostLineId: string;
  t?: number;
};

function makeActions(calls: AnchorCall[]): PendingLineCommitRelationActions {
  return {
    setSketchMidpointAnchor: vi.fn((vertexId: string, hostLineId: string) => {
      calls.push({ kind: "midpoint", vertexId, hostLineId });
    }),
    setSketchPointLineAnchor: vi.fn(
      (vertexId: string, hostLineId: string, t: number) => {
        calls.push({ kind: "pointLine", vertexId, hostLineId, t });
      },
    ),
    setSketchPerpendicularConstraint: vi.fn(),
    setSketchLineConstraint: vi.fn(),
    setSketchTangentConstraint: vi.fn(),
    setSketchParallelConstraint: vi.fn(),
  };
}

function makeRefs(
  pending: {
    midpointAnchor?: PendingLineCommitRelationRefs["midpointAnchor"]["current"];
    pointLineAnchor?: PendingLineCommitRelationRefs["pointLineAnchor"]["current"];
  } = {},
): PendingLineCommitRelationRefs {
  const ref = <T,>(value: T) => ({ current: value });
  return {
    midpointAnchor: ref(pending.midpointAnchor ?? null),
    pointLineAnchor: ref(pending.pointLineAnchor ?? null),
    perpendicularConstraint: ref(null),
    axisConstraint: ref(null),
    tangentConstraint: ref(null),
    parallelConstraint: ref(null),
  };
}

// One host line already in the sketch (count 1), then the stem is committed, so the
// apply runs with a baseline of 1 and a current count of 2.
function sketchWithNewLine() {
  return {
    lines: [
      { line_id: "line-1", start_vertex_id: "v1", end_vertex_id: "v2" },
      {
        line_id: "line-2",
        start_vertex_id: "vertex-start",
        end_vertex_id: "vertex-end",
      },
    ],
  } as never;
}

function apply(
  refs: PendingLineCommitRelationRefs,
  actions: PendingLineCommitRelationActions,
) {
  applyPendingLineCommitRelations({
    sketchParameters: sketchWithNewLine(),
    previousLineCount: 1,
    currentLineCount: 2,
    refs,
    actions,
  });
}

describe("pending line commit anchors", () => {
  it("binds a midpoint anchor at the START of the new line", () => {
    const calls: AnchorCall[] = [];
    apply(
      makeRefs({
        midpointAnchor: {
          fromLineCount: 1,
          startHostLineId: "line-1",
          endHostLineId: null,
        },
      }),
      makeActions(calls),
    );
    expect(calls).toEqual([
      { kind: "midpoint", vertexId: "vertex-start", hostLineId: "line-1" },
    ]);
  });

  it("still binds a midpoint anchor at the END of the new line", () => {
    const calls: AnchorCall[] = [];
    apply(
      makeRefs({
        midpointAnchor: {
          fromLineCount: 1,
          startHostLineId: null,
          endHostLineId: "line-1",
        },
      }),
      makeActions(calls),
    );
    expect(calls).toEqual([
      { kind: "midpoint", vertexId: "vertex-end", hostLineId: "line-1" },
    ]);
  });

  it("binds a point-on-line anchor at the START of the new line", () => {
    const calls: AnchorCall[] = [];
    apply(
      makeRefs({
        pointLineAnchor: {
          fromLineCount: 1,
          startHost: { lineId: "line-1", t: 0.25 },
          endHost: null,
        },
      }),
      makeActions(calls),
    );
    expect(calls).toEqual([
      {
        kind: "pointLine",
        vertexId: "vertex-start",
        hostLineId: "line-1",
        t: 0.25,
      },
    ]);
  });

  it("prefers the midpoint binding when a snap carries both", () => {
    // A midpoint snap IS a point-on-line snap at t = 0.5; binding both would
    // create two anchors for one vertex and they would fight on every drag.
    const calls: AnchorCall[] = [];
    apply(
      makeRefs({
        midpointAnchor: {
          fromLineCount: 1,
          startHostLineId: "line-1",
          endHostLineId: null,
        },
        pointLineAnchor: {
          fromLineCount: 1,
          startHost: { lineId: "line-1", t: 0.5 },
          endHost: null,
        },
      }),
      makeActions(calls),
    );
    expect(calls).toEqual([
      { kind: "midpoint", vertexId: "vertex-start", hostLineId: "line-1" },
    ]);
  });
});

describe("a point attachment beats an angle relation to the same line", () => {
  // Reported: two lines drawn from a stem, one near-perpendicular - and the stem could
  // then only be dragged along its own axis, with a relation badge on that line.
  // A perpendicular/parallel relation fixes the angle BETWEEN the two lines, so the
  // stem legitimately stops rotating. Starting a line ON a line says nothing about the
  // angle; the anchor is the truth and the relation must not be inferred.
  const snapToStem = {
    local: [50, 0] as [number, number],
    snapMidpointHostLineId: "stem",
    snapMidpointT: 0.5,
    snapPerpendicularHostLineId: "stem",
    snapParallelHostLineId: "stem",
  };

  it("drops perpendicular and parallel when the host is the anchored line", () => {
    const relations = lineCommitRelations({
      sketchPoint: snapToStem,
      fromLineCount: 1,
      startMidpointHostLineId: null,
      startLineBodyHost: null,
    });
    expect(relations.midpointAnchor?.endHostLineId).toBe("stem");
    expect(relations.perpendicularConstraint).toBeNull();
    expect(relations.parallelConstraint).toBeNull();
  });

  it("keeps them when the angle host is a different line", () => {
    const relations = lineCommitRelations({
      sketchPoint: { ...snapToStem, snapPerpendicularHostLineId: "other" },
      fromLineCount: 1,
      startMidpointHostLineId: null,
      startLineBodyHost: null,
    });
    expect(relations.midpointAnchor?.endHostLineId).toBe("stem");
    expect(relations.perpendicularConstraint?.hostLineId).toBe("other");
  });
});