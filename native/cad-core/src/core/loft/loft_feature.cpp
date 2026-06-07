#include "core/loft_feature.h"

#include <sstream>
#include <stdexcept>

#include "core/feature_shape.h"

namespace polysmith::core {
namespace {

void validate_parameters(const LoftFeatureParameters& parameters) {
  if (parameters.sections.size() < 2) {
    throw std::runtime_error("Loft requires at least two profiles");
  }

  for (const auto& section : parameters.sections) {
    if (section.profile_points.size() < 3) {
      throw std::runtime_error("Loft profiles must be closed regions");
    }
  }
}

std::string make_parameters_summary(const LoftFeatureParameters& parameters) {
  std::ostringstream stream;
  stream << parameters.sections.size() << " sections";
  if (parameters.ruled) {
    stream << " · ruled";
  }
  return stream.str();
}

}  // namespace

FeatureEntry create_loft_feature(int feature_index,
                                 const LoftFeatureParameters& parameters) {
  validate_parameters(parameters);
  const TopoDS_Shape shape = build_loft_shape(parameters);
  if (shape.IsNull()) {
    throw std::runtime_error("OCCT failed to create a lofted shape");
  }

  return FeatureEntry{
      .id = "feature-" + std::to_string(feature_index),
      .kind = "loft",
      .name = "Loft",
      .status = "healthy",
      .parameters_summary = make_parameters_summary(parameters),
      .box_parameters = std::nullopt,
      .cylinder_parameters = std::nullopt,
      .extrude_parameters = std::nullopt,
      .sketch_parameters = std::nullopt,
      .fillet_parameters = std::nullopt,
      .chamfer_parameters = std::nullopt,
      .construction_plane_parameters = std::nullopt,
      .loft_parameters = parameters,
  };
}

void update_loft_ruled(FeatureEntry& feature, bool ruled) {
  if (feature.kind != "loft" || !feature.loft_parameters.has_value()) {
    throw std::runtime_error("Only loft features can update ruled mode");
  }

  LoftFeatureParameters next = feature.loft_parameters.value();
  next.ruled = ruled;
  validate_parameters(next);
  const TopoDS_Shape shape = build_loft_shape(next);
  if (shape.IsNull()) {
    throw std::runtime_error("OCCT failed to update lofted shape");
  }

  feature.loft_parameters = next;
  feature.parameters_summary = make_parameters_summary(next);
}

}  // namespace polysmith::core
