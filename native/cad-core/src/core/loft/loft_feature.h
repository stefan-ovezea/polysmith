#pragma once

#include "core/feature.h"

namespace polysmith::core {

FeatureEntry create_loft_feature(int feature_index,
                                 const LoftFeatureParameters& parameters);

void update_loft_ruled(FeatureEntry& feature, bool ruled);

}  // namespace polysmith::core
