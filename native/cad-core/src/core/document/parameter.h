#pragma once

#include <string>

namespace polysmith::core {

struct ParameterEntry {
  std::string name;           // e.g. "width", "thickness"
  std::string expression;     // e.g. "50", "width * 2", "height / 3 + 10"
  double resolved_value = 0;  // cached evaluated result
  std::string kind = "length";  // "length" (mm) or "angle" (degrees)
  bool has_error = false;     // true if expression couldn't be resolved
  std::string error_message;  // e.g. "Unknown parameter: foo"
};

}  // namespace polysmith::core
