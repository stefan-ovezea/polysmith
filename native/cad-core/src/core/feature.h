#pragma once

#include <optional>
#include <string>
#include <vector>

namespace polysmith::core {

struct BoxFeatureParameters {
  double width;
  double height;
  double depth;
};

struct CylinderFeatureParameters {
  double radius;
  double height;
};

struct SketchLine {
  std::string id;
  double start_x;
  double start_y;
  double end_x;
  double end_y;
  std::optional<std::string> constraint_hint;
};

struct SketchCircle {
  std::string id;
  double center_x;
  double center_y;
  double radius;
};

struct SketchFeatureParameters {
  std::string plane_id;
  std::string active_tool;
  std::vector<SketchLine> lines;
  std::vector<SketchCircle> circles;
};

struct FeatureEntry {
  std::string id;
  std::string kind;
  std::string name;
  std::string status;
  std::string parameters_summary;
  std::optional<BoxFeatureParameters> box_parameters;
  std::optional<CylinderFeatureParameters> cylinder_parameters;
  std::optional<SketchFeatureParameters> sketch_parameters;
};

}  // namespace polysmith::core
