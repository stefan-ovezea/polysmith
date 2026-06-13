#pragma once

#include "core/document/feature.h"
#include "core/plugin/plugin_types.h"

namespace polysmith::core {

void validate_plugin_feature_parameters(const PluginFeatureParameters& parameters);
FeatureEntry create_plugin_feature(int feature_index,
                                   const PluginFeatureParameters& parameters);
void update_plugin_feature(FeatureEntry& feature,
                           const PluginFeatureParameters& parameters);
void confirm_plugin_feature(FeatureEntry& feature);

}  // namespace polysmith::core
