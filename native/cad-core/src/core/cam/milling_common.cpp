#include "core/cam/milling_common.h"

namespace polysmith::core {
namespace milling {

bool check_tool_axis_supported(const CamOperation& op, std::string& error) {
  const auto& mode = op.parameters.tool_axis_mode;
  if (mode.empty() || mode == "fixed_z") {
    return true;
  }
  error = "operation '" + op.name + "' uses tool axis mode '" + mode +
          "', which no generator supports yet (only \"fixed_z\").";
  return false;
}

}  // namespace milling
}  // namespace polysmith::core
