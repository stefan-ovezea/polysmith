#pragma once

#include "protocol/ipc.h"

namespace polysmith::core {
class DocumentManager;
}

namespace polysmith {

// Handles the simple sketch-edit tool commands (extend / split / offset /
// trim-stroke). Returns true when it consumed the command. Promoted out of the
// matching `app/impl/*_command_handlers.inc` fragments (Stage 4 of the
// refactoring plan).
bool handle_sketch_tool_command(
    const polysmith::protocol::CommandMessage& command,
    polysmith::core::DocumentManager& manager);

}  // namespace polysmith
