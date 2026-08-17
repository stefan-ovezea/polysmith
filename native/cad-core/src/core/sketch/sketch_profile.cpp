#include "core/sketch/sketch_profile.h"

#include "core/diagnostics/logger.h"
#include "core/document/feature.h"
#include "core/sketch/trim_engine.h"
#include "core/sketch/impl/private_vertex_lookup_helpers.inc"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <deque>
#include <limits>
#include <map>
#include <optional>
#include <set>
#include <sstream>
#include <string>
#include <vector>

namespace polysmith::core {
namespace {

#include "core/sketch/impl/sketch_profile_basic_helpers.inc"
#include "core/sketch/impl/sketch_profile_topology.inc"

}  // namespace

#include "core/sketch/impl/build_sketch_profile_regions.inc"
#include "core/sketch/impl/detect_sketch_profiles.inc"

void refresh_sketch_profiles(FeatureEntry& feature) {
  if (feature.kind != "sketch" || !feature.sketch_parameters.has_value()) {
    return;
  }
  feature.sketch_parameters->profiles =
      build_sketch_profile_regions(*feature.sketch_parameters);
}

}  // namespace polysmith::core
