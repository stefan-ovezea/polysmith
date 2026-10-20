// Layer-role resolution: which fabrication layer is this file?
//
// Part of the PCB / Gerber workspace (wiki/PCB-Workspace.md §3).  Pure text
// matching, no I/O, so it is unit-testable and the document layer only has to
// decide *when* to call it.
//
// The tiers that feed it disagree on vocabulary — KiCad writes
// `TF.FileFunction,Paste,Bot` in the Gerber header and `"SolderPaste,Bot"` in
// the job file for the same layer, `Legend` vs `Silkscreen`, `Profile` vs
// `Edge_Cuts` — so matching is by normalised keyword, never by equality, and
// the caller always keeps the raw value for audit.

#pragma once

#include <string>

#include "core/pcb/pcb_types.h"

namespace polysmith::core::pcb {

struct PcbRoleResolution {
  PcbRole role = PcbRole::Unknown;
  /// True when the file's own attributes named the role; false when the
  /// filename fallback decided it (or nothing matched).
  bool from_attributes = false;
  /// Short human-readable reason for the log ("TF.FileFunction,Copper,L1,Top").
  std::string note;
};

/// Resolves a role from the file's declared function (`TF.FileFunction`, empty
/// when the file has none) and, failing that, from its filename.  `pasted`
/// covers both vocabularies ("Paste" and "SolderPaste").
PcbRoleResolution resolve_pcb_role(const std::string& file_function,
                                   const std::string& file_name);

}  // namespace polysmith::core::pcb
