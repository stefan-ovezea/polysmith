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
inline const std::vector<std::string>& kSupportedSketchTools() {
  static const std::vector<std::string> tools = {
      "select", "line",    "rectangle", "circle", "polygon", "arc",
      "fillet", "trim",    "project",   "dimension",
      "move",   "text",    "ellipse",   "slot",    "chamfer",
      "extend", "offset",
  };
  return tools;
}

inline bool is_supported_sketch_tool_id(const std::string& tool) {
  const auto& tools = kSupportedSketchTools();
  return std::find(tools.begin(), tools.end(), tool) != tools.end();
}

}  // namespace polysmith::core
