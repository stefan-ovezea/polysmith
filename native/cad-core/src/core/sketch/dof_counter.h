#pragma once

#include <string>
#include <vector>

#include "core/feature.h"

namespace polysmith::core {

enum class DofStatus {
  UnderConstrained,
  FullyConstrained,
  OverConstrained,
};

struct EntityDofResult {
  std::string entity_id;
  std::string entity_kind;
  int total_dof;
  int consumed_dof;
  DofStatus status;
};

std::vector<EntityDofResult> count_sketch_dof(
    const SketchFeatureParameters& params);

DofStatus get_entity_dof_status(
    const SketchFeatureParameters& params,
    const std::string& entity_id);

} // namespace polysmith::core
