#pragma once

#include <string>
#include <vector>

namespace polysmith::core {

struct ViewportToolpathPoint {
  double x;
  double y;
  double z;
  bool is_rapid;
};

struct ViewportToolpathPrimitive {
  std::string id;
  std::string label;
  std::vector<ViewportToolpathPoint> points;
};

}  // namespace polysmith::core
