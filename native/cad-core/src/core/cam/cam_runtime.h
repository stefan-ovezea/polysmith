#pragma once

#include <string>
#include <unordered_map>

#include "core/cam/toolpath.h"

namespace polysmith::core {

struct DocumentState;

// ── Toolpath runtime cache (memory-only, never serialized) ────────
//
// Toolpaths are pure functions of (operation parameters, resolved
// geometry, document revision).  They are too large for the JSON
// document format, so generated paths live here, keyed by document id
// + operation id, and are validated against document.revision on every
// read.  Any geometry bump invalidates every cached path of that
// document; a miss just means the operation needs regeneration.
//
// Process-global rather than a DocumentManager member because the
// document is a plain value struct copied wholesale for undo
// snapshots, and build_viewport_state() receives only the document.

namespace cam_runtime {

struct Entry {
  int revision = -1;
  Toolpath toolpath;
};

struct PerDocument {
  int last_revision = -1;
  std::unordered_map<std::string, Entry> generated;  // op_id -> full path
  std::unordered_map<std::string, Entry> previews;   // op_id -> wireframe
};

PerDocument& document_state(const std::string& document_id);
void clear(const std::string& document_id);

// Returns nullptr when nothing is cached or the cache predates the
// current revision.
const Toolpath* cached_toolpath(const DocumentState& document,
                                const std::string& op_id);
const Toolpath* cached_preview(const DocumentState& document,
                               const std::string& op_id);

// Revision-explicit variants: the refresh pass runs inside
// bump_geometry_revision BEFORE the revision counter increments, so it
// must validate entries against the upcoming revision, not the current
// one.
const Toolpath* cached_toolpath_at(const DocumentState& document,
                                   const std::string& op_id, int revision);
const Toolpath* cached_preview_at(const DocumentState& document,
                                  const std::string& op_id, int revision);

void store_generated(const DocumentState& document,
                     const std::string& op_id, Toolpath toolpath);
void store_preview(const DocumentState& document,
                   const std::string& op_id, Toolpath toolpath);

// Drops cached paths whose revision no longer matches (called by the
// CAM dependency refresh pass after a geometry bump).
void drop_stale(const DocumentState& document, int target_revision);

}  // namespace cam_runtime

}  // namespace polysmith::core
