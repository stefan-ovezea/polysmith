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
using polysmith::core::add_sketch_circle;
using polysmith::core::add_sketch_line;
using polysmith::core::add_sketch_rectangle;
using polysmith::core::create_sketch_feature;
using polysmith::core::detect_sketch_profiles;
using polysmith::core::set_sketch_point_fixed;
using polysmith::core::update_sketch_dimension;
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

  return expect(feature.sketch_parameters->points.size() == 1,
                "expected circle sketch to store one center point") &&
         expect(feature.sketch_parameters->points.front().kind == "center",
                "expected stored circle point to be a center point") &&
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
  if (!test_fixed_endpoint_stays_put_when_redimensioning()) {
    return EXIT_FAILURE;
  }
  if (!test_rejects_dimension_drive_when_both_endpoints_are_fixed()) {
    return EXIT_FAILURE;
  }

  std::cout << "sketch_profile_test passed\n";
  return EXIT_SUCCESS;
}
