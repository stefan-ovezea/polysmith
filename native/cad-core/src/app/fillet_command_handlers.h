#pragma once

#include "protocol/ipc.h"

namespace polysmith::core {
class DocumentManager;
}

namespace polysmith {

// Handles the fillet commands (create / update-edges / update-radius /
// confirm). Returns true when it consumed the command. Promoted out of
// `app/impl/feature_operation_fillet_command_handlers.inc` (Stage 4).
bool handle_fillet_command(const polysmith::protocol::CommandMessage& command,
                           polysmith::core::DocumentManager& manager);

}  // namespace polysmith
