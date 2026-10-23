// @vitest-environment node
// Commit-path test for the rounded-rectangle tool mode. This is the click
// commit (Path 2 of the two-commit-paths rule); the Enter commit in
// ViewportPanel.tsx calls the same rounded command with the same arguments.
//
// The assertions pin the things that are easy to get wrong: the radius the
// panel supplies is forwarded, the plain rectangle command is NOT sent, and
// the auto side dimensions are dropped even when the user typed values
// (a rounded rectangle's side dimension measures the trimmed side, not the
// overall width the draft badge showed).
import { describe, expect, it, vi } from "vitest";

import {
  commitDraftPointerUp,
  type DraftPointerUpCommitOptions,
} from "./draftCommit";

function makeOptions(
  overrides: Partial<DraftPointerUpCommitOptions> = {},
): DraftPointerUpCommitOptions {
  return {
    activeSketchTool: "rectangle",
    sketchPoint: { local: [40, 20] },
    draftDimensionSession: null,
    sketchCircleCount: 0,
    isConstruction: false,
    refs: {
      draftStart: { current: [0, 0] },
      arcSecondPoint: { current: null },
      rectSecondPoint: { current: null },
      circleSecondPoint: { current: null },
      ellipseSecondPoint: { current: null },
      chainBreakRequested: { current: false },
      previousLineAngle: { current: null },
      draftStartMidpointHost: { current: null },
      draftStartEndpointHost: { current: null },
      draftStartLineBodyHost: { current: null },
      draftDimensionSession: { current: null },
      draftDimensionInputs: { current: {} },
    },
    modes: {
      arc: "three_point",
      rectangle: "corner_corner",
      circle: "center_radius",
      polygon: "inscribed",
      slot: "center_point",
    },
    polygonSides: 6,
    rectangleCornerRadius: 6,
    slotType: "straight",
    slotArcSweep: Math.PI / 2,
    clearPreviews: vi.fn(),
    clearDraftDimensionSession: vi.fn(),
    suppressDimensionEditorAfterSketchCommit: vi.fn(),
    scheduleDimensionDeletion: vi.fn(),
    scheduleDraftDimensionExpressionUpdate: vi.fn(),
    setPendingCircleDimensionPlacement: vi.fn(),
    captureLineCommitRelations: vi.fn(() => ({
      endHostLineId: null,
      endLineBodyHost: null,
    })),
    createLineDraftDimensionSession: vi.fn(),
    clearDraftDimGroup: vi.fn(),
    setDraftDimensionSession: vi.fn(),
    focusDraftField: vi.fn(),
    addSketchArc: vi.fn(),
    addSketchRectangle: vi.fn(),
    addSketchRoundedRectangle: vi.fn(),
    addSketchCircle: vi.fn(),
    addSketchCircleMode: vi.fn(),
    addSketchPolygon: vi.fn(),
    addSketchLine: vi.fn(),
    addSketchEllipse: vi.fn(),
    addSketchSlot: vi.fn(),
    ...overrides,
  } as DraftPointerUpCommitOptions;
}

describe("commitDraftPointerUp — rounded rectangle", () => {
  it("sends one rounded-rectangle command with the panel's radius", () => {
    const options = makeOptions();
    options.modes.rectangle = "rounded";

    commitDraftPointerUp(options);

    expect(options.addSketchRoundedRectangle).toHaveBeenCalledTimes(1);
    expect(options.addSketchRoundedRectangle).toHaveBeenCalledWith(
      0,
      0,
      40,
      20,
      6,
      false,
    );
    expect(options.addSketchRectangle).not.toHaveBeenCalled();
  });

  it("always drops the auto side dimensions and never replays typed values", () => {
    const options = makeOptions();
    options.modes.rectangle = "rounded";
    // A session that typed both fields — the plain rectangle path would keep
    // its dimensions in this case.
    options.draftDimensionSession = {
      tool: "rectangle",
      start: [0, 0],
      current: [40, 20],
      secondPoint: null,
      values: {
        width: "40",
        length: "20",
        angle: "",
        radius: "",
        diameter: "",
        radiusX: "",
        radiusY: "",
      },
      activeField: "width",
      lockedFields: { width: true, length: true },
      touchedFields: { width: true, length: true },
    };

    commitDraftPointerUp(options);

    expect(options.scheduleDimensionDeletion).toHaveBeenCalledWith(
      "rectangle",
      undefined,
      { deleteRectangleDimensions: true },
    );
    expect(options.scheduleDraftDimensionExpressionUpdate).not.toHaveBeenCalled();
  });

  it("leaves the plain corner-corner rectangle path unchanged", () => {
    const options = makeOptions();

    commitDraftPointerUp(options);

    expect(options.addSketchRectangle).toHaveBeenCalledWith(0, 0, 40, 20, false);
    expect(options.addSketchRoundedRectangle).not.toHaveBeenCalled();
    expect(options.scheduleDimensionDeletion).toHaveBeenCalledWith(
      "rectangle",
      undefined,
      { deleteRectangleDimensions: false },
    );
  });
});

describe("commitDraftPointerUp — slot modes", () => {
  it("forwards the mode's centre / length / radius / rotation to the core command", () => {
    const options = makeOptions();
    options.activeSketchTool = "slot";
    options.modes.slot = "overall";
    options.sketchPoint = { local: [40, 0] };

    commitDraftPointerUp(options);

    // overall on a 40 mm span: centre 20, length 36, radius 2.
    expect(options.addSketchSlot).toHaveBeenCalledTimes(1);
    expect(options.addSketchSlot).toHaveBeenCalledWith(
      20,
      0,
      36,
      2,
      0,
      false,
    );
  });

  it("center_to_center: the clicks are the arc centres", () => {
    const options = makeOptions();
    options.activeSketchTool = "slot";
    options.modes.slot = "center_to_center";
    options.sketchPoint = { local: [40, 0] };

    commitDraftPointerUp(options);

    expect(options.addSketchSlot).toHaveBeenCalledWith(20, 0, 40, 2, 0, false);
  });

  // T2.3: an arc slot takes the first click as the ARC centre and the second
  // as a point on the centreline; the sweep comes from the panel. The commit
  // must send mode "arc" with the derived radius / start angle / sweep, and no
  // stadium length or rotation.
  it("arc slot: sends mode arc with the centreline geometry and the panel sweep", () => {
    const options = makeOptions({
      activeSketchTool: "slot",
      slotType: "arc",
      slotArcSweep: Math.PI / 2,
      sketchPoint: { local: [0, 10] },
    });

    commitDraftPointerUp(options);

    expect(options.addSketchSlot).toHaveBeenCalledTimes(1);
    const args = (options.addSketchSlot as unknown as {
      mock: { calls: unknown[][] };
    }).mock.calls[0];
    // centre (0,0) — click 1; radius is the auto half width; no stadium data.
    expect(args[0]).toBe(0);
    expect(args[1]).toBe(0);
    expect(args[2]).toBe(0);
    expect(args[4]).toBe(0);
    expect(args[5]).toBe(false);
    const arc = args[6] as {
      mode: string;
      arcRadius: number;
      startAngle: number;
      sweepAngle: number;
    };
    expect(arc.mode).toBe("arc");
    expect(arc.arcRadius).toBeCloseTo(10, 9);
    expect(arc.startAngle).toBeCloseTo(Math.PI / 2, 9);
    expect(arc.sweepAngle).toBeCloseTo(Math.PI / 2, 9);
    expect(args[3] as number).toBeLessThan(arc.arcRadius);
  });
});
