#pragma once

#include <string>
#include <vector>

namespace polysmith::core {

// A point anchored to the midpoint of a line. The anchored point is
// also (typically) an endpoint of some other line; the solver pulls
// that endpoint to (start+end)/2 of the host line on every edit so
// the relation stays satisfied. Created automatically when the user
// snaps a sketch line endpoint to a midpoint snap target.
struct SketchMidpointAnchor {
  std::string id;
  std::string vertex_id;
  std::string line_id;
};

// A point anchored to the body of a line (not just its midpoint).
// The solver re-projects the bound point onto the host line on every
// edit, parametrized by `t` in [0, 1]. Created automatically when the
// user starts/ends a draft on another line's body via the line-body
// snap. Distinct from `SketchMidpointAnchor`, which is a degenerate
// special case at t=0.5.
struct SketchVertexLineAnchor {
  std::string id;
  std::string vertex_id;
  std::string line_id;
  // Stored fraction along the host line at the time the anchor was
  // created. The solver uses this to keep the bound point at the
  // same relative position even when the host line moves; without
  // it, every solve would re-project to the closest point on the
  // moving line, which can drift.
  double t;
};

// A general geometric or dimensional constraint between sketch
// entities or points. Used for constraint types not yet covered by
// `SketchLineRelation` (equal_length, perpendicular) or inline
// `SketchLine.constraint` (horizontal, vertical) — e.g. concentric,
// symmetric, or coincident constraints created by the inference
// engine at entity-commit time.
struct SketchConstraint {
  std::string constraint_id;       // "constraint-{N}"
  std::string kind;                // "coincident", "concentric", ...
  std::vector<std::string> target_ids;
  double value = 0.0;              // for dimensional constraints
  bool driven = false;             // true = reference only
};

// User-configurable selection filter that controls which geometric
// element types are visible, selectable, snappable, and constrainable.
// Stored per-session (persisted to localStorage on the TS side in v1).
struct SelectionFilter {
  // Sketch geometry toggles
  bool select_curves        = true;
  bool select_points        = true;
  bool select_construction  = false;
  bool select_constraints   = true;

  // Snap toggles (derived from selection filter)
  bool snap_endpoint        = true;
  bool snap_midpoint        = true;
  bool snap_center          = true;
  bool snap_intersection    = true;
  bool snap_nearest         = true;
  bool snap_quadrant        = true;
  bool snap_perpendicular   = true;
  bool snap_parallel        = true;
  bool snap_tangent         = true;
  bool snap_grid            = true;
  bool snap_grid_line       = true;
  bool snap_polar           = true;

  // Global settings
  int tolerance_px           = 10;
  int polar_angle_degrees    = 15;
  std::vector<std::string> snap_priority;
  bool magnetic_pull         = true;
};

struct SketchLineRelation {
  std::string id;
  std::string kind;
  std::string first_line_id;
  std::string second_line_id;
};

}  // namespace polysmith::core
