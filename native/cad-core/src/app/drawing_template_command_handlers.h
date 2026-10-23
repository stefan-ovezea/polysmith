#pragma once

#include "protocol/ipc.h"

namespace polysmith::core {
class DocumentManager;
}

namespace polysmith {

// Handles the drawing-template save/load commands (CREATE DRAWING dialog).
// Returns true when it consumed the command. Promoted out of
// `app/impl/drawing_template_commands.inc` (Stage 4 of the refactoring plan).
bool handle_drawing_template_command(
    const polysmith::protocol::CommandMessage& command,
    polysmith::core::DocumentManager& manager);

}  // namespace polysmith
