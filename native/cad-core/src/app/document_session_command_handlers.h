#pragma once

#include "protocol/ipc.h"

namespace polysmith::core {
class DocumentManager;
}

namespace polysmith {

// Handles the document session commands (ping, create/get/save/load, viewport,
// sketch-profiles, exports). Returns true when it consumed the command.
// Promoted out of `app/impl/document_session_commands.inc` (Stage 4).
bool handle_document_session_command(
    const polysmith::protocol::CommandMessage& command,
    polysmith::core::DocumentManager& manager);

}  // namespace polysmith
