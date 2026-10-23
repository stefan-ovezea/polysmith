#pragma once

#include "protocol/ipc.h"

namespace polysmith::core {
class DocumentManager;
}

namespace polysmith {

// Handles the sketch text, array, parameter and selection-filter commands.
// Returns true when it consumed the command. Promoted out of the matching
// `app/impl/sketch_text_*`, `sketch_array_*` and `parameter_filter_trim_*`
// fragments (Stage 4 of the refactoring plan).
bool handle_sketch_utility_command(
    const polysmith::protocol::CommandMessage& command,
    polysmith::core::DocumentManager& manager);

}  // namespace polysmith
