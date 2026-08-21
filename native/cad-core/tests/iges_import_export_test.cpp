// Regression test for the IGES import + export features:
//   - import_iges (parse-once, self-contained B-rep snapshot body)
//   - unit conversion (file units -> mm on read)
//   - multi-solid files stay ONE compound body (CompiledBody.id ==
//     feature_id invariant)
//   - downstream parametric ops (extrude cut) on the imported body
//   - export_document_as_iges + re-import round-trip (IGES has no
//     magic header, so re-importing the exported file and checking the
//     geometry is the strongest export assertion)
//   - serialization round-trip incl. include_opaque gating, undo/redo,
//     parse-before-mutate error paths, and independence from the
//     source file after import.
//
// Fixtures follow the in-repo convention: real geometry generated in
// the test, written to the temp directory at runtime via
// IGESControl_Writer (the same convention as the STL import suite).

#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include <BRepBndLib.hxx>
#include <BRepGProp.hxx>
#include <BRepPrimAPI_MakeBox.hxx>
#include <BRepPrimAPI_MakeCylinder.hxx>
#include <Bnd_Box.hxx>
#include <GProp_GProps.hxx>
#include <IGESControl_Controller.hxx>
#include <IGESControl_Writer.hxx>
#include <Interface_Static.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include <gp_Pnt.hxx>

#include <nlohmann/json.hpp>

#include "core/document/document.h"
#include "core/export/export.h"
#include "core/geometry/body_compiler.h"
#include "core/geometry/iges_import_helpers.h"
#include "core/viewport/viewport.h"
#include "protocol/serialization.h"

