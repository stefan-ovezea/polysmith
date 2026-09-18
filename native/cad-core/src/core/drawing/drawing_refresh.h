#pragma once

#include "core/document/document_state.h"

namespace polysmith::core {

// The drawing dependency pass — runs inside bump_geometry_revision
// right after refresh_cam_dependencies, so every geometry bump
// re-projects the views that depend on it.
//
// One dirty model: views whose projection is already cached at the
// target revision are skipped (cheap); everything else re-resolves
// its frame (standard view or custom frame), compiles its source
// bodies, and re-projects.  A missing source body degrades the view
// to its last-known projection marked stale with broken_ref +
// warning — never a crash, never a silent substitute.  Generated
// results are stamped with the target revision via the
// revision-explicit store (the pass runs BEFORE the revision counter
// increments).
void refresh_drawing_dependencies(DocumentState& document,
                                  int target_revision);

}  // namespace polysmith::core
