// Regression: two intersecting arcs enclosed by two lines (the
// geometry of res/part.json, user-reported 2026-08) extruded with part
// of an arc forming a thin wall.
//
// The sketch holds a bottom line (line-7), a left vertical line
// (line-8), and two clockwise arcs that cross each other:
//
//            arc-2
//      +-------\  ..------..
//      |        \/          \        <- lens between the two arcs
//      |        /\______     \
//      +-------/        \     \  arc-1
//      | line-7          `--.  /
//      |                  `''     <- below line-7 is the unbounded
//      +-----------------------+      exterior, not a region
//
// The arrangement yields three bounded regions: the lobe between the
// arcs and line-7 (area ~5105), the thin lens between the two arcs
// (area ~2479), and the big U enclosing everything (area ~6789).
// The lobe is SOLID — it has no hole.
//
// The reported bug: the face walk assigned the arrangement's exterior
// cycle (area ~14372 — larger than the lobe itself) as the lobe's
// inner loop, because the exterior's probe point lies exactly on the
// lobe's own boundary and the ray-cast rounds it onto the inside. The
// bogus hole cut the extruded lobe face down to a thin sliver — the
// "thin wall" in the render.
//
// The test asserts the full region set and that the lobe carries no
// inner loop, so the walk defect cannot slip through a partial
// assertion again.

#include <algorithm>
#include <cmath>
#include <iostream>
#include <string>
#include <vector>

#include "core/document/feature.h"
#include "core/sketch/sketch_feature.h"
#include "core/sketch/sketch_profile.h"

