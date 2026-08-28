#pragma once

#include <functional>
#include <string>

#include "core/cam/cam_generator.h"

namespace polysmith::core {

struct DocumentState;

// ── Generate driver ───────────────────────────────────────────────
//
// Runs a CAM operation through the generator registry: resolves the
// operation's geometry references against the live document, finds the
// tool and setup, builds the generator context, and calls the
// generator.  Returns the result plus a progress callback fired
// between stages (resolve → generate → finalize) so the app handler
// can emit cam_generation_progress events.  Synchronous — both v1
// generators complete in well under 100 ms; the registry interface
// keeps the door open for a background job pool later.
//
// Does NOT touch the runtime cache or the operation's status — the
// caller (the app handler) owns that so preview and generate share
// one code path.

struct CamGenerateOutcome {
  bool found = false;  // operation was located
  CamGenerateResult result;
};

CamGenerateOutcome generate_operation_toolpath(
    const DocumentState& document, const std::string& op_id, bool preview,
    const std::function<void(int)>& progress = nullptr);

}  // namespace polysmith::core
