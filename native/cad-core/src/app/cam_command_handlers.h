#pragma once

#include "protocol/ipc.h"

namespace polysmith::core {
class DocumentManager;
}

namespace polysmith {

// Handles the CAM commands (setups, stock, tools, operations, post processors,
// machine library, tool parsing/export, gcode export, toolpath generation).
// Returns true when it consumed the command. Promoted out of
// `app/impl/cam_commands.inc` (Stage 4 of the refactoring plan).
bool handle_cam_command(const polysmith::protocol::CommandMessage& command,
                        polysmith::core::DocumentManager& manager);

}  // namespace polysmith
