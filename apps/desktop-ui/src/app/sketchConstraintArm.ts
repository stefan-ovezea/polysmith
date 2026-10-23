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
  // Midpoint is point-then-line, so it starts with the point slot empty.
  if (constraint === "midpoint") {
    return { kind: constraint, firstPointId: null };
  }
  // Tangent starts with a line; the circle/arc completes it.
  if (constraint === "tangent") {
    return { kind: constraint, firstLineId: null };
  }
  // Tangent pair takes two entities in either order.
  if (constraint === "tangent_pair") {
    return { kind: constraint, firstEntityId: null };
  }
  // Symmetric takes two points, then the axis line.
  if (constraint === "symmetric") {
    return { kind: constraint, firstPointId: null, secondPointId: null };
  }
  if (isPairLineConstraint(constraint)) {
    return { kind: constraint, firstLineId: null };
  }
  return { kind: constraint } as ArmedSketchConstraint;
}

function isPairLineConstraint(
  constraint: ConstraintType,
): constraint is "equal_length" | "perpendicular" | "parallel" | "collinear" {
  return (
    constraint === "equal_length" ||
    constraint === "perpendicular" ||
    constraint === "parallel" ||
    constraint === "collinear"
  );
}
