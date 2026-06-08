export interface SketchLineConstraintActions {
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
}