namespace {

using polysmith::core::compile_bodies;
using polysmith::core::DocumentManager;
using polysmith::core::DocumentState;

bool expect(bool condition, const char* message) {
  if (condition) return true;
  std::cerr << message << "\n";
  return false;
}

bool near(double actual, double expected, double tolerance, const char* message) {
  if (std::abs(actual - expected) <= tolerance) return true;
  std::cerr << message << " (expected " << expected << ", got " << actual
            << ")\n";
  return false;
}

int count_faces(const TopoDS_Shape& shape) {
  int count = 0;
  for (TopExp_Explorer exp(shape, TopAbs_FACE); exp.More(); exp.Next()) {
    ++count;
  }
  return count;
}

bool shape_has_solid(const TopoDS_Shape& shape) {
  for (TopExp_Explorer exp(shape, TopAbs_SOLID); exp.More(); exp.Next()) {
    return true;
  }
  return false;
}

double shape_volume(const TopoDS_Shape& shape) {
  GProp_GProps props;
  BRepGProp::VolumeProperties(shape, props);
  return props.Mass();
}

// Returns {xmin, ymin, zmin, xmax, ymax, zmax}.
std::array<double, 6> shape_bounds(const TopoDS_Shape& shape) {
  Bnd_Box box;
  BRepBndLib::Add(shape, box);
  std::array<double, 6> bounds{};
  box.Get(bounds[0], bounds[1], bounds[2], bounds[3], bounds[4], bounds[5]);
  return bounds;
}

std::string temp_iges_path(const std::string& name) {
  return (std::filesystem::temp_directory_path() /
          ("polysmith_iges_import_" + name + ".iges"))
      .string();
}

void write_fixture(const std::string& name, const std::string& content) {
  const auto path = std::filesystem::temp_directory_path() /
                    ("polysmith_iges_import_" + name + ".iges");
  std::ofstream out(path);
  out << content;
}

// Writes `shapes` as an IGES file and returns the path. The writer's
// units come from the write.iges.unit static (default "MM"). BRep
// mode (write.iges.brep.mode = 1) writes solids as IGES 186 MSBO
// entities — the default Faces mode exports surfaces only and the
// file would re-import without solids.
std::string write_iges_file(const std::string& name,
                            const std::vector<TopoDS_Shape>& shapes) {
  const std::string path = temp_iges_path(name);
  // Init() must run BEFORE setting the static: the first Init call
  // registers write.iges.brep.mode with its default (Faces=0) and
  // would clobber a value set earlier.
  IGESControl_Controller::Init();
  const int previous_mode = Interface_Static::IVal("write.iges.brep.mode");
  Interface_Static::SetIVal("write.iges.brep.mode", 1);
  IGESControl_Writer writer;  // reads the static in its constructor
  Interface_Static::SetIVal("write.iges.brep.mode", previous_mode);
  for (const auto& shape : shapes) {
    if (!writer.AddShape(shape)) {
      std::cerr << "fixture: AddShape failed\n";
      return "";
    }
  }
  writer.ComputeModel();
  if (!writer.Write(path.c_str())) {
    std::cerr << "fixture: Write failed\n";
    return "";
  }
  return path;
}

std::string write_iges(const std::string& name, const TopoDS_Shape& shape) {
  return write_iges_file(name, {shape});
}

// 40x20x10 box spanning z -5..5 (mid-height XY plane) and x/y
// -20..20 / -10..10.
std::string write_box_iges(const std::string& name) {
  return write_iges(name, BRepPrimAPI_MakeBox(gp_Pnt(-20.0, -10.0, -5.0),
                                              40.0, 20.0, 10.0)
                              .Shape());
}

// Two disjoint 10x10x10 boxes added as two separate roots, so the
// reader exercises the multi-root -> OneShape compound path (the
// shape real multi-solid IGES exports take).
std::string write_two_solid_iges(const std::string& name) {
  return write_iges_file(
      name,
      {BRepPrimAPI_MakeBox(gp_Pnt(0.0, 0.0, 0.0), 10.0, 10.0, 10.0).Shape(),
       BRepPrimAPI_MakeBox(gp_Pnt(20.0, 0.0, 0.0), 10.0, 10.0, 10.0).Shape()});
}

// The same 40x20x10 box written with the writer unit set to INCH, so
// the reader must convert back to mm (the unit-conversion case).
std::string write_inch_box_iges(const std::string& name) {
  Interface_Static::SetCVal("write.iges.unit", "INCH");
  const std::string path = write_box_iges(name);
  Interface_Static::SetCVal("write.iges.unit", "MM");
  return path;
}

// A 40x20x10 box written in the DEFAULT Faces mode — a surface-only
// file (trimmed faces, no solid entities), the shape real-world IGES
// exports take. The reader must sew it into a solid so the solid-only
// modifiers work.
std::string write_faces_box_iges(const std::string& name) {
  const std::string path = temp_iges_path(name);
  IGESControl_Controller::Init();
  const int previous_mode = Interface_Static::IVal("write.iges.brep.mode");
  Interface_Static::SetIVal("write.iges.brep.mode", 0);
  IGESControl_Writer writer;  // reads the static in its constructor
  Interface_Static::SetIVal("write.iges.brep.mode", previous_mode);
  const TopoDS_Shape box =
      BRepPrimAPI_MakeBox(gp_Pnt(-20.0, -10.0, -5.0), 40.0, 20.0, 10.0)
          .Shape();
  if (!writer.AddShape(box)) {
    std::cerr << "fixture: faces-mode AddShape failed\n";
    return "";
  }
  writer.ComputeModel();
  if (!writer.Write(path.c_str())) {
    std::cerr << "fixture: faces-mode Write failed\n";
    return "";
  }
  return path;
}

// --- tests -------------------------------------------------------------

bool test_box_import_creates_body() {
  const std::string path = write_box_iges("box");

  DocumentManager manager;
  manager.create_document();
  const DocumentState document = manager.import_iges(path);

  const auto& feature = document.feature_history.back();
  if (!expect(feature.kind == "iges_import" &&
                  feature.iges_import_parameters.has_value() &&
                  feature.iges_import_parameters->file_path == path &&
                  !feature.iges_import_parameters->serialized_shape.empty(),
              "import: feature must be iges_import with path + snapshot")) {
    return false;
  }

  const auto compiled = compile_bodies(document);
  if (!expect(compiled.bodies.size() == 1,
              "import: exactly one compiled body")) {
    return false;
  }
  if (!expect(compiled.bodies.front().id == feature.id,
              "import: body id must match the feature id")) {
    return false;
  }
  const TopoDS_Shape shape = compiled.bodies.front().shape;
  if (!expect(count_faces(shape) == 6, "import: box must have 6 faces") ||
      !expect(shape_has_solid(shape), "import: shape must contain a solid")) {
    return false;
  }
  const auto bounds = shape_bounds(shape);
  if (!near(bounds[0], -20.0, 1e-6, "import: xmin") ||
      !near(bounds[1], -10.0, 1e-6, "import: ymin") ||
      !near(bounds[2], -5.0, 1e-6, "import: zmin") ||
      !near(bounds[3], 20.0, 1e-6, "import: xmax") ||
      !near(bounds[4], 10.0, 1e-6, "import: ymax") ||
      !near(bounds[5], 5.0, 1e-6, "import: zmax")) {
    return false;
  }
  if (!near(shape_volume(shape), 8000.0, 8000.0 * 0.02,
            "import: box volume")) {
    return false;
  }

  // The imported body must flow through the native mesh path into the
  // viewport (no legacy primitive renderer exists for it).
  const auto viewport = polysmith::core::build_viewport_state(
      std::optional<DocumentState>(document));
  return expect(!viewport.meshes.empty(),
                "import: viewport must emit the imported body");
}

bool test_inch_file_converts_to_mm() {
  const std::string path = write_inch_box_iges("inch_box");

  DocumentManager manager;
  manager.create_document();
  const DocumentState document = manager.import_iges(path);

  const auto& feature = document.feature_history.back();
  if (!expect(feature.iges_import_parameters.has_value() &&
                  !feature.iges_import_parameters->source_units.empty(),
              "inch: feature must carry the source units")) {
    return false;
  }
  if (!expect(feature.iges_import_parameters->source_units.find("INCH") !=
                      std::string::npos,
              "inch: source_units must report INCH") ||
      !expect(feature.parameters_summary.find("→ mm") != std::string::npos,
              "inch: summary must mention the conversion")) {
    return false;
  }

  const auto compiled = compile_bodies(document);
  if (!expect(compiled.bodies.size() == 1, "inch: exactly one body")) {
    return false;
  }
  const auto bounds = shape_bounds(compiled.bodies.front().shape);
  return near(bounds[0], -20.0, 1e-3, "inch: xmin") &&
         near(bounds[1], -10.0, 1e-3, "inch: ymin") &&
         near(bounds[2], -5.0, 1e-3, "inch: zmin") &&
         near(bounds[3], 20.0, 1e-3, "inch: xmax") &&
         near(bounds[4], 10.0, 1e-3, "inch: ymax") &&
         near(bounds[5], 5.0, 1e-3, "inch: zmax");
}

bool test_multi_solid_stays_one_body() {
  const std::string path = write_two_solid_iges("two_solids");

  DocumentManager manager;
  manager.create_document();
  const DocumentState document = manager.import_iges(path);

  const auto& feature = document.feature_history.back();
  const auto compiled = compile_bodies(document);
  if (!expect(compiled.bodies.size() == 1,
              "multi: two solids must stay ONE body (one feature)")) {
    return false;
  }
  if (!expect(compiled.bodies.front().id == feature.id,
              "multi: body id must match the feature id")) {
    return false;
  }
  const TopoDS_Shape shape = compiled.bodies.front().shape;
  return expect(count_faces(shape) == 12,
                "multi: two boxes must have 12 faces") &&
         expect(shape_has_solid(shape), "multi: shape must contain solids") &&
         near(shape_volume(shape), 2000.0, 2000.0 * 0.02,
              "multi: total volume");
}

bool test_downstream_cut_extrude() {
  const std::string path = write_box_iges("cut");

  DocumentManager manager;
  manager.create_document();
  DocumentState document = manager.import_iges(path);
  const std::string body_id = document.feature_history.back().id;

  document = manager.start_sketch_on_plane("ref-plane-xy");
  document = manager.add_sketch_rectangle(-20.0, -10.0, 20.0, 10.0);
  const auto& profiles =
      document.feature_history.back().sketch_parameters->profiles;
  if (!expect(profiles.size() == 1, "cut: fixture needs one profile")) {
    return false;
  }
  // Cut the top half (z 0..5) of the imported box through the normal
  // parametric extrude path — the imported body must behave as a
  // boolean target like any other body.
  document = manager.extrude_profile(profiles.front().id, /*depth=*/5.0,
                                     /*mode=*/"cut", body_id,
                                     /*parameters=*/std::nullopt);

  const auto compiled = compile_bodies(document);
  if (!expect(compiled.bodies.size() == 1,
              "cut: the cut modifies the target body, not a new one")) {
    return false;
  }
  return near(shape_volume(compiled.bodies.front().shape), 4000.0,
              4000.0 * 0.02, "cut: remaining volume must be the bottom half");
}

bool test_cylinder_curved_faces() {
  const std::string path =
      write_iges("cylinder", BRepPrimAPI_MakeCylinder(10.0, 30.0).Shape());

  DocumentManager manager;
  manager.create_document();
  const DocumentState document = manager.import_iges(path);

  const auto compiled = compile_bodies(document);
  if (!expect(compiled.bodies.size() == 1, "cylinder: exactly one body")) {
    return false;
  }
  const TopoDS_Shape shape = compiled.bodies.front().shape;
  const auto bounds = shape_bounds(shape);
  // MakeCylinder(r, h) sits on the XY plane (z 0..30). Loose bounds
  // tolerance for curved faces (same BndLib seam-split inflation seen
  // in the STEP suite — a bounding-box quirk, not a geometry defect;
  // the volume below is exact).
  return expect(count_faces(shape) == 3,
                "cylinder: two caps + one lateral face") &&
         near(bounds[2], 0.0, 0.06, "cylinder: zmin") &&
         near(bounds[5], 30.0, 0.06, "cylinder: zmax") &&
         near(shape_volume(shape), 3.141592653589793 * 100.0 * 30.0,
              3.141592653589793 * 100.0 * 30.0 * 0.02,
              "cylinder: volume");
}

bool test_serialization_round_trip() {
  const std::string path = write_box_iges("roundtrip");

  DocumentManager manager;
  manager.create_document();
  DocumentState document = manager.import_iges(path);

  const auto file_path = std::filesystem::temp_directory_path() /
                         "polysmith_iges_roundtrip_test.polysmith";
  manager.save_document_to_path(file_path.string());

  DocumentManager loaded_manager;
  loaded_manager.create_document();
  const DocumentState loaded =
      loaded_manager.load_document_from_path(file_path.string());

  const auto iges_it = std::find_if(
      loaded.feature_history.begin(), loaded.feature_history.end(),
      [](const auto& feature) { return feature.kind == "iges_import"; });
  if (!expect(iges_it != loaded.feature_history.end() &&
                  iges_it->iges_import_parameters.has_value() &&
                  iges_it->iges_import_parameters->file_path == path &&
                  !iges_it->iges_import_parameters->serialized_shape.empty(),
              "roundtrip: iges_import path + snapshot must survive")) {
    return false;
  }
  // The live handle is a cache — after load it is null and the
  // compiler must fall back to deserializing the snapshot.
  if (!expect(iges_it->iges_import_parameters->imported_shape.IsNull(),
              "roundtrip: live handle must be null after load")) {
    return false;
  }

  const auto compiled = compile_bodies(loaded);
  if (!expect(compiled.bodies.size() == 1, "roundtrip: one body") ||
      !expect(count_faces(compiled.bodies.front().shape) == 6,
              "roundtrip: 6 faces") ||
      !near(shape_volume(compiled.bodies.front().shape), 8000.0,
            8000.0 * 0.02, "roundtrip: volume")) {
    return false;
  }

  // Event payloads strip the opaque B-rep snapshot (the UI never reads
  // it); saved files keep it.
  const auto find_iges = [](const nlohmann::json& payload) {
    for (const auto& feature : payload["feature_history"]) {
      if (feature["kind"] == "iges_import") {
        return feature;
      }
    }
    return nlohmann::json();
  };
  const nlohmann::json ui_payload = polysmith::protocol::to_payload(document);
  const nlohmann::json full_payload =
      polysmith::protocol::to_payload(document, /*include_opaque=*/true);
  const nlohmann::json ui_feature = find_iges(ui_payload);
  const nlohmann::json full_feature = find_iges(full_payload);
  return expect(
      !ui_feature.is_null() &&
          ui_feature["iges_import_parameters"]["serialized_shape"]
                  .get<std::string>()
                  .empty() &&
          !full_feature.is_null() &&
          !full_feature["iges_import_parameters"]["serialized_shape"]
               .get<std::string>()
               .empty(),
      "roundtrip: event payloads must strip the snapshot; saved "
      "payloads must keep it");
}

bool test_undo_redo_after_import() {
  const std::string path = write_box_iges("undo");

  DocumentManager manager;
  manager.create_document();
  DocumentState document = manager.import_iges(path);

  const size_t imported_count = document.feature_history.size();

  document = manager.undo();
  if (!expect(document.feature_history.size() == imported_count - 1 &&
                  std::none_of(document.feature_history.begin(),
                               document.feature_history.end(),
                               [](const auto& feature) {
                                 return feature.kind == "iges_import";
                               }),
              "undo: import must be undone as one step")) {
    return false;
  }

  document = manager.redo();
  if (!expect(document.feature_history.size() == imported_count &&
                  std::any_of(document.feature_history.begin(),
                              document.feature_history.end(),
                              [](const auto& feature) {
                                return feature.kind == "iges_import";
                              }),
              "redo: import must be restored")) {
    return false;
  }
  const auto compiled = compile_bodies(document);
  return expect(compiled.bodies.size() == 1 &&
                    count_faces(compiled.bodies.front().shape) == 6,
                "redo: restored body must compile");
}

bool test_missing_file_throws_and_leaves_document_untouched() {
  DocumentManager manager;
  manager.create_document();
  const DocumentState before = manager.get_document().value();

  bool threw = false;
  try {
    manager.import_iges(temp_iges_path("does_not_exist"));
  } catch (const std::runtime_error&) {
    threw = true;
  }
  const DocumentState after = manager.get_document().value();
  return expect(threw, "missing: import must throw") &&
         expect(after.feature_history.size() == before.feature_history.size(),
                "missing: no feature added") &&
         expect(after.revision == before.revision,
                "missing: revision unchanged (parse-before-mutate)");
}

bool test_garbage_file_throws_and_leaves_document_untouched() {
  write_fixture("garbage", "this is not an iges file\n");

  DocumentManager manager;
  manager.create_document();
  const DocumentState before = manager.get_document().value();

  bool threw = false;
  try {
    manager.import_iges(temp_iges_path("garbage"));
  } catch (const std::runtime_error&) {
    threw = true;
  }
  const DocumentState after = manager.get_document().value();
  return expect(threw, "garbage: import must throw") &&
         expect(after.feature_history.size() == before.feature_history.size(),
                "garbage: no feature added") &&
         expect(after.revision == before.revision,
                "garbage: revision unchanged (parse-before-mutate)");
}

bool test_export_round_trip() {
  const std::string path = write_box_iges("export_source");

  DocumentManager manager;
  manager.create_document();
  const DocumentState document = manager.import_iges(path);

  const std::string out_path = temp_iges_path("export_out");
  const polysmith::core::ExportResult result =
      polysmith::core::export_document_as_iges(document, out_path);
  if (!expect(result.format == "iges", "export: format must be iges")) {
    return false;
  }

  // IGES has no magic header, so the strongest export assertion is a
  // re-import round-trip: the exported file must come back with the
  // same geometry.
  DocumentManager reimport_manager;
  reimport_manager.create_document();
  const DocumentState reimported = reimport_manager.import_iges(out_path);
  const auto compiled = compile_bodies(reimported);
  return expect(compiled.bodies.size() == 1 &&
                    count_faces(compiled.bodies.front().shape) == 6 &&
                    near(shape_volume(compiled.bodies.front().shape), 8000.0,
                         8000.0 * 0.02, "export: round-trip volume"),
                "export: re-imported file must carry the same geometry");
}

bool test_faces_only_file_sews_to_solid() {
  const std::string path = write_faces_box_iges("faces_box");

  DocumentManager manager;
  manager.create_document();
  DocumentState document = manager.import_iges(path);
  const std::string body_id = document.feature_history.back().id;

  // The reader must have sewn the surface model into a solid.
  const auto& feature = document.feature_history.back();
  if (!expect(feature.iges_import_parameters.has_value() &&
                  feature.iges_import_parameters->solid_count == 1,
              "sew: import must report one sewn solid")) {
    return false;
  }

  // And the sewn solid must accept a boolean — the regression the
  // solid-only guard would otherwise silently reject.
  document = manager.start_sketch_on_plane("ref-plane-xy");
  document = manager.add_sketch_rectangle(-20.0, -10.0, 20.0, 10.0);
  const auto& profiles =
      document.feature_history.back().sketch_parameters->profiles;
  if (!expect(profiles.size() == 1, "sew: fixture needs one profile")) {
    return false;
  }
  document = manager.extrude_profile(profiles.front().id, /*depth=*/5.0,
                                     /*mode=*/"cut", body_id,
                                     /*parameters=*/std::nullopt);

  const auto compiled = compile_bodies(document);
  return expect(compiled.bodies.size() == 1, "sew: one body after cut") &&
         near(shape_volume(compiled.bodies.front().shape), 4000.0,
              4000.0 * 0.02, "sew: cut must halve the sewn solid volume");
}

// Faces-mode file written from a REVERSED box: the face orientations
// come out inverted, so the sewn solid has negative volume unless the
// reader normalizes it (the real-world IGES orientation problem).
std::string write_reversed_faces_box_iges(const std::string& name) {
  const std::string path = temp_iges_path(name);
  IGESControl_Controller::Init();
  const int previous_mode = Interface_Static::IVal("write.iges.brep.mode");
  Interface_Static::SetIVal("write.iges.brep.mode", 0);
  IGESControl_Writer writer;  // reads the static in its constructor
  Interface_Static::SetIVal("write.iges.brep.mode", previous_mode);
  const TopoDS_Shape box =
      BRepPrimAPI_MakeBox(gp_Pnt(-20.0, -10.0, -5.0), 40.0, 20.0, 10.0)
          .Shape()
          .Reversed();
  if (!writer.AddShape(box)) {
    std::cerr << "fixture: reversed AddShape failed\n";
    return "";
  }
  writer.ComputeModel();
  if (!writer.Write(path.c_str())) {
    std::cerr << "fixture: reversed Write failed\n";
    return "";
  }
  return path;
}

bool test_inverted_faces_file_gets_reoriented() {
  const std::string path = write_reversed_faces_box_iges("reversed_box");

  DocumentManager manager;
  manager.create_document();
  const DocumentState document = manager.import_iges(path);

  const auto& feature = document.feature_history.back();
  if (!expect(feature.iges_import_parameters.has_value() &&
                  feature.iges_import_parameters->solid_count == 1,
              "inverted: import must report one sewn solid")) {
    return false;
  }

  const auto compiled = compile_bodies(document);
  if (!expect(compiled.bodies.size() == 1, "inverted: one body")) {
    return false;
  }
  // The solid must come out POSITIVE (the reader normalizes inverted
  // orientation — a negative volume breaks booleans downstream).
  return near(shape_volume(compiled.bodies.front().shape), 8000.0,
              8000.0 * 0.02, "inverted: volume must be positive and exact");
}

bool test_file_move_does_not_break_part() {
  const std::string path = write_box_iges("move_source");

  DocumentManager manager;
  manager.create_document();
  const DocumentState document = manager.import_iges(path);

  std::filesystem::remove(path);

  // Self-contained snapshot: the compile must still produce the body
  // without the source file (unlike mesh_import, which re-reads it).
  const auto compiled = compile_bodies(document);
  return expect(compiled.bodies.size() == 1 &&
                    count_faces(compiled.bodies.front().shape) == 6,
                "move: body must survive source-file deletion");
}

}  // namespace

