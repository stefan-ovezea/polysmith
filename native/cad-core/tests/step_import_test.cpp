// Regression test for the STEP import feature:
//   - import_step (parse-once, self-contained B-rep snapshot body)
//   - unit conversion (file units -> mm on read)
//   - multi-solid files stay ONE compound body (CompiledBody.id ==
//     feature_id invariant)
//   - downstream parametric ops (extrude cut) on the imported body
//   - serialization round-trip incl. include_opaque gating
//   - undo/redo, parse-before-mutate error paths, re-export, and
//     independence from the source file after import.
//
// Fixtures follow the two in-repo conventions, both written to the
// temp directory at runtime:
//   (a) writer-generated STEP files (STEPControl_Writer, like the STL
//       import suite generates its own STLs), and
//   (b) a hand-assembled minimal AP203 ASCII fixture in INCHES that
//       exercises third-party file syntax + unit conversion.
//
// Note: the inch fixture is deliberately NOT produced by OCCT so that
// the reader is validated against external-file syntax (entity
// numbering, header/product structure, SI_UNIT(.INCH.) handling).

#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>

#include <BRepBndLib.hxx>
#include <BRepGProp.hxx>
#include <BRepPrimAPI_MakeBox.hxx>
#include <BRepPrimAPI_MakeCylinder.hxx>
#include <BRep_Builder.hxx>
#include <Bnd_Box.hxx>
#include <GProp_GProps.hxx>
#include <IFSelect_ReturnStatus.hxx>
#include <STEPControl_StepModelType.hxx>
#include <STEPControl_Writer.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Compound.hxx>
#include <gp_Pnt.hxx>

#include <nlohmann/json.hpp>

#include "core/document/document.h"
#include "core/export/export.h"
#include "core/geometry/body_compiler.h"
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

std::string temp_step_path(const std::string& name) {
  return (std::filesystem::temp_directory_path() /
          ("polysmith_step_import_" + name + ".step"))
      .string();
}

void write_fixture(const std::string& name, const std::string& content) {
  const auto path = std::filesystem::temp_directory_path() /
                    ("polysmith_step_import_" + name + ".step");
  std::ofstream out(path);
  out << content;
}

// --- fixture (a): writer-generated STEP files -------------------------

// Writes `shape` as a STEP file and returns the path.
std::string write_step(const std::string& name, const TopoDS_Shape& shape) {
  const std::string path = temp_step_path(name);
  STEPControl_Writer writer;
  const IFSelect_ReturnStatus status =
      writer.Transfer(shape, STEPControl_AsIs);
  if (status != IFSelect_RetDone) {
    std::cerr << "fixture: writer.Transfer failed\n";
    return "";
  }
  if (writer.Write(path.c_str()) != IFSelect_RetDone) {
    std::cerr << "fixture: writer.Write failed\n";
    return "";
  }
  return path;
}

// 40x20x10 box spanning z -5..5 (mid-height XY plane) and x/y
// -20..20 / -10..10.
std::string write_box_step(const std::string& name) {
  return write_step(name, BRepPrimAPI_MakeBox(gp_Pnt(-20.0, -10.0, -5.0),
                                              40.0, 20.0, 10.0)
                              .Shape());
}

// Two disjoint 10x10x10 boxes, written as two separate transfer roots
// so the reader exercises the multi-root -> OneShape compound path
// (the shape real multi-solid STEP exports take).
std::string write_two_solid_step(const std::string& name) {
  const std::string path = temp_step_path(name);
  STEPControl_Writer writer;
  if (writer.Transfer(BRepPrimAPI_MakeBox(gp_Pnt(0.0, 0.0, 0.0), 10.0, 10.0,
                                          10.0)
                          .Shape(),
                      STEPControl_AsIs) != IFSelect_RetDone) {
    std::cerr << "fixture: first transfer failed\n";
    return "";
  }
  if (writer.Transfer(BRepPrimAPI_MakeBox(gp_Pnt(20.0, 0.0, 0.0), 10.0, 10.0,
                                          10.0)
                          .Shape(),
                      STEPControl_AsIs) != IFSelect_RetDone) {
    std::cerr << "fixture: second transfer failed\n";
    return "";
  }
  if (writer.Write(path.c_str()) != IFSelect_RetDone) {
    std::cerr << "fixture: writer.Write failed\n";
    return "";
  }
  return path;
}

