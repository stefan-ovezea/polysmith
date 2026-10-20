// Region assembly: drawn primitives → non-overlapping cut contours.
//
// Part of the PCB / Gerber workspace (wiki/PCB-Workspace.md §4).  A Gerber
// layer as *drawn* is not a set of independent outlines:
//
//   * an aperture macro expands to overlapping primitives — KiCad's RoundRect
//     pad is nine (one outline, four corner circles, four edge lines), so
//     cutting them as drawn traces the same pad nine times, including lines
//     that lie INSIDE the union;
//   * `%LPC` erases what earlier dark draws added, in file order.
//
// Union (dark) / difference (clear) with robust integer arithmetic is what
// turns those primitives into the regions a machine can actually cut.  The
// drawn primitives are kept in `PcbLayerGeometry::drawn_rings` for diagnostics
// (and for the tests that pin macro expansion), while `rings` carries the
// assembled result.

#pragma once

#include <vector>

#include "core/pcb/gerber_polygonize.h"

namespace polysmith::core::pcb {

/// How far rounding may move a coordinate.  Clipper2 works in integers, so
/// millimetres are scaled by 10^this before clipping and scaled back after:
/// 1e-6 mm resolution, three orders finer than the 1 µm plotting grid.
constexpr double kBooleanScale = 1.0e6;

/// Assembles drawn rings into non-overlapping region boundaries, applying
/// polarity in draw order: a dark ring unions into the result, a clear ring
/// subtracts from it.
///
/// A region with holes comes back as several rings — the outer boundary plus
/// one per hole, distinguished by signed area (outer positive, hole negative)
/// — because that is exactly what a cutter needs: every boundary is its own
/// closed contour.
///
/// Degenerate input (fewer than three points, zero area) is dropped.
std::vector<PcbRing> assemble_region_rings(const std::vector<PcbRing>& drawn);

}  // namespace polysmith::core::pcb
