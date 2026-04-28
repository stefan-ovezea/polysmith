#include "core/export.h"

#include <stdexcept>
#include <vector>

#include <BRep_Builder.hxx>
#include <BRepBuilderAPI_MakeFace.hxx>
#include <BRepBuilderAPI_MakePolygon.hxx>
#include <BRepMesh_IncrementalMesh.hxx>
#include <BRepPrimAPI_MakeBox.hxx>
#include <BRepPrimAPI_MakeCylinder.hxx>
#include <BRepPrimAPI_MakePrism.hxx>
#include <STEPControl_StepModelType.hxx>
#include <STEPControl_Writer.hxx>
#include <StlAPI_Writer.hxx>
#include <TopoDS_Compound.hxx>
#include <TopoDS_Shape.hxx>
#include <gp_Ax2.hxx>
#include <gp_Dir.hxx>
#include <gp_Pnt.hxx>
#include <gp_Vec.hxx>
#include <IFSelect_ReturnStatus.hxx>

#include "core/document.h"

namespace polysmith::core {
namespace {

gp_Pnt to_world_point(const std::string& plane_id, double local_x, double local_y) {
  if (plane_id == "ref-plane-xy") {
    return gp_Pnt(local_x, 0.0, local_y);
  }

  if (plane_id == "ref-plane-yz") {
    return gp_Pnt(0.0, local_x, local_y);
  }

  if (plane_id == "ref-plane-xz") {
    return gp_Pnt(local_x, local_y, 0.0);
  }

  throw std::runtime_error("Unsupported sketch plane for export: " + plane_id);
}

gp_Pnt to_world_point(const PlaneFrame& frame,
                      double local_x,
                      double local_y) {
  return gp_Pnt(frame.origin_x + frame.x_axis_x * local_x +
                    frame.y_axis_x * local_y,
                frame.origin_y + frame.x_axis_y * local_x +
                    frame.y_axis_y * local_y,
                frame.origin_z + frame.x_axis_z * local_x +
                    frame.y_axis_z * local_y);
}

gp_Pnt to_world_point(const SketchFeatureParameters& parameters,
                      double local_x,
                      double local_y) {
  if (parameters.plane_frame.has_value()) {
    const auto& frame = parameters.plane_frame.value();
    return gp_Pnt(
        frame.origin_x + frame.x_axis_x * local_x + frame.y_axis_x * local_y,
        frame.origin_y + frame.x_axis_y * local_x + frame.y_axis_y * local_y,
        frame.origin_z + frame.x_axis_z * local_x + frame.y_axis_z * local_y);
  }

  return to_world_point(parameters.plane_id, local_x, local_y);
}

gp_Vec extrusion_vector(const std::string& plane_id, double depth) {
  if (plane_id == "ref-plane-xy") {
    return gp_Vec(0.0, depth, 0.0);
  }

  if (plane_id == "ref-plane-yz") {
    return gp_Vec(depth, 0.0, 0.0);
  }

  if (plane_id == "ref-plane-xz") {
    return gp_Vec(0.0, 0.0, depth);
  }

  throw std::runtime_error("Unsupported sketch plane for export: " + plane_id);
}

gp_Vec extrusion_vector(const PlaneFrame& frame,
                        double depth) {
  return gp_Vec(frame.normal_x * depth,
                frame.normal_y * depth,
                frame.normal_z * depth);
}

TopoDS_Shape make_box_shape(const BoxFeatureParameters& parameters) {
  const TopoDS_Shape shape =
      BRepPrimAPI_MakeBox(parameters.width, parameters.height, parameters.depth)
          .Shape();

  if (shape.IsNull()) {
    throw std::runtime_error("Failed to build box shape for export");
  }

  return shape;
}

TopoDS_Shape make_cylinder_shape(const CylinderFeatureParameters& parameters) {
  const gp_Ax2 axis(gp_Pnt(parameters.radius, 0.0, parameters.radius),
                    gp_Dir(0.0, 1.0, 0.0));
  const TopoDS_Shape shape =
      BRepPrimAPI_MakeCylinder(axis, parameters.radius, parameters.height).Shape();

  if (shape.IsNull()) {
    throw std::runtime_error("Failed to build cylinder shape for export");
  }

  return shape;
}

TopoDS_Shape make_polygon_prism_shape(const ExtrudeFeatureParameters& parameters) {
  BRepBuilderAPI_MakePolygon polygon_builder;
  for (const auto& point : parameters.profile_points) {
    polygon_builder.Add(parameters.plane_frame.has_value()
                            ? to_world_point(parameters.plane_frame.value(),
                                             point.x,
                                             point.y)
                            : to_world_point(parameters.plane_id,
                                             point.x,
                                             point.y));
  }
  polygon_builder.Close();

  if (!polygon_builder.IsDone()) {
    throw std::runtime_error("Failed to build polygon wire for export");
  }

  const TopoDS_Shape face = BRepBuilderAPI_MakeFace(polygon_builder.Wire()).Shape();
  if (face.IsNull()) {
    throw std::runtime_error("Failed to build polygon face for export");
  }

  const TopoDS_Shape shape =
      BRepPrimAPI_MakePrism(
          face,
          parameters.plane_frame.has_value()
              ? extrusion_vector(parameters.plane_frame.value(), parameters.depth)
              : extrusion_vector(parameters.plane_id, parameters.depth))
          .Shape();

  if (shape.IsNull()) {
    throw std::runtime_error("Failed to build polygon extrude for export");
  }

  return shape;
}

TopoDS_Shape make_extrude_shape(const ExtrudeFeatureParameters& parameters) {
  if (parameters.profile_kind == "rectangle") {
    if (parameters.plane_id == "ref-plane-xy") {
      const TopoDS_Shape shape =
          BRepPrimAPI_MakeBox(gp_Pnt(parameters.start_x, 0.0, parameters.start_y),
                              parameters.width,
                              parameters.depth,
                              parameters.height)
              .Shape();

      if (shape.IsNull()) {
        throw std::runtime_error("Failed to build XY rectangle extrude for export");
      }

      return shape;
    }

    if (parameters.plane_id == "ref-plane-yz") {
      const TopoDS_Shape shape =
          BRepPrimAPI_MakeBox(gp_Pnt(0.0, parameters.start_x, parameters.start_y),
                              parameters.depth,
                              parameters.width,
                              parameters.height)
              .Shape();

      if (shape.IsNull()) {
        throw std::runtime_error("Failed to build YZ rectangle extrude for export");
      }

      return shape;
    }

    if (parameters.plane_id == "ref-plane-xz") {
      const TopoDS_Shape shape =
          BRepPrimAPI_MakeBox(gp_Pnt(parameters.start_x, parameters.start_y, 0.0),
                              parameters.width,
                              parameters.height,
                              parameters.depth)
              .Shape();

      if (shape.IsNull()) {
        throw std::runtime_error("Failed to build XZ rectangle extrude for export");
      }

      return shape;
    }
  }

  if (parameters.profile_kind == "circle") {
    const gp_Pnt center = to_world_point(parameters.plane_id,
                                         parameters.start_x,
                                         parameters.start_y);
    const gp_Dir axis_direction = parameters.plane_id == "ref-plane-xy"
                                      ? gp_Dir(0.0, 1.0, 0.0)
                                      : parameters.plane_id == "ref-plane-yz"
                                          ? gp_Dir(1.0, 0.0, 0.0)
                                          : gp_Dir(0.0, 0.0, 1.0);
    const gp_Ax2 axis(center, axis_direction);
    const TopoDS_Shape shape =
        BRepPrimAPI_MakeCylinder(axis, parameters.radius, parameters.depth).Shape();

    if (shape.IsNull()) {
      throw std::runtime_error("Failed to build circle extrude for export");
    }

    return shape;
  }

  if (parameters.profile_kind == "polygon") {
    return make_polygon_prism_shape(parameters);
  }

  throw std::runtime_error("Unsupported extrude profile kind for export: " +
                           parameters.profile_kind);
}

std::vector<TopoDS_Shape> collect_export_shapes(const DocumentState& document) {
  std::vector<TopoDS_Shape> shapes;

  for (const auto& feature : document.feature_history) {
    if (feature.kind == "box" && feature.box_parameters.has_value()) {
      shapes.push_back(make_box_shape(feature.box_parameters.value()));
      continue;
    }

    if (feature.kind == "cylinder" && feature.cylinder_parameters.has_value()) {
      shapes.push_back(make_cylinder_shape(feature.cylinder_parameters.value()));
      continue;
    }

    if (feature.kind == "extrude" && feature.extrude_parameters.has_value()) {
      shapes.push_back(make_extrude_shape(feature.extrude_parameters.value()));
      continue;
    }
  }

  return shapes;
}

}  // namespace

ExportResult export_document_as_step(const DocumentState& document,
                                     const std::string& file_path) {
  if (file_path.empty()) {
    throw std::runtime_error("Export path cannot be empty");
  }

  const std::vector<TopoDS_Shape> shapes = collect_export_shapes(document);
  if (shapes.empty()) {
    throw std::runtime_error("No solid features are available to export");
  }

  BRep_Builder builder;
  TopoDS_Compound compound;
  builder.MakeCompound(compound);

  for (const auto& shape : shapes) {
    builder.Add(compound, shape);
  }

  STEPControl_Writer writer;
  const IFSelect_ReturnStatus transfer_status =
      writer.Transfer(compound, STEPControl_AsIs);
  if (transfer_status != IFSelect_RetDone) {
    throw std::runtime_error("STEP transfer failed");
  }

  const IFSelect_ReturnStatus write_status = writer.Write(file_path.c_str());
  if (write_status != IFSelect_RetDone) {
    throw std::runtime_error("STEP write failed for path: " + file_path);
  }

  return ExportResult{
      .file_path = file_path,
      .format = "step",
      .exported_feature_count = static_cast<int>(shapes.size()),
  };
}

ExportResult export_document_as_stl(const DocumentState& document,
                                    const std::string& file_path) {
  if (file_path.empty()) {
    throw std::runtime_error("Export path cannot be empty");
  }

  const std::vector<TopoDS_Shape> shapes = collect_export_shapes(document);
  if (shapes.empty()) {
    throw std::runtime_error("No solid features are available to export");
  }

  BRep_Builder builder;
  TopoDS_Compound compound;
  builder.MakeCompound(compound);

  for (const auto& shape : shapes) {
    builder.Add(compound, shape);
  }

  // Tessellate the compound. The default ASCII STL writer requires that
  // every face in the shape carry a triangulation. Linear deflection of
  // 0.1 mm and angular deflection of 0.5 rad gives smooth-enough output
  // for hobbyist 3D-print slicers without producing huge files.
  constexpr double kLinearDeflection = 0.1;
  constexpr double kAngularDeflection = 0.5;
  BRepMesh_IncrementalMesh mesher(compound,
                                  kLinearDeflection,
                                  /*isRelative=*/false,
                                  kAngularDeflection,
                                  /*isInParallel=*/true);
  if (!mesher.IsDone()) {
    throw std::runtime_error("STL meshing failed");
  }

  StlAPI_Writer writer;
  writer.ASCIIMode() = false;  // binary STL, smaller files
  if (!writer.Write(compound, file_path.c_str())) {
    throw std::runtime_error("STL write failed for path: " + file_path);
  }

  return ExportResult{
      .file_path = file_path,
      .format = "stl",
      .exported_feature_count = static_cast<int>(shapes.size()),
  };
}

}  // namespace polysmith::core
