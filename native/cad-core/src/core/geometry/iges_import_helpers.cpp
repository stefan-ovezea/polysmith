#include "core/geometry/iges_import_helpers.h"

#include <filesystem>
#include <stdexcept>
#include <string>

#include <BRepMesh_IncrementalMesh.hxx>
#include <IFSelect_ReturnStatus.hxx>
#include <IGESControl_Reader.hxx>
#include <IGESData_IGESModel.hxx>
#include <Interface_Static.hxx>
#include <Standard_Failure.hxx>
#include <Standard_Transient.hxx>
#include <TCollection_AsciiString.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>

#include "core/diagnostics/logger.h"
#include "core/geometry/import_sew_helpers.h"

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

IgesImportResult read_iges_file(const std::string& file_path) {
  if (file_path.empty() || !std::filesystem::exists(file_path)) {
    throw std::runtime_error("IGES file not found: " + file_path);
  }

  try {
    IGESControl_Reader reader;
    // Convert the file's units to mm on transfer. IGES honors the same
    // xstep.cascade.unit static as STEP (see DEIGES_ConfigurationNode).
    Interface_Static::SetCVal("xstep.cascade.unit", "MM");

    const IFSelect_ReturnStatus read_status =
        reader.ReadFile(file_path.c_str());
    if (read_status != IFSelect_RetDone) {
      throw std::runtime_error("Failed to read IGES file '" + file_path +
                               "' — not a valid IGES file");
    }

    if (reader.TransferRoots() <= 0) {
      throw std::runtime_error(
          "IGES file contains no transferable geometry: " + file_path);
    }

    IgesImportResult result;
    result.shape = reader.OneShape();
    if (result.shape.IsNull()) {
      throw std::runtime_error(
          "IGES file produced an empty shape: " + file_path);
    }

    // The original unit name comes from the global section (e.g.
    // "INCH", "MM"); the shape itself is already converted to mm by
    // the transfer above.
    const auto iges_model =
        occ::down_cast<IGESData_IGESModel>(reader.Model());
    const auto& global_section = iges_model->GlobalSection();
    result.source_units =
        global_section.UnitName().IsNull()
            ? "unknown"
            : global_section.UnitName()->ToCString();

    result.solid_count = count_subshapes(result.shape, TopAbs_SOLID);
    result.face_count = count_subshapes(result.shape, TopAbs_FACE);

    if (result.solid_count == 0) {
      // Faces-only surface model: try to sew it into solids so the
      // solid-only modifiers work on real-world files. Two tolerances:
      // tight first (exact surfaces should match closely), looser
      // second (classic IGES files often carry small gaps between
      // adjacent trimmed faces). Shared with the STEP import.
      TopoDS_Shape sewn = sew_faces_to_solids(result.shape, 1e-6);
      double used_tolerance = 1e-6;
      if (sewn.IsNull()) {
        sewn = sew_faces_to_solids(result.shape, 1e-4);
        used_tolerance = 1e-4;
      }
      if (!sewn.IsNull()) {
        result.shape = sewn;
        result.solid_count = count_subshapes(sewn, TopAbs_SOLID);
        result.face_count = count_subshapes(sewn, TopAbs_FACE);
        log_info("iges_import",
                 "Sewed " + std::to_string(result.face_count) +
                     " faces into " + std::to_string(result.solid_count) +
                     " solid(s) at tolerance " +
                     std::to_string(used_tolerance));
      }
    }

    // Guarantee every face carries a triangulation — same rationale
    // and parameters as the STEP and STL import helpers.
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
        std::string("Failed to read IGES file '") + file_path +
        "': " + failure.GetMessageString());
  }
}

TopoDS_Shape build_iges_import_shape(const IgesImportFeatureParameters& params) {
  return params.imported_shape;
}

}  // namespace polysmith::core
