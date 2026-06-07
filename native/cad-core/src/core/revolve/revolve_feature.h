#pragma once

#include "core/feature.h"

namespace polysmith::core {

FeatureEntry create_revolve_feature(int feature_index,
                                    const RevolveFeatureParameters& parameters);

void update_revolve_angle(FeatureEntry& feature, double angle_degrees);

}  // namespace polysmith::core
