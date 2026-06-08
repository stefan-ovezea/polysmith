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
  setArmedSketchConstraint: (constraint: ArmedSketchConstraint) => void;
  addMessage: (message: string) => void;
}

export async function handleSketchConstraintPointPickFromContext(
  context: SketchConstraintPointPickContext,
) {
  const { armedSketchConstraint } = context;

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
