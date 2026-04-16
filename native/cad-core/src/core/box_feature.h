#pragma once

#include "core/feature.h"

namespace polysmith::core {

FeatureEntry create_box_feature(int feature_index,
                                const BoxFeatureParameters& parameters);
void update_box_feature(FeatureEntry& feature,
                        const BoxFeatureParameters& parameters);

}  // namespace polysmith::core
