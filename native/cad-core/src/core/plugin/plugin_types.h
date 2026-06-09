#pragma once

#include <string>
#include <vector>

namespace polysmith::core {

struct PluginGeometryOperation {
  std::string operation = "add";
  std::string primitive = "box";
  double x = 0.0;
  double y = 0.0;
  double z = 0.0;
  double width = 1.0;
  double depth = 1.0;
  double height = 1.0;
  double radius = 0.0;
  double top_width = 0.0;
  double top_depth = 0.0;
  double top_radius = 0.0;
};

struct PluginFeatureParameters {
  std::string plugin_id;
  std::string feature_type;
  int schema_version = 1;
  bool is_pending = true;
  std::string display_name = "Plugin Feature";
  std::string parameters_summary;
  std::string parameters_json = "{}";
  std::vector<PluginGeometryOperation> geometry;
};

}  // namespace polysmith::core
