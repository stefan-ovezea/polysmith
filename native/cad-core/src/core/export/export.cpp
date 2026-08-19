#include "core/export/export.h"

#include <stdexcept>
#include <string>
#include <vector>

#include <BRep_Builder.hxx>
#include <BRepMesh_IncrementalMesh.hxx>
#include <IFSelect_ReturnStatus.hxx>
#include <STEPControl_StepModelType.hxx>
#include <STEPControl_Writer.hxx>
#include <ShapeFix_Shape.hxx>
#include <StlAPI_Writer.hxx>
#include <TopoDS_Compound.hxx>
#include <TopoDS_Shape.hxx>

#include "core/geometry/body_compiler.h"
#include "core/document/document.h"

namespace polysmith::core {
namespace {

std::vector<TopoDS_Shape> collect_export_shapes(const DocumentState& document) {
  std::vector<TopoDS_Shape> shapes;
  const CompiledBodies compiled = compile_bodies(document, /*include_meshes=*/false);
  for (const auto& body : compiled.bodies) {
    if (!body.shape.IsNull()) {
      shapes.push_back(body.shape);
    }
  }
  return shapes;
}

TopoDS_Shape collect_export_body_shape(const DocumentState& document,
                                       const std::string& body_id) {
  const CompiledBodies compiled = compile_bodies(document, /*include_meshes=*/false);
  for (const auto& body : compiled.bodies) {
    if (body.id == body_id && !body.shape.IsNull()) {
      return body.shape;
    }
  }
  throw std::runtime_error("No solid body is available to export for id: " +
                           body_id);
}

// Tessellate one shape and write it as binary STL through the standard
// OCCT writer.  OCCT 8.0's StlAPI_Writer only serializes existing face
// triangulations (it does not mesh), so the shape is meshed first with
// fixed linear/angular deflections; the writer handles face locations,
// orientation, and facet normals.
ExportResult write_stl_shape(const TopoDS_Shape& shape,
                             const std::string& file_path,
                             int exported_feature_count) {
  // Heal shape before meshing — the mesher can crash on unhealed shapes.
  ShapeFix_Shape fixer(shape);
  fixer.Perform();
  const TopoDS_Shape& healed = fixer.Shape();

  constexpr double kLinearDeflection = 0.1;
  constexpr double kAngularDeflection = 0.5;

  BRepMesh_IncrementalMesh mesher(healed,
                                  kLinearDeflection,
                                  /*isRelative=*/false,
                                  kAngularDeflection,
                                  /*isInParallel=*/false);
  if (!mesher.IsDone()) {
    throw std::runtime_error("STL meshing failed");
  }

  StlAPI_Writer writer;
  writer.ASCIIMode() = false;
  if (!writer.Write(healed, file_path.c_str())) {
    throw std::runtime_error("Cannot write STL file: " + file_path);
  }

  return ExportResult{
      .file_path = file_path,
      .format = "stl",
      .exported_feature_count = exported_feature_count,
  };
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

  // One writer session with one transfer per body, so every body lands
  // in a single STEP file as its own root — same single-file behaviour
  // as before, but real B-rep geometry (with units) instead of a
  // tessellated AP203 approximation.
  STEPControl_Writer writer;
  for (const auto& shape : shapes) {
    const IFSelect_ReturnStatus status =
        writer.Transfer(shape, STEPControl_AsIs);
    if (status != IFSelect_RetDone) {
      throw std::runtime_error("STEP transfer failed for a body");
    }
  }
  if (writer.Write(file_path.c_str()) != IFSelect_RetDone) {
    throw std::runtime_error("Cannot write STEP file: " + file_path);
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

  if (shapes.size() == 1) {
    return write_stl_shape(shapes[0], file_path, 1);
  }

  // Multiple bodies share one file: gather them in a compound and let
  // write_stl_shape heal + mesh + serialize the whole set.
  BRep_Builder builder;
  TopoDS_Compound compound;
  builder.MakeCompound(compound);

  for (const auto& shape : shapes) {
    builder.Add(compound, shape);
  }

  return write_stl_shape(compound, file_path, static_cast<int>(shapes.size()));
}

ExportResult export_body_as_stl(const DocumentState& document,
                                const std::string& file_path,
                                const std::string& body_id) {
  if (file_path.empty()) {
    throw std::runtime_error("Export path cannot be empty");
  }
  if (body_id.empty()) {
    throw std::runtime_error("Body id cannot be empty");
  }

  const TopoDS_Shape body_shape = collect_export_body_shape(document, body_id);
  return write_stl_shape(body_shape, file_path, 1);
}

}  // namespace polysmith::core
