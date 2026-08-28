#pragma once

#include <string>

namespace polysmith::core {

struct DocumentState;

// ── CAM G-code export ─────────────────────────────────────────────
//
// Serializes every enabled operation to G-code through the
// post-processor registry and writes the file.  Toolpaths are
// generated on demand when the runtime cache is stale (the user can
// export without pressing Generate first).  Operations whose
// generation fails are skipped with a comment line in the file and a
// structured warning — export never throws for a single bad op.

struct CamExportResult {
  std::string file_path;
  std::string format = "gcode";
  int exported_feature_count = 0;
};

CamExportResult export_cam_gcode(const DocumentState& document,
                                 const std::string& file_path);

}  // namespace polysmith::core
