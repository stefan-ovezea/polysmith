#include "core/viewport.h"

namespace polysmith::core {
namespace {

constexpr double kBoxSpacing = 10.0;

}  // namespace

ViewportState build_viewport_state(const std::optional<DocumentState>& document) {
  if (!document.has_value()) {
    return ViewportState{
        .has_active_document = false,
        .boxes = {},
        .scene_width = 0.0,
        .scene_height = 0.0,
        .scene_depth = 0.0,
    };
  }

  std::vector<ViewportBoxPrimitive> boxes;
  double current_x_offset = 0.0;
  double max_height = 0.0;
  double max_depth = 0.0;

  for (const auto& feature : document->feature_history) {
    if (feature.kind != "box" || !feature.box_parameters.has_value()) {
      continue;
    }

    const auto& parameters = feature.box_parameters.value();
    boxes.push_back(ViewportBoxPrimitive{
        .id = feature.id,
        .label = feature.name,
        .width = parameters.width,
        .height = parameters.height,
        .depth = parameters.depth,
        .x_offset = current_x_offset,
        .is_selected =
            document->selected_feature_id.has_value() &&
            document->selected_feature_id.value() == feature.id,
    });

    current_x_offset += parameters.width + kBoxSpacing;
    if (parameters.height > max_height) {
      max_height = parameters.height;
    }
    if (parameters.depth > max_depth) {
      max_depth = parameters.depth;
    }
  }

  const double scene_width = boxes.empty() ? 0.0 : current_x_offset - kBoxSpacing;

  return ViewportState{
      .has_active_document = true,
      .boxes = boxes,
      .scene_width = scene_width,
      .scene_height = max_height,
      .scene_depth = max_depth,
  };
}

}  // namespace polysmith::core
