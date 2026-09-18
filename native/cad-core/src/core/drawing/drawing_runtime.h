#pragma once

#include <string>
#include <unordered_map>

#include "core/drawing/drawing_types.h"

namespace polysmith::core {

struct DocumentState;

// ── Drawing projection runtime cache (memory-only, never serialized)
//
// ProjectionResults are pure functions of (view definition, resolved
// source geometry, document revision).  They are too large for the
// JSON document format, so generated projections live here, keyed by
// document id + view id, and are validated against document.revision
// on every read.  Any geometry bump invalidates every cached
// projection of that document; a miss just means the view needs
// re-projection.
//
// Process-global rather than a DocumentManager member for the same
// reason as cam_runtime: the document is a plain value struct copied
// wholesale for undo snapshots, and build_viewport_state() receives
// only the document.

namespace drawing_runtime {

struct Entry {
  int revision = -1;
  ProjectionResult projection;
};

struct PerDocument {
  int last_revision = -1;
  std::unordered_map<std::string, Entry> projections;  // view_id -> result
  // Results pruned by drop_stale land here so a broken view can hold
  // its last-known projection (marked stale) instead of going blank
  // when its source body disappears — the SolidWorks-detached
  // precedent, never a silent substitute.
  std::unordered_map<std::string, Entry> last_known;   // view_id -> result
};

PerDocument& document_state(const std::string& document_id);
void clear(const std::string& document_id);

// Returns nullptr when nothing is cached or the cache predates the
// current revision.
const ProjectionResult* cached_projection(const DocumentState& document,
                                          const std::string& view_id);

// The most recently dropped result for a view (any revision) — the
// refresh pass's source for last-known state on broken views.
const ProjectionResult* last_known_projection(const DocumentState& document,
                                              const std::string& view_id);

// Revision-explicit variant: the refresh pass runs inside
// bump_geometry_revision BEFORE the revision counter increments, so it
// must validate entries against the upcoming revision, not the current
// one.
const ProjectionResult* cached_projection_at(const DocumentState& document,
                                             const std::string& view_id,
                                             int revision);

void store_projection(const DocumentState& document,
                      const std::string& view_id, ProjectionResult result);

// Revision-explicit store: a mutation that bumps the revision as part
// of the same command stamps the result with the UPCOMING revision so
// the dependency pass running inside the bump sees it as current.
void store_projection_at(const DocumentState& document,
                         const std::string& view_id, ProjectionResult result,
                         int target_revision);

// Drops cached projections whose revision no longer matches (called
// by the drawing dependency refresh pass after a geometry bump).
void drop_stale(const DocumentState& document, int target_revision);

// Erases every cached projection for the document.  Undo/redo call
// this BEFORE the restore's refresh pass: a revision stamp alone
// cannot distinguish a result stored on the abandoned branch from a
// current one (the restored revision can collide with the abandoned
// branch's bumped value), so a branch switch invalidates everything
// and the views honestly fall back to re-projection.
void invalidate(const std::string& document_id);

}  // namespace drawing_runtime

}  // namespace polysmith::core
