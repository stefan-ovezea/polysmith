#pragma once

#include "core/document/feature.h"

namespace polysmith::core {

FeatureEntry create_sweep_feature(int feature_index,
                                  const SweepFeatureParameters& parameters);

}  // namespace polysmith::core
