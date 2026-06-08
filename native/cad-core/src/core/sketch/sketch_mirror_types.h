#pragma once

#include <string>

namespace polysmith::core {

/// Records a persistent mirror relationship: `mirror_id` is the
/// mirror of `source_id` across `axis_line_id`. The recompute pass
/// re-projects `source_id`'s geometry across the axis and writes
/// the result into `mirror_id`.
struct SketchMirrorRelation {
  std::string id;
  std::string source_id;
  std::string mirror_id;
  std::string axis_line_id;
};

}  // namespace polysmith::core
