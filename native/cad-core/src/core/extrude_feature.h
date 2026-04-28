#pragma once

#include "core/feature.h"

namespace polysmith::core {

FeatureEntry create_extrude_feature(int feature_index,
                                    const ExtrudeFeatureParameters& parameters);

void update_extrude_depth(FeatureEntry& feature, double depth);

}  // namespace polysmith::core
