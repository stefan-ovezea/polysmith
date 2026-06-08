import type { ArmedSketchConstraint } from "../types";

type PairLineConstraint = Extract<
  NonNullable<ArmedSketchConstraint>,
  { firstLineId: string | null }
>;

export interface SketchConstraintLinePickContext {
  lineId: string;
  additive: boolean;
  armedSketchConstraint: ArmedSketchConstraint;
  selectSketchEntity: (entityId: string, additive?: boolean) => Promise<void>;
  setSketchLineConstraint: (
    lineId: string,
    constraint: "horizontal" | "vertical",
  ) => Promise<void>;
  clearSketchLineConstraints: (lineId: string) => Promise<void>;
  setSketchEqualLengthConstraint: (
    lineId: string,
    otherLineId: string,
  ) => Promise<void>;
  setSketchParallelConstraint: (
    lineId: string,
    otherLineId: string,
  ) => Promise<void>;
  setSketchPerpendicularConstraint: (
    lineId: string,
    otherLineId: string,
  ) => Promise<void>;
  setArmedSketchConstraint: (constraint: ArmedSketchConstraint) => void;
}

export async function handleSketchConstraintLinePickFromContext(
  context: SketchConstraintLinePickContext,
) {
  const { armedSketchConstraint } = context;
  if (!armedSketchConstraint) {
    await context.selectSketchEntity(context.lineId, context.additive);
    return;
  }

  if (await handleSingleLineConstraint(context, armedSketchConstraint)) {
    return;
  }

  if (!isPairLineConstraint(armedSketchConstraint)) {
    await context.selectSketchEntity(context.lineId);
    return;
  }

  await handlePairLineConstraint(context, armedSketchConstraint);
}

async function handleSingleLineConstraint(
  context: SketchConstraintLinePickContext,
  constraint: NonNullable<ArmedSketchConstraint>,
) {
  if (constraint.kind === "coincident") {
    await context.selectSketchEntity(context.lineId);
    return true;
  }
  if (constraint.kind === "horizontal" || constraint.kind === "vertical") {
    await context.setSketchLineConstraint(context.lineId, constraint.kind);
    context.setArmedSketchConstraint(null);
    return true;
  }
  if (constraint.kind === "clear") {
    await context.clearSketchLineConstraints(context.lineId);
    return true;
  }
  return false;
}

async function handlePairLineConstraint(
  context: SketchConstraintLinePickContext,
  constraint: PairLineConstraint,
) {
  if (!constraint.firstLineId) {
    await armFirstLinePick(context, constraint);
    return;
  }
  if (constraint.firstLineId === context.lineId) {
    return;
  }
  await applyPairLineConstraint(context, constraint);
  context.setArmedSketchConstraint({
    kind: constraint.kind,
    firstLineId: null,
  });
}

async function armFirstLinePick(
  context: SketchConstraintLinePickContext,
  constraint: PairLineConstraint,
) {
  await context.selectSketchEntity(context.lineId);
  context.setArmedSketchConstraint({
    kind: constraint.kind,
    firstLineId: context.lineId,
  });
}

async function applyPairLineConstraint(
  context: SketchConstraintLinePickContext,
  constraint: PairLineConstraint,
) {
  if (constraint.kind === "equal_length") {
    await context.setSketchEqualLengthConstraint(
      context.lineId,
      constraint.firstLineId!,
    );
    return;
  }
  if (constraint.kind === "parallel") {
    await context.setSketchParallelConstraint(
      context.lineId,
      constraint.firstLineId!,
    );
    return;
  }
  await context.setSketchPerpendicularConstraint(
    context.lineId,
    constraint.firstLineId!,
  );
}

function isPairLineConstraint(
  constraint: NonNullable<ArmedSketchConstraint>,
): constraint is PairLineConstraint {
  return (
    constraint.kind === "equal_length" ||
    constraint.kind === "parallel" ||
    constraint.kind === "perpendicular"
  );
}
