// @vitest-environment node
// S8: a refused dimension drive must be REPORTED, not swallowed.
//
// The old code ended in `.catch(() => {})`, so a value the core rejected left
// the typed number on screen but not in the model, with no message. These cases
// pin the reporting contract and that the rejection is still consumed.
import { describe, expect, it, vi } from "vitest";

import type { SketchFeatureParameters } from "@/types";
import { applyPendingDraftDimensionExpressions } from "./draftDimensionPostCommit";

function makeSketch(): SketchFeatureParameters {
  return {
    lines: [
      {
        line_id: "line-7",
        start_vertex_id: "vertex-1",
        end_vertex_id: "vertex-2",
        start_x: 0,
        start_y: 0,
        end_x: 10,
        end_y: 0,
        is_construction: false,
      },
    ],
    circles: [],
    arcs: [],
    polygons: [],
  } as unknown as SketchFeatureParameters;
}

const pendingLine = {
  tool: "line" as const,
  fromLineCount: 0,
  fromCircleCount: 0,
  fromPolygonCount: 0,
  fromArcCount: 0,
  expressions: { length: "20", angle: "" },
};

describe("draft dimension drive failures", () => {
  it("reports the dimension id, the expression and the error", async () => {
    const onDriveFailed = vi.fn();
    const failure = new Error("sketch over-constrained");
    const updateSketchDimension = vi.fn(async () => {
      throw failure;
    });

    applyPendingDraftDimensionExpressions({
      pendingDraftDimensionExpressionsRef: { current: pendingLine as never },
      sketch: makeSketch(),
      updateSketchDimension,
      onDriveFailed,
    });
    // Let the swallowed-then-reported promise settle.
    await Promise.resolve();
    await Promise.resolve();

    expect(updateSketchDimension).toHaveBeenCalledWith("dim-line-line-7", "20");
    expect(onDriveFailed).toHaveBeenCalledTimes(1);
    expect(onDriveFailed).toHaveBeenCalledWith({
      dimensionId: "dim-line-line-7",
      expression: "20",
      error: failure,
    });
  });

  it("does not report when the drive succeeds", async () => {
    const onDriveFailed = vi.fn();
    const updateSketchDimension = vi.fn(async () => {});

    applyPendingDraftDimensionExpressions({
      pendingDraftDimensionExpressionsRef: { current: pendingLine as never },
      sketch: makeSketch(),
      updateSketchDimension,
      onDriveFailed,
    });
    await Promise.resolve();
    await Promise.resolve();

    expect(updateSketchDimension).toHaveBeenCalled();
    expect(onDriveFailed).not.toHaveBeenCalled();
  });

  it("clears the pending record once applied, even when the drive fails", async () => {
    const ref = { current: pendingLine as never };
    applyPendingDraftDimensionExpressions({
      pendingDraftDimensionExpressionsRef: ref,
      sketch: makeSketch(),
      updateSketchDimension: vi.fn(async () => {
        throw new Error("refused");
      }),
      onDriveFailed: vi.fn(),
    });
    await Promise.resolve();
    // The pending record is consumed by the command, not by the drive result —
    // a retry would otherwise re-send the same rejected expression forever.
    expect(ref.current).toBeNull();
  });
});
