#pragma once

#include <string>

namespace polysmith::core {

// A single snap candidate emitted in viewport state snapshots.
// The TS UI measures cursor distance against these to resolve
// static snaps (endpoint, midpoint, center, etc.).
struct SnapCandidate {
  std::string kind;
  std::string entity_id;
  std::string vertex_id;
  double local_x;
  double local_y;
  double distance;
  std::string label;
  // Parametric position along the host entity, when meaningful.
  // For "nearest" (line-body): t in [0,1] along the line segment.
  // For "midpoint": 0.5.
  // For other kinds: -1.0 (undefined).
  double param_t = -1.0;
};

}  // namespace polysmith::core
