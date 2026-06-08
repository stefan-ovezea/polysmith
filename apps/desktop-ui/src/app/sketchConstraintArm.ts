import type { ArmedSketchConstraint, ConstraintType } from "../types";

export interface ArmedSketchConstraintTransition {
  next: ArmedSketchConstraint;
  shouldSwitchToSelect: boolean;
}

export function toggleArmedSketchConstraint(
  current: ArmedSketchConstraint,
  constraint: ConstraintType,
): ArmedSketchConstraintTransition {
  if (current?.kind === constraint) {
    return { next: null, shouldSwitchToSelect: false };
  }

  if (constraint === "mirror") {
    return { next: current, shouldSwitchToSelect: false };
  }

  return {
    next: createArmedSketchConstraint(constraint),
    shouldSwitchToSelect: true,
  };
}

function createArmedSketchConstraint(
  constraint: ConstraintType,
): ArmedSketchConstraint {
  if (constraint === "coincident") {
    return { kind: constraint, firstPointId: null };
  }
  if (isPairLineConstraint(constraint)) {
    return { kind: constraint, firstLineId: null };
  }
  return { kind: constraint } as ArmedSketchConstraint;
}

function isPairLineConstraint(
  constraint: ConstraintType,
): constraint is "equal_length" | "perpendicular" | "parallel" {
  return (
    constraint === "equal_length" ||
    constraint === "perpendicular" ||
    constraint === "parallel"
  );
}
