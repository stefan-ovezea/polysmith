#include "core/plugin/plugin_feature.h"

#include <stdexcept>

namespace polysmith::core {
namespace {

std::string summary_for_plugin_feature(const PluginFeatureParameters& parameters) {
  if (!parameters.parameters_summary.empty()) {
    return parameters.parameters_summary;
  }
  return parameters.feature_type;
}

}  // namespace

void validate_plugin_feature_parameters(
    const PluginFeatureParameters& parameters) {
  if (parameters.plugin_id.empty()) {
    throw std::runtime_error("Plugin feature requires a plugin id");
  }
  if (parameters.feature_type.empty()) {
    throw std::runtime_error("Plugin feature requires a feature type");
  }
  if (parameters.display_name.empty()) {
    throw std::runtime_error("Plugin feature requires a display name");
  }
  if (parameters.geometry.empty()) {
    throw std::runtime_error("Plugin feature requires geometry operations");
  }
  for (const auto& operation : parameters.geometry) {
    if (operation.operation != "add" && operation.operation != "subtract") {
      throw std::runtime_error("Unsupported plugin geometry operation: " +
                               operation.operation);
    }
    if (operation.primitive != "box" &&
        operation.primitive != "rounded_box" &&
        operation.primitive != "tapered_rounded_box" &&
        operation.primitive != "cylinder") {
      throw std::runtime_error("Unsupported plugin geometry primitive: " +
                               operation.primitive);
    }
    if (operation.height <= 0.0) {
      throw std::runtime_error(
          "Plugin geometry dimensions must be greater than zero");
    }
    if ((operation.primitive == "box" || operation.primitive == "rounded_box" ||
         operation.primitive == "tapered_rounded_box") &&
        (operation.width <= 0.0 || operation.depth <= 0.0)) {
      throw std::runtime_error(
          "Plugin box dimensions must be greater than zero");
    }
    if (operation.primitive == "tapered_rounded_box" &&
        ((operation.top_width != 0.0 && operation.top_width <= 0.0) ||
         (operation.top_depth != 0.0 && operation.top_depth <= 0.0))) {
      throw std::runtime_error(
          "Plugin tapered box top dimensions must be greater than zero");
    }
    if (operation.primitive == "cylinder" && operation.radius <= 0.0) {
      throw std::runtime_error("Plugin cylinder radius must be greater than zero");
    }
  }
}

FeatureEntry create_plugin_feature(
    int feature_index,
    const PluginFeatureParameters& parameters) {
  validate_plugin_feature_parameters(parameters);

  return FeatureEntry{
      .id = "feature-" + std::to_string(feature_index),
      .kind = "plugin_feature",
      .name = parameters.display_name,
      .status = "healthy",
      .parameters_summary = summary_for_plugin_feature(parameters),
      .plugin_parameters = parameters,
  };
}

void update_plugin_feature(
    FeatureEntry& feature,
    const PluginFeatureParameters& parameters) {
  if (feature.kind != "plugin_feature" ||
      !feature.plugin_parameters.has_value()) {
    throw std::runtime_error("Only plugin features can be updated");
  }
  validate_plugin_feature_parameters(parameters);
  feature.name = parameters.display_name;
  feature.parameters_summary = summary_for_plugin_feature(parameters);
  feature.plugin_parameters = parameters;
}

void confirm_plugin_feature(FeatureEntry& feature) {
  if (feature.kind != "plugin_feature" ||
      !feature.plugin_parameters.has_value()) {
    throw std::runtime_error("Only plugin features can be confirmed");
  }
  feature.plugin_parameters->is_pending = false;
}

}  // namespace polysmith::core
