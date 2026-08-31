#pragma once

// ── Laser test patterns ──────────────────────────────────────────
//
// LightBurn-style material test cards: a grid of cells sweeping power
// along columns (ascending left→right) and speed along rows
// (ascending top→bottom).  Engrave cells are filled squares; cut
// cells are through-cut square contours.  Cut the card, pick the best
// cell, and copy its power/speed into the real operation.
//
// Cells live directly in MACHINE coordinates (no sketch frame) — a
// test card is a machine calibration aid, not geometry.

#include "core/cam/cam_generator.h"

namespace polysmith::core::laser {

CamGenerateResult generate_laser_test_pattern_toolpath(
    const polysmith::core::CamGenerateContext& context);

void register_laser_test_pattern_generator();

}  // namespace polysmith::core::laser
