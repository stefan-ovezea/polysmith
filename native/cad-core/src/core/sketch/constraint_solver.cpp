#include "core/sketch/constraint_solver.h"

#include <GCS.h>
#include <Geo.h>
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace polysmith::core {

void ConstraintSolver::build(const SketchFeatureParameters& params) {
#include "core/sketch/impl/constraint_solver_parameter_storage.inc"
#include "core/sketch/impl/constraint_solver_entity_mapping.inc"
#include "core/sketch/impl/constraint_solver_system_setup.inc"
#include "core/sketch/impl/constraint_solver_geometric_constraints.inc"
#include "core/sketch/impl/constraint_solver_dimension_constraints.inc"
}

#include "core/sketch/impl/constraint_solver_lifecycle.inc"

}  // namespace polysmith::core
