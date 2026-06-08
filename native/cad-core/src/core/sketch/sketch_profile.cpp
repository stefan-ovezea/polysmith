#include "core/sketch/sketch_profile.h"

#include "core/document/feature.h"

#include <algorithm>
#include <cmath>
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
#include "core/sketch/impl/sketch_profile_edge_helpers.inc"
#include "core/sketch/impl/sketch_profile_topology.inc"

}  // namespace

#include "core/sketch/impl/build_sketch_profile_regions.inc"
#include "core/sketch/impl/detect_sketch_profiles.inc"

}  // namespace polysmith::core
