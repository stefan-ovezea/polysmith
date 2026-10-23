import type { ArmedSketchConstraint } from "../types";
import type { SketchLineConstraintActions } from "./sketchLineConstraintActions";

type PairLineConstraint = Extract<
  NonNullable<ArmedSketchConstraint>,
  { firstLineId: string | null }
>;

export interface SketchConstraintLinePickContext
  extends SketchLineConstraintActions {
  lineId: string;
  additive: boolean;
  armedSketchConstraint: ArmedSketchConstraint;
  /** True when the picked entity is a LINE (as opposed to a circle or arc).
   *  The viewport funnels every sketch-entity click through the same handler,
   *  and the tangent / tangent-pair flows need to tell the kinds apart. */
  isLineEntity?: (entityId: string) => boolean;
  selectSketchEntity: (entityId: string, additive?: boolean) => Promise<void>;
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
  const isLine = context.isLineEntity
    ? context.isLineEntity(context.lineId)
    : true;

  if (constraint.kind === "coincident") {
    await context.selectSketchEntity(context.lineId);
    return true;
  }
  // Midpoint: the POINT was picked first. This line completes the pair.
  if (constraint.kind === "midpoint") {
    if (!constraint.firstPointId) {
      // No point armed yet — a line cannot start a midpoint constraint.
      await context.selectSketchEntity(context.lineId);
      return true;
    }
    await context.setSketchMidpointConstraint(
      constraint.firstPointId,
      context.lineId,
    );
    context.setArmedSketchConstraint({ kind: "midpoint", firstPointId: null });
    return true;
  }
  // Tangent (S1): line first, then a circle/arc.
  if (constraint.kind === "tangent") {
    if (!constraint.firstLineId) {
      if (isLine) {
        await context.selectSketchEntity(context.lineId);
        context.setArmedSketchConstraint({
          kind: "tangent",
          firstLineId: context.lineId,
        });
      } else {
        // A circle cannot start the line+circle tangent flow.
        await context.selectSketchEntity(context.lineId);
      }
      return true;
    }
    if (constraint.firstLineId === context.lineId) {
      return true;
    }
    if (isLine) {
      // Still a line: the flow needs a circle/arc for the second pick.
      await context.selectSketchEntity(context.lineId);
      return true;
    }
    await context.setSketchTangentConstraint(
      constraint.firstLineId,
      context.lineId,
    );
    context.setArmedSketchConstraint({ kind: "tangent", firstLineId: null });
    return true;
  }
  // Tangent pair (S1): any two tangent-capable entities, in either order.
  if (constraint.kind === "tangent_pair") {
    if (!constraint.firstEntityId) {
      await context.selectSketchEntity(context.lineId);
      context.setArmedSketchConstraint({
        kind: "tangent_pair",
        firstEntityId: context.lineId,
      });
      return true;
    }
    if (constraint.firstEntityId === context.lineId) {
      return true;
    }
    await context.setSketchTangentPairConstraint(
      constraint.firstEntityId,
      context.lineId,
    );
    context.setArmedSketchConstraint({
      kind: "tangent_pair",
      firstEntityId: null,
    });
    return true;
  }
  // Symmetric (S1): the two POINTS were picked first; this line is the axis.
  if (constraint.kind === "symmetric") {
    if (!constraint.firstPointId || !constraint.secondPointId) {
      await context.selectSketchEntity(context.lineId);
      return true;
    }
    if (!isLine) {
      await context.selectSketchEntity(context.lineId);
      return true;
    }
    await context.setSketchSymmetricConstraint(
      constraint.firstPointId,
      constraint.secondPointId,
      context.lineId,
    );
    context.setArmedSketchConstraint({
      kind: "symmetric",
      firstPointId: null,
      secondPointId: null,
    });
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
  if (constraint.kind === "collinear") {
    await context.setSketchCollinearConstraint(
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
    constraint.kind === "perpendicular" ||
    constraint.kind === "collinear"
  );
}
