#pragma once

#include <vector>

#include "core/document.h"

namespace polysmith::core {

struct ViewportBoxPrimitive {
  std::string id;
  std::string label;
  double width;
  double height;
  double depth;
  double x_offset;
  bool is_selected;
};

struct ViewportState {
  bool has_active_document;
  std::vector<ViewportBoxPrimitive> boxes;
  double scene_width;
  double scene_height;
  double scene_depth;
};

ViewportState build_viewport_state(const std::optional<DocumentState>& document);

}  // namespace polysmith::core
