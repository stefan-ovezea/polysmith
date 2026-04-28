#include "core/extrude_feature.h"

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

namespace polysmith::core {
namespace {

void validate_parameters(const ExtrudeFeatureParameters& parameters) {
  if (parameters.depth <= 0.0) {
    throw std::runtime_error("Extrude depth must be greater than zero");
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
  TopoDS_Shape shape;
  if (parameters.profile_kind == "rectangle") {
    shape = BRepPrimAPI_MakeBox(
                parameters.width, parameters.depth, parameters.height)
                .Shape();
  } else if (parameters.profile_kind == "circle") {
    shape = BRepPrimAPI_MakeCylinder(parameters.radius, parameters.depth).Shape();
  } else {
    BRepBuilderAPI_MakePolygon polygon_builder;
    for (const auto& point : parameters.profile_points) {
      polygon_builder.Add(gp_Pnt(point.x, point.y, 0.0));
    }
    polygon_builder.Close();

    if (!polygon_builder.IsDone()) {
      throw std::runtime_error("OCCT failed to create a polygon wire");
    }

    const TopoDS_Shape face =
        BRepBuilderAPI_MakeFace(polygon_builder.Wire()).Shape();
    shape = BRepPrimAPI_MakePrism(face, gp_Vec(0.0, 0.0, parameters.depth)).Shape();
  }

  if (shape.IsNull()) {
    throw std::runtime_error("OCCT failed to create an extruded shape");
  }
}

std::string make_parameters_summary(const ExtrudeFeatureParameters& parameters) {
  std::ostringstream stream;
  stream << parameters.profile_id << " · " << parameters.depth << " mm";
  return stream.str();
}

}  // namespace

FeatureEntry create_extrude_feature(int feature_index,
                                    const ExtrudeFeatureParameters& parameters) {
  validate_parameters(parameters);
  validate_occt_shape(parameters);

  return FeatureEntry{
      .id = "feature-" + std::to_string(feature_index),
      .kind = "extrude",
      .name = "Extrude",
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
