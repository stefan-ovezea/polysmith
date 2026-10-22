#pragma once

#include "protocol/ipc.h"

namespace polysmith::core {
class DocumentManager;
}

namespace polysmith {

// Handles the trim-preview command (returns per-entity-kind intersection
// segment data). Returns true when it consumed the command. Promoted out of
// `app/impl/trim_preview_commands.inc` (Stage 4 of the refactoring plan).
bool handle_trim_preview_command(
    const polysmith::protocol::CommandMessage& command,
    polysmith::core::DocumentManager& manager);

}  // namespace polysmith
