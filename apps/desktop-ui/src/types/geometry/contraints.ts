export type ConstraintType =
  | "horizontal"
  | "vertical"
  | "parallel"
  | "perpendicular"
  | "fixed"
  | "clear"
  | "coincident"
  | "concentric"
  | "tangent"
  // Two entities tangent to each other (arcs/circles/line-circle) — the
  // pair form, distinct from the line+circle "tangent" constraint.
  | "tangent_pair"
  // Two points mirrored about an axis line (S1).
  | "symmetric"
  | "equal_length"
  | "equal_radius"
  // Two lines on the same infinite line (S1). Shares the pair-line pick flow
  // with parallel / perpendicular / equal-length.
  | "collinear"
  // A point bound to the midpoint of a line (Phase 2 of the sketch
  // improvements). Rendered as an "M" badge on the host line.
  | "midpoint"
  // A point bound to a host line's body at parametric position `t`.
  // Rendered as a "/" badge at the bound point's position.
  | "on_line"
  // Cardinal (quadrant) point badge on circles and arcs.
  // Rendered as a "Q" badge at the quadrant position.
  | "quadrant"
  // Mirror tool (technically an editing op, not a constraint, but
  // it shares the armed-sketch-constraint flow: pick axis line,
  // pick entities, each pick mirrors immediately).
  | "mirror"
  // Array tool (also an editing op — linear/circular exploded copies
  // of the current selection). Shares the toolbar icon slot with the
  // constraint buttons; opens its own floating panel.
  | "array";

export type ArmedSketchConstraint =
  | null
  | { kind: "horizontal" | "vertical" | "clear" }
  | { kind: "fixed" }
  | {
      kind: "equal_length" | "perpendicular" | "parallel" | "collinear";
      firstLineId: string | null;
    }
  | { kind: "coincident"; firstPointId: string | null }
  // Midpoint (S1): pick a POINT first, then the line it should sit on. The
  // point pick stores the vertex; the following line pick applies it.
  | { kind: "midpoint"; firstPointId: string | null }
  // Tangent (S1): a line, then a circle/arc. Applied on the second pick.
  | { kind: "tangent"; firstLineId: string | null }
  // Tangent pair (S1): two tangent-capable entities, in either order.
  | { kind: "tangent_pair"; firstEntityId: string | null }
  // Symmetric (S1): point, point, then the axis line.
  | {
      kind: "symmetric";
      firstPointId: string | null;
      secondPointId: string | null;
    };
// Note: Mirror is NOT an armed constraint — it's a full CAD-
// style tool with its own floating panel and lifecycle. See
// `mirrorFocusedSlot` in App.tsx and `MirrorToolPanel`.
