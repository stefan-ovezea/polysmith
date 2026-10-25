// Regression tests for the dimension editor's value routing.
//
// Numeric entries must always go to `update_sketch_dimension` as numbers.
// The old `parsed > 0` gate sent zero/negative numbers through the string
// branch, so a negative angle ("-45") was stored as an EXPRESSION and
// re-evaluated on every parameter change.  Non-positive lengths still
// reach the core as numbers; the core rejects them with its own
// validation message.

import { describe, expect, it } from "vitest";

import type { SketchDimensionScene } from "@/types";
import { submitDimensionEditorValue } from "./dimensionEditorInput";
import { dimensionCoreValue } from "./dimensionValueDisplay";

function dimension(
  kind: SketchDimensionScene["kind"],
): SketchDimensionScene {
  return {
    dimensionId: `dim-${kind}`,
    planeId: "ref-plane-xy",
    kind,
    entityId: "line-1",
    label: "",
    rawValue: 0,
    unitSuffix: "",
    isSelected: true,
    anchorStart: [0, 0, 0],
    anchorEnd: [0, 0, 0],
    dimensionStart: [0, 0, 0],
    dimensionEnd: [0, 0, 0],
    labelPosition: [0, 0, 0],
  };
}

async function submit(
  kind: SketchDimensionScene["kind"],
  rawValue: string,
): Promise<unknown> {
  const calls: unknown[] = [];
  await submitDimensionEditorValue({
    dimension: dimension(kind),
    rawValue,
    displayUnits: "mm",
    updateSketchDimension: async (_dimensionId, value) => {
      calls.push(value);
    },
    toCoreValue: (scene, displayValue) =>
      dimensionCoreValue({ dimension: scene, displayValue, sketch: null }),
  });
  expect(calls).toHaveLength(1);
  return calls[0];
}

describe("dimension editor value routing", () => {
  it("sends a negative angle numerically, not as an expression", async () => {
    expect(await submit("angle", "-45")).toBe(-45);
  });

  it("sends a zero/negative length numerically (core validates)", async () => {
    expect(await submit("line_length", "0")).toBe(0);
    expect(await submit("line_length", "-12.5")).toBe(-12.5);
  });

  it("sends a plain positive angle in display degrees", async () => {
    expect(await submit("angle", "45")).toBe(45);
  });

  it("still sends non-numeric text as an expression", async () => {
    expect(await submit("angle", "width * 2")).toBe("width * 2");
    expect(await submit("line_length", "width / 2")).toBe("width / 2");
  });
});
