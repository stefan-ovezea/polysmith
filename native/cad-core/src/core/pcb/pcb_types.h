#pragma once

#include <string>
#include <vector>

namespace polysmith::core {

/// Layer roles the PCB workspace understands.  A role is the *semantic*
/// identity of a fabrication layer; it is resolved from the file's own
/// attributes (or its name as a fallback), never guessed from its contents.
enum class PcbRole {
  Unknown,
  CopperTop,
  CopperInner,
  CopperBottom,
  SolderMaskTop,
  SolderMaskBottom,
  PasteTop,
  PasteBottom,
  SilkscreenTop,
  SilkscreenBottom,
  Profile,
  DrillPlated,
  DrillNonPlated,
};

/// Stable id used in JSON, the IPC payload and the UI (`copper_top`, …).
const char* pcb_role_id(PcbRole role);
/// Parses the id back; anything unrecognised yields PcbRole::Unknown.
PcbRole parse_pcb_role(const std::string& id);
bool pcb_role_is_drill(PcbRole role);
/// True for roles that live on the bottom of the board and therefore cut
/// mirrored.
bool pcb_role_is_bottom(PcbRole role);
/// Human-readable label for the layer table (English; the UI translates its
/// own copy — this is the log/fallback text).
const char* pcb_role_label(PcbRole role);

enum class PcbLayerFormat { Gerber, Excellon };

/// Everything the document stores about an imported fabrication layer.
///
/// Deliberately NOT the geometry.  A copper layer is ~2k rings and tens of
/// thousands of points; embedding that in every save (the STEP-import
/// precedent, which snapshots the translated shape) would grow part files by
/// megabytes of data that can be re-derived.  So the feature keeps the source
/// path plus a summary, and the geometry is parsed on demand and held in a
/// core-side cache.  The trade-off: a moved or deleted Gerber file marks the
/// feature `dependency_broken` with a warning (never a crash) where a STEP
/// import would still open.  If self-contained saves are wanted later, the
/// parsed rings can be embedded as a sidecar without changing this contract.
struct PcbLayerParameters {
  std::string file_path;
  PcbLayerFormat format = PcbLayerFormat::Gerber;
  PcbRole role = PcbRole::Unknown;
  /// What the FILE declares ("Positive"/"Negative"), which is not the role: a
  /// solder-mask Gerber is negative because it describes the openings.  Kept
  /// verbatim for the UI and for the CAM side.
  std::string file_polarity;
  std::string file_function;  // raw TF.FileFunction value, for audit
  /// True when the role came from the file's attributes rather than its name —
  /// the layer table shows this as "confident" vs "guessed".
  bool role_from_attributes = false;
  bool mirror_x = false;
  // Summary, refreshed by every successful load and serialized so a part can
  // be inspected without the Gerber set at hand.
  int ring_count = 0;
  int hit_count = 0;   // Excellon holes
  int tool_count = 0;  // Excellon tools
  bool bounds_valid = false;
  double min_x = 0.0;
  double min_y = 0.0;
  double max_x = 0.0;
  double max_y = 0.0;
  std::vector<std::string> warnings;
};

}  // namespace polysmith::core
