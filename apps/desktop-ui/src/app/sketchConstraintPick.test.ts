// @vitest-environment node
// Constraint arming and pick sequencing for the S1 toolbar additions:
// collinear (line + line) and midpoint (point, then line). These modules are
// pure — they take callbacks — so the pick ORDER is testable without a viewport
// or a core process, which is exactly where an off-by-one pick bug would hide.
import { describe, expect, it, vi } from "vitest";

import type { ArmedSketchConstraint } from "../types";
import { toggleArmedSketchConstraint } from "./sketchConstraintArm";
import { handleSketchConstraintLinePickFromContext } from "./sketchConstraintLinePick";
import { handleSketchConstraintVertexPickFromContext } from "./sketchConstraintPointPick";

function linePickContext(
  armedSketchConstraint: ArmedSketchConstraint,
  lineId = "line-2",
) {
  const setArmed = vi.fn();
  return {
    setArmed,
    context: {
      lineId,
      additive: false,
      isLineEntity: (entityId: string) => entityId.startsWith("line-"),
      armedSketchConstraint,
      selectSketchEntity: vi.fn(async () => {}),
      setSketchLineConstraint: vi.fn(async () => {}),
      clearSketchLineConstraints: vi.fn(async () => {}),
      setSketchEqualLengthConstraint: vi.fn(async () => {}),
      setSketchParallelConstraint: vi.fn(async () => {}),
      setSketchPerpendicularConstraint: vi.fn(async () => {}),
      setSketchCollinearConstraint: vi.fn(async () => {}),
      setSketchMidpointConstraint: vi.fn(async () => {}),
      setSketchTangentConstraint: vi.fn(async () => {}),
      setSketchTangentPairConstraint: vi.fn(async () => {}),
      setSketchSymmetricConstraint: vi.fn(async () => {}),
      setArmedSketchConstraint: setArmed,
    },
  };
}

function pointPickContext(
  armedSketchConstraint: ArmedSketchConstraint,
  vertexId = "vertex-7",
) {
  const setArmed = vi.fn();
  return {
    setArmed,
    context: {
      vertexId,
      kind: "endpoint" as const,
      additive: false,
      armedSketchConstraint,
      selectSketchPoint: vi.fn(async () => {}),
      setSketchCoincidentConstraint: vi.fn(async () => {}),
      setSketchPointFixed: vi.fn(async () => {}),
      setArmedSketchConstraint: setArmed,
      addMessage: vi.fn(),
    },
  };
}

describe("arming the new constraints", () => {
  it("arms collinear as a line pair and midpoint as a point-then-line flow", () => {
    expect(toggleArmedSketchConstraint(null, "collinear").next).toEqual({
      kind: "collinear",
      firstLineId: null,
    });
    expect(toggleArmedSketchConstraint(null, "midpoint").next).toEqual({
      kind: "midpoint",
      firstPointId: null,
    });
    // Clicking the armed button again disarms.
    expect(
      toggleArmedSketchConstraint({ kind: "collinear", firstLineId: null }, "collinear")
        .next,
    ).toBeNull();
  });
});

describe("collinear pick flow", () => {
  it("first line pick arms the constraint, second pick applies it", async () => {
    const first = linePickContext({ kind: "collinear", firstLineId: null });
    await handleSketchConstraintLinePickFromContext(first.context);

    expect(first.context.selectSketchEntity).toHaveBeenCalledWith("line-2");
    expect(first.context.setSketchCollinearConstraint).not.toHaveBeenCalled();
    expect(first.setArmed).toHaveBeenCalledWith({
      kind: "collinear",
      firstLineId: "line-2",
    });

    const second = linePickContext(
      { kind: "collinear", firstLineId: "line-1" },
      "line-2",
    );
    await handleSketchConstraintLinePickFromContext(second.context);

    expect(second.context.setSketchCollinearConstraint).toHaveBeenCalledWith(
      "line-2",
      "line-1",
    );
    // Re-armed for the next pair, like parallel/perpendicular.
    expect(second.setArmed).toHaveBeenCalledWith({
      kind: "collinear",
      firstLineId: null,
    });
  });

  it("ignores the same line clicked twice", async () => {
    const { context, setArmed } = linePickContext(
      { kind: "collinear", firstLineId: "line-2" },
      "line-2",
    );
    await handleSketchConstraintLinePickFromContext(context);
    expect(context.setSketchCollinearConstraint).not.toHaveBeenCalled();
    expect(setArmed).not.toHaveBeenCalled();
  });
});

describe("midpoint pick flow", () => {
  it("the point pick arms the point, the line pick applies the constraint", async () => {
    const pointPick = pointPickContext({ kind: "midpoint", firstPointId: null });
    await handleSketchConstraintVertexPickFromContext(pointPick.context);

    expect(pointPick.context.setSketchPointFixed).not.toHaveBeenCalled();
    expect(pointPick.setArmed).toHaveBeenCalledWith({
      kind: "midpoint",
      firstPointId: "vertex-7",
    });

    const linePick = linePickContext({
      kind: "midpoint",
      firstPointId: "vertex-7",
    });
    await handleSketchConstraintLinePickFromContext(linePick.context);

    expect(linePick.context.setSketchMidpointConstraint).toHaveBeenCalledWith(
      "vertex-7",
      "line-2",
    );
    expect(linePick.setArmed).toHaveBeenCalledWith({
      kind: "midpoint",
      firstPointId: null,
    });
  });

  it("a line picked before any point only selects it", async () => {
    const { context, setArmed } = linePickContext({
      kind: "midpoint",
      firstPointId: null,
    });
    await handleSketchConstraintLinePickFromContext(context);
    expect(context.setSketchMidpointConstraint).not.toHaveBeenCalled();
    expect(context.selectSketchEntity).toHaveBeenCalledWith("line-2");
    expect(setArmed).not.toHaveBeenCalled();
  });
});

