#pragma once

namespace polysmith::core {

struct DocumentState;

// ── CAM dependency refresh pass ───────────────────────────────────
//
// The CAM analogue of refresh_history_dependencies: re-resolves every
// operation's geometry references against the live document after a
// revision bump, and degrades failures the way CAD features do —
// status "error" + a human-readable status_message, never a throw,
// never a crash.  Also prunes toolpath cache entries whose revision
// no longer matches (the runtime cache is keyed on document.revision,
// which advances on every bump).
//
// Hooked into DocumentManager::bump_geometry_revision() right after
// refresh_history_dependencies, so it runs on every geometry mutation
// and on load.  Early-outs when the document has no CAM operations,
// so ordinary CAD edits pay nothing.
//
// `target_revision` is the revision the document is about to become:
// bump_geometry_revision increments the counter AFTER the refresh
// runs, and the runtime cache must be validated against the upcoming
// revision, not the one being left behind.

void refresh_cam_dependencies(DocumentState& document, int target_revision);

}  // namespace polysmith::core
