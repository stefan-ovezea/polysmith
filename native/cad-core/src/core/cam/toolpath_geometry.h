#pragma once

#include <array>
#include <vector>

#include "core/cam/toolpath.h"

namespace polysmith::core {

// ── Toolpath geometry helpers ─────────────────────────────────────
//
// Shared by the viewport emitter (which needs linearized polylines)
// and the post-processors (which need lengths, bounds, and time
// estimates).  Arc math follows the IR convention: i/j is the center
// offset from the arc START point, sweep direction from the move kind.

// Finalizes bounds and the derived stats (length, time, move counts,
// linearized point count) after a generator fills `moves`.
void finalize_toolpath(Toolpath& toolpath);

// Chord-linearizes an arc move between `from` (the previous point) and
// `arc` (the arc move) into `out`, exclusive of the start point and
// inclusive of the endpoint.  `chord_tolerance_mm` bounds the sagitta.
void linearize_arc_move(const ToolpathMove& from, const ToolpathMove& arc,
                        double chord_tolerance_mm,
                        std::vector<std::array<double, 3>>& out);

// Length of a single move from the given start point.
double move_length(const ToolpathMove& from, const ToolpathMove& to);

}  // namespace polysmith::core
