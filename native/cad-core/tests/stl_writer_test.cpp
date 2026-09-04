// Regression test for the standard OCCT 8.0 data-exchange writers used
// by core/export/export.cpp (StlAPI_Writer for binary/ASCII STL and
// STEPControl_Writer for STEP).
//
// History: an earlier OCCT 8.0 *precompiled* binary crashed inside these
// writers, so export.cpp hand-wrote binary STL and triangle-based AP203
// STEP.  After switching to the self-built OCCT (and configuring
// CSF_OCCTResourcePath), the writers run fine — this suite guards that
// they keep working and that the document-level export functions produce
// valid files.
//
// Note: OCCT 8.0's StlAPI_Writer only serializes existing face
// triangulations — it does not mesh the shape itself — so the direct
// writer tests tessellate first with the same parameters the export
// path uses.

#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>

#include <BRepMesh_IncrementalMesh.hxx>
#include <BRepPrimAPI_MakeBox.hxx>
#include <IFSelect_ReturnStatus.hxx>
#include <STEPControl_Reader.hxx>
#include <STEPControl_StepModelType.hxx>
#include <STEPControl_Writer.hxx>
#include <StlAPI_Reader.hxx>
#include <StlAPI_Writer.hxx>
#include <TopExp_Explorer.hxx>
#include <TopExp.hxx>
#include <NCollection_IndexedDataMap.hxx>
#include <NCollection_List.hxx>
#include <TopTools_ShapeMapHasher.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Face.hxx>

#include <gp_Pnt.hxx>

#include "core/document/document.h"
#include "core/export/export.h"
#include "core/geometry/body_compiler.h"
#include "core/viewport/viewport.h"

