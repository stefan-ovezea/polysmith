#pragma once

#include "protocol/ipc.h"

namespace polysmith::core {
class DocumentManager;
}

namespace polysmith {

// Handles the PCB layer commands (capture reference, import, update, remove).
// Returns true when it consumed the command. Promoted out of
// `app/impl/pcb_command_handlers.inc` (Stage 4 of the refactoring plan).
bool handle_pcb_command(const polysmith::protocol::CommandMessage& command,
                        polysmith::core::DocumentManager& manager);

}  // namespace polysmith
