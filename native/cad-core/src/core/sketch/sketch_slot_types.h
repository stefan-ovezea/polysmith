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
  // two arcs never overlap.
  double length = 10.0;
  double radius = 2.0;
  // Slot axis angle in radians (sketch-plane coordinates), mirrors
  // SketchEllipse::rotation.
  double rotation = 0.0;
  // Only "straight" exists in v1 (curved slots deferred).
  std::string mode = "straight";
  bool is_construction = false;
};

}  // namespace polysmith::core
