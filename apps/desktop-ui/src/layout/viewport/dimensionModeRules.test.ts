// @vitest-environment node
// S1: the seven sketch dimension modes and what each one accepts.
//
// Before this, clicking a LINE while Radius / Diameter / Arc length was armed
// fell through to the auto path and quietly created a linear dimension — the
// mode promised one thing and produced another. The acceptance table is now the
// single source for both the picking logic and the panel hint.
import { describe, expect, it } from "vitest";

import {
  DIMENSION_MODE_LABEL_SUFFIX,
  DIMENSION_TOOL_MODES,
  dimensionModeAcceptsEntity,
  dimensionModeHintKey,
} from "./dimensionModeRules";

const KINDS = ["line", "circle", "arc", "polygon", "spline"] as const;

describe("dimension tool modes", () => {
  it("offers exactly the seven sketch modes (the drawing-sheet modes are gone)", () => {
    expect([...DIMENSION_TOOL_MODES]).toEqual([
      "auto",
      "linear",
      "aligned",
      "angular",
      "radius",
      "diameter",
      "arc_length",
    ]);
    // Every mode has a label suffix and a hint.
    for (const mode of DIMENSION_TOOL_MODES) {
      expect(DIMENSION_MODE_LABEL_SUFFIX[mode]).toBeTruthy();
      expect(dimensionModeHintKey(mode)).toMatch(/^viewport\.dimensionHint/);
    }
  });

  it("auto / linear / aligned accept any entity", () => {
    for (const mode of ["auto", "linear", "aligned"] as const) {
      for (const kind of KINDS) {
        expect(dimensionModeAcceptsEntity(mode, kind)).toBe(true);
      }
    }
  });

  it("angular accepts lines only", () => {
    expect(dimensionModeAcceptsEntity("angular", "line")).toBe(true);
    for (const kind of ["circle", "arc", "polygon", "spline"] as const) {
      expect(dimensionModeAcceptsEntity("angular", kind)).toBe(false);
    }
  });

  it("radius and diameter accept circles and arcs only", () => {
    for (const mode of ["radius", "diameter"] as const) {
      expect(dimensionModeAcceptsEntity(mode, "circle")).toBe(true);
      expect(dimensionModeAcceptsEntity(mode, "arc")).toBe(true);
      expect(dimensionModeAcceptsEntity(mode, "line")).toBe(false);
      expect(dimensionModeAcceptsEntity(mode, "polygon")).toBe(false);
      expect(dimensionModeAcceptsEntity(mode, null)).toBe(false);
    }
  });

  it("arc length accepts arcs only", () => {
    expect(dimensionModeAcceptsEntity("arc_length", "arc")).toBe(true);
    for (const kind of ["line", "circle", "polygon", "spline"] as const) {
      expect(dimensionModeAcceptsEntity("arc_length", kind)).toBe(false);
    }
  });
});
