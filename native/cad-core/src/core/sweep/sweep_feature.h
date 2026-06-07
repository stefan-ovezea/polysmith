#pragma once

#include "core/feature.h"

namespace polysmith::core {

FeatureEntry create_sweep_feature(int feature_index,
                                  const SweepFeatureParameters& parameters);

}  // namespace polysmith::core
