#pragma once

#include "protocol/ipc.h"

namespace polysmith::core {
class DocumentManager;
}

namespace polysmith {

// Handles the threaded-feature commands (helix / thread / fastener). Returns
// true when it consumed the command. Promoted out of
// `app/impl/feature_operation_threaded_command_handlers.inc` (Stage 4).
bool handle_threaded_command(
    const polysmith::protocol::CommandMessage& command,
    polysmith::core::DocumentManager& manager);

}  // namespace polysmith
