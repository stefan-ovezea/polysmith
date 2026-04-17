#include "core/cylinder_feature.h"

#include <sstream>
#include <stdexcept>

#include <BRepPrimAPI_MakeCylinder.hxx>
#include <TopoDS_Shape.hxx>

namespace polysmith::core {
namespace {

void validate_dimensions(const CylinderFeatureParameters& parameters) {
  if (parameters.radius <= 0.0 || parameters.height <= 0.0) {
    throw std::runtime_error(
        "Cylinder radius and height must be greater than zero");
  }
}

std::string make_parameters_summary(const CylinderFeatureParameters& parameters) {
  std::ostringstream stream;
  stream << "r " << parameters.radius << " x h " << parameters.height << " mm";
  return stream.str();
}

void validate_occt_cylinder(const CylinderFeatureParameters& parameters) {
  const TopoDS_Shape cylinder =
      BRepPrimAPI_MakeCylinder(parameters.radius, parameters.height).Shape();

  if (cylinder.IsNull()) {
    throw std::runtime_error("OCCT failed to create a cylinder shape");
  }
}

}  // namespace

FeatureEntry create_cylinder_feature(
    int feature_index, const CylinderFeatureParameters& parameters) {
  validate_dimensions(parameters);
  validate_occt_cylinder(parameters);

  return FeatureEntry{
      .id = "feature-" + std::to_string(feature_index),
      .kind = "cylinder",
      .name = "Cylinder",
      .status = "healthy",
      .parameters_summary = make_parameters_summary(parameters),
      .box_parameters = std::nullopt,
      .cylinder_parameters = parameters,
      .extrude_parameters = std::nullopt,
      .sketch_parameters = std::nullopt,
  };
}

}  // namespace polysmith::core
