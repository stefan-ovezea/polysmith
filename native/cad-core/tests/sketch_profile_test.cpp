#include <cstdlib>
#include <cmath>
#include <iostream>
#include <exception>

#include "core/feature.h"
#include "core/sketch_feature.h"
#include "core/sketch_profile.h"

namespace {

using polysmith::core::DetectedSketchProfiles;
using polysmith::core::FeatureEntry;
using polysmith::core::SketchFeatureParameters;
using polysmith::core::SketchLine;
using polysmith::core::add_sketch_arc;
using polysmith::core::add_sketch_circle;
using polysmith::core::add_sketch_fillet;
using polysmith::core::add_sketch_line;
using polysmith::core::add_sketch_rectangle;
using polysmith::core::create_sketch_feature;
using polysmith::core::delete_sketch_fillet;
using polysmith::core::detect_sketch_profiles;
using polysmith::core::set_sketch_midpoint_anchor;
using polysmith::core::set_sketch_point_fixed;
using polysmith::core::set_sketch_point_line_anchor;
using polysmith::core::update_sketch_dimension;
using polysmith::core::update_sketch_fillet_radius;
using polysmith::core::build_sketch_profile_regions;

bool expect(bool condition, const char* message) {
  if (condition) {
    return true;
  }

  std::cerr << message << "\n";
  return false;
}

const polysmith::core::SketchPoint* find_point(
    const FeatureEntry& feature,
    const char* point_id) {
  for (const auto& point : feature.sketch_parameters->points) {
    if (point.id == point_id) {
      return &point;
    }
  }

  return nullptr;
}

FeatureEntry make_sketch_with_shared_point_ids() {
  FeatureEntry feature{
      .id = "feature-1",
      .kind = "sketch",
      .name = "Sketch",
      .status = "healthy",
      .parameters_summary = "test",
      .box_parameters = std::nullopt,
      .cylinder_parameters = std::nullopt,
      .extrude_parameters = std::nullopt,
      .sketch_parameters =
          SketchFeatureParameters{
              .plane_id = "ref-plane-xy",
              .plane_frame = std::nullopt,
              .active_tool = "select",
              .lines =
                  {
                      SketchLine{
                          .id = "line-1",
                          .start_point_id = "point-a",
                          .end_point_id = "point-b",
                          .start_x = 0.0,
                          .start_y = 0.0,
                          .end_x = 40.0,
                          .end_y = 0.0,
                          .constraint = std::nullopt,
                      },
                      SketchLine{
                          .id = "line-2",
                          .start_point_id = "point-b",
                          .end_point_id = "point-c",
                          .start_x = 40.03,
                          .start_y = 0.0,
                          .end_x = 40.0,
                          .end_y = 20.0,
                          .constraint = std::nullopt,
                      },
                      SketchLine{
                          .id = "line-3",
                          .start_point_id = "point-c",
                          .end_point_id = "point-d",
                          .start_x = 40.0,
                          .start_y = 20.0,
                          .end_x = 0.0,
                          .end_y = 20.0,
                          .constraint = std::nullopt,
                      },
                      SketchLine{
                          .id = "line-4",
                          .start_point_id = "point-d",
                          .end_point_id = "point-a",
                          .start_x = 0.0,
                          .start_y = 20.0,
                          .end_x = 0.0,
                          .end_y = 0.0,
                          .constraint = std::nullopt,
                      },
                  },
              .circles = {},
              .points = {},
              .dimensions = {},
              .line_relations = {},
              .profiles = {},
          },
  };

  feature.sketch_parameters->profiles =
      build_sketch_profile_regions(feature.sketch_parameters.value());
  return feature;
}

bool test_detects_polygon_from_shared_point_topology() {
  const DetectedSketchProfiles profiles =
      detect_sketch_profiles(make_sketch_with_shared_point_ids());

  return expect(profiles.polygons.size() == 1,
                "expected one polygon profile from shared point ids") &&
         expect(profiles.polygons.front().points.size() == 4,
                "expected the polygon profile to keep four corners");
}

bool test_stores_explicit_points_and_profiles_for_rectangles() {
  FeatureEntry feature = create_sketch_feature(2, "ref-plane-xy");
  add_sketch_line(feature, 1, 0.0, 0.0, 40.0, 0.0);
  add_sketch_line(feature, 2, 40.0, 0.0, 40.0, 20.0);
  add_sketch_line(feature, 3, 40.0, 20.0, 0.0, 20.0);
  add_sketch_line(feature, 4, 0.0, 20.0, 0.0, 0.0);

  return expect(feature.sketch_parameters->points.size() == 4,
                "expected rectangle sketch to store four explicit points") &&
         expect(feature.sketch_parameters->profiles.size() == 1,
                "expected rectangle sketch to store one explicit profile") &&
         expect(feature.sketch_parameters->profiles.front().kind == "polygon",
                "expected stored rectangle profile to be polygonal");
}

bool test_redimensioning_preserves_stored_profile_topology() {
  FeatureEntry feature = create_sketch_feature(3, "ref-plane-xy");
  int next_line_index = 1;
  add_sketch_rectangle(feature, next_line_index, 0.0, 0.0, 40.0, 20.0);

  update_sketch_dimension(feature, "dim-line-line-1", 60.0);

  return expect(feature.sketch_parameters->points.size() == 4,
                "expected redimensioned rectangle to keep four explicit points") &&
         expect(feature.sketch_parameters->profiles.size() == 1,
                "expected redimensioned rectangle to keep one explicit profile") &&
         expect(feature.sketch_parameters->profiles.front().points.size() == 4,
                "expected redimensioned rectangle profile to keep four corners");
}

bool test_circle_creates_center_point_and_profile() {
  FeatureEntry feature = create_sketch_feature(4, "ref-plane-xy");
  add_sketch_circle(feature, 1, 12.0, 8.0, 5.0);

  // Each circle creates 1 center point + 4 quadrant points = 5 total.
  bool points_ok =
      expect(feature.sketch_parameters->points.size() == 5,
             "expected circle sketch to store 5 points (1 center + 4 quadrants)");
  // The center point is the first one pushed by rebuild_sketch_points.
  bool center_ok =
      expect(!feature.sketch_parameters->points.empty() &&
                 feature.sketch_parameters->points[0].kind == "center",
             "expected first stored circle point to be a center point");

  return points_ok && center_ok &&
         expect(feature.sketch_parameters->profiles.size() == 1,
                "expected circle sketch to store one profile") &&
         expect(feature.sketch_parameters->profiles.front().kind == "circle",
                "expected stored circle profile to be circular");
}

bool test_fixed_points_persist_through_rebuilds() {
  FeatureEntry feature = create_sketch_feature(5, "ref-plane-xy");
  int next_line_index = 1;
  add_sketch_rectangle(feature, next_line_index, 0.0, 0.0, 40.0, 20.0);

  set_sketch_point_fixed(feature, "point-line-1-start", true);
  update_sketch_dimension(feature, "dim-line-line-1", 60.0);

  const auto* fixed_point = find_point(feature, "point-line-1-start");
  return expect(fixed_point != nullptr,
                "expected fixed rectangle point to survive rebuild") &&
         expect(fixed_point->is_fixed,
                "expected fixed rectangle point to stay fixed after rebuild");
}

bool test_fixed_endpoint_stays_put_when_redimensioning() {
  FeatureEntry feature = create_sketch_feature(6, "ref-plane-xy");
  int next_line_index = 1;
  add_sketch_rectangle(feature, next_line_index, 0.0, 0.0, 40.0, 20.0);

  set_sketch_point_fixed(feature, "point-line-1-end", true);
  update_sketch_dimension(feature, "dim-line-line-1", 60.0);

  const auto* fixed_point = find_point(feature, "point-line-1-end");
  return expect(fixed_point != nullptr,
                "expected fixed endpoint to remain addressable") &&
         expect(std::abs(fixed_point->x - 40.0) < 1e-6,
                "expected fixed endpoint x coordinate to stay unchanged") &&
         expect(std::abs(fixed_point->y - 0.0) < 1e-6,
                "expected fixed endpoint y coordinate to stay unchanged") &&
         expect(std::abs(feature.sketch_parameters->lines.front().start_x + 20.0) <
                    1e-6,
                "expected opposite endpoint to move when driving from fixed end");
}

bool test_midpoint_anchor_follows_host_length_change() {
  FeatureEntry feature = create_sketch_feature(8, "ref-plane-xy");
  int next_line_index = 1;
  add_sketch_rectangle(feature, next_line_index, 0.0, 0.0, 40.0, 20.0);
  // Driven side. line-1 is the y=start_y horizontal in
  // add_sketch_rectangle ordering.
  add_sketch_line(feature, next_line_index++, 20.0, 0.0, 20.0, 20.0);
  set_sketch_midpoint_anchor(feature, "point-line-5-start", "line-1");

  update_sketch_dimension(feature, "dim-line-line-1", 60.0);

  const auto& line5 = feature.sketch_parameters->lines.back();
  return expect(std::abs(line5.start_x - 30.0) < 1e-6,
                "midpoint anchor (driven host): start_x") &&
         expect(std::abs(line5.start_y - 0.0) < 1e-6,
                "midpoint anchor (driven host): start_y");
}

bool test_midpoint_anchor_both_ends_follow_perpendicular_resize() {
  // User-reported repro (screenshot): rectangle, vertical line from
  // bottom-midpoint to top-midpoint, then shrink one of the
  // *vertical* sides — the line that's perpendicular to the user's
  // line. The user's line should shrink to fit the new rectangle
  // height; before the fix it kept its original length and poked
  // outside the rectangle.
  FeatureEntry feature = create_sketch_feature(11, "ref-plane-xy");
  int next_line_index = 1;
  add_sketch_rectangle(feature, next_line_index, 0.0, 0.0, 40.0, 20.0);
  add_sketch_line(feature, next_line_index++, 20.0, 0.0, 20.0, 20.0);
  set_sketch_midpoint_anchor(feature, "point-line-5-start", "line-1");
  set_sketch_midpoint_anchor(feature, "point-line-5-end", "line-3");

  // line-2 is the right vertical side of the rectangle (length 20).
  // Drive it to length 10 — the rectangle's height halves.
  update_sketch_dimension(feature, "dim-line-line-2", 10.0);

  const auto& line5 = feature.sketch_parameters->lines.back();
  const double length = std::hypot(line5.end_x - line5.start_x,
                                   line5.end_y - line5.start_y);
  return expect(std::abs(length - 10.0) < 1e-6,
                "perpendicular resize: line-5 length should match rect height") &&
         expect(std::abs(line5.start_x - 20.0) < 1e-6 &&
                    std::abs(line5.end_x - 20.0) < 1e-6,
                "perpendicular resize: line-5 should stay vertical at x=20") &&
         expect(std::min(line5.start_y, line5.end_y) >= -1e-6 &&
                    std::max(line5.start_y, line5.end_y) <= 10.0 + 1e-6,
                "perpendicular resize: line-5 must lie inside [0, 10]");
}

bool test_midpoint_anchor_both_ends_follow_host_length_change() {
  // User-reported repro: rectangle, draw a vertical line from
  // bottom-midpoint to top-midpoint (so both endpoints carry a
  // midpoint anchor), then increase the rectangle's length.
  FeatureEntry feature = create_sketch_feature(10, "ref-plane-xy");
  int next_line_index = 1;
  add_sketch_rectangle(feature, next_line_index, 0.0, 0.0, 40.0, 20.0);
  add_sketch_line(feature, next_line_index++, 20.0, 0.0, 20.0, 20.0);
  set_sketch_midpoint_anchor(feature, "point-line-5-start", "line-1");
  set_sketch_midpoint_anchor(feature, "point-line-5-end", "line-3");

  update_sketch_dimension(feature, "dim-line-line-1", 60.0);

  const auto& line5 = feature.sketch_parameters->lines.back();
  return expect(std::abs(line5.start_x - 30.0) < 1e-6,
                "two anchors: start_x should track new bottom midpoint") &&
         expect(std::abs(line5.end_x - 30.0) < 1e-6,
                "two anchors: end_x should track new top midpoint");
}

bool test_midpoint_anchor_follows_indirect_host_length_change() {
  // Anchor to the OPPOSITE side from the one being driven — that
  // side moves only via equal_length propagation, so this exercises
  // the multi-pass solver path.
  FeatureEntry feature = create_sketch_feature(9, "ref-plane-xy");
  int next_line_index = 1;
  add_sketch_rectangle(feature, next_line_index, 0.0, 0.0, 40.0, 20.0);
  // line-3 is the y=end_y horizontal (the "bottom" in code naming).
  add_sketch_line(feature, next_line_index++, 20.0, 20.0, 20.0, 0.0);
  set_sketch_midpoint_anchor(feature, "point-line-5-start", "line-3");

  update_sketch_dimension(feature, "dim-line-line-1", 60.0);

  const auto& line5 = feature.sketch_parameters->lines.back();
  return expect(std::abs(line5.start_x - 30.0) < 1e-6,
                "midpoint anchor (indirect host): start_x") &&
         expect(std::abs(line5.start_y - 20.0) < 1e-6,
                "midpoint anchor (indirect host): start_y");
}

bool test_rejects_dimension_drive_when_both_endpoints_are_fixed() {
  FeatureEntry feature = create_sketch_feature(7, "ref-plane-xy");
  int next_line_index = 1;
  add_sketch_rectangle(feature, next_line_index, 0.0, 0.0, 40.0, 20.0);

  set_sketch_point_fixed(feature, "point-line-1-start", true);
  set_sketch_point_fixed(feature, "point-line-1-end", true);

  try {
    update_sketch_dimension(feature, "dim-line-line-1", 60.0);
  } catch (const std::exception&) {
    return true;
  }

  std::cerr << "expected dimension drive to fail when both endpoints are fixed\n";
  return false;
}

// Build a closed loop made of three lines plus one arc — a "stadium
// half" shape: two parallel lines plus a base line, closed by a
// semicircular arc on the right end. This exercises the generalized
// loop detector's ability to chain mixed line+arc edges through their
// shared endpoint points and to sample arc interiors when materializing
// the profile polyline.
//
//   (-20, 10) ─────────── (20, 10)
//                                 \
//                                   ) arc, ccw, center (20, 0), r=10
//                                 /
//   (-20,-10) ─────────── (20,-10)
//   (closing line on the left side)
bool test_detects_polygon_loop_with_line_and_arc_edges() {
  FeatureEntry feature = create_sketch_feature(2, "ref-plane-xy");

  // Three lines plus the arc share endpoint points "point-N" via the
  // unified counter; we'll drive add_sketch_arc directly with explicit
  // point indices. The lines we add via the public helper will
  // consume point-1..point-8 (two endpoints per line); we then add
  // an arc whose start/end re-use the existing endpoint ids of the
  // top-right and bottom-right line endpoints by passing matching
  // coordinates — the loop detector keys on point id when present and
  // fall back to coordinate match.

  // Top edge: (-20, 10) -> (20, 10)
  add_sketch_line(feature, /*line_index=*/1, -20.0, 10.0, 20.0, 10.0);
  // Bottom edge: (-20, -10) -> (20, -10)
  add_sketch_line(feature, /*line_index=*/2, -20.0, -10.0, 20.0, -10.0);
  // Left edge: (-20, -10) -> (-20, 10)
  add_sketch_line(feature, /*line_index=*/3, -20.0, -10.0, -20.0, 10.0);

  // Right semicircle arc from (20, 10) to (20, -10), bulging right
  // through (30, 0). Center is (20, 0), radius 10. ccw=false because
  // going from (20,10) to (20,-10) through (30,0) is clockwise in
  // sketch-plane coordinates.
  //
  // For the loop to chain via shared point ids, the arc's endpoint
  // points need ids that match the line endpoint ids. add_sketch_arc
  // assigns its own new "point-N" ids, so the chain falls back to
  // coordinate matching. The ProfileEdge graph quantizes coordinates
  // into a node key when no shared id is found, so two edges meeting
  // at the same coordinate still merge into one graph node. This
  // mirrors the legacy line-loop behaviour for sketches that don't
  // wire shared point ids.
  add_sketch_arc(feature,
                 /*arc_index=*/1,
                 /*start_point_index=*/100,
                 /*end_point_index=*/101,
                 /*start_x=*/20.0,
                 /*start_y=*/10.0,
                 /*end_x=*/20.0,
                 /*end_y=*/-10.0,
                 /*center_x=*/20.0,
                 /*center_y=*/0.0,
                 /*radius=*/10.0,
                 /*ccw=*/false);

  feature.sketch_parameters->profiles =
      build_sketch_profile_regions(feature.sketch_parameters.value());

  const DetectedSketchProfiles profiles = detect_sketch_profiles(feature);
  if (!expect(profiles.polygons.size() == 1,
              "expected one polygon profile from the line+arc loop")) {
    return false;
  }

  // The sample count is 4 corners (top-left, top-right, bottom-right,
  // bottom-left) plus 15 interior arc samples (kArcSampleSegments-1).
  // We don't assert the exact count to keep the test resilient to
  // future tuning of the sample density, but we do require the
  // polygon to have markedly more than 4 vertices to confirm the
  // arc was sampled.
  const auto& polygon = profiles.polygons.front();
  if (!expect(polygon.points.size() > 4,
              "expected polygon to include arc samples beyond the "
              "four corner vertices")) {
    return false;
  }

  // Sanity-check the bounding box: the loop should span x in
  // [-20, 30] (left edge to arc bulge) and y in [-10, 10]. We just
  // check the extremes are present in the sample list.
  double min_x = 0.0;
  double max_x = 0.0;
  double min_y = 0.0;
  double max_y = 0.0;
  for (size_t i = 0; i < polygon.points.size(); ++i) {
    const auto& point = polygon.points[i];
    if (i == 0) {
      min_x = max_x = point.x;
      min_y = max_y = point.y;
    } else {
      min_x = std::min(min_x, point.x);
      max_x = std::max(max_x, point.x);
      min_y = std::min(min_y, point.y);
      max_y = std::max(max_y, point.y);
    }
  }
  return expect(std::abs(min_x - (-20.0)) < 0.5,
                "expected polygon to reach x = -20") &&
         expect(std::abs(max_x - 30.0) < 0.5,
                "expected polygon's arc bulge to reach x = 30") &&
         expect(std::abs(min_y - (-10.0)) < 0.5,
                "expected polygon to reach y = -10") &&
         expect(std::abs(max_y - 10.0) < 0.5,
                "expected polygon to reach y = 10");
}

// Find the line in `feature` whose start or end matches the given
// 2D coordinate (within `tolerance`). Returns nullptr if none.
const polysmith::core::SketchLine* find_line_with_endpoint_at(
    const FeatureEntry& feature,
    double x,
    double y,
    double tolerance = 0.01) {
  for (const auto& line : feature.sketch_parameters->lines) {
    if ((std::abs(line.start_x - x) < tolerance &&
         std::abs(line.start_y - y) < tolerance) ||
        (std::abs(line.end_x - x) < tolerance &&
         std::abs(line.end_y - y) < tolerance)) {
      return &line;
    }
  }
  return nullptr;
}

// Adds a 40x20 rectangle whose top-left corner is the origin and
// returns the feature plus the corner point id at (40, 0). v1
// fillets pin themselves to a shared point id, so the test needs
// that id to call add_sketch_fillet.
struct RectWithCorner {
  FeatureEntry feature;
  std::string top_right_corner_id;
  std::string top_line_id;
  std::string right_line_id;
};

RectWithCorner make_rect_with_top_right_corner(int feature_index) {
  RectWithCorner result{};
  result.feature = create_sketch_feature(feature_index, "ref-plane-xy");
  int next_line_index = 1;
  add_sketch_rectangle(
      result.feature, next_line_index, 0.0, 0.0, 40.0, 20.0);

  // Locate the two lines that meet at (40, 0): the top edge runs
  // along y=0 from (0,0) to (40,0), and the right edge runs along
  // x=40 from (40, 0) to (40, 20). The order add_sketch_rectangle
  // emits is start→x, x→corner, corner→y, y→start (from inspecting
  // the builder) but we don't rely on that — we look up by coords.
  for (const auto& line : result.feature.sketch_parameters->lines) {
    const bool start_at_corner =
        std::abs(line.start_x - 40.0) < 0.01 &&
        std::abs(line.start_y - 0.0) < 0.01;
    const bool end_at_corner =
        std::abs(line.end_x - 40.0) < 0.01 &&
        std::abs(line.end_y - 0.0) < 0.01;
    if (!start_at_corner && !end_at_corner) {
      continue;
    }
    const std::string corner_id =
        start_at_corner ? line.start_point_id : line.end_point_id;
    if (result.top_right_corner_id.empty()) {
      result.top_right_corner_id = corner_id;
    }
    // Distinguish top vs right by inspecting the *other* endpoint:
    // top edge's other end is at (0, 0); right edge's other end is at
    // (40, 20).
    const double other_x = start_at_corner ? line.end_x : line.start_x;
    const double other_y = start_at_corner ? line.end_y : line.start_y;
    if (std::abs(other_x - 0.0) < 0.01 &&
        std::abs(other_y - 0.0) < 0.01) {
      result.top_line_id = line.id;
    } else if (std::abs(other_x - 40.0) < 0.01 &&
               std::abs(other_y - 20.0) < 0.01) {
      result.right_line_id = line.id;
    }
  }
  return result;
}

bool test_fillet_creates_arc_and_trims_lines() {
  RectWithCorner setup = make_rect_with_top_right_corner(20);
  if (setup.top_right_corner_id.empty() || setup.top_line_id.empty() ||
      setup.right_line_id.empty()) {
    return expect(false, "fillet test setup: failed to locate corner / lines");
  }

  add_sketch_fillet(setup.feature,
                    /*fillet_index=*/1,
                    /*trim_a_point_index=*/100,
                    /*trim_b_point_index=*/101,
                    /*arc_index=*/1,
                    setup.top_right_corner_id,
                    setup.top_line_id,
                    setup.right_line_id,
                    /*radius=*/5.0);

  const auto& params = setup.feature.sketch_parameters.value();

  if (!expect(params.fillets.size() == 1,
              "expected exactly one fillet record")) {
    return false;
  }
  if (!expect(params.arcs.size() == 1,
              "expected exactly one generated arc")) {
    return false;
  }
  const auto& arc = params.arcs.front();
  if (!expect(std::abs(arc.radius - 5.0) < 1e-6,
              "expected the fillet arc to honor the requested radius")) {
    return false;
  }

  // Top edge originally ended at (40, 0); trim distance = r/tan(45°)
  // = 5 for a 90° corner, so the trimmed end should be at (35, 0).
  const polysmith::core::SketchLine* trimmed_top =
      find_line_with_endpoint_at(setup.feature, 35.0, 0.0);
  if (!expect(trimmed_top != nullptr,
              "expected the top edge to be trimmed to (35, 0)")) {
    return false;
  }
  const polysmith::core::SketchLine* trimmed_right =
      find_line_with_endpoint_at(setup.feature, 40.0, 5.0);
  if (!expect(trimmed_right != nullptr,
              "expected the right edge to be trimmed to (40, 5)")) {
    return false;
  }

  // Arc center for a 90° corner with r=5 sits at (35, 5) (corner -
  // r along the bisector toward the rectangle interior).
  return expect(std::abs(arc.center_x - 35.0) < 1e-6 &&
                    std::abs(arc.center_y - 5.0) < 1e-6,
                "expected the fillet arc center at (35, 5)");
}

bool test_fillet_radius_update_re_derives_geometry() {
  RectWithCorner setup = make_rect_with_top_right_corner(21);
  add_sketch_fillet(setup.feature,
                    /*fillet_index=*/1,
                    /*trim_a_point_index=*/100,
                    /*trim_b_point_index=*/101,
                    /*arc_index=*/1,
                    setup.top_right_corner_id,
                    setup.top_line_id,
                    setup.right_line_id,
                    /*radius=*/5.0);

  const std::string fillet_id =
      setup.feature.sketch_parameters->fillets.front().id;
  update_sketch_fillet_radius(setup.feature, fillet_id, /*radius=*/8.0);

  const auto& arc = setup.feature.sketch_parameters->arcs.front();
  if (!expect(std::abs(arc.radius - 8.0) < 1e-6,
              "expected arc radius to update to the new fillet radius")) {
    return false;
  }
  // Trimmed top edge should now end at (40 - 8, 0) = (32, 0).
  return expect(find_line_with_endpoint_at(setup.feature, 32.0, 0.0) != nullptr,
                "expected the top edge to retrim to (32, 0) at r=8");
}

bool test_fillet_delete_restores_corner() {
  RectWithCorner setup = make_rect_with_top_right_corner(22);
  add_sketch_fillet(setup.feature,
                    /*fillet_index=*/1,
                    /*trim_a_point_index=*/100,
                    /*trim_b_point_index=*/101,
                    /*arc_index=*/1,
                    setup.top_right_corner_id,
                    setup.top_line_id,
                    setup.right_line_id,
                    /*radius=*/5.0);

  const std::string fillet_id =
      setup.feature.sketch_parameters->fillets.front().id;
  delete_sketch_fillet(setup.feature, fillet_id);

  const auto& params = setup.feature.sketch_parameters.value();
  if (!expect(params.fillets.empty(),
              "expected fillet record to be removed on delete")) {
    return false;
  }
  if (!expect(params.arcs.empty(),
              "expected the fillet's generated arc to be removed on delete")) {
    return false;
  }
  // After delete, top and right edges must once again share an
  // endpoint at the original corner (40, 0).
  return expect(
      find_line_with_endpoint_at(setup.feature, 40.0, 0.0) != nullptr,
      "expected the original (40, 0) corner to be restored after fillet delete");
}

bool test_fillet_rejects_oversized_radius() {
  RectWithCorner setup = make_rect_with_top_right_corner(23);
  // The right edge is 20 long; with a 90° corner the trim distance
  // equals the radius, so r=25 won't fit on the 20-tall edge.
  bool threw = false;
  try {
    add_sketch_fillet(setup.feature,
                      /*fillet_index=*/1,
                      /*trim_a_point_index=*/100,
                      /*trim_b_point_index=*/101,
                      /*arc_index=*/1,
                      setup.top_right_corner_id,
                      setup.top_line_id,
                      setup.right_line_id,
                      /*radius=*/25.0);
  } catch (const std::exception&) {
    threw = true;
  }
  return expect(threw,
                "expected oversized fillet radius to throw a structured error");
}

}  // namespace

