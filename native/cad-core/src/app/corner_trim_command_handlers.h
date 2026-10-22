#pragma once

#include "protocol/ipc.h"

namespace polysmith::core {
class DocumentManager;
}

namespace polysmith {

// Handles the corner-trim commands (commit + preview). Returns true when it
// consumed the command. Promoted out of
// `app/impl/corner_trim_command_handlers.inc` (Stage 4 of the refactoring plan).
bool handle_corner_trim_command(
    const polysmith::protocol::CommandMessage& command,
    polysmith::core::DocumentManager& manager);

}  // namespace polysmith
