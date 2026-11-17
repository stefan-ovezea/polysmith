#include "core/geometry/step_import_helpers.h"

#include <cstdint>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <vector>

#include <BRepAdaptor_Surface.hxx>
#include <BRepMesh_IncrementalMesh.hxx>
#include <BRep_Builder.hxx>
#include <IFSelect_ReturnStatus.hxx>
#include <Interface_Static.hxx>
#include <NCollection_DataMap.hxx>
#include <NCollection_IndexedMap.hxx>
#include <NCollection_Sequence.hxx>
#include <Quantity_Color.hxx>
#include <Standard_Failure.hxx>
#include <STEPCAFControl_Reader.hxx>
#include <TCollection_AsciiString.hxx>
#include <TDF_Label.hxx>
#include <TDocStd_Document.hxx>
#include <TopExp.hxx>
#include <TopExp_Explorer.hxx>
#include <TopTools_ShapeMapHasher.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Compound.hxx>
#include <XCAFApp_Application.hxx>
#include <XCAFDoc_ColorTool.hxx>
#include <XCAFDoc_ColorType.hxx>
#include <XCAFDoc_DocumentTool.hxx>
#include <XCAFDoc_ShapeTool.hxx>
#include <gp_Trsf.hxx>

#include "core/diagnostics/logger.h"
#include "core/geometry/import_runtime.h"
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

// One byte marks "this face was styled": pure black (0x000000) is a real
// colour and must not collide with "no style".
constexpr std::uint32_t kColorValidMask = 0xFF000000u;

std::uint32_t pack_display_color(const Quantity_Color& color) {
  const auto channel = [](double value) -> std::uint32_t {
    if (!(value > 0.0)) {
      return 0u;
    }
    if (value >= 1.0) {
      return 255u;
    }
    return static_cast<std::uint32_t>(value * 255.0 + 0.5);
  };
  return kColorValidMask | (channel(color.Red()) << 16) |
         (channel(color.Green()) << 8) | channel(color.Blue());
}

// The file's own presentation style for `shape`, surface style first and
// the generic style second — exporters use one or the other.
bool lookup_color(const occ::handle<XCAFDoc_ColorTool>& color_tool,
                  const TopoDS_Shape& shape,
                  Quantity_Color& color) {
  if (shape.IsNull()) {
    return false;
  }
  if (color_tool->GetColor(shape, XCAFDoc_ColorSurf, color)) {
    return true;
  }
  return color_tool->GetColor(shape, XCAFDoc_ColorGen, color);
}

bool lookup_color(const occ::handle<XCAFDoc_ColorTool>& color_tool,
                  const TDF_Label& label,
                  Quantity_Color& color) {
  if (label.IsNull()) {
    return false;
  }
  if (XCAFDoc_ColorTool::GetColor(label, XCAFDoc_ColorSurf, color)) {
    return true;
  }
  return XCAFDoc_ColorTool::GetColor(label, XCAFDoc_ColorGen, color);
}

// Split the transferred XCAF document into one shape. A plain part (or an
// assembly) transfers as a compound; one free shape keeps its own shape so
// face indices match what a single-root STEP would have produced.
TopoDS_Shape shape_from_xcaf_document(
    const occ::handle<TDocStd_Document>& document) {
  const occ::handle<XCAFDoc_ShapeTool> shape_tool =
      XCAFDoc_DocumentTool::ShapeTool(document->Main());
  if (shape_tool.IsNull()) {
    return TopoDS_Shape{};
  }
  NCollection_Sequence<TDF_Label> roots;
  shape_tool->GetFreeShapes(roots);
  if (roots.IsEmpty()) {
    return TopoDS_Shape{};
  }
  if (roots.Length() == 1) {
    return XCAFDoc_ShapeTool::GetShape(roots.Value(1));
  }
  BRep_Builder builder;
  TopoDS_Compound compound;
  builder.MakeCompound(compound);
  for (Standard_Integer i = 1; i <= roots.Length(); ++i) {
    const TopoDS_Shape shape = XCAFDoc_ShapeTool::GetShape(roots.Value(i));
    if (!shape.IsNull()) {
      builder.Add(compound, shape);
    }
  }
  return compound;
}

