// Gerber command stream → closed rings (the P1b polygonizer).
//
// Part of the PCB / Gerber workspace (wiki/PCB-Workspace.md §4).  Turns
// the parser's operations into closed polylines in millimetres, tagged
// with the polarity in effect.  It deliberately does **not** run set
// booleans: cutting treats every ring as a closed loop regardless of
// polarity, and fill/side decisions are derived downstream from ring
// orientation plus nesting (cam2d::loop_contains).  True booleans are
// only needed for the dense-copper trim case, and that decision is
// deferred until a real board demands it — see §4.
//
// Arcs stay as sampled polylines here (rings are polylines by
// definition); the *offsetting* later re-uses cam2d, which preserves
// true arcs where the source has them.

#pragma once

#include <vector>

#include "core/cam/cam2d.h"
#include "core/pcb/gerber_parser.h"

namespace polysmith::core::pcb {

/// One closed ring, in millimetres.  Orientation is inherited from the
/// source — callers should use cam2d::xy_signed_area rather than assume a
/// walk direction.  The closing point is NOT repeated.
struct PcbRing {
  std::vector<cam2d::XY> points;
  bool dark = true;          // LPD/LPC in effect when it was emitted
  int aperture_code = 0;     // 0 for regions and macro parts
  int line = 0;              // source line, for diagnostics
  bool from_region = false;  // G36/G37 contour rather than a stroke/flash
  /// Macro parts only: the macro primitive code that produced the ring
  /// (1 circle, 4 outline, 5 polygon, 20 vector line, 21 rect, 22 obround).
  int macro_primitive = 0;
};

struct PcbLayerGeometry {
  std::vector<PcbRing> rings;
  std::vector<PcbDiagnostic> diagnostics;
  GerberBounds bounds;
  /// Operations that produced nothing (unsupported aperture on a stroke,
  /// macro that could not be resolved, degenerate ring).
  size_t skipped_operations = 0;
};

/// Tessellation tolerance for arcs and circles: the maximum sagitta, in
/// millimetres.  10 µm is well below any PCB process tolerance and keeps
/// the vertex count for a 0.1 mm laser spot sane.
constexpr double kArcToleranceMm = 0.01;

/// Polygonizes a parsed file.  Never throws; problems land in
/// `diagnostics` and the affected operation is counted in
/// `skipped_operations`.
PcbLayerGeometry polygonize_gerber(const GerberFile& file,
                                   double tolerance_mm = kArcToleranceMm);

}  // namespace polysmith::core::pcb
