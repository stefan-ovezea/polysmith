#include "core/sketch/sketch_feature.h"
#include "core/sketch/dof_counter.h"
#include "core/sketch/sketch_tool_ids.h"
#include "core/diagnostics/logger.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <deque>
#include <sstream>
#include <stdexcept>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "core/sketch/constraint_solver.h"
#include "core/sketch/formula_eval.h"
#include "core/sketch/inference_engine.h"
#include "core/sketch/impl/private_vertex_lookup_helpers.inc"
#include "core/sketch/sketch_profile.h"
#include "core/sketch/trim_engine.h"
#include "core/text_engine.h"

namespace polysmith::core {

// Forward declaration — defined later but used in refresh_mirror pass.
std::pair<double, double> reflect_point_across_line(double px, double py,
                                                     double ax, double ay,
                                                     double bx, double by);

namespace {

#include "core/sketch/impl/private_basic_helpers.inc"
#include "core/sketch/impl/private_point_profile_helpers.inc"
#include "core/sketch/impl/private_fillet_refresh.inc"
#include "core/sketch/impl/private_point_propagation.inc"
#include "core/sketch/impl/private_dimension_relation_sync.inc"
#include "core/sketch/impl/private_relation_enforcement.inc"
#include "core/sketch/impl/constraint_completion_helpers.inc"
#include "core/sketch/impl/constraint_completion_enforcement.inc"
}  // namespace

#include "core/sketch/impl/state_and_create.inc"
#include "core/sketch/impl/text_expansion.inc"
#include "core/sketch/impl/sketch_text_commands.inc"
#include "core/sketch/impl/line_constraints.inc"
#include "core/sketch/impl/vertex_and_circle_updates.inc"
#include "core/sketch/impl/constraint_completion_commands.inc"
#include "core/sketch/impl/dimensions.inc"
#include "core/sketch/impl/line_primitives.inc"
#include "core/sketch/impl/mirror.inc"
#include "core/sketch/impl/curve_primitives.inc"
#include "core/sketch/impl/fillet_polygon_trim.inc"

}  // namespace polysmith::core
