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
  setSketchCollinearConstraint: (
    lineId: string,
    otherLineId: string,
  ) => Promise<void>;
  // Midpoint pairs a POINT with a line; the point is picked first.
  setSketchMidpointConstraint: (
    vertexId: string,
    lineId: string,
  ) => Promise<void>;
  // Tangent: a LINE plus a circle/arc.
  setSketchTangentConstraint: (lineId: string, circleId: string) => Promise<void>;
  // Tangent pair: two tangent-capable entities.
  setSketchTangentPairConstraint: (
    firstId: string,
    secondId: string,
  ) => Promise<void>;
  // Symmetric: two points about an axis line.
  setSketchSymmetricConstraint: (
    pointId: string,
    otherPointId: string,
    axisLineId: string,
  ) => Promise<void>;
}
