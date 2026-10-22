#pragma once

#include "protocol/ipc.h"

namespace polysmith::core {
class DocumentManager;
}

namespace polysmith {

// Handles the sketch session, geometry-update, line-constraint and mirror
// commands. Returns true when it consumed the command. Promoted out of the
// matching `app/impl/sketch_*_inc` fragments (Stage 4 of the refactoring plan).
bool handle_sketch_edit_command(
    const polysmith::protocol::CommandMessage& command,
    polysmith::core::DocumentManager& manager);

}  // namespace polysmith
