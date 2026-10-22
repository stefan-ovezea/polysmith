#pragma once

#include "protocol/ipc.h"

namespace polysmith::core {
class DocumentManager;
}

namespace polysmith {

// Handles the chamfer and shell commands. Returns true when it consumed the
// command. Promoted out of the matching `app/impl/feature_operation_*_inc`
// fragments (Stage 4 of the refactoring plan).
bool handle_body_modifier_command(
    const polysmith::protocol::CommandMessage& command,
    polysmith::core::DocumentManager& manager);

}  // namespace polysmith