int main(int argc, char** argv) {
  // `probe <path>` imports an arbitrary file and prints its topology —
  // used to diagnose real-world files without editing the suite.
  if (argc > 1 && std::string(argv[1]) == "probe" && argc > 2) {
    try {
      const polysmith::core::IgesImportResult result =
          polysmith::core::read_iges_file(argv[2]);
      std::cerr << "probe: solids=" << result.solid_count
                << " faces=" << result.face_count
                << " units=" << result.source_units
                << " volume=" << shape_volume(result.shape) << "\n";
      return 0;
    } catch (const std::exception& error) {
      std::cerr << "probe failed: " << error.what() << "\n";
      return 2;
    }
  }

  std::cerr << "[iges_import_export_test] test 1: box import creates body\n";
  if (!test_box_import_creates_body()) return 1;
  std::cerr << "[iges_import_export_test] test 2: inch file converts to mm\n";
  if (!test_inch_file_converts_to_mm()) return 1;
  std::cerr << "[iges_import_export_test] test 3: multi-solid stays one body\n";
  if (!test_multi_solid_stays_one_body()) return 1;
  std::cerr << "[iges_import_export_test] test 4: downstream cut extrude\n";
  if (!test_downstream_cut_extrude()) return 1;
  std::cerr << "[iges_import_export_test] test 5: cylinder curved faces\n";
  if (!test_cylinder_curved_faces()) return 1;
  std::cerr << "[iges_import_export_test] test 6: serialization round-trip\n";
  if (!test_serialization_round_trip()) return 1;
  std::cerr << "[iges_import_export_test] test 7: undo/redo\n";
  if (!test_undo_redo_after_import()) return 1;
  std::cerr << "[iges_import_export_test] test 8: missing file\n";
  if (!test_missing_file_throws_and_leaves_document_untouched()) return 1;
  std::cerr << "[iges_import_export_test] test 9: garbage file\n";
  if (!test_garbage_file_throws_and_leaves_document_untouched()) return 1;
  std::cerr << "[iges_import_export_test] test 10: export re-import round-trip\n";
  if (!test_export_round_trip()) return 1;
  std::cerr << "[iges_import_export_test] test 11: source-file deletion\n";
  if (!test_file_move_does_not_break_part()) return 1;
  std::cerr << "[iges_import_export_test] test 12: faces-only file sews to solid\n";
  if (!test_faces_only_file_sews_to_solid()) return 1;
  std::cerr << "[iges_import_export_test] test 13: inverted faces reoriented\n";
  if (!test_inverted_faces_file_gets_reoriented()) return 1;
  std::cout << "iges_import_export_test passed\n";
  return 0;
}
