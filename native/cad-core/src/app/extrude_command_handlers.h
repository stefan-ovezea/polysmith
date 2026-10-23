#pragma once

#include "protocol/ipc.h"

namespace polysmith::core {
class DocumentManager;
}

namespace polysmith {

// Handles the extrude commands (extrude profile/face, update mode/target/
// parameters/profiles). Returns true when it consumed the command. Promoted
// out of `app/impl/solid_feature_extrude_command_handlers.inc` (Stage 4).
bool handle_extrude_command(
    const polysmith::protocol::CommandMessage& command,
    polysmith::core::DocumentManager& manager);

}  // namespace polysmith
