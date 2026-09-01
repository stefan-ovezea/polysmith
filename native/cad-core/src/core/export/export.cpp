#include "core/export/export.h"

#include <stdexcept>
#include <string>
#include <vector>

#include <BRep_Builder.hxx>
#include <BRepMesh_IncrementalMesh.hxx>
#include <IFSelect_ReturnStatus.hxx>
#include <IGESControl_Controller.hxx>
#include <IGESControl_Writer.hxx>
#include <Interface_Static.hxx>
#include <STEPControl_StepModelType.hxx>
#include <STEPControl_Writer.hxx>
#include <ShapeFix_Shape.hxx>
#include <StlAPI_Writer.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS_Compound.hxx>
#include <TopoDS_Shape.hxx>

#include "core/geometry/body_compiler.h"
#include "core/document/document.h"

namespace polysmith::core {
namespace {

bool shape_has_solid(const TopoDS_Shape& shape) {
  for (TopExp_Explorer exp(shape, TopAbs_SOLID); exp.More(); exp.Next()) {
    return true;
  }
  return false;
}

// Document-level export shapes.  A mesh-import body is per-triangle B-rep
// faces with no solid — serializing one as STEP/IGES writes thousands of
// one-face shells, producing huge files that break re-import.  For
// solids_only, prefer bodies that contain solids; only when NO body has a
// solid (mesh-only document) fall back to exporting all bodies, preserving
// the previous behaviour for that case.
std::vector<TopoDS_Shape> collect_export_shapes(const DocumentState& document,
                                                bool solids_only) {
  std::vector<TopoDS_Shape> all_shapes;
  std::vector<TopoDS_Shape> solid_shapes;
  const CompiledBodies compiled = compile_bodies(document, /*include_meshes=*/false);
  for (const auto& body : compiled.bodies) {
    if (body.shape.IsNull()) continue;
    all_shapes.push_back(body.shape);
    if (shape_has_solid(body.shape)) {
      solid_shapes.push_back(body.shape);
    }
  }
  if (!solids_only) return all_shapes;
  return solid_shapes.empty() ? all_shapes : solid_shapes;
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

  const std::vector<TopoDS_Shape> shapes =
      collect_export_shapes(document, /*solids_only=*/true);
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

ExportResult export_document_as_iges(const DocumentState& document,
                                     const std::string& file_path) {
  if (file_path.empty()) {
    throw std::runtime_error("Export path cannot be empty");
  }

  const std::vector<TopoDS_Shape> shapes =
      collect_export_shapes(document, /*solids_only=*/true);
  if (shapes.empty()) {
    throw std::runtime_error("No solid features are available to export");
  }

  // One writer session with one AddShape per body, so every body lands
  // in a single IGES file. BRep mode (write.iges.brep.mode = 1) writes
  // solids as IGES 186 MSBO entities — the default Faces mode would
  // export surfaces only, and the file would re-import without solids.
  // Init() must run BEFORE setting the static: the first Init call
  // registers the static with its default and would clobber a value
  // set earlier.
  IGESControl_Controller::Init();
  const int previous_mode = Interface_Static::IVal("write.iges.brep.mode");
  Interface_Static::SetIVal("write.iges.brep.mode", 1);
  IGESControl_Writer writer;  // reads the static in its constructor
  Interface_Static::SetIVal("write.iges.brep.mode", previous_mode);
  for (const auto& shape : shapes) {
    if (!writer.AddShape(shape)) {
      throw std::runtime_error("IGES transfer failed for a body");
    }
  }
  writer.ComputeModel();
  if (!writer.Write(file_path.c_str())) {
    throw std::runtime_error("Cannot write IGES file: " + file_path);
  }

  return ExportResult{
      .file_path = file_path,
      .format = "iges",
      .exported_feature_count = static_cast<int>(shapes.size()),
  };
}

ExportResult export_document_as_stl(const DocumentState& document,
                                    const std::string& file_path) {
  if (file_path.empty()) {
    throw std::runtime_error("Export path cannot be empty");
  }

  // STL keeps every body (including mesh-import bodies) — mesh data is
  // valid STL content and documents whose only body is a mesh must export.
  const std::vector<TopoDS_Shape> shapes =
      collect_export_shapes(document, /*solids_only=*/false);
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

ExportResult export_body_as_step(const DocumentState& document,
                                 const std::string& file_path,
                                 const std::string& body_id) {
  if (file_path.empty()) {
    throw std::runtime_error("Export path cannot be empty");
  }
  if (body_id.empty()) {
    throw std::runtime_error("Body id cannot be empty");
  }

  const TopoDS_Shape body_shape = collect_export_body_shape(document, body_id);

  // Single-transfer STEP session — same writer usage as
  // export_document_as_step, but for exactly one body.
  STEPControl_Writer writer;
  const IFSelect_ReturnStatus status =
      writer.Transfer(body_shape, STEPControl_AsIs);
  if (status != IFSelect_RetDone) {
    throw std::runtime_error("STEP transfer failed for body: " + body_id);
  }
  if (writer.Write(file_path.c_str()) != IFSelect_RetDone) {
    throw std::runtime_error("Cannot write STEP file: " + file_path);
  }

  return ExportResult{
      .file_path = file_path,
      .format = "step",
      .exported_feature_count = 1,
  };
}

}  // namespace polysmith::core
