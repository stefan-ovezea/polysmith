#pragma once

#include "protocol/ipc.h"

namespace polysmith::core {
class DocumentManager;
}

namespace polysmith {

// Handles the feature creation/update, plugin-feature, undo/redo, selection and
// appearance commands. Returns true when it consumed the command. Promoted out
// of `app/impl/feature_selection_appearance_commands.inc` (Stage 4).
bool handle_feature_command(const polysmith::protocol::CommandMessage& command,
                            polysmith::core::DocumentManager& manager);

}  // namespace polysmith
