// @vitest-environment node
import { describe, expect, it } from "vitest";

import {
  autoConstrainSummaryIsEmpty,
  readSketchSnapshot,
  summarizeAutoConstrainRun,
} from "./autoConstrainSummary";

function feature(parameters: {
  solver_dofs?: number;
  dimensions?: Array<{ driven?: boolean; is_auto?: boolean; expression?: string }>;
  vertices?: Array<{ is_fixed?: boolean }>;
  constraints?: unknown[];
  line_relations?: unknown[];
}) {
  return { sketch_parameters: parameters };
}

describe("auto-constrain summary", () => {
  it("reads only driving dimensions and pinned corners", () => {
    const snapshot = readSketchSnapshot(
      feature({
        solver_dofs: 2,
        dimensions: [
          // A driving length the pass added.
          { driven: false, is_auto: false },
          // An automatic measurement: not something the pass decided.
          { driven: false, is_auto: true, expression: "" },
          // A reference dimension.
          { driven: true, is_auto: false },
        ],
        vertices: [{ is_fixed: true }, { is_fixed: false }, {}],
        constraints: [{}, {}],
        line_relations: [{}],
      }),
    );
    expect(snapshot).toEqual({
      dofs: 2,
      dimensions: 1,
      pins: 1,
      constraints: 3,
    });
  });

  it("treats a missing solver number as unknown, not as zero", () => {
    expect(readSketchSnapshot(feature({ solver_dofs: -1 })).dofs).toBe(null);
    expect(readSketchSnapshot(null).dofs).toBe(null);
  });

  it("reports the deltas the run produced", () => {
    const before = readSketchSnapshot(
      feature({ solver_dofs: 4, dimensions: [], vertices: [{}] }),
    );
    const after = readSketchSnapshot(
      feature({
        solver_dofs: 0,
        dimensions: [{ driven: false, is_auto: false }, { driven: false, is_auto: false }],
        vertices: [{ is_fixed: true }],
      }),
    );
    const summary = summarizeAutoConstrainRun(before, after);
    expect(summary).toEqual({
      dofsBefore: 4,
      dofsAfter: 0,
      dimensions: 2,
      pins: 1,
      constraints: 0,
    });
    expect(autoConstrainSummaryIsEmpty(summary)).toBe(false);
  });

  it("never reports a negative delta when the run removed something", () => {
    const before = readSketchSnapshot(
      feature({ solver_dofs: 4, dimensions: [{}, {}], vertices: [{ is_fixed: true }] }),
    );
    const after = readSketchSnapshot(feature({ solver_dofs: 6, dimensions: [] }));
    const summary = summarizeAutoConstrainRun(before, after);
    expect(summary.dimensions).toBe(0);
    expect(summary.pins).toBe(0);
    expect(autoConstrainSummaryIsEmpty(summary)).toBe(true);
  });
});