int main() {
  if (!test_detects_polygon_from_shared_point_topology()) {
    return EXIT_FAILURE;
  }
  if (!test_stores_explicit_points_and_profiles_for_rectangles()) {
    return EXIT_FAILURE;
  }
  if (!test_redimensioning_preserves_stored_profile_topology()) {
    return EXIT_FAILURE;
  }
  if (!test_circle_creates_center_point_and_profile()) {
    return EXIT_FAILURE;
  }
  if (!test_fixed_points_persist_through_rebuilds()) {
    return EXIT_FAILURE;
  }
  if (!test_midpoint_anchor_follows_host_length_change()) {
    return EXIT_FAILURE;
  }
  if (!test_midpoint_anchor_follows_indirect_host_length_change()) {
    return EXIT_FAILURE;
  }
  if (!test_midpoint_anchor_both_ends_follow_host_length_change()) {
    return EXIT_FAILURE;
  }
  if (!test_midpoint_anchor_both_ends_follow_perpendicular_resize()) {
    return EXIT_FAILURE;
  }
  if (!test_fixed_endpoint_stays_put_when_redimensioning()) {
    return EXIT_FAILURE;
  }
  if (!test_rejects_dimension_drive_when_both_endpoints_are_fixed()) {
    return EXIT_FAILURE;
  }
  if (!test_detects_polygon_loop_with_line_and_arc_edges()) {
    return EXIT_FAILURE;
  }
  if (!test_fillet_creates_arc_and_trims_lines()) {
    return EXIT_FAILURE;
  }
  if (!test_fillet_radius_update_re_derives_geometry()) {
    return EXIT_FAILURE;
  }
  if (!test_fillet_delete_restores_corner()) {
    return EXIT_FAILURE;
  }
  if (!test_fillet_rejects_oversized_radius()) {
    return EXIT_FAILURE;
  }

  std::cout << "sketch_profile_test passed\n";
  return EXIT_SUCCESS;
}
