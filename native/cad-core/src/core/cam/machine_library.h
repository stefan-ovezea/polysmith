#pragma once

#include <string>
#include <vector>

#include "core/cam/cam_types.h"

namespace polysmith::core {

// ── Machine library ───────────────────────────────────────────────
//
// A machine definition is a FILE, not code: one <slug>.json per machine
// in the user's machines directory (resolved by the shell and handed to
// the core via POLYSMITH_MACHINES_DIR), seeded with the built-in
// definitions on first use and re-read on every list — saving a machine
// is writing a file, and edits in an external editor apply on the next
// list.  Mirrors the post-processor library (post_processor.h).

// Lists every machine definition: built-ins first, then files in the
// machines directory (a user file with the same name overrides the
// built-in), seeding the directory with the built-ins first.  Unreadable
// files are skipped with a log_warn.
std::vector<MachineDefinition> load_machine_library();

// Validates the definition (non-empty name, supported machine type,
// positive work area for lasers) and writes it as <slug>.json next to
// the other machines — an existing file with the same slug is
// overwritten (save = user intent, same as post import).  Returns the
// saved filename stem; sets `error` on failure.
std::string save_machine_definition(const MachineDefinition& machine,
                                    std::string& error);

}  // namespace polysmith::core
