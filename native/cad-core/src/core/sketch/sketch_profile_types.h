#pragma once

#include <optional>
#include <string>
#include <vector>
#include "core/sketch/sketch_geometry_types.h"

namespace polysmith::core {

struct SketchProfileRegion {
  std::string id;
  std::string kind;
  std::vector<std::string> vertex_ids;
  // Unique set of edge IDs that form this region (not ordered).
  std::vector<std::string> line_ids;
  // Edge IDs in boundary walk order — one entry per profile boundary
  // segment. When non-empty, downstream extrude builders use exact OCCT
  // curves instead of polygon-approximating the face.
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
};

}  // namespace polysmith::core
