// Gerber command stream → closed rings (the P1b polygonizer).
//
// Part of the PCB / Gerber workspace (wiki/PCB-Workspace.md §4).  Turns
// the parser's operations into closed polylines in millimetres, tagged
// with the polarity in effect.  Since §4.1 the drawn primitives are also
// assembled into non-overlapping cuttable regions (`pcb_region_assembly`),
// and the stroke CENTRE-LINES are kept, because a layer drawn with a
// hairline aperture (a board profile, silkscreen) means "cut along this
// line", not "cut around this 0.1 mm ribbon" — see PcbLayerGeometry.
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

/// One drawn STROKE's centre-line, in millimetres: where the aperture was
/// dragged, not the area it swept.  This is what a centre-line cut follows.
struct PcbStrokePath {
  std::vector<cam2d::XY> points;  // start, sampled arc, end
  int aperture_code = 0;
  double width_mm = 0.0;  // the aperture's width, where it has one
  int line = 0;
  bool dark = true;
};

/// Adjacent stroke centre-lines joined end to end.
///
/// A profile is drawn as one stroke per side, and silkscreen text is drawn as
/// many short strokes; in both cases the *meaning* is the continuous line, so
/// the chains — not the individual strokes — are what a cut should follow.
struct PcbChain {
  std::vector<cam2d::XY> points;
  /// True when the last point meets the first: a closed contour (a board
  /// outline).  False for line art that simply ends (a text stroke, a logo
  /// flourish), which needs an OPEN cut path — see `PlannedLoop::is_open`.
  bool closed = false;
  int aperture_code = 0;
  double width_mm = 0.0;
  int line = 0;
  bool dark = true;
};

struct PcbLayerGeometry {
  /// The contours to CUT: non-overlapping region boundaries with polarity
  /// already applied, so a macro pad's nine overlapping primitives arrive as
  /// the single rounded rectangle a machine should trace (`pcb_region_assembly`).
  std::vector<PcbRing> rings;
  /// The primitives AS DRAWN, before assembly — one per flash, stroke, region
  /// and macro part.  Kept for diagnostics and for tests that pin macro
  /// expansion; `aperture_code`, `from_region` and `macro_primitive` are only
  /// meaningful here, since an assembled boundary can be a union of several.
  std::vector<PcbRing> drawn_rings;
  /// Every drawn stroke centre-line, one per stroke run (`PcbStrokePath`).
  std::vector<PcbStrokePath> stroke_paths;
  /// Those centre-lines CHAINED end to end (`PcbChain`): a profile layer's
  /// outline is one chain, and silkscreen text is many.  Both closed contours
  /// and open line art live here, distinguished by `PcbChain::closed`, because
  /// both are cut — one as a loop, one as an open path.
  std::vector<PcbChain> stroke_chains;
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
