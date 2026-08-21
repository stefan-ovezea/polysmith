#include "core/geometry/step_import_helpers.h"

#include <filesystem>
#include <stdexcept>
#include <string>

#include <BRepMesh_IncrementalMesh.hxx>
#include <IFSelect_ReturnStatus.hxx>
#include <Interface_Static.hxx>
#include <NCollection_Sequence.hxx>
#include <Standard_Failure.hxx>
#include <STEPControl_Reader.hxx>
#include <TCollection_AsciiString.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>

namespace polysmith::core {
namespace {

int count_subshapes(const TopoDS_Shape& shape, TopAbs_ShapeEnum kind) {
  int count = 0;
  for (TopExp_Explorer exp(shape, kind); exp.More(); exp.Next()) {
    ++count;
  }
  return count;
}

}  // namespace

StepImportResult read_step_file(const std::string& file_path) {
  if (file_path.empty() || !std::filesystem::exists(file_path)) {
    throw std::runtime_error("STEP file not found: " + file_path);
  }

  try {
    STEPControl_Reader reader;
    // Convert the file's units to mm on transfer (the standard
    // xstep.cascade.unit static) so imported bodies live in the same
    // millimeter world as every parametric feature.
    Interface_Static::SetCVal("xstep.cascade.unit", "MM");

    const IFSelect_ReturnStatus read_status =
        reader.ReadFile(file_path.c_str());
    if (read_status != IFSelect_RetDone) {
      throw std::runtime_error("Failed to read STEP file '" + file_path +
                               "' — not a valid ISO-10303-21 file");
    }

    if (reader.TransferRoots() <= 0) {
      throw std::runtime_error(
          "STEP file contains no transferable geometry: " + file_path);
    }

    StepImportResult result;
    result.shape = reader.OneShape();
    if (result.shape.IsNull()) {
      throw std::runtime_error(
          "STEP file produced an empty shape: " + file_path);
    }

    // The original unit names come from the file header; the first
    // length unit is what the coordinates were written in (the shape
    // itself is already converted to mm by the transfer above).
    NCollection_Sequence<TCollection_AsciiString> length_units;
    NCollection_Sequence<TCollection_AsciiString> angle_units;
    NCollection_Sequence<TCollection_AsciiString> solid_angle_units;
    reader.FileUnits(length_units, angle_units, solid_angle_units);
    result.source_units =
        length_units.IsEmpty() ? "unknown" : length_units.First().ToCString();

    result.solid_count = count_subshapes(result.shape, TopAbs_SOLID);
    result.face_count = count_subshapes(result.shape, TopAbs_FACE);

    // Guarantee every face carries a triangulation. The STEP reader
    // transfers faces without triangulations, and downstream consumers
    // (viewport tessellation, exports) read face triangulations
    // directly — an unmeshed shape silently produces empty results
    // there. Same rationale and parameters as mesh_import_helpers.
    try {
      BRepMesh_IncrementalMesh mesher(result.shape, /*linearDeflection=*/0.1,
                                      /*isRelative=*/false,
                                      /*angularDeflection=*/0.5,
                                      /*isInParallel=*/false);
      (void)mesher;
    } catch (const Standard_Failure&) {
      // Meshing failed — return the shape as-is and let consumers
      // degrade instead of failing the import.
    }
    return result;
  } catch (const Standard_Failure& failure) {
    throw std::runtime_error(
        std::string("Failed to read STEP file '") + file_path +
        "': " + failure.GetMessageString());
  }
}

TopoDS_Shape build_step_import_shape(const StepImportFeatureParameters& params) {
  return params.imported_shape;
}

}  // namespace polysmith::core
