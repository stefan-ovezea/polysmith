#pragma once

#include "protocol/ipc.h"

namespace polysmith::core {
class DocumentManager;
}

namespace polysmith {

// Handles the loft / revolve / sweep and sketch-profile-selection commands.
// Returns true when it consumed the command. Promoted out of the matching
// `app/impl/solid_feature_*_inc` fragments (Stage 4 of the refactoring plan).
bool handle_solid_feature_command(
    const polysmith::protocol::CommandMessage& command,
    polysmith::core::DocumentManager& manager);

}  // namespace polysmith
