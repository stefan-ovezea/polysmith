#include "core/revolve_feature.h"

#include <cmath>
#include <stdexcept>
#include <string>

#include <TopoDS_Shape.hxx>

#include "core/feature_shape.h"

namespace polysmith::core {
namespace {

void validate_parameters(const RevolveFeatureParameters& parameters) {
  if (parameters.profile_points.size() < 3) {
    throw std::runtime_error("Revolve requires a closed sketch profile");
  }
  const double dx = parameters.axis_end_x - parameters.axis_start_x;
  const double dy = parameters.axis_end_y - parameters.axis_start_y;
  const double dz = parameters.axis_end_z - parameters.axis_start_z;
  if (std::sqrt(dx * dx + dy * dy + dz * dz) <= 1.0e-9) {
    throw std::runtime_error("Revolve axis must have non-zero length");
  }
  if (!std::isfinite(parameters.angle_degrees) ||
      std::abs(parameters.angle_degrees) <= 0.0 ||
      std::abs(parameters.angle_degrees) > 360.0) {
    throw std::runtime_error("Revolve angle must be between 0 and 360 degrees");
  }
}

std::string make_parameters_summary(const RevolveFeatureParameters& parameters) {
  return parameters.profile_id + " · " +
         std::to_string(parameters.angle_degrees) + " deg";
}

}  // namespace

FeatureEntry create_revolve_feature(
    int feature_index,
    const RevolveFeatureParameters& parameters) {
  validate_parameters(parameters);
  const TopoDS_Shape shape = build_revolve_shape(parameters);
  if (shape.IsNull()) {
    throw std::runtime_error("OCCT failed to create a revolved shape");
  }

  return FeatureEntry{
      .id = "feature-" + std::to_string(feature_index),
      .kind = "revolve",
      .name = "Revolve",
      .status = "healthy",
      .parameters_summary = make_parameters_summary(parameters),
      .revolve_parameters = parameters,
  };
}

void update_revolve_angle(FeatureEntry& feature, double angle_degrees) {
  if (feature.kind != "revolve" || !feature.revolve_parameters.has_value()) {
    throw std::runtime_error("Only revolve features can update angle");
  }

  RevolveFeatureParameters next = feature.revolve_parameters.value();
  next.angle_degrees = angle_degrees;
  validate_parameters(next);
  const TopoDS_Shape shape = build_revolve_shape(next);
  if (shape.IsNull()) {
    throw std::runtime_error("OCCT failed to update revolved shape");
  }

  feature.revolve_parameters = next;
  feature.parameters_summary = make_parameters_summary(next);
  feature.status = "healthy";
  feature.dependency_broken = false;
  feature.dependency_warning.clear();
}

}  // namespace polysmith::core
