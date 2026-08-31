#pragma once

#include <array>
#include <optional>
#include <string>
#include <vector>

#include "core/cam/cam_types.h"
#include "core/cam/post_definition.h"
#include "core/cam/toolpath.h"

namespace polysmith::core {

struct CamSetup;

// ── Post-processor framework ──────────────────────────────────────
//
// A post processor is a FILE, not code: one <name>.json per dialect in
// the user's posts directory (resolved by the shell and handed to the
// core via POLYSMITH_POSTS_DIR), seeded with the built-in definitions
// on first use and re-read on every export — edits apply immediately,
// importing a post is copying a file.  The definition's line templates
// control the output shape; the engine keeps the modal state.

struct PostContext {
  const Toolpath& toolpath;
  const CamSetup& setup;
  const ToolEntry& tool;
  std::string op_name;
  // Spindle rpm for mill operations (CamOperationParameters).
  double spindle_rpm = 8000.0;
  // Resolved machine origin (WcsOrigin::position); identity offset
  // when absent.  Machine coordinates = world - wcs_origin.
  std::array<double, 3> wcs_origin = {0.0, 0.0, 0.0};
  // Present for laser operations — drives M3 vs M4 and S scaling.
  std::optional<LaserCutParameters> laser;
  // The loaded post definition (defaulted; filled by post_process).
  PostDefinition definition;
};

// Looks the definition up by name (user file first, built-in fallback)
// and renders the toolpath through its templates.  `include_footer`
// gates the footer (program-end) lines — multi-op exports render the
// footer only on the LAST op.  Returns an empty list when the post
// type is unknown.
std::vector<std::string> post_process(const std::string& type,
                                      const PostContext& context,
                                      bool include_footer = true);

struct PostListEntry {
  std::string name;
  std::string path;  // user-editable file; empty for built-ins only
};

// Lists every available post processor (built-ins + files in the
// posts directory), seeding the directory with the built-ins first.
std::vector<PostListEntry> list_post_processors();

// Copies a post-definition JSON into the user's posts directory after
// validating it.  Returns the imported name; sets `error` on failure.
std::string import_post_processor(const std::string& source_path,
                                  std::string& error);

}  // namespace polysmith::core
