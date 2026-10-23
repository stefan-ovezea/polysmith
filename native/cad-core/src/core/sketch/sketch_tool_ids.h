#pragma once

#include <algorithm>
#include <string>
#include <vector>

namespace polysmith::core {

// Canonical list of sketch tool ids accepted over IPC. There are two
// whitelists that must stay in sync — validate_tool (sketch core) and
// is_supported_sketch_tool (document layer) — and both delegate to this
// list so they cannot drift apart (they drifted once before: "move" was
// accepted by one and rejected by the other; see
// cad_core_tool_whitelist_test).
//
// This list must cover every id the desktop UI can arm, i.e. the `SketchTool`
// union in `apps/desktop-ui/src/types/geometry/sketch.ts` (Shape2D +
// select/project/dimension/move/text). `corner` and `split` were missing
// while both tools were already in the toolbar, so arming them answered
// "Unsupported sketch tool: split" (reported 2026-10-23; now pinned by
// tool_whitelist_test's required-tools check).
inline const std::vector<std::string>& kSupportedSketchTools() {
  static const std::vector<std::string> tools = {
      "select", "line",    "rectangle", "circle", "polygon", "arc",
      "fillet", "trim",    "project",   "dimension",
      "move",   "text",    "ellipse",   "slot",    "chamfer",
      "extend", "offset",  "spline",    "corner",  "split",
      "point",
  };
  return tools;
}

inline bool is_supported_sketch_tool_id(const std::string& tool) {
  const auto& tools = kSupportedSketchTools();
  return std::find(tools.begin(), tools.end(), tool) != tools.end();
}

}  // namespace polysmith::core
