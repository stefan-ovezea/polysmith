#pragma once

// ── Laser cut ordering ───────────────────────────────────────────
//
// Orders the planned loops into a cutting sequence.  The nesting
// (depth/parent links) is computed by the generator; this module only
// sequences.  Ordering strategies:
//
//   "inner_first"      — deepest nesting first (holes and enclosed
//                        parts are released before their containers),
//                        ties by area ascending, parent-child
//                        adjacency preferred.
//   "nearest_neighbor" — same depth discipline, but each depth bucket
//                        is walked with a greedy nearest-neighbor
//                        travel minimization.
//   "by_area"          — legacy: regions by outer area ascending,
//                        holes before their outer.

#include <string>
#include <vector>

#include "core/cam/laser/laser_generate.h"

namespace polysmith::core::laser {

void order_laser_loops(std::vector<PlannedLoop>& loops,
                       const std::string& cut_order);

}  // namespace polysmith::core::laser
