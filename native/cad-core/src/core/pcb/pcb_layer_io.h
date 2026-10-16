// Loading a fabrication layer: file → parsed geometry + resolved role.
//
// The bridge between the pure format layer (gerber_parser / polygonize /
// excellon_parser) and the document feature.  It owns the "which format is
// this file" decision and folds format diagnostics into the small summary the
// document stores, so `DocumentManager` never touches parser types directly.

#pragma once

#include <string>
#include <vector>

#include "core/pcb/excellon_parser.h"
#include "core/pcb/gerber_polygonize.h"
#include "core/pcb/pcb_types.h"

namespace polysmith::core::pcb {

struct PcbLayerLoad {
  bool ok = false;
  /// Set when `ok` is false — user-facing, already prefixed with the file.
  std::string error;
  PcbLayerFormat format = PcbLayerFormat::Gerber;
  PcbRole role = PcbRole::Unknown;
  bool role_from_attributes = false;
  std::string role_note;
  std::string file_function;
  std::string file_polarity;
  /// Gerber layers: the polygonized rings (empty for drill files).
  PcbLayerGeometry geometry;
  /// Drill files: tools and hits (empty for Gerber layers).
  DrillFile drills;
  /// Parser diagnostics, rendered as short strings for the feature summary.
  std::vector<std::string> warnings;
  /// Counts for the document summary.
  int ring_count = 0;
  int hit_count = 0;
  int tool_count = 0;
  PcbBounds bounds;
};

/// Picks the format from the file's content (an Excellon program starts with
/// `M48` or `M71`/`M72`; everything else is treated as Gerber) — extension
/// alone is not enough, because exporters write `.txt` and `.drl` alike.
PcbLayerFormat detect_pcb_layer_format(const std::string& text, const std::string& file_name);

/// Parses layer text already in memory.  `role_override` is the user's
/// explicit choice from the layer table; when empty the role is resolved from
/// the file's attributes and, failing that, its name.
PcbLayerLoad parse_pcb_layer_text(const std::string& text, const std::string& file_name,
                                  const std::string& file_path = std::string(),
                                  PcbRole role_override = PcbRole::Unknown);

/// Reads `file_path` and parses it.  A read failure yields `ok = false` with
/// `error` set — never an exception, so the document layer can report it.
PcbLayerLoad load_pcb_layer(const std::string& file_path,
                            PcbRole role_override = PcbRole::Unknown);

}  // namespace polysmith::core::pcb
