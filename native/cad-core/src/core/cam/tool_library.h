#pragma once

#include <string>
#include <vector>

#include "core/cam/cam_types.h"

namespace polysmith::core {

// ── Shared tool library ────────────────────────────────────────────
//
// A tool in the SHARED library is a FILE, not code: one
// <tN>-<slug>.tool.json per tool in the user's tools directory
// (resolved by the shell and handed to the core via
// POLYSMITH_TOOLS_DIR), seeded with the built-in generic catalog on
// first use and re-read on every list — saving a tool is writing a
// file, and edits in an external editor apply on the next list.
// Mirrors the machine library (machine_library.h) and the
// post-processor library.

// Lists every shared tool: the built-in generic catalog first, then
// files in the tools directory (a user file whose filename stem
// matches a seeded one overrides it), seeding the directory with the
// built-ins first.  Unreadable files are skipped with a log_warn.
std::vector<ToolEntry> load_tool_library();

// Validates the tool (name, type, positive geometry) and writes it as
// <tN>-<slug>.tool.json next to the other tools — an existing file
// with the same slug is overwritten (save = user intent, same as
// machine save).  Returns the saved filename stem; sets `error` on
// failure.
std::string save_tool_entry(const ToolEntry& tool, std::string& error);

// True for the built-in catalog entries (guid "generic-tNN").  The
// UI uses this to distinguish the shipped catalog from user tools
// and to reconcile imports by guid.
bool is_generic_catalog_tool(const ToolEntry& tool);

}  // namespace polysmith::core
