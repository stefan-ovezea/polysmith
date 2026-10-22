#pragma once

#include "protocol/ipc.h"

namespace polysmith::core {
class DocumentManager;
}

namespace polysmith {

// Handles the sketch-geometry creation commands (line, rectangle, arc, circle,
// polygon, ellipse, spline, slot + construction/anchor setters). Returns true
// when it consumed the command. Promoted out of
// `app/impl/sketch_create_geometry_command_handlers.inc` (Stage 4).
bool handle_sketch_create_command(
    const polysmith::protocol::CommandMessage& command,
    polysmith::core::DocumentManager& manager);

}  // namespace polysmith
