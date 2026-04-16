#include "core/box_feature.h"

#include <sstream>
#include <stdexcept>

#include <BRepPrimAPI_MakeBox.hxx>
#include <TopoDS_Shape.hxx>

namespace polysmith::core {
namespace {

void validate_dimensions(const BoxFeatureParameters& parameters) {
  if (parameters.width <= 0.0 || parameters.height <= 0.0 ||
      parameters.depth <= 0.0) {
    throw std::runtime_error(
        "Box dimensions must be greater than zero");
  }
}

std::string make_parameters_summary(const BoxFeatureParameters& parameters) {
  std::ostringstream stream;
  stream << parameters.width << " x " << parameters.height << " x "
         << parameters.depth << " mm";
  return stream.str();
}

void validate_occt_box(const BoxFeatureParameters& parameters) {
  const TopoDS_Shape box = BRepPrimAPI_MakeBox(
                               parameters.width,
                               parameters.height,
                               parameters.depth)
                               .Shape();

  if (box.IsNull()) {
    throw std::runtime_error("OCCT failed to create a box shape");
  }
}

}  // namespace

FeatureEntry create_box_feature(int feature_index,
                                const BoxFeatureParameters& parameters) {
  validate_dimensions(parameters);
  validate_occt_box(parameters);

  return FeatureEntry{
      .id = "feature-" + std::to_string(feature_index),
      .kind = "box",
      .name = "Box",
      .status = "healthy",
      .parameters_summary = make_parameters_summary(parameters),
      .box_parameters = parameters,
      .cylinder_parameters = std::nullopt,
      .sketch_parameters = std::nullopt,
  };
}

void update_box_feature(FeatureEntry& feature,
                        const BoxFeatureParameters& parameters) {
  if (feature.kind != "box") {
    throw std::runtime_error("Only box features can be updated with box parameters");
  }

  validate_dimensions(parameters);
  validate_occt_box(parameters);

  feature.parameters_summary = make_parameters_summary(parameters);
  feature.box_parameters = parameters;
}

}  // namespace polysmith::core