// Cylinder r10 h30 standing on the XY plane (curved-face coverage).
std::string write_cylinder_step(const std::string& name) {
  return write_step(name,
                    BRepPrimAPI_MakeCylinder(10.0, 30.0).Shape());
}

// --- fixture (b): hand-assembled minimal AP203 box in INCHES ----------
//
// A 1x1x1 inch box at the origin. Assembled entity-by-entity (the DXF
// fixture convention) so the reader is exercised against external-file
// syntax rather than OCCT's own writer output.
std::string make_inch_box_step() {
  int n = 0;
  auto next = [&n] { return ++n; };
  std::ostringstream os;
  os << std::fixed << std::setprecision(6);
  auto cartesian_point = [&](double x, double y, double z) {
    const int id = next();
    os << "#" << id << " = CARTESIAN_POINT('',(" << x << "," << y << ","
       << z << "));\n";
    return id;
  };
  auto direction = [&](double x, double y, double z) {
    const int id = next();
    os << "#" << id << " = DIRECTION('',(" << x << "," << y << "," << z
       << "));\n";
    return id;
  };

  os << "ISO-10303-21;\n";
  os << "HEADER;\n";
  os << "FILE_DESCRIPTION(('a minimal inch box'),'2;1');\n";
  os << "FILE_NAME('inch_box.step','2026-08-22T00:00:00',(''),(''),"
        "'','Polysmith test','');\n";
  os << "FILE_SCHEMA(('CONFIG_CONTROL_DESIGN'));\n";
  os << "ENDSEC;\n";
  os << "DATA;\n";

  // 8 corner points: bottom z=0 then top z=1, index 0..7.
  const double corners[8][3] = {
      {0, 0, 0}, {1, 0, 0}, {1, 1, 0}, {0, 1, 0},
      {0, 0, 1}, {1, 0, 1}, {1, 1, 1}, {0, 1, 1},
  };
  int points[8];
  int vertices[8];
  for (int i = 0; i < 8; ++i) {
    points[i] = cartesian_point(corners[i][0], corners[i][1], corners[i][2]);
    vertices[i] = next();
    os << "#" << vertices[i] << " = VERTEX_POINT('',#" << points[i]
       << ");\n";
  }

  // 12 edges, canonical direction start -> end (see the face loops
  // below for the orientation flags each face uses).
  const struct {
    int start;
    int end;
  } edges[12] = {
      {0, 1}, {1, 2}, {2, 3}, {3, 0},   // bottom ring
      {4, 5}, {5, 6}, {6, 7}, {7, 4},   // top ring
      {0, 4}, {1, 5}, {2, 6}, {3, 7},   // verticals
  };
  int edge_ids[12];
  for (int i = 0; i < 12; ++i) {
    const double* a = corners[edges[i].start];
    const double* b = corners[edges[i].end];
    const int origin =
        cartesian_point(a[0], a[1], a[2]);
    const int dir =
        direction(b[0] - a[0], b[1] - a[1], b[2] - a[2]);
    const int vec = next();
    os << "#" << vec << " = VECTOR('',#" << dir << ",1.);\n";
    const int line = next();
    os << "#" << line << " = LINE('',#" << origin << ",#" << vec << ");\n";
    const int curve = next();
    // No pcurves — the reader re-projects (third-party files omit them
    // all the time).
    os << "#" << curve << " = SURFACE_CURVE('',#" << line
       << ",(),.PCURVE_S1.);\n";
    edge_ids[i] = next();
    os << "#" << edge_ids[i] << " = EDGE_CURVE('',#" << vertices[edges[i].start]
       << ",#" << vertices[edges[i].end] << ",#" << curve << ",.T.);\n";
  }

  // 6 faces. Each face: plane (axis = outward normal), loop of 4
  // oriented edges (CCW viewed from outside), shell inclusion.
  // Loop edges as {edge index, forward flag}.
  const struct {
    double px, py, pz;          // plane origin
    double ax, ay, az;          // outward normal
    int loop[4][2];             // {edge index, .T./.F. flag}
  } faces[6] = {
      // bottom (z=0, normal -Z): loop V1->V4->V3->V2 is CW from +Z,
      // so the closing V2->V1 segment reverses edge 0 (V1->V2).
      {0, 0, 0, 0, 0, -1, {{3, 0}, {2, 0}, {1, 0}, {0, 0}}},
      // top (z=1, normal +Z)
      {0, 0, 1, 0, 0, 1, {{4, 1}, {5, 1}, {6, 1}, {7, 1}}},
      // front (y=0, normal -Y)
      {0, 0, 0, 0, -1, 0, {{0, 1}, {9, 1}, {4, 0}, {8, 0}}},
      // back (y=1, normal +Y)
      {0, 1, 0, 0, 1, 0, {{11, 1}, {6, 0}, {10, 0}, {2, 0}}},
      // left (x=0, normal -X)
      {0, 0, 0, -1, 0, 0, {{8, 1}, {7, 0}, {11, 0}, {3, 1}}},
      // right (x=1, normal +X)
      {1, 0, 0, 1, 0, 0, {{1, 1}, {10, 1}, {5, 0}, {9, 0}}},
  };
  int face_ids[6];
  for (int i = 0; i < 6; ++i) {
    const auto& f = faces[i];
    // The point/direction helpers write their own entity lines, so
    // capture their ids first, then compose the axis line.
    const int axis_point = cartesian_point(f.px, f.py, f.pz);
    const int axis_dir = direction(f.ax, f.ay, f.az);
    const int axis = next();
    os << "#" << axis << " = AXIS2_PLACEMENT_3D('',#" << axis_point << ",#"
       << axis_dir << ",$);\n";
    const int plane = next();
    os << "#" << plane << " = PLANE('',#" << axis << ");\n";
    const int loop = next();
    os << "#" << loop << " = EDGE_LOOP('',(";
    for (int k = 0; k < 4; ++k) {
      const int oriented = next();
      os << "#" << oriented;
      if (k < 3) os << ",";
    }
    os << "));\n";
    // Now emit the oriented edges in order (ids were reserved above in
    // the same order, so the refs line up with the loop list).
    int ref = loop + 1;  // first oriented-edge id reserved above
    for (int k = 0; k < 4; ++k) {
      os << "#" << ref++ << " = ORIENTED_EDGE('',*,*,#"
         << edge_ids[f.loop[k][0]] << "," << (f.loop[k][1] ? ".T." : ".F.")
         << ");\n";
    }
    const int bound = next();
    os << "#" << bound << " = FACE_BOUND('',#" << loop << ",.T.);\n";
    face_ids[i] = next();
    os << "#" << face_ids[i] << " = ADVANCED_FACE('',(#" << bound
       << "),#" << plane << ",.T.);\n";
  }

  const int shell = next();
  os << "#" << shell << " = CLOSED_SHELL('',(";
  for (int i = 0; i < 6; ++i) {
    os << "#" << face_ids[i];
    if (i < 5) os << ",";
  }
  os << "));\n";
  const int solid = next();
  os << "#" << solid << " = MANIFOLD_SOLID_BREP('',#" << shell << ");\n";

  // Units + context (INCHES — the point of this fixture). Inches are
  // not an SI unit: AP203 expresses them as a CONVERSION_BASED_UNIT
  // whose measure references the millimetre SI unit — the ISO-correct
  // form, and exactly what OCCT's own writer emits for INCH.
  const int si_unit = next();
  os << "#" << si_unit
     << " = ( LENGTH_UNIT() NAMED_UNIT(*) SI_UNIT(.MILLI.,.METRE.) );\n";
  const int dims = next();
  os << "#" << dims << " = DIMENSIONAL_EXPONENTS(1.,0.,0.,0.,0.,0.,0.);\n";
  const int measure = next();
  os << "#" << measure << " = LENGTH_MEASURE_WITH_UNIT(LENGTH_MEASURE(25.4),#"
     << si_unit << ");\n";
  const int length_unit = next();
  // Complex-entity order: subtype components first, common supertype
  // (NAMED_UNIT) last — the OCCT reader parses them in that order.
  os << "#" << length_unit << " = ( CONVERSION_BASED_UNIT('INCH',#" << measure
     << ") LENGTH_UNIT() NAMED_UNIT(#" << dims << ") );\n";
  const int angle_unit = next();
  os << "#" << angle_unit
     << " = ( NAMED_UNIT(*) PLANE_ANGLE_UNIT() SI_UNIT($,.RADIAN.) );\n";
  const int uncertainty = next();
  os << "#" << uncertainty
     << " = UNCERTAINTY_MEASURE_WITH_UNIT(LENGTH_MEASURE(1.E-07),#"
     << length_unit << ",'distance_accuracy_value','confusion accuracy');\n";
  const int context = next();
  os << "#" << context
     << " = ( GEOMETRIC_REPRESENTATION_CONTEXT(3) "
        "GLOBAL_UNCERTAINTY_ASSIGNED_CONTEXT((#"
     << uncertainty << ")) GLOBAL_UNIT_ASSIGNED_CONTEXT((#" << length_unit
     << ",#" << angle_unit
     << ")) REPRESENTATION_CONTEXT('Context #1','3D Context with UNIT and "
        "UNCERTAINTY') );\n";
  const int adv_brep = next();
  os << "#" << adv_brep << " = ADVANCED_BREP_SHAPE_REPRESENTATION('',(#"
     << solid << "),#" << context << ");\n";

  // Minimal AP203 product structure pointing at the representation.
  const int app_context = next();
  os << "#" << app_context
     << " = APPLICATION_CONTEXT('configuration controlled 3d designs of "
        "mechanical parts and assemblies');\n";
  const int app_protocol = next();
  os << "#" << app_protocol
     << " = APPLICATION_PROTOCOL_DEFINITION('international "
        "standard','config_control_design',1994,#"
     << app_context << ");\n";
  const int product_context = next();
  os << "#" << product_context << " = PRODUCT_CONTEXT('',#" << app_context
     << ",'mechanical');\n";
  const int product = next();
  os << "#" << product << " = PRODUCT('inch_box','inch_box','',(#"
     << product_context << "));\n";
  const int formation = next();
  os << "#" << formation << " = PRODUCT_DEFINITION_FORMATION('','',#"
     << product << ");\n";
  const int definition = next();
  os << "#" << definition << " = PRODUCT_DEFINITION('design','',#"
     << formation << ",#" << product_context << ");\n";
  const int definition_shape = next();
  os << "#" << definition_shape << " = PRODUCT_DEFINITION_SHAPE('','',#"
     << definition << ");\n";
  const int shape_definition = next();
  os << "#" << shape_definition << " = SHAPE_DEFINITION_REPRESENTATION(#"
     << definition_shape << ",#" << adv_brep << ");\n";

  os << "ENDSEC;\n";
  os << "END-ISO-10303-21;\n";
  return os.str();
}

