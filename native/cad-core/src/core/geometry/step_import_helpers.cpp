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
#include <Standard_PCharacter.hxx>
#include <STEPCAFControl_Reader.hxx>
#include <TCollection_AsciiString.hxx>
#include <TCollection_ExtendedString.hxx>
#include <TDataStd_Name.hxx>
#include <TDF_Label.hxx>
#include <TDocStd_Document.hxx>
#include <TopExp.hxx>
#include <TopExp_Explorer.hxx>
#include <TopLoc_Location.hxx>
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

// OCCT keeps names as UTF-16; the rest of the core speaks UTF-8 (the names
// travel to the UI inside JSON, which rejects malformed UTF-8).
std::string to_utf8(const TCollection_ExtendedString& text) {
  if (text.IsEmpty()) {
    return {};
  }
  const int length = text.LengthOfCString();
  if (length <= 0) {
    return {};
  }
  // ToUTF8CString writes a terminator, so the buffer needs one extra byte
  // and must be handed over as a pointer variable (the parameter is a
  // reference to the pointer).
  std::string buffer(static_cast<std::size_t>(length) + 1, '\0');
  Standard_PCharacter cursor = buffer.data();
  const int written = text.ToUTF8CString(cursor);
  if (written <= 0) {
    return {};
  }
  buffer.resize(static_cast<std::size_t>(written));
  return buffer;
}

std::string label_name(const TDF_Label& label) {
  occ::handle<TDataStd_Name> attribute;
  if (label.IsNull() ||
      !label.FindAttribute(TDataStd_Name::GetID(), attribute) ||
      attribute.IsNull()) {
    return {};
  }
  return to_utf8(attribute->Get());
}

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

// The solids of `shape`, geometry only — used when the XCAF document is not
// available (plain-reader fallback, sewn surface models) so the import still
// arrives as separate bodies, just without names or styles.
std::vector<StepImportedSolid> solids_from_shape(const TopoDS_Shape& shape) {
  std::vector<StepImportedSolid> solids;
  if (shape.IsNull()) {
    return solids;
  }
  NCollection_IndexedMap<TopoDS_Shape, TopTools_ShapeMapHasher> solid_map;
  TopExp::MapShapes(shape, TopAbs_SOLID, solid_map);
  solids.reserve(static_cast<std::size_t>(solid_map.Extent()));
  for (Standard_Integer i = 1; i <= solid_map.Extent(); ++i) {
    StepImportedSolid entry;
    entry.shape = solid_map(i);
    NCollection_IndexedMap<TopoDS_Shape, TopTools_ShapeMapHasher> face_map;
    TopExp::MapShapes(entry.shape, TopAbs_FACE, face_map);
    entry.face_count = face_map.Extent();
    solids.push_back(std::move(entry));
  }
  return solids;
}

// Split the transferred XCAF document into its solids, each with the part
// name and display colour the file gives it.
//
// Names come from the product structure: a part label names the whole part,
// so every solid under it inherits that name. The lookup is keyed by the
// UNLOCATED solid, because the same part is often placed several times and
// each placement must resolve to the same name.
//
// XCAF attaches a style to a labelled shape and `GetColor(shape)` finds it by
// walking UP the assembly, so a solid inherits its part's colour. One lookup
// per SOLID is enough for these files (styles are per part; the silkscreen
// lettering is its own solids) and it costs ~1k searches instead of one per
// face — the per-face walk took 44 s on this board.
std::vector<StepImportedSolid> solids_from_xcaf(
    const occ::handle<TDocStd_Document>& document,
    const TopoDS_Shape& shape) {
  std::vector<StepImportedSolid> solids = solids_from_shape(shape);
  if (solids.empty()) {
    return solids;
  }
  const occ::handle<XCAFDoc_ShapeTool> shape_tool =
      XCAFDoc_DocumentTool::ShapeTool(document->Main());
  const occ::handle<XCAFDoc_ColorTool> color_tool =
      XCAFDoc_DocumentTool::ColorTool(document->Main());

  NCollection_DataMap<TopoDS_Shape, std::string, TopTools_ShapeMapHasher>
      part_names;
  if (!shape_tool.IsNull()) {
    NCollection_Sequence<TDF_Label> labels;
    shape_tool->GetShapes(labels);
    for (Standard_Integer i = 1; i <= labels.Length(); ++i) {
      const TDF_Label& label = labels.Value(i);
      // An assembly label names a grouping, not a part; the parts under it
      // carry the names worth showing.
      if (XCAFDoc_ShapeTool::IsAssembly(label)) {
        continue;
      }
      const std::string name = label_name(label);
      if (name.empty()) {
        continue;
      }
      const TopoDS_Shape part = XCAFDoc_ShapeTool::GetShape(label);
      if (part.IsNull()) {
        continue;
      }
      for (TopExp_Explorer exp(part.Located(TopLoc_Location()), TopAbs_SOLID);
           exp.More(); exp.Next()) {
        const TopoDS_Shape solid = exp.Current().Located(TopLoc_Location());
        if (!part_names.IsBound(solid)) {
          part_names.Bind(solid, name);
        }
      }
    }
  }

  for (StepImportedSolid& entry : solids) {
    const TopoDS_Shape key = entry.shape.Located(TopLoc_Location());
    if (part_names.IsBound(key)) {
      entry.name = part_names.Find(key);
    }
    Quantity_Color color;
    if (!color_tool.IsNull() && lookup_color(color_tool, entry.shape, color)) {
      // The style is per part/solid, so every face of the solid shares it.
      entry.face_colors.assign(static_cast<std::size_t>(entry.face_count),
                               pack_display_color(color));
    }
  }
  return solids;
}
}  // namespace

