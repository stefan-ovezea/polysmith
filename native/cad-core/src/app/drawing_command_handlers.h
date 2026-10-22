#pragma once

#include "protocol/ipc.h"

namespace polysmith::core {
class DocumentManager;
}

namespace polysmith {

// Handles the drawing commands (create/delete, sheets, views, sections, title
// block, export, dimensions, notes, annotations + their previews). Returns
// true when it consumed the command. Promoted out of
// `app/impl/drawing_commands.inc` (Stage 4 of the refactoring plan).
bool handle_drawing_command(const polysmith::protocol::CommandMessage& command,
                            polysmith::core::DocumentManager& manager);

}  // namespace polysmith
