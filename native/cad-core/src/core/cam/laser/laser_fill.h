#pragma once

// ── Engrave fill / hatch ─────────────────────────────────────────
//
// Scan-line hatch for the engrave mode: each region's loops are
// filled with parallel feed lines at line_spacing_mm, clipped to the
// region (holes excluded via even-odd nesting), alternating direction
// per line (bidirectional) so the laser stays on across the fill.
//
// Fill geometry is computed in loop coordinates (sketch-local or
// world-XY) and emitted through the same frame mapping as contours.

#include <vector>

#include "core/cam/cam2d.h"
#include "core/cam/cam_types.h"
#include "core/cam/laser/laser_generate.h"

namespace polysmith::core::laser {

// One hatch pass (a straight span).
struct FillLine {
  cam2d::XY start;
  cam2d::XY end;
};

// Computes the hatch for ONE region (its loops: the outer plus its
// holes).  Returns one span list per scan line, in scan order; each
// span runs left→right (the emitter reverses odd lines when
// bidirectional).
std::vector<std::vector<FillLine>> hatch_region(
    const std::vector<PlannedLoop>& loops,
    const LaserCutParameters& params);

}  // namespace polysmith::core::laser
