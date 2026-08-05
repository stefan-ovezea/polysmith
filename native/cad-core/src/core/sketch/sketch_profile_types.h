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
};

}  // namespace polysmith::core