StepImportResult read_step_file(const std::string& file_path) {
  if (file_path.empty() || !std::filesystem::exists(file_path)) {
    throw std::runtime_error("STEP file not found: " + file_path);
  }
  // First line of a multi-second operation: the UI shows it in its busy
  // overlay so a 16 s import does not look like a frozen window.
  log_info("step_import",
           "Reading '" + std::filesystem::path(file_path).filename().string() +
               "' ...");

  try {
    // Transfer the file's units to mm (the standard xstep.cascade.unit
    // static) so imported bodies live in the same millimeter world as
    // every parametric feature.
    Interface_Static::SetCVal("xstep.cascade.unit", "MM");

    // Read through the XCAF reader: STEPControl_Reader would give the
    // geometry only, dropping the file's presentation styles (the colours
    // that make a PCB's silkscreen and component markings visible) and the
    // product structure that names the parts. Layers carry nothing we show,
    // so they stay off.
    occ::handle<TDocStd_Document> document;
    const occ::handle<XCAFApp_Application> application =
        XCAFApp_Application::GetApplication();
    if (!application.IsNull()) {
      application->NewDocument("MDTV-XCAF", document);
    }

    STEPCAFControl_Reader reader;
    reader.SetColorMode(true);
    reader.SetNameMode(true);
    reader.SetLayerMode(false);

    if (reader.ReadFile(file_path.c_str()) != IFSelect_RetDone) {
      throw std::runtime_error("Failed to read STEP file '" + file_path +
                               "' — not a valid ISO-10303-21 file");
    }

    StepImportResult result;
    if (!document.IsNull() && reader.Transfer(document)) {
      result.shape = shape_from_xcaf_document(document);
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

    bool topology_rebuilt = false;
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
        // Sewing rebuilds the topology, so the shapes the file's styles and
        // names were attached to no longer exist.
        topology_rebuilt = true;
        log_info("step_import",
                 "Sewed " + std::to_string(result.face_count) +
                     " faces into " + std::to_string(result.solid_count) +
                     " solid(s) at tolerance " +
                     std::to_string(used_tolerance));
      }
    }

    // Split the file's solids apart: each becomes its own body, so an
    // assembly arrives as its named, coloured parts instead of one lump.
    if (!topology_rebuilt && !document.IsNull()) {
      result.solids = solids_from_xcaf(document, result.shape);
    }
    if (result.solids.empty()) {
      result.solids = solids_from_shape(result.shape);
    }

    if (!result.solids.empty()) {
      int colored = 0;
      int named = 0;
      int solid_faces = 0;
      for (const StepImportedSolid& solid : result.solids) {
        solid_faces += solid.face_count;
        if (!solid.face_colors.empty()) {
          ++colored;
        }
        if (!solid.name.empty()) {
          ++named;
        }
      }
      if (colored > 0 || named > 0) {
        log_info("step_import",
                 "Read " + std::to_string(result.solids.size()) +
                     " solid(s): " + std::to_string(named) + " named, " +
                     std::to_string(colored) + " coloured");
      }
      // Bodies are built from the solids, so anything the file keeps as a
      // loose face or sheet is not part of any body. Say so instead of
      // dropping it silently.
      if (solid_faces < result.face_count) {
        log_warn("step_import",
                 std::to_string(result.face_count - solid_faces) +
                     " face(s) outside any solid were not imported");
      }
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
