#include "core/extrude_feature.h"

#include <cmath>
#include <sstream>
#include <stdexcept>

#include <BRepBuilderAPI_MakeFace.hxx>
#include <BRepBuilderAPI_MakePolygon.hxx>
#include <BRepPrimAPI_MakeBox.hxx>
#include <BRepPrimAPI_MakeCylinder.hxx>
#include <BRepPrimAPI_MakePrism.hxx>
#include <gp_Pnt.hxx>
#include <gp_Vec.hxx>
#include <TopoDS_Shape.hxx>

#include "core/feature_shape.h"

namespace polysmith::core {
namespace {

void validate_parameters(const ExtrudeFeatureParameters& parameters) {
  // Depth may be positive (extrude in +normal direction) or negative
  // (extrude in -normal direction). Zero depth is rejected because it
  // would build a degenerate face-only shape with no volume to boolean
  // against.
  if (parameters.depth == 0.0) {
    throw std::runtime_error("Extrude depth must be non-zero");
  }
  const auto validate_side = [](const ExtrudeFeatureParameters::SideParameters& side,
                                const std::string& label) {
    if (side.distance <= 0.0) {
      throw std::runtime_error(label + " distance must be greater than zero");
    }
    if (side.start_offset < 0.0) {
      throw std::runtime_error(label + " start offset must be non-negative");
    }
    if (std::abs(side.taper_angle_degrees) >= 89.0) {
      throw std::runtime_error(label + " taper angle must be less than 89 degrees");
    }
  };
  validate_side(parameters.side1, "Extrude side 1");
  if (parameters.extent_mode == "two_sides" ||
      parameters.extent_mode == "symmetric") {
    if (parameters.side2.has_value()) {
      validate_side(parameters.side2.value(), "Extrude side 2");
    }
  }
  if (parameters.thin.enabled) {
    if (parameters.thin.thickness <= 0.0) {
      throw std::runtime_error("Thin extrude thickness must be greater than zero");
    }
    if (parameters.thin.placement != "center" &&
        parameters.thin.placement != "inside" &&
        parameters.thin.placement != "outside") {
      throw std::runtime_error("Unsupported thin extrude placement: " +
                               parameters.thin.placement);
    }
  }
  if (parameters.extent_mode != "one_side" &&
      parameters.extent_mode != "symmetric" &&
      parameters.extent_mode != "two_sides") {
    throw std::runtime_error("Unsupported extrude extent mode: " +
                             parameters.extent_mode);
  }

  if (parameters.profile_kind == "open_chain") {
    if (!parameters.thin.enabled || parameters.profile_points.size() < 2) {
      throw std::runtime_error("Open-chain extrude requires thin mode and at least two points");
    }
    return;
  }

  if (parameters.profile_kind == "rectangle") {
    if (parameters.width <= 0.0 || parameters.height <= 0.0) {
      throw std::runtime_error("Rectangle extrude dimensions must be greater than zero");
    }
    return;
  }

  if (parameters.profile_kind == "circle") {
    if (parameters.radius <= 0.0) {
      throw std::runtime_error("Circle extrude radius must be greater than zero");
    }
    return;
  }

  if (parameters.profile_kind == "polygon") {
    if (parameters.profile_points.size() < 3) {
      throw std::runtime_error("Polygon extrude requires at least three profile points");
    }
    return;
  }

  throw std::runtime_error("Unsupported extrude profile kind: " + parameters.profile_kind);
}

void validate_occt_shape(const ExtrudeFeatureParameters& parameters) {
  const TopoDS_Shape shape = build_extrude_shape(parameters);
  if (shape.IsNull()) {
    throw std::runtime_error("OCCT failed to create an extruded shape");
  }
}

std::string make_parameters_summary(const ExtrudeFeatureParameters& parameters) {
  std::ostringstream stream;
  stream << parameters.profile_id << " · " << parameters.depth << " mm";
  return stream.str();
}

std::string make_default_name(const ExtrudeFeatureParameters& parameters) {
  return parameters.mode == "new_body" ? "Body" : "Extrude";
}

}  // namespace

FeatureEntry create_extrude_feature(int feature_index,
                                    const ExtrudeFeatureParameters& parameters) {
  validate_parameters(parameters);
  validate_occt_shape(parameters);

  return FeatureEntry{
      .id = "feature-" + std::to_string(feature_index),
      .kind = "extrude",
      .name = make_default_name(parameters),
      .status = "healthy",
      .parameters_summary = make_parameters_summary(parameters),
      .box_parameters = std::nullopt,
      .cylinder_parameters = std::nullopt,
      .extrude_parameters = parameters,
      .sketch_parameters = std::nullopt,
  };
}

void update_extrude_depth(FeatureEntry& feature, double depth) {
  if (feature.kind != "extrude") {
    throw std::runtime_error(
        "Only extrude features can be updated with extrude depth");
  }

  if (!feature.extrude_parameters.has_value()) {
    throw std::runtime_error("Extrude feature is missing parameters");
  }

  ExtrudeFeatureParameters next = feature.extrude_parameters.value();
  next.depth = depth;

  validate_parameters(next);
  validate_occt_shape(next);

  feature.parameters_summary = make_parameters_summary(next);
  feature.extrude_parameters = next;
}

}  // namespace polysmith::core
