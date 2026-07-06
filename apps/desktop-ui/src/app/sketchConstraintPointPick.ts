import type { ArmedSketchConstraint } from "../types";

export type SketchConstraintPointKind = "endpoint" | "center" | "quadrant";

export interface SketchConstraintPointPickContext {
  pointId: string;
  kind: SketchConstraintPointKind;
  additive: boolean;
  armedSketchConstraint: ArmedSketchConstraint;
  selectSketchPoint: (pointId: string, additive?: boolean) => Promise<void>;
  setSketchCoincidentConstraint: (
    pointId: string,
    otherPointId: string,
  ) => Promise<void>;
  setSketchPointFixed: (pointId: string, isFixed: boolean) => Promise<void>;
  setArmedSketchConstraint: (constraint: ArmedSketchConstraint) => void;
  addMessage: (message: string) => void;
}

export async function handleSketchConstraintPointPickFromContext(
  context: SketchConstraintPointPickContext,
) {
  const { armedSketchConstraint } = context;

  // Fix constraint: arm, click a point → fix it, then clear the arm
  // so the next click doesn't accidentally fix another point.
  if (armedSketchConstraint?.kind === "fixed") {
    await context.setSketchPointFixed(context.pointId, true);
    context.setArmedSketchConstraint(null);
    return;
  }

  // Clear constraint: arm, click a point → unfix it.
  if (armedSketchConstraint?.kind === "clear") {
    await context.setSketchPointFixed(context.pointId, false);
    context.setArmedSketchConstraint(null);
    return;
  }

  if (!armedSketchConstraint || armedSketchConstraint.kind !== "coincident") {
    await context.selectSketchPoint(context.pointId, context.additive);
    return;
  }

  if (!armedSketchConstraint.firstPointId) {
    context.addMessage(
      `coincident: first point ${context.pointId} (${context.kind})`,
    );
    await context.selectSketchPoint(context.pointId);
    context.setArmedSketchConstraint({
      kind: "coincident",
      firstPointId: context.pointId,
    });
    return;
  }

  if (armedSketchConstraint.firstPointId === context.pointId) {
    context.addMessage("coincident: same point clicked twice, ignoring");
    return;
  }

  context.addMessage(
    `coincident: second point ${context.pointId} (${context.kind}) - applying constraint`,
  );
  await context.setSketchCoincidentConstraint(
    context.pointId,
    armedSketchConstraint.firstPointId,
  );
  context.addMessage("coincident: constraint applied, armed for next pair");
  context.setArmedSketchConstraint({
    kind: "coincident",
    firstPointId: null,
  });
}
