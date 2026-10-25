// Unit tests for the dimension display/core value convention.
//
// The IPC `update_sketch_dimension` boundary takes the DISPLAYED value
// and converts it once (degrees→radians for angle kinds, diameter→radius
// for diameter-mode circles).  Converting in the UI as well double-
// converted every edit: typing 45° on an angle dimension stored 0.79°,
// which collapsed the sketch.  This pins the convention so the editor's
// value, the cancel-restore path and the label-drag commit all agree.

import { describe, expect, it } from "vitest";

import type { SketchDimensionScene } from "@/types";
import {
  dimensionCoreValue,
  dimensionDisplayValue,
} from "./dimensionValueDisplay";

function dimension(
  kind: SketchDimensionScene["kind"],
  dimensionId = `dim-${kind}`,
): SketchDimensionScene {
  return {
    dimensionId,
    planeId: "ref-plane-xy",
    kind,
    entityId: "line-1",
    label: "",
    rawValue: 0,
    unitSuffix: "",
    isSelected: false,
    anchorStart: [0, 0, 0],
    anchorEnd: [0, 0, 0],
    dimensionStart: [0, 0, 0],
    dimensionEnd: [0, 0, 0],
    labelPosition: [0, 0, 0],
  };
}

describe("dimension value units at the IPC boundary", () => {
  it("sends a typed angle in display degrees (the core converts once)", () => {
    for (const kind of ["angle", "line_angle"] as const) {
      expect(
        dimensionCoreValue({
          dimension: dimension(kind),
          displayValue: 45,
          sketch: null,
        }),
      ).toBe(45);
    }
  });

  it("renders a stored radian angle as display degrees", () => {
    for (const kind of ["angle", "line_angle"] as const) {
      expect(
        dimensionDisplayValue({
          dimension: dimension(kind),
          coreValue: Math.PI / 4,
          sketch: null,
        }),
      ).toBeCloseTo(45, 10);
    }
  });

  it("round-trips display degrees through the core (radians) representation", () => {
    const angle = dimension("angle");
    const typed = 37.5;
    const coreRadians = (typed * Math.PI) / 180;
    const display = dimensionDisplayValue({
      dimension: angle,
      coreValue: coreRadians,
      sketch: null,
    });
    expect(
      dimensionCoreValue({
        dimension: angle,
        displayValue: display,
        sketch: null,
      }),
    ).toBeCloseTo(typed, 10);
  });

  it("keeps an unsigned display value for a negative directed angle", () => {
    expect(
      dimensionDisplayValue({
        dimension: dimension("angle"),
        coreValue: -Math.PI / 4,
        sketch: null,
      }),
    ).toBeCloseTo(45, 10);
  });

  it("passes length kinds through unchanged", () => {
    const length = dimension("line_length");
    expect(
      dimensionCoreValue({ dimension: length, displayValue: 12.5, sketch: null }),
    ).toBe(12.5);
    expect(
      dimensionDisplayValue({ dimension: length, coreValue: 12.5, sketch: null }),
    ).toBe(12.5);
  });
});
