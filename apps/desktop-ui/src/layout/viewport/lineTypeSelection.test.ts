// @vitest-environment node
// S1: the line-type segmented control maps four visible flavours onto the two
// payload inputs (line_type + is_construction). The subtle rule is that the
// reference flavours do NOT set the construction flag — the core marks them as
// construction geometry itself.
import { describe, expect, it } from "vitest";

import {
  lineTypeFlagsToSelection,
  lineTypeSelectionToFlags,
} from "./lineTypeSelection";

describe("line type selection ↔ flags", () => {
  it("solid and construction are the ordinary line with the flag toggled", () => {
    expect(lineTypeSelectionToFlags("solid")).toEqual({
      lineType: "",
      isConstruction: false,
    });
    expect(lineTypeSelectionToFlags("construction")).toEqual({
      lineType: "",
      isConstruction: true,
    });
  });

  it("reference flavours set line_type and leave the construction flag clear", () => {
    expect(lineTypeSelectionToFlags("centerline")).toEqual({
      lineType: "centerline",
      isConstruction: false,
    });
    expect(lineTypeSelectionToFlags("infinite")).toEqual({
      lineType: "infinite",
      isConstruction: false,
    });
  });

  it("round-trips every flavour", () => {
    const selections = ["solid", "construction", "centerline", "infinite"] as const;
    for (const selection of selections) {
      expect(lineTypeFlagsToSelection(lineTypeSelectionToFlags(selection))).toBe(
        selection,
      );
    }
  });

  it("treats a reference line type as authoritative over the flag", () => {
    // A construction centreline (possible in an old document) shows as
    // centreline: the line type wins, so the segment matches the geometry.
    expect(
      lineTypeFlagsToSelection({ lineType: "centerline", isConstruction: true }),
    ).toBe("centerline");
  });
});
