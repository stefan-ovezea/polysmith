export type ConstraintType =
  | "horizontal"
  | "vertical"
  | "parallel"
  | "perpendicular"
  | "clear"
  | "coincident"
  | "tangent"
  | "equal_length"
  | "equal_radius";

export type ArmedSketchConstraint =
  | null
  | { kind: "horizontal" | "vertical" | "clear" }
  | {
      kind: "equal_length" | "perpendicular" | "parallel";
      firstLineId: string | null;
    }
  | { kind: "coincident"; firstPointId: string | null };
