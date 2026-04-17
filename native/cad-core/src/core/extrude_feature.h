#pragma once

#include "core/feature.h"

namespace polysmith::core {

FeatureEntry create_extrude_feature(int feature_index,
                                    const ExtrudeFeatureParameters& parameters);

}  // namespace polysmith::core