namespace {

using polysmith::core::FeatureEntry;
using polysmith::core::SketchProfilePoint;
using polysmith::core::SketchProfileRegion;
using polysmith::core::add_sketch_arc;
using polysmith::core::add_sketch_line;
using polysmith::core::build_sketch_profile_regions;
using polysmith::core::create_sketch_feature;

bool expect(bool condition, const char* message) {
  if (condition) {
    return true;
  }
  std::cerr << message << "\n";
  return false;
}

double polygon_area(const std::vector<SketchProfilePoint>& points) {
  double area = 0.0;
  for (size_t i = 0; i < points.size(); ++i) {
    const auto& p = points[i];
    const auto& q = points[(i + 1) % points.size()];
    area += p.x * q.y - q.x * p.y;
  }
  return std::abs(area) / 2.0;
}

const SketchProfileRegion* find_region(
    const FeatureEntry& feature,
    const std::vector<std::string>& entity_ids) {
  const auto& profiles = feature.sketch_parameters->profiles;
  for (const auto& profile : profiles) {
    std::vector<std::string> sorted_ids = profile.line_ids;
    std::sort(sorted_ids.begin(), sorted_ids.end());
    std::vector<std::string> want = entity_ids;
    std::sort(want.begin(), want.end());
    if (sorted_ids == want) {
      return &profile;
    }
  }
  return nullptr;
}

bool test_intersecting_arcs_regions_and_lobe_hole() {
  FeatureEntry feature = create_sketch_feature(2, "ref-plane-xy");

  // Exact geometry from res/part.json. Line ids: line-1 == line-7
  // (bottom), line-2 == line-8 (left vertical).
  add_sketch_line(feature, /*line_index=*/1, -200.0, -100.0, -50.0, -100.0);
  add_sketch_line(feature, /*line_index=*/2,
                  -236.61996783866272, -16.848632124230235,
                  -236.61996783866272, -106.3158848458636);
  // Both arcs are clockwise (ccw=false) in the saved part.
  add_sketch_arc(feature,
                 /*arc_index=*/1,
                 /*start_point_index=*/100,
                 /*end_point_index=*/101,
                 /*start_x=*/-200.0,
                 /*start_y=*/-100.0,
                 /*end_x=*/-50.0,
                 /*end_y=*/-100.0,
                 /*center_x=*/-125.0,
                 /*center_y=*/-109.08058027039424,
                 /*radius=*/75.54771299018304,
                 /*ccw=*/false);
  add_sketch_arc(feature,
                 /*arc_index=*/2,
                 /*start_point_index=*/102,
                 /*end_point_index=*/103,
                 /*start_x=*/-236.61996783866272,
                 /*start_y=*/-16.848632124230235,
                 /*end_x=*/-236.61996783866272,
                 /*end_y=*/-106.3158848458636,
                 /*center_x=*/-199.61182791898423,
                 /*center_y=*/-61.58225848504692,
                 /*radius=*/58.05772771736467,
                 /*ccw=*/false);

  feature.sketch_parameters->profiles =
      build_sketch_profile_regions(feature.sketch_parameters.value());

  // ── Full region set: lens, lobe, big U — none may be missing ────
  const size_t region_count = feature.sketch_parameters->profiles.size();
  if (!expect(region_count == 3,
              "expected exactly the lens, lobe and big-U regions")) {
    std::cerr << "  regions detected:\n";
    for (const auto& profile : feature.sketch_parameters->profiles) {
      std::cerr << "    [";
      for (const auto& id : profile.line_ids) {
        std::cerr << id << " ";
      }
      std::cerr << "] points=" << profile.points.size()
                << " holes=" << profile.inner_loops.size() << "\n";
      for (const auto& e : profile.boundary_edges) {
        std::cerr << "      " << e.entity_id << " p " << e.param_start
                  << " -> " << e.param_end << " ccw=" << e.ccw << "\n";
      }
    }
    return false;
  }
  const auto* big_u =
      find_region(feature, {"arc-1", "arc-2", "line-1", "line-2"});
  if (!expect(big_u != nullptr, "big-U region present")) {
    return false;
  }
  // The lens and the lobe share the same entity-id set; pick them by
  // area (lens ~2479, lobe ~5105).
  const SketchProfileRegion* lens = nullptr;
  const SketchProfileRegion* lobe = nullptr;
  for (const auto& profile : feature.sketch_parameters->profiles) {
    std::vector<std::string> sorted_ids = profile.line_ids;
    std::sort(sorted_ids.begin(), sorted_ids.end());
    if (sorted_ids != std::vector<std::string>({"arc-1", "arc-2", "line-1"})) {
      continue;
    }
    const double area = polygon_area(profile.points);
    if (area < 4000.0) {
      lens = &profile;
    } else {
      lobe = &profile;
    }
  }
  if (!expect(lens != nullptr && lobe != nullptr,
              "lens and lobe regions both present")) {
    return false;
  }

  // ── The lobe is solid — the arrangement exterior must not become
  // its hole. Before the fix the walk assigned the exterior cycle
  // (area ~14372, larger than the lobe itself) as the lobe's inner
  // loop, which cut the extruded face down to a thin sliver — the
  // reported "thin wall".
  if (!expect(lobe->inner_loops.empty(),
              "lobe carries no inner loop (the exterior is not its hole)")) {
    std::cerr << "  all regions:\n";
    for (const auto& profile : feature.sketch_parameters->profiles) {
      std::cerr << "    [";
      for (const auto& id : profile.line_ids) {
        std::cerr << id << " ";
      }
      std::cerr << "] area=" << polygon_area(profile.points)
                << " holes=" << profile.inner_loops.size() << "\n";
      for (const auto& loop : profile.inner_loops) {
        std::cerr << "      hole area=" << polygon_area(loop) << "\n";
      }
      for (const auto& e : profile.boundary_edges) {
        std::cerr << "      " << e.entity_id << " p " << e.param_start
                  << " -> " << e.param_end << " ccw=" << e.ccw << "\n";
      }
    }
    return false;
  }
  return true;
}

}  // namespace

int main() {
  try {
    if (!test_intersecting_arcs_regions_and_lobe_hole()) {
      return 1;
    }
    std::cout << "intersecting_arcs_extrude_test passed\n";
    return 0;
  } catch (const std::exception& e) {
    std::cerr << "EXCEPTION: " << e.what() << std::endl;
    return 1;
  }
}
