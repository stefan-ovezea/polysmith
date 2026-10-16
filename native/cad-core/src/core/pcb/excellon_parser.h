// Excellon drill parser (KiCad plot output as the target dialect).
//
// Part of the PCB / Gerber workspace (wiki/PCB-Workspace.md, P1c).  Same
// contract as the Gerber parser: text in, millimetres out, diagnostics as
// data, no logging, no exceptions escaping, bounded for untrusted input.
//
// Supported: `M48` header with `FMAT,2`, `METRIC`/`INCH` (with `,LZ`/`,TZ`),
// `TnC<diameter>` tool definitions, KiCad's `; #@! TF.` / `TA.` attribute
// comments (plated vs non-plated, per-tool function), `G90`/`G91`, `G05`,
// `T<n>` selection, decimal **and** fixed-point coordinates, `R<n>[X..][Y..]`
// repeats, `M71`/`M72` body unit switches and `M30`.
//
// Explicitly not supported (recorded and skipped, never a hard failure):
// slots (`G85`), route mode (`M15`/`M16`, `G00`/`G01`) and any tool that is
// used before it is defined.

#pragma once

#include <map>
#include <string>
#include <vector>

#include "core/pcb/gerber_parser.h"

namespace polysmith::core::pcb {

/// One `TnC<diameter>` definition.  KiCad precedes each with a
/// `; #@! TA.AperFunction,…` comment; `function` keeps that value verbatim
/// (e.g. "Plated,PTH,ViaDrill"), because the useful part differs per
// exporter.
struct DrillTool {
  int number = 0;             // T<n>
  double diameter_mm = 0.0;
  std::string function;       // raw TA.AperFunction value, may be empty
  int line = 0;
};

struct DrillHit {
  double x = 0.0;  // millimetres
  double y = 0.0;
  int tool = 0;    // T<n> in effect
  int line = 0;
};

struct DrillFile {
  /// False only when the text held no drill program at all (empty, binary
  /// junk).  An NPTH file with a header and zero holes is *valid*.
  bool ok = true;
  PcbUnit unit = PcbUnit::Millimeter;
  bool plated = false;
  /// True when the file said so (TF.FileFunction); a bare drill file often
  /// does not, and the caller then decides from the filename.
  bool plated_known = false;
  std::string file_function;  // e.g. "Plated,1,4,PTH"
  std::map<int, DrillTool> tools;
  std::vector<DrillHit> hits;
  std::vector<PcbDiagnostic> diagnostics;
  std::vector<std::string> unsupported;
  PcbBounds bounds;
  /// Holes whose tool was never defined (counted, not silently dropped).
  size_t hits_without_tool = 0;
};

/// Parses one Excellon drill file.  Never throws; problems land in
/// `diagnostics` / `unsupported`.
DrillFile parse_excellon(const std::string& text);

}  // namespace polysmith::core::pcb
