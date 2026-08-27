#pragma once

#include <optional>
#include <string>
#include <vector>
#include "core/sketch/sketch_geometry_types.h"

namespace polysmith::core {

// Describes a single contiguous circular arc on a profile boundary.
// Emitted by build_sketch_profile_regions so the wire builder can
// construct an exact GC_MakeArcOfCircle without guessing from polygon
// samples.
struct SketchArcDescriptor {
  std::string circle_id;    // which circle this arc belongs to
  double center_x;          // circle centre (sketch coords)
  double center_y;
  double radius;
  double start_angle_rad;   // angle of the first profile point on this arc
  double end_angle_rad;     // angle of the last profile point on this arc
  bool ccw;                 // true = arc traverses CCW from start to end
};

// One exact boundary edge of a profile region, in walk order.  The
// wire builder consumes this list directly — every edge is anchored to
// its entity's exact parametric geometry, so no grouping, dedup or
// hint-based guessing is needed downstream.
struct ProfileBoundaryEdge {
  std::string entity_id;
  std::string entity_kind;   // "line" | "circle" | "arc" | "ellipse" | "spline"
  // Line: t in [0, 1].  Circle/arc: sketch angle in the entity's sweep
  // frame (may be lifted beyond 2π; the span's sign follows the walk).
  double param_start;
  double param_end;
  double start_x;
  double start_y;
  double end_x;
  double end_y;
  // Circular pieces only.
  double center_x = 0.0;
  double center_y = 0.0;
  double radius = 0.0;
  bool ccw = true;           // walk direction around the centre
  // Ellipse pieces: minor radius + major-axis angle (radius = major).
  double minor_radius = 0.0;
  double rotation = 0.0;
  // Spline pieces: control poles + degree (clamped open-uniform —
  // spline_math.h). The wire builder reconstructs the exact
  // Geom_BSplineCurve from these, trimmed to [param_start, param_end].
  int spline_degree = 3;
  std::vector<double> spline_pole_xs;
  std::vector<double> spline_pole_ys;
};

struct SketchProfileRegion {
  std::string id;
  std::string kind;
  std::vector<std::string> vertex_ids;
  std::vector<std::string> line_ids;
  std::vector<std::string> ordered_edge_ids;
  std::vector<SketchProfilePoint> points;
  // Inner loops cut out of this profile region. v1 uses this for the
  // common "circle inside polygon" case so selecting the outer area
  // extrudes a face with a circular hole, while selecting the circle
  // separately extrudes the disk.
  std::vector<std::vector<SketchProfilePoint>> inner_loops;
  std::optional<std::string> source_circle_id;
  double center_x;
  double center_y;
  double radius;
  // Exact arc descriptors for every contiguous run of circle edges in
  // ordered_edge_ids.  Empty for legacy profiles; populated by
  // build_sketch_profile_regions so make_sketch_wire never guesses.
  std::vector<SketchArcDescriptor> arc_descriptors;
  // Exact boundary edges (new exact-curve detector).  Empty for legacy
  // profiles restored from old saves — the wire builder then falls back
  // to the legacy grouping/descriptor path.
  std::vector<ProfileBoundaryEdge> boundary_edges;
};

}  // namespace polysmith::core
