#pragma once

#include "protocol/ipc.h"

namespace polysmith::core {
class DocumentManager;
}

namespace polysmith {

// Handles the import/export commands (STEP/DXF/IGES/STL import, DXF/IGES
// export, mesh conversion). Returns true when it consumed the command.
// Promoted out of `app/impl/{step,dxf,iges,mesh}_command_handlers.inc` into one
// translation unit (Stage 4 of the refactoring plan).
bool handle_import_command(const polysmith::protocol::CommandMessage& command,
                           polysmith::core::DocumentManager& manager);

}  // namespace polysmith