namespace {

using polysmith::core::compile_bodies;
using polysmith::core::DocumentManager;
using polysmith::core::DocumentState;

bool expect(bool condition, const char* message) {
  if (condition) return true;
  std::cerr << message << "\n";
  return false;
}

// Rectangle-extruded slab via the normal document pipeline — the same
// shape the real export path feeds the writers.
bool build_extruded_slab(DocumentManager& manager, DocumentState& document,
                         TopoDS_Shape& shape) {
  manager.create_document();
  manager.start_sketch_on_plane("ref-plane-xy");
  document = manager.add_sketch_rectangle(0.0, 0.0, 40.0, 20.0);
  const auto& profiles =
      document.feature_history.back().sketch_parameters->profiles;
  if (!expect(profiles.size() == 1,
              "slab fixture: expected exactly one profile")) {
    return false;
  }
  document = manager.extrude_profile(profiles.front().id, /*depth=*/10.0,
                                     /*mode=*/"", /*target_body_id=*/std::nullopt,
                                     /*parameters=*/std::nullopt);
  const auto compiled = compile_bodies(document);
  if (!expect(compiled.bodies.size() == 1,
              "slab fixture: expected exactly one body")) {
    return false;
  }
  shape = compiled.bodies.front().shape;
  return true;
}

// Same tessellation parameters as the current export path.
void mesh_for_export(TopoDS_Shape& shape) {
  BRepMesh_IncrementalMesh mesher(shape, /*linearDeflection=*/0.1,
                                  /*isRelative=*/false,
                                  /*angularDeflection=*/0.5,
                                  /*isInParallel=*/false);
  (void)mesher;
}

// Binary STL fixture for mesh-import tests — a simple box tessellated with
// the same parameters as the export path.  Written through StlAPI_Writer
// so import_stl re-reads it exactly like a user file.
bool write_box_stl(const std::filesystem::path& path) {
  TopoDS_Shape box =
      BRepPrimAPI_MakeBox(gp_Pnt(-20.0, -10.0, 5.0), 40.0, 20.0, 10.0).Shape();
  BRepMesh_IncrementalMesh mesher(box, /*linearDeflection=*/0.1,
                                  /*isRelative=*/false,
                                  /*angularDeflection=*/0.5,
                                  /*isInParallel=*/false);
  (void)mesher;
  StlAPI_Writer writer;
  writer.ASCIIMode() = false;
  return writer.Write(box, path.string().c_str());
}

// Little-endian decode — the binary STL count field is always LE,
// independent of the host.
uint32_t read_u32_le(const char* buf) {
  return static_cast<uint32_t>(static_cast<unsigned char>(buf[0])) |
         (static_cast<uint32_t>(static_cast<unsigned char>(buf[1])) << 8) |
         (static_cast<uint32_t>(static_cast<unsigned char>(buf[2])) << 16) |
         (static_cast<uint32_t>(static_cast<unsigned char>(buf[3])) << 24);
}

bool test_stlapi_writer_binary() {
  DocumentManager manager;
  DocumentState document;
  TopoDS_Shape shape;
  if (!build_extruded_slab(manager, document, shape)) return false;
  mesh_for_export(shape);

  const auto path = std::filesystem::temp_directory_path() /
                    "polysmith_stlapi_binary_test.stl";
  StlAPI_Writer writer;
  writer.ASCIIMode() = false;
  if (!expect(writer.Write(shape, path.string().c_str()),
              "stlapi binary: Write must succeed")) {
    return false;
  }

  std::ifstream in(path, std::ios::binary);
  if (!expect(in.good(), "stlapi binary: file must be readable")) {
    return false;
  }
  in.seekg(0, std::ios::end);
  const auto size = in.tellg();
  if (!expect(size >= 84,
              "stlapi binary: file must contain header + triangle count")) {
    return false;
  }
  in.seekg(80);
  char count_buf[4] = {};
  in.read(count_buf, 4);
  const uint32_t tri_count = read_u32_le(count_buf);
  if (!expect(tri_count > 0, "stlapi binary: triangle count must be > 0")) {
    return false;
  }
  std::cerr << "stlapi binary: " << tri_count << " triangles, " << size
            << " bytes\n";
  return expect(size == 84 + static_cast<std::streamoff>(tri_count) * 50,
                "stlapi binary: size must equal header + 50 bytes per facet");
}

bool test_stlapi_writer_ascii() {
  DocumentManager manager;
  DocumentState document;
  TopoDS_Shape shape;
  if (!build_extruded_slab(manager, document, shape)) return false;
  mesh_for_export(shape);

  const auto path = std::filesystem::temp_directory_path() /
                    "polysmith_stlapi_ascii_test.stl";
  StlAPI_Writer writer;
  writer.ASCIIMode() = true;
  if (!expect(writer.Write(shape, path.string().c_str()),
              "stlapi ascii: Write must succeed")) {
    return false;
  }

  std::ifstream in(path);
  if (!expect(in.good(), "stlapi ascii: file must be readable")) {
    return false;
  }
  std::string head(64, '\0');
  in.read(head.data(), 64);
  return expect(head.rfind("solid", 0) == 0,
                "stlapi ascii: file must start with 'solid'");
}

bool test_stepcontrol_writer() {
  DocumentManager manager;
  DocumentState document;
  TopoDS_Shape shape;
  if (!build_extruded_slab(manager, document, shape)) return false;

  const auto path = std::filesystem::temp_directory_path() /
                    "polysmith_stepcontrol_test.step";
  STEPControl_Writer writer;
  const IFSelect_ReturnStatus transfer = writer.Transfer(shape, STEPControl_AsIs);
  if (!expect(transfer == IFSelect_RetDone,
              "step: Transfer must return RetDone")) {
    return false;
  }
  const IFSelect_ReturnStatus written = writer.Write(path.string().c_str());
  if (!expect(written == IFSelect_RetDone,
              "step: Write must return RetDone")) {
    return false;
  }

  std::ifstream in(path);
  if (!expect(in.good(), "step: file must be readable")) {
    return false;
  }
  std::string head(64, '\0');
  in.read(head.data(), 64);
  return expect(head.rfind("ISO-10303-21", 0) == 0,
                "step: file must start with ISO-10303-21 header");
}

// Control: the document-level export functions must keep producing
// valid files through the standard writers.
bool test_document_export_stl() {
  DocumentManager manager;
  DocumentState document;
  TopoDS_Shape shape;
  if (!build_extruded_slab(manager, document, shape)) return false;

  const auto path = std::filesystem::temp_directory_path() /
                    "polysmith_document_export_test.stl";
  const auto result = polysmith::core::export_document_as_stl(
      document, path.string());
  std::ifstream in(result.file_path, std::ios::binary);
  if (!expect(in.good(), "document stl: file must be readable")) {
    return false;
  }
  in.seekg(0, std::ios::end);
  return expect(in.tellg() >= 84,
                "document stl: file must contain header + triangle count");
}

bool test_document_export_step() {
  DocumentManager manager;
  DocumentState document;
  TopoDS_Shape shape;
  if (!build_extruded_slab(manager, document, shape)) return false;

  const auto path = std::filesystem::temp_directory_path() /
                    "polysmith_document_export_test.step";
  const auto result = polysmith::core::export_document_as_step(
      document, path.string());
  std::ifstream in(result.file_path);
  if (!expect(in.good(), "document step: file must be readable")) {
    return false;
  }
  std::string head(64, '\0');
  in.read(head.data(), 64);
  return expect(head.rfind("ISO-10303-21", 0) == 0,
                "document step: file must start with ISO-10303-21 header");
}

// Document-level STEP export must skip mesh-import bodies when the
// document also contains real solids.  A mesh body is one B-rep face per
// triangle with no solid; including it writes thousands of one-face shells
// that balloon the file and break re-import (2026-09: a 243 KB STL became
// a 28 MB STEP that could not be re-imported).  The exported file must
// re-read as exactly one root containing the slab solid — 6 faces, no
// mesh triangles.
bool test_document_export_step_skips_mesh_bodies() {
  DocumentManager manager;
  DocumentState document;
  TopoDS_Shape shape;
  if (!build_extruded_slab(manager, document, shape)) return false;

  const auto stl_path = std::filesystem::temp_directory_path() /
                        "polysmith_import_fixture_box.stl";
  if (!expect(write_box_stl(stl_path),
              "mesh skip: box STL fixture must be written")) {
    return false;
  }
  // Second body: the imported mesh (one face per triangle, no solid).
  // import_stl stores only the path, so the fixture must stay on disk
  // until the export re-compiles the document.
  document = manager.import_stl(stl_path.string(), /*scale=*/1.0);
  const auto compiled = compile_bodies(document);
  if (!expect(compiled.bodies.size() == 2,
              "mesh skip: fixture expected exactly two bodies")) {
    return false;
  }

  const auto path = std::filesystem::temp_directory_path() /
                    "polysmith_document_export_skip_mesh_test.step";
  const auto result =
      polysmith::core::export_document_as_step(document, path.string());
  if (!expect(result.exported_feature_count == 1,
              "mesh skip: only the solid body must be exported")) {
    return false;
  }

  STEPControl_Reader reader;
  if (!expect(reader.ReadFile(path.string().c_str()) == IFSelect_RetDone,
              "mesh skip: re-read must succeed")) {
    return false;
  }
  if (!expect(reader.NbRootsForTransfer() == 1,
              "mesh skip: re-read must have exactly one root")) {
    return false;
  }
  if (!expect(reader.TransferRoots(),
              "mesh skip: TransferRoots must succeed")) {
    return false;
  }

  const TopoDS_Shape imported = reader.OneShape();
  bool has_solid = false;
  for (TopExp_Explorer exp(imported, TopAbs_SOLID); exp.More(); exp.Next()) {
    has_solid = true;
    break;
  }
  if (!expect(has_solid, "mesh skip: re-imported shape must contain a solid")) {
    return false;
  }

  int face_count = 0;
  for (TopExp_Explorer exp(imported, TopAbs_FACE); exp.More(); exp.Next()) {
    ++face_count;
  }
  // 6 = the slab's exact faces; the mesh body would add its 12 triangles.
  if (!expect(face_count == 6,
              "mesh skip: re-imported shape must have only the slab faces")) {
    return false;
  }
  return true;
}

// Per-body STEP export (the "Send to Slicer ▸ As STEP" path) — one body,
// one transferred shape, a valid B-rep file.
bool test_body_export_step() {
  DocumentManager manager;
  DocumentState document;
  TopoDS_Shape shape;
  if (!build_extruded_slab(manager, document, shape)) return false;

  // A body id IS its root feature id; recompile so the test tracks the
  // same resolution the export path uses.
  const auto compiled = compile_bodies(document);
  if (!expect(compiled.bodies.size() == 1,
              "body step: fixture expected exactly one body")) {
    return false;
  }
  const std::string body_id = compiled.bodies.front().id;

  const auto path = std::filesystem::temp_directory_path() /
                    "polysmith_body_export_test.step";
  const auto result =
      polysmith::core::export_body_as_step(document, path.string(), body_id);
  if (!expect(result.format == "step", "body step: format must be 'step'")) {
    return false;
  }
  if (!expect(result.exported_feature_count == 1,
              "body step: exported_feature_count must be 1")) {
    return false;
  }

  std::ifstream in(result.file_path);
  if (!expect(in.good(), "body step: file must be readable")) {
    return false;
  }
  std::string head(64, '\0');
  in.read(head.data(), 64);
  return expect(head.rfind("ISO-10303-21", 0) == 0,
                "body step: file must start with ISO-10303-21 header");
}

// The slab's top face (normal_z = +1).  Mirrors face_projection_arc_test.cpp.
std::optional<polysmith::core::ViewportSolidFace> top_face(
    const DocumentState& document) {
  const auto viewport = polysmith::core::build_viewport_state(
      std::optional<polysmith::core::DocumentState>(document));
  for (const auto& face : viewport.solid_faces) {
    if (std::abs(face.normal_z - 1.0) < 1e-6) {
      return face;
    }
  }
  return std::nullopt;
}

// STL export tessellation pins both historic failures of write_stl_shape:
//
//   1. The display mesh's 0.5 angular deflection (~30°/segment) exported
//      fillets as ~4 chords per quarter turn — flat facets that read as
//      "missing facets" in a slicer.
//   2. The 1° (0.01745) first fix exploded doubly-curved faces: sin(1°) >
//      0.01745 fails the mesher's strict-inequality check, forcing 0.5°
//      segments — a r2 fillet on a r13 hole rim (a torus) meshed at
//      720×180 ≈ 260k triangles — a 13 MB STL for a 60×40×10 part.
//
// The current setting (0.1 ≈ 5.7°/segment) lands between the two: the
// torus fillet meshes to a few thousand triangles while every arc keeps
// sub-print-resolution sagitta.  The fixture is a slab with a through
// hole and a fillet on the hole rim — the exact torus shape — exported
// through the real document path and re-read as a slicer would.
bool test_document_export_stl_fillet_quality() {
  DocumentManager manager;
  DocumentState document;
  TopoDS_Shape shape;
  if (!build_extruded_slab(manager, document, shape)) return false;

  // Through hole on the top face (z=10 plane).
  const auto top = top_face(document);
  if (!expect(top.has_value(), "fillet stl: top face found")) {
    return false;
  }
  polysmith::core::HoleFeatureParameters hole;
  hole.extent_type = "through_all";
  hole.diameter = 6.0;
  document = manager.create_hole(top->face_id, 20.0, 10.0, 10.0, hole);
  document = manager.confirm_hole(document.feature_history.back().id);

  // The top rim of the hole: a circular edge whose samples all sit at
  // z≈10 while x/y vary (outer slab edges keep one coordinate constant).
  std::string rim_edge_id;
  {
    const auto viewport = polysmith::core::build_viewport_state(
        std::optional<polysmith::core::DocumentState>(document));
    for (const auto& edge : viewport.edges) {
      if (edge.points.size() < 6) continue;
      bool all_at_top = true;
      for (size_t i = 2; i + 3 <= edge.points.size(); i += 3) {
        if (std::abs(edge.points[i] - 10.0) > 1e-6) {
          all_at_top = false;
          break;
        }
      }
      if (!all_at_top) continue;
      // A circle varies in BOTH x and y; a straight slab edge keeps one
      // coordinate constant.  Testing only one of them picked the top-front
      // slab edge instead of the hole rim (and filleted a straight edge).
      bool varies_x = false;
      bool varies_y = false;
      for (size_t i = 3; i + 3 <= edge.points.size(); i += 3) {
        if (std::abs(edge.points[i] - edge.points[0]) > 1e-6) varies_x = true;
        if (std::abs(edge.points[i + 1] - edge.points[1]) > 1e-6) {
          varies_y = true;
        }
      }
      if (varies_x && varies_y) {
        rim_edge_id = edge.id;
        break;
      }
    }
  }
  if (!expect(!rim_edge_id.empty(), "fillet stl: hole rim edge found")) {
    return false;
  }

  // Fillet the rim — this creates the torus face.
  document = manager.create_fillet({rim_edge_id}, 2.0);
  document = manager.confirm_fillet(document.feature_history.back().id);

  const auto path = std::filesystem::temp_directory_path() /
                    "polysmith_document_export_fillet_quality_test.stl";
  const auto result =
      polysmith::core::export_document_as_stl(document, path.string());
  (void)result;

  StlAPI_Reader reader;
  TopoDS_Shape imported;
  if (!expect(reader.Read(imported, path.string().c_str()),
              "fillet stl: re-read must succeed")) {
    return false;
  }

  // OCCT 8.0's StlAPI_Reader builds one polygon-only face per facet
  // (BRepBuilderAPI_MakeShapeOnMesh — no triangulations to read back),
  // so the face count IS the triangle count a slicer would ingest.
  int triangle_count = 0;
  for (TopExp_Explorer exp(imported, TopAbs_FACE); exp.More(); exp.Next()) {
    ++triangle_count;
  }
  std::cerr << "fillet stl: re-read " << triangle_count << " triangles\n";

  // Closed shell: every edge is shared by exactly two triangles.  A free
  // edge (one ancestor) means the STL carries an open boundary.
  NCollection_IndexedDataMap<TopoDS_Shape, NCollection_List<TopoDS_Shape>,
                             TopTools_ShapeMapHasher>
      face_ancestors;
  TopExp::MapShapesAndAncestors(imported, TopAbs_EDGE, TopAbs_FACE,
                                face_ancestors);
  int free_edges = 0;
  for (int i = 1; i <= face_ancestors.Extent(); ++i) {
    if (face_ancestors.FindFromIndex(i).Extent() == 1) {
      ++free_edges;
    }
  }
  if (!expect(free_edges == 0, "fillet stl: closed shell (no free edges)")) {
    return false;
  }

  // Coarse (0.5): ~13 segments around a circle — the total lands below
  // 1000.  Fine (0.1): ~63 around × ~16 across the quarter-torus — a few
  // thousand.  Exploded (0.01745): ~720×180 — hundreds of thousands.
  if (!expect(triangle_count >= 1000,
              "fillet stl: slicer-grade tessellation (>= 1000 triangles)")) {
    return false;
  }
  return expect(triangle_count <= 30000,
                "fillet stl: no torus explosion (<= 30000 triangles)");
}

// Unknown body id must throw (never crash) and must not write a file.
bool test_body_export_step_unknown_body() {
  DocumentManager manager;
  DocumentState document;
  TopoDS_Shape shape;
  if (!build_extruded_slab(manager, document, shape)) return false;

  const auto path = std::filesystem::temp_directory_path() /
                    "polysmith_body_export_unknown_test.step";
  bool threw = false;
  try {
    (void)polysmith::core::export_body_as_step(document, path.string(),
                                               "unknown-body-id");
  } catch (const std::runtime_error&) {
    threw = true;
  }
  if (!expect(threw, "body step: unknown body id must throw runtime_error")) {
    return false;
  }
  return expect(!std::filesystem::exists(path),
                "body step: unknown body id must not write a file");
}

}  // namespace

int main() {
  if (!test_stlapi_writer_binary()) return 1;
  if (!test_stlapi_writer_ascii()) return 1;
  if (!test_stepcontrol_writer()) return 1;
  if (!test_document_export_stl()) return 1;
  if (!test_document_export_step()) return 1;
  if (!test_document_export_step_skips_mesh_bodies()) return 1;
  if (!test_document_export_stl_fillet_quality()) return 1;
  if (!test_body_export_step()) return 1;
  if (!test_body_export_step_unknown_body()) return 1;

  std::cout << "stl_writer_test passed\n";
  return 0;
}
