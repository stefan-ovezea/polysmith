#pragma once

#include "core/feature.h"

namespace polysmith::core {

FeatureEntry create_cylinder_feature(
    int feature_index, const CylinderFeatureParameters& parameters);
void update_cylinder_feature(FeatureEntry& feature,
                             const CylinderFeatureParameters& parameters);

}  // namespace polysmith::core