// Per-face display colours in TopExp::MapShapes(..., TopAbs_FACE) order.
//
// XCAF attaches a style to a labelled shape and `GetColor(shape)` finds it
// by walking UP the assembly, so a solid inherits its part's colour. One
// lookup per SOLID is enough for these files (styles are per part/solid;
// the silkscreen lettering is its own solids) and it costs ~300 searches
// instead of one per face � the per-face walk took 44 s on this board.
//
// Returns an empty vector when the file styles nothing.
std::vector<std::uint32_t> face_colors_from_xcaf(
    const occ::handle<TDocStd_Document>& document,
    const TopoDS_Shape& shape) {
  std::vector<std::uint32_t> colors;
  if (shape.IsNull()) {
    return colors;
  }
  const occ::handle<XCAFDoc_ColorTool> color_tool =
      XCAFDoc_DocumentTool::ColorTool(document->Main());
  if (color_tool.IsNull()) {
    return colors;
  }

  NCollection_IndexedMap<TopoDS_Shape, TopTools_ShapeMapHasher> face_map;
  TopExp::MapShapes(shape, TopAbs_FACE, face_map);
  if (face_map.Extent() == 0) {
    return colors;
  }
  NCollection_IndexedMap<TopoDS_Shape, TopTools_ShapeMapHasher> solid_map;
  TopExp::MapShapes(shape, TopAbs_SOLID, solid_map);

  // One colour per solid, and the face -> owning-solid table in a single
  // walk of the solids.
  NCollection_DataMap<TopoDS_Shape, std::uint32_t, TopTools_ShapeMapHasher>
      solid_colors;
  std::vector<TopoDS_Shape> owner_solids(
      static_cast<std::size_t>(face_map.Extent()), TopoDS_Shape{});
  for (Standard_Integer s = 1; s <= solid_map.Extent(); ++s) {
    const TopoDS_Shape& solid = solid_map(s);
    Quantity_Color color;
    const bool styled = lookup_color(color_tool, solid, color);
    if (styled) {
      solid_colors.Bind(solid, pack_display_color(color));
    }
    for (TopExp_Explorer exp(solid, TopAbs_FACE); exp.More(); exp.Next()) {
      const Standard_Integer index = face_map.FindIndex(exp.Current());
      if (index >= 1) {
        owner_solids[static_cast<std::size_t>(index - 1)] = solid;
      }
    }
  }
  if (solid_colors.IsEmpty()) {
    return colors;
  }

  colors.assign(static_cast<std::size_t>(face_map.Extent()), 0u);
  bool any = false;
  for (Standard_Integer i = 1; i <= face_map.Extent(); ++i) {
    const TopoDS_Shape& solid = owner_solids[static_cast<std::size_t>(i - 1)];
    if (solid.IsNull() || !solid_colors.IsBound(solid)) {
      continue;
    }
    colors[static_cast<std::size_t>(i - 1)] = solid_colors.Find(solid);
    any = true;
  }
  if (!any) {
    colors.clear();
  }
  return colors;
}
}  // namespace

StepImportResult read_step_file(const std::string& file_path) {
  if (file_path.empty() || !std::filesystem::exists(file_path)) {
    throw std::runtime_error("STEP file not found: " + file_path);
  }

  try {
    // Transfer the file's units to mm (the standard xstep.cascade.unit
    // static) so imported bodies live in the same millimeter world as
    // every parametric feature.
    Interface_Static::SetCVal("xstep.cascade.unit", "MM");

    // Read through the XCAF reader: STEPControl_Reader would give the
    // geometry only, dropping the file's presentation styles (the colours
    // that make a PCB's silkscreen and component markings visible) and the
    // product structure. Colour mode is what we need here; names and layers
    // are not used yet and only cost transfer time.
    occ::handle<TDocStd_Document> document;
    const occ::handle<XCAFApp_Application> application =
        XCAFApp_Application::GetApplication();
    if (!application.IsNull()) {
      application->NewDocument("MDTV-XCAF", document);
    }

    STEPCAFControl_Reader reader;
    reader.SetColorMode(true);
    reader.SetNameMode(false);
    reader.SetLayerMode(false);

    if (reader.ReadFile(file_path.c_str()) != IFSelect_RetDone) {
      throw std::runtime_error("Failed to read STEP file '" + file_path +
                               "' — not a valid ISO-10303-21 file");
    }

    StepImportResult result;
    if (!document.IsNull() && reader.Transfer(document)) {
      result.shape = shape_from_xcaf_document(document);
      if (!result.shape.IsNull()) {
        result.face_colors = face_colors_from_xcaf(document, result.shape);
      }
    }
    if (result.shape.IsNull()) {
      // XCAF transfer unavailable — fall back to the plain reader so the
      // import still happens (without colours).
      if (reader.ChangeReader().TransferRoots() <= 0) {
        throw std::runtime_error(
            "STEP file contains no transferable geometry: " + file_path);
      }
      result.shape = reader.ChangeReader().OneShape();
    }
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
    reader.ChangeReader().FileUnits(length_units, angle_units,
                                 solid_angle_units);
    result.source_units =
        length_units.IsEmpty() ? "unknown" : length_units.First().ToCString();

    result.solid_count = count_subshapes(result.shape, TopAbs_SOLID);
    result.face_count = count_subshapes(result.shape, TopAbs_FACE);

    if (result.solid_count == 0) {
      // Faces/shells-only surface model: try to sew it into solids so
      // the solid-only modifiers work (same class of files the IGES
      // import sews — see sew_faces_to_solids).
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
        // Sewing rebuilds the topology, so the face order the colours were
        // indexed by no longer exists.
        result.face_colors.clear();
        log_info("step_import",
                 "Sewed " + std::to_string(result.face_count) +
                     " faces into " + std::to_string(result.solid_count) +
                     " solid(s) at tolerance " +
                     std::to_string(used_tolerance));
      }
    }

    if (!result.face_colors.empty()) {
      int colored = 0;
      for (const std::uint32_t packed : result.face_colors) {
        if ((packed & kColorValidMask) != 0u) {
          ++colored;
        }
      }
      log_info("step_import", "Read " + std::to_string(colored) + " of " +
                                  std::to_string(result.face_count) +
                                  " face colour(s) from the file");
    }

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

TopoDS_Shape build_step_import_shape(const std::string& feature_id) {
  const TopoDS_Shape* cached = find_imported_shape(feature_id);
  return cached ? *cached : TopoDS_Shape{};
}

}  // namespace polysmith::core
