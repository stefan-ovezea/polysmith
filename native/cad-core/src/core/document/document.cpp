#include "core/document/document.h"
#include "core/document/document_internal.h"
#include "core/cam/cam_runtime.h"
#include "core/drawing/drawing_annotation_geometry.h"
#include "core/drawing/drawing_projection.h"
#include "core/drawing/drawing_refresh.h"
#include "core/drawing/drawing_resolution.h"
#include "core/drawing/drawing_runtime.h"
#include "core/drawing/export/drawing_export.h"
#include "core/diagnostics/logger.h"
#include "core/pcb/pcb_layer_io.h"
#include "core/pcb/pcb_role.h"
#include "core/sketch/sketch_feature.h"
#include "core/sketch/sketch_profile.h"
#include "core/sketch/spline_math.h"
#include "core/sketch/sketch_tool_ids.h"
#include "dxf/dxf_export.h"
#include "dxf/dxf_import.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <limits>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>

#include <nlohmann/json.hpp>

#include "core/geometry/body_compiler.h"
#include "core/cam/cam_export.h"
#include "core/cam/cam_profile_reference.h"
#include "core/cam/cam_refresh.h"
#include "core/construction/construction_plane_feature.h"
#include "core/geometry/edge_geometry.h"
#include "core/geometry/extrude_helpers.h"
#include "core/geometry/face_geometry.h"
#include "core/geometry/face_signature.h"
#include "core/geometry/feature_shape.h"
#include "core/geometry/import_runtime.h"
#include "core/geometry/shape_snapshot.h"
#include "core/geometry/mesh_import_helpers.h"
#include "core/geometry/mesh_projection.h"
#include "core/geometry/step_import_helpers.h"
#include "core/geometry/iges_import_helpers.h"
#include "core/sketch/formula_eval.h"
#include "core/sketch/impl/private_vertex_lookup_helpers.inc"
#include "core/geometry/refresh_dependents.h"
#include "protocol/serialization.h"

namespace polysmith::core {
namespace {

#include "core/document/impl/private_document_helpers.inc"
#include "core/document/impl/private_extrude_frame_helpers.inc"
#include "core/document/impl/private_profile_feature_helpers.inc"
#include "core/document/impl/private_sketch_feature_parameter_helpers.inc"
#include "core/document/impl/private_linked_feature_refresh.inc"
#include "core/document/impl/private_selection_projection_helpers.inc"
#include "core/document/impl/private_projection_command_helpers.inc"
#include "core/document/impl/private_threaded_feature_helpers.inc"
}  // namespace

#include "core/document/impl/lifecycle.inc"
#include "core/document/impl/edge_features.inc"
#include "core/document/impl/sketch_commands.inc"
#include "core/document/impl/sketch_dimension_commands.inc"
#include "core/document/impl/sketch_trim_commands.inc"
#include "core/document/impl/sketch_trim_stroke_commands.inc"
#include "core/document/impl/sketch_extend_commands.inc"
#include "core/document/impl/sketch_corner_trim_commands.inc"
#include "core/document/impl/sketch_split_commands.inc"
#include "core/document/impl/sketch_offset_commands.inc"
#include "core/document/impl/sketch_array_commands.inc"
#include "core/document/impl/profile_selection_commands.inc"
#include "core/document/impl/extrude_commands.inc"
#include "core/document/impl/loft_commands.inc"
#include "core/document/impl/revolve_commands.inc"
#include "core/document/impl/sweep_commands.inc"
#include "core/document/impl/sketch_entity_commands.inc"
#include "core/document/impl/sketch_selection_commands.inc"
#include "core/document/impl/projection_commands.inc"
#include "core/document/impl/construction_commands.inc"
#include "core/document/impl/hole_commands.inc"
#include "core/document/impl/body_appearance_commands.inc"
#include "core/document/impl/mesh_commands.inc"
#include "core/document/impl/dxf_commands.inc"
#include "core/document/impl/pcb_commands.inc"

// Re-derive the source-profile caches of every sketch-derived feature after a
// document load. `refresh_linked_extrudes` lives in this TU's anonymous
// namespace; this wrapper is the one place other TUs (the load path) can call
// it through. Each sketch's profiles are already refreshed by the caller, so
// the builder rebuilds extrude / loft / revolve / sweep profile geometry from
// the current sketch state.
void refresh_linked_features_after_load(DocumentState& document) {
  const std::vector<FeatureEntry> sketches = [&document]() {
    std::vector<FeatureEntry> result;
    for (const auto& feature : document.feature_history) {
      if (feature.kind == "sketch" && feature.sketch_parameters.has_value()) {
        result.push_back(feature);
      }
    }
    return result;
  }();
  for (const auto& sketch : sketches) {
    // The sketch copy keeps a dangling-feature scenario harmless: the refresh
    // matches by feature id against the LIVE history, so a sketch removed by
    // a later migration simply matches nothing.
    refresh_linked_extrudes(document, sketch);
  }
}
}  // namespace polysmith::core