// --- tests -------------------------------------------------------------

bool test_box_import_creates_body() {
  const std::string path = write_box_step("box");

  DocumentManager manager;
  manager.create_document();
  const DocumentState document = manager.import_step(path);

  const auto& feature = document.feature_history.back();
  if (!expect(feature.kind == "step_import" &&
                  feature.step_import_parameters.has_value() &&
                  feature.step_import_parameters->file_path == path &&
                  !feature.step_import_parameters->serialized_shape.empty(),
              "import: feature must be step_import with path + snapshot")) {
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

bool test_inch_fixture_converts_to_mm() {
  write_fixture("inch_box", make_inch_box_step());

  DocumentManager manager;
  manager.create_document();
  const DocumentState document = manager.import_step(temp_step_path("inch_box"));

  const auto& feature = document.feature_history.back();
  if (!expect(feature.step_import_parameters.has_value() &&
                  !feature.step_import_parameters->source_units.empty(),
              "inch: feature must carry the source units")) {
    return false;
  }
  if (!expect(feature.step_import_parameters->source_units.find("INCH") !=
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
  // 1 inch = 25.4 mm; the box corner stays at the origin.
  return near(bounds[0], 0.0, 1e-3, "inch: xmin") &&
         near(bounds[1], 0.0, 1e-3, "inch: ymin") &&
         near(bounds[2], 0.0, 1e-3, "inch: zmin") &&
         near(bounds[3], 25.4, 1e-3, "inch: xmax") &&
         near(bounds[4], 25.4, 1e-3, "inch: ymax") &&
         near(bounds[5], 25.4, 1e-3, "inch: zmax");
}

bool test_multi_solid_stays_one_body() {
  const std::string path = write_two_solid_step("two_solids");

  DocumentManager manager;
  manager.create_document();
  const DocumentState document = manager.import_step(path);

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
  const std::string path = write_box_step("cut");

  DocumentManager manager;
  manager.create_document();
  DocumentState document = manager.import_step(path);
  const std::string body_id = document.feature_history.back().id;

  document = manager.start_sketch_on_plane("ref-plane-xy");
  document = manager.add_sketch_rectangle(-20.0, -10.0, 20.0, 10.0);
  const auto& profiles =
      document.feature_history.back().sketch_parameters->profiles;
  if (!expect(profiles.size() == 1, "cut: fixture needs one profile")) {
    return false;
  }
  // Cut the top half (z 0..5) of the imported box through the normal
  // parametric extrude path — this is the regression-risk area: the
  // imported body must behave as a boolean target like any other body.
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
  const std::string path = write_cylinder_step("cylinder");

  DocumentManager manager;
  manager.create_document();
  const DocumentState document = manager.import_step(path);

  const auto compiled = compile_bodies(document);
  if (!expect(compiled.bodies.size() == 1, "cylinder: exactly one body")) {
    return false;
  }
  const TopoDS_Shape shape = compiled.bodies.front().shape;
  const auto bounds = shape_bounds(shape);
  // MakeCylinder(r, h) sits on the XY plane (z 0..30). Note the loose
  // bounds tolerance: BndLib inflates the box of a seam-split
  // cylindrical face by ~0.5% of the radius (verified: all transferred
  // vertices sit exactly on the ideal seam and the volume below is
  // exact, so this is a bounding-box quirk, not a geometry defect).
  return expect(count_faces(shape) == 3,
                "cylinder: two caps + one lateral face") &&
         near(bounds[2], 0.0, 0.06, "cylinder: zmin") &&
         near(bounds[5], 30.0, 0.06, "cylinder: zmax") &&
         near(shape_volume(shape), 3.141592653589793 * 100.0 * 30.0,
              3.141592653589793 * 100.0 * 30.0 * 0.02,
              "cylinder: volume");
}

bool test_serialization_round_trip() {
  const std::string path = write_box_step("roundtrip");

  DocumentManager manager;
  manager.create_document();
  DocumentState document = manager.import_step(path);

  const auto file_path = std::filesystem::temp_directory_path() /
                         "polysmith_step_roundtrip_test.polysmith";
  manager.save_document_to_path(file_path.string());

  DocumentManager loaded_manager;
  loaded_manager.create_document();
  const DocumentState loaded =
      loaded_manager.load_document_from_path(file_path.string());

  const auto step_it = std::find_if(
      loaded.feature_history.begin(), loaded.feature_history.end(),
      [](const auto& feature) { return feature.kind == "step_import"; });
  if (!expect(step_it != loaded.feature_history.end() &&
                  step_it->step_import_parameters.has_value() &&
                  step_it->step_import_parameters->file_path == path &&
                  !step_it->step_import_parameters->serialized_shape.empty(),
              "roundtrip: step_import path + snapshot must survive")) {
    return false;
  }
  // The live handle is a cache — after load it is null and the
  // compiler must fall back to deserializing the snapshot.
  if (!expect(step_it->step_import_parameters->imported_shape.IsNull(),
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
  const auto find_step = [](const nlohmann::json& payload) {
    for (const auto& feature : payload["feature_history"]) {
      if (feature["kind"] == "step_import") {
        return feature;
      }
    }
    return nlohmann::json();
  };
  const nlohmann::json ui_payload = polysmith::protocol::to_payload(document);
  const nlohmann::json full_payload =
      polysmith::protocol::to_payload(document, /*include_opaque=*/true);
  const nlohmann::json ui_feature = find_step(ui_payload);
  const nlohmann::json full_feature = find_step(full_payload);
  return expect(
      !ui_feature.is_null() &&
          ui_feature["step_import_parameters"]["serialized_shape"]
                  .get<std::string>()
                  .empty() &&
          !full_feature.is_null() &&
          !full_feature["step_import_parameters"]["serialized_shape"]
               .get<std::string>()
               .empty(),
      "roundtrip: event payloads must strip the snapshot; saved "
      "payloads must keep it");
}

bool test_undo_redo_after_import() {
  const std::string path = write_box_step("undo");

  DocumentManager manager;
  manager.create_document();
  DocumentState document = manager.import_step(path);

  const size_t imported_count = document.feature_history.size();

  document = manager.undo();
  if (!expect(document.feature_history.size() == imported_count - 1 &&
                  std::none_of(document.feature_history.begin(),
                               document.feature_history.end(),
                               [](const auto& feature) {
                                 return feature.kind == "step_import";
                               }),
              "undo: import must be undone as one step")) {
    return false;
  }

  document = manager.redo();
  if (!expect(document.feature_history.size() == imported_count &&
                  std::any_of(document.feature_history.begin(),
                              document.feature_history.end(),
                              [](const auto& feature) {
                                return feature.kind == "step_import";
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
    manager.import_step(temp_step_path("does_not_exist"));
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
  write_fixture("garbage", "this is not a step file\n");

  DocumentManager manager;
  manager.create_document();
  const DocumentState before = manager.get_document().value();

  bool threw = false;
  try {
    manager.import_step(temp_step_path("garbage"));
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

bool test_reexport_starts_with_header() {
  const std::string path = write_box_step("reexport");

  DocumentManager manager;
  manager.create_document();
  const DocumentState document = manager.import_step(path);

  const std::string out_path = temp_step_path("reexport_out");
  polysmith::core::export_document_as_step(document, out_path);

  std::ifstream in(out_path, std::ios::binary);
  std::string head(12, '\0');
  in.read(head.data(), 12);
  return expect(head == "ISO-10303-21",
                "reexport: file must start with the STEP header");
}

bool test_file_move_does_not_break_part() {
  const std::string path = write_box_step("move_source");

  DocumentManager manager;
  manager.create_document();
  const DocumentState document = manager.import_step(path);

  std::filesystem::remove(path);

  // Self-contained snapshot: the compile must still produce the body
  // without the source file (unlike mesh_import, which re-reads it).
  const auto compiled = compile_bodies(document);
  return expect(compiled.bodies.size() == 1 &&
                    count_faces(compiled.bodies.front().shape) == 6,
                "move: body must survive source-file deletion");
}

}  // namespace

int main() {
  std::cerr << "[step_import_test] test 1: box import creates body\n";
  if (!test_box_import_creates_body()) return 1;
  std::cerr << "[step_import_test] test 2: inch fixture converts to mm\n";
  if (!test_inch_fixture_converts_to_mm()) return 1;
  std::cerr << "[step_import_test] test 3: multi-solid stays one body\n";
  if (!test_multi_solid_stays_one_body()) return 1;
  std::cerr << "[step_import_test] test 4: downstream cut extrude\n";
  if (!test_downstream_cut_extrude()) return 1;
  std::cerr << "[step_import_test] test 5: cylinder curved faces\n";
  if (!test_cylinder_curved_faces()) return 1;
  std::cerr << "[step_import_test] test 6: serialization round-trip\n";
  if (!test_serialization_round_trip()) return 1;
  std::cerr << "[step_import_test] test 7: undo/redo\n";
  if (!test_undo_redo_after_import()) return 1;
  std::cerr << "[step_import_test] test 8: missing file\n";
  if (!test_missing_file_throws_and_leaves_document_untouched()) return 1;
  std::cerr << "[step_import_test] test 9: garbage file\n";
  if (!test_garbage_file_throws_and_leaves_document_untouched()) return 1;
  std::cerr << "[step_import_test] test 10: re-export header\n";
  if (!test_reexport_starts_with_header()) return 1;
  std::cerr << "[step_import_test] test 11: source-file deletion\n";
  if (!test_file_move_does_not_break_part()) return 1;
  std::cout << "step_import_test passed\n";
  return 0;
}
