#pragma once

#include <optional>
#include <string>

namespace polysmith::core {

// Parametric straight slot (stadium shape). The boundary geometry is NOT
// stored here — `refresh_sketch_slots` expands every record into two
// lines + two arcs (tangent by construction, owned via `generated_by`)
// on every recompute, mirroring the sketch text expansion pattern. The
// existing profile detection / extrude / viewport pipeline consumes the
// generated plain entities with zero downstream changes.
//
// Generated geometry (CCW loop, see slot_expansion.inc):
//   line-slot-<id>-bottom  bl -> br
//   arc-slot-<id>-right    br -> tr  (ccw)
//   line-slot-<id>-top     tr -> tl
//   arc-slot-<id>-left     tl -> bl  (cw)
// The center vertex is a regular movable "vertex-N" vertex so the slot
// joins the vertex graph for distance dimensions; the 4 corner vertices
// and 2 arc centers carry the "vertex-slot-" prefix and are re-marked
// fixed by the vertex rebuild (same mechanism as text glyph vertices).
struct SketchSlot {
  std::string id;  // "slot-N", assigned by the document manager counter
  // Movable center vertex (stable "vertex-N" id created at slot
  // creation, like circle centers).
  std::string center_vertex_id;
  double center_x = 0.0;
  double center_y = 0.0;
  // Distance between the two arc centers. Must be >= 2 * radius so the
  // two arcs never overlap. ("straight" only.)
  double length = 10.0;
  // Half the slot's width: the end-cap radius (straight) or the radial
  // half-width (arc).
  double radius = 2.0;
  // Slot axis angle in radians (sketch-plane coordinates), mirrors
  // SketchEllipse::rotation. ("straight" only.)
  double rotation = 0.0;
  // "straight" (stadium: 2 lines + 2 arcs) or "arc" (a slot whose
  // centreline is a circular arc: 2 concentric arcs + 2 straight radial
  // end caps). Arc slots use center_x/center_y as the ARC CENTRE and the
  // three fields below.
  std::string mode = "straight";
  // Arc mode: radius of the centreline arc. Must be > radius (so the
  // inner boundary radius arc_radius - radius stays positive).
  double arc_radius = 10.0;
  // Arc mode: angle of the first end cap and the (positive) sweep to the
  // second, in radians, measured from +X about the arc centre.
  double start_angle = 0.0;
  double sweep_angle = 1.5707963267948966;
  bool is_construction = false;
};

}  // namespace polysmith::core
