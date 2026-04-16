#pragma once

#include <optional>
#include <string>

namespace polysmith::core {

struct BoxFeatureParameters {
  double width;
  double height;
  double depth;
};

struct FeatureEntry {
  std::string id;
  std::string kind;
  std::string name;
  std::string status;
  std::string parameters_summary;
  std::optional<BoxFeatureParameters> box_parameters;
};

}  // namespace polysmith::core
