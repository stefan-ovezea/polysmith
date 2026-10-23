// Line-type ↔ flags mapping for the sketch line tool (S1).
//
// The segmented control shows four flavours, but the IPC payload has two
// inputs: `line_type` ("", "centerline", "infinite") and `is_construction`.
// Reference flavours are construction geometry in the CORE, so they are sent
// with the flag clear instead of setting it here — otherwise the UI would
// double-mark them and a construction centreline could be toggled back to a
// plain solid line by the construction switch.

export type LineTypeValue = "" | "centerline" | "infinite";

/** The four segments, in display order. */
export type LineTypeSelection = "solid" | "construction" | "centerline" | "infinite";

export interface LineTypeFlags {
  lineType: LineTypeValue;
  isConstruction: boolean;
}

export function lineTypeSelectionToFlags(
  selection: LineTypeSelection,
): LineTypeFlags {
  switch (selection) {
    case "construction":
      return { lineType: "", isConstruction: true };
    case "centerline":
      return { lineType: "centerline", isConstruction: false };
    case "infinite":
      return { lineType: "infinite", isConstruction: false };
    case "solid":
    default:
      return { lineType: "", isConstruction: false };
  }
}

/** Which segment is active for the current flags (the inverse mapping). */
export function lineTypeFlagsToSelection(
  flags: LineTypeFlags,
): LineTypeSelection {
  if (flags.lineType === "centerline") {
    return "centerline";
  }
  if (flags.lineType === "infinite") {
    return "infinite";
  }
  return flags.isConstruction ? "construction" : "solid";
}
