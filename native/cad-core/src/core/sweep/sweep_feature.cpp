#include "core/sweep_feature.h"

#include <cmath>
#include <stdexcept>
#include <string>

#include <TopoDS_Shape.hxx>

#include "core/feature_shape.h"

namespace polysmith::core {
namespace {

constexpr double kPi = 3.14159265358979323846;

void validate_parameters(const SweepFeatureParameters& parameters) {
  if (parameters.profile_points.size() < 3) {
    throw std::runtime_error("Sweep requires a closed sketch profile");
  }
  if (parameters.path_segments.empty()) {
    throw std::runtime_error("Sweep path must have non-zero length");
  }
}

std::string make_parameters_summary(const SweepFeatureParameters& parameters) {
  double length = 0.0;
  for (const auto& segment : parameters.path_segments) {
    const double dx = segment.end_x - segment.start_x;
    const double dy = segment.end_y - segment.start_y;
    const double dz = segment.end_z - segment.start_z;
    if (segment.kind == "arc" && segment.radius > 0.0) {
      const double start_angle =
          std::atan2(segment.start_y - segment.center_y,
                     segment.start_x - segment.center_x);
      const double end_angle =
          std::atan2(segment.end_y - segment.center_y,
                     segment.end_x - segment.center_x);
      double sweep = end_angle - start_angle;
      if (segment.ccw) {
        while (sweep <= 0.0) {
          sweep += 2.0 * kPi;
        }
      } else {
        while (sweep >= 0.0) {
          sweep -= 2.0 * kPi;
        }
      }
      length += std::abs(sweep) * segment.radius;
    } else {
      length += std::sqrt(dx * dx + dy * dy + dz * dz);
    }
  }
  return "Profile · " + std::to_string(length) + " mm path";
}

}  // namespace

FeatureEntry create_sweep_feature(int feature_index,
                                  const SweepFeatureParameters& parameters) {
  validate_parameters(parameters);
  const TopoDS_Shape shape = build_sweep_shape(parameters);
  if (shape.IsNull()) {
    throw std::runtime_error("OCCT failed to create a swept shape");
  }

  return FeatureEntry{
      .id = "feature-" + std::to_string(feature_index),
      .kind = "sweep",
      .name = "Sweep",
      .status = "healthy",
      .parameters_summary = make_parameters_summary(parameters),
      .sweep_parameters = parameters,
  };
}

}  // namespace polysmith::core
