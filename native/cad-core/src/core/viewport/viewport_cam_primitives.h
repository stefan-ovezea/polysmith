#pragma once

#include <string>
#include <vector>

namespace polysmith::core {

struct ViewportToolpathPoint {
  double x;
  double y;
  double z;
  bool is_rapid;
  bool pierce;  // the pierce dwell point (laser on + dwell > 0)
};

struct ViewportToolpathPrimitive {
  std::string id;
  std::string label;
  std::vector<ViewportToolpathPoint> points;
};

}  // namespace polysmith::core
