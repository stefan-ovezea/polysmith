#pragma once

// ── Laser tabs / bridges ─────────────────────────────────────────
//
// Tabs hold a cut part in the sheet: short spans of the contour where
// the laser switches off (or drops to a low micro-joint power) so the
// material stays connected.  Tabs are distributed evenly along the
// loop's arc length, split segment boundaries as needed, and never
// touch leads.
//
// The toolpath IR needs no new fields — per-move laser_on and
// power_percent already exist (the post emits the M5 / power change
// transitions).

#include <string>
#include <vector>

#include "core/cam/cam2d.h"
#include "core/cam/cam_types.h"
#include "core/cam/laser/laser_generate.h"

namespace polysmith::core::laser {

struct TabbedSegment {
  cam2d::OffsetSegment segment;
  bool in_tab = false;
};

// Splits the loop's segments at the tab boundaries and marks the tab
// spans.  Returns an empty list when tabs do not apply (disabled,
// engrave, holes without tabs_on_holes, or a loop too small to carry
// them — with a warning).
std::vector<TabbedSegment> apply_loop_tabs(
    const PlannedLoop& loop, const LaserCutParameters& params,
    std::vector<std::string>& warnings);

}  // namespace polysmith::core::laser