describe("tangent pick flow (line, then a circle/arc)", () => {
  it("arms on the line and applies on the circle", async () => {
    const first = linePickContext({ kind: "tangent", firstLineId: null });
    await handleSketchConstraintLinePickFromContext(first.context);
    expect(first.setArmed).toHaveBeenCalledWith({
      kind: "tangent",
      firstLineId: "line-2",
    });
    expect(first.context.setSketchTangentConstraint).not.toHaveBeenCalled();

    const second = linePickContext(
      { kind: "tangent", firstLineId: "line-1" },
      "circle-3",
    );
    await handleSketchConstraintLinePickFromContext(second.context);
    expect(second.context.setSketchTangentConstraint).toHaveBeenCalledWith(
      "line-1",
      "circle-3",
    );
    expect(second.setArmed).toHaveBeenCalledWith({
      kind: "tangent",
      firstLineId: null,
    });
  });

  it("a circle cannot start the flow, and a second line does not apply it", async () => {
    const circleFirst = linePickContext({ kind: "tangent", firstLineId: null }, "circle-1");
    await handleSketchConstraintLinePickFromContext(circleFirst.context);
    expect(circleFirst.setArmed).not.toHaveBeenCalled();
    expect(circleFirst.context.setSketchTangentConstraint).not.toHaveBeenCalled();

    const secondLine = linePickContext(
      { kind: "tangent", firstLineId: "line-1" },
      "line-2",
    );
    await handleSketchConstraintLinePickFromContext(secondLine.context);
    expect(secondLine.context.setSketchTangentConstraint).not.toHaveBeenCalled();
  });
});

describe("tangent pair pick flow", () => {
  it("applies on the second entity in either order", async () => {
    const first = linePickContext(
      { kind: "tangent_pair", firstEntityId: null },
      "arc-1",
    );
    await handleSketchConstraintLinePickFromContext(first.context);
    expect(first.setArmed).toHaveBeenCalledWith({
      kind: "tangent_pair",
      firstEntityId: "arc-1",
    });

    const second = linePickContext(
      { kind: "tangent_pair", firstEntityId: "arc-1" },
      "circle-2",
    );
    await handleSketchConstraintLinePickFromContext(second.context);
    expect(second.context.setSketchTangentPairConstraint).toHaveBeenCalledWith(
      "arc-1",
      "circle-2",
    );
  });

  it("ignores the same entity picked twice", async () => {
    const { context, setArmed } = linePickContext(
      { kind: "tangent_pair", firstEntityId: "circle-1" },
      "circle-1",
    );
    await handleSketchConstraintLinePickFromContext(context);
    expect(context.setSketchTangentPairConstraint).not.toHaveBeenCalled();
    expect(setArmed).not.toHaveBeenCalled();
  });
});

describe("symmetric pick flow", () => {
  it("takes two points, then the axis line", async () => {
    const firstPoint = pointPickContext({
      kind: "symmetric",
      firstPointId: null,
      secondPointId: null,
    });
    await handleSketchConstraintVertexPickFromContext(firstPoint.context);
    expect(firstPoint.setArmed).toHaveBeenCalledWith({
      kind: "symmetric",
      firstPointId: "vertex-7",
      secondPointId: null,
    });

    const secondPoint = pointPickContext(
      {
        kind: "symmetric",
        firstPointId: "vertex-7",
        secondPointId: null,
      },
      "vertex-8",
    );
    await handleSketchConstraintVertexPickFromContext(secondPoint.context);
    expect(secondPoint.setArmed).toHaveBeenCalledWith({
      kind: "symmetric",
      firstPointId: "vertex-7",
      secondPointId: "vertex-8",
    });
  });

  it("applies on the axis line once both points are picked", async () => {
    const axis = linePickContext({
      kind: "symmetric",
      firstPointId: "vertex-3",
      secondPointId: "vertex-5",
    });
    await handleSketchConstraintLinePickFromContext(axis.context);
    expect(axis.context.setSketchSymmetricConstraint).toHaveBeenCalledWith(
      "vertex-3",
      "vertex-5",
      "line-2",
    );
    expect(axis.setArmed).toHaveBeenCalledWith({
      kind: "symmetric",
      firstPointId: null,
      secondPointId: null,
    });
  });

  it("a line pick before both points only selects it", async () => {
    const { context, setArmed } = linePickContext({
      kind: "symmetric",
      firstPointId: "vertex-3",
      secondPointId: null,
    });
    await handleSketchConstraintLinePickFromContext(context);
    expect(context.setSketchSymmetricConstraint).not.toHaveBeenCalled();
    expect(context.selectSketchEntity).toHaveBeenCalledWith("line-2");
    expect(setArmed).not.toHaveBeenCalled();
  });
});
