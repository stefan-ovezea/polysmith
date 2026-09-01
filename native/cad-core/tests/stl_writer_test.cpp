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

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>

#include <BRepMesh_IncrementalMesh.hxx>
#include <IFSelect_ReturnStatus.hxx>
#include <STEPControl_StepModelType.hxx>
#include <STEPControl_Writer.hxx>
#include <StlAPI_Writer.hxx>

#include "core/document/document.h"
#include "core/export/export.h"
#include "core/geometry/body_compiler.h"

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
  if (!test_body_export_step()) return 1;
  if (!test_body_export_step_unknown_body()) return 1;

  std::cout << "stl_writer_test passed\n";
  return 0;
}
