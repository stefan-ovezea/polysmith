// CAM document mutator test.
//
// Exercises the DocumentManager CAM CRUD methods against the
// target-schema types: setup, stock, tool library, and operations.
// Also proves error paths throw human-readable messages and leave the
// document untouched, ids are assigned monotonically, and undo/redo
// covers CAM mutations.

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <variant>

#include "core/document/document.h"
#include "core/cam/cam_operation.h"
#include "core/cam/cam_profile_reference.h"
#include "core/geometry/body_compiler.h"
#include "protocol/serialization.h"

#include <BRepAdaptor_Curve.hxx>
#include <BRep_Tool.hxx>
#include <GeomAbs_CurveType.hxx>
#include <NCollection_IndexedMap.hxx>
#include <TopExp.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Edge.hxx>
#include <TopoDS_Vertex.hxx>
#include <TopTools_ShapeMapHasher.hxx>

namespace {

using polysmith::core::CamOperation;
using polysmith::core::CamSetup;
using polysmith::core::DocumentManager;
using polysmith::core::DocumentState;
using polysmith::core::LaserCutParameters;
using polysmith::core::SketchProfileAttestation;
using polysmith::core::GeometryReference;
using polysmith::core::ToolEntry;

bool expect(bool condition, const char* message) {
  if (condition) {
    return true;
  }
  std::cerr << "FAIL: " << message << "\n";
  return false;
}

bool near(double a, double b, double tolerance = 1e-6) {
  return std::abs(a - b) < tolerance;
}

CamSetup make_setup(const std::string& machine = "3_axis_mill") {
  CamSetup setup;
  setup.name = "Setup 1";
  setup.machine_type = machine;
  return setup;
}

ToolEntry make_tool(const std::string& type = "endmill_flat") {
  ToolEntry tool;
  tool.name = "6mm endmill";
  tool.type = type;
  return tool;
}

CamOperation make_op(const std::string& type, const std::string& toolId) {
  CamOperation op;
  op.name = "Op";
  op.type = type;
  op.tool_id = toolId;
  return op;
}

bool test_setup_crud() {
  DocumentManager manager;
  manager.create_document();

  const DocumentState created = manager.cam_setup_create(make_setup());
  if (!expect(created.cam.setups.size() == 1, "setup create: one setup")) {
    return false;
  }
  if (!expect(created.cam.setups[0].setup_id == "cam-setup-1",
              "setup create: id assigned from counter")) {
    return false;
  }

  const auto got = manager.cam_setup_get();
  if (!expect(got.has_value() && got->setup_id == "cam-setup-1" &&
                  got->machine_type == "3_axis_mill",
              "setup get: returns the stored setup")) {
    return false;
  }

  CamSetup updated = got.value();
  updated.machine_type = "laser";
  const DocumentState after = manager.cam_setup_update(updated);
  if (!expect(after.cam.setups.size() == 1 &&
                  after.cam.setups[0].machine_type == "laser",
              "setup update: replaces the stored setup")) {
    return false;
  }

  // Unknown setup id must throw.
  bool threw = false;
  CamSetup missing = make_setup();
  missing.setup_id = "cam-setup-99";
  try {
    manager.cam_setup_update(missing);
  } catch (const std::runtime_error& error) {
    threw = std::string(error.what()).find("not found") != std::string::npos;
  }
  if (!expect(threw, "setup update: unknown id throws")) {
    return false;
  }

  return true;
}

bool test_stock_access() {
  DocumentManager manager;
  manager.create_document();
  CamSetup setup = make_setup("laser");
  setup.stock.type = "bounding_box";
  setup.stock.size = std::array<double, 3>{300.0, 200.0, 3.0};
  setup.stock.origin = std::array<double, 3>{-150.0, -100.0, 0.0};
  const DocumentState created = manager.cam_setup_create(setup);

  const auto got = manager.cam_setup_get();
  return expect(got.has_value() && got->stock.type == "bounding_box" &&
                    got->stock.size.has_value() &&
                    got->stock.size.value()[0] == 300.0 &&
                    got->stock.origin.has_value() &&
                    got->stock.origin.value()[1] == -100.0,
                "stock: stored on the setup and readable back");
}

bool test_tool_library_crud() {
  DocumentManager manager;
  manager.create_document();
  manager.cam_setup_create(make_setup("laser"));

  const DocumentState withLaser = manager.cam_tool_add(make_tool("laser"));
  if (!expect(withLaser.cam.tool_library.size() == 1 &&
                  withLaser.cam.tool_library[0].tool_id == "tool-1",
              "tool add: id assigned from counter")) {
    return false;
  }
  const DocumentState withMill = manager.cam_tool_add(make_tool("endmill_flat"));
  if (!expect(withMill.cam.tool_library.size() == 2 &&
                  withMill.cam.tool_library[1].tool_id == "tool-2",
              "tool add: second id continues the counter")) {
    return false;
  }

  ToolEntry renamed = withMill.cam.tool_library[1];
  renamed.name = "6mm flat endmill";
  const DocumentState updated = manager.cam_tool_update("tool-2", renamed);
  if (!expect(updated.cam.tool_library[1].name == "6mm flat endmill",
              "tool update: replaces by id")) {
    return false;
  }

  const auto listed = manager.cam_tool_list();
  if (!expect(listed.size() == 2, "tool list: returns the library")) {
    return false;
  }

  // Errors: unknown tool type and duplicate id must throw.
  bool badType = false;
  ToolEntry invalid = make_tool("wrench");
  try {
    manager.cam_tool_add(invalid);
  } catch (const std::runtime_error& error) {
    badType = std::string(error.what()).find("Unknown tool type") !=
              std::string::npos;
  }
  if (!expect(badType, "tool add: unknown type throws")) {
    return false;
  }

  bool duplicate = false;
  ToolEntry clash = make_tool();
  clash.tool_id = "tool-1";
  try {
    manager.cam_tool_add(clash);
  } catch (const std::runtime_error& error) {
    duplicate = std::string(error.what()).find("already exists") !=
                std::string::npos;
  }
  if (!expect(duplicate, "tool add: duplicate id throws")) {
    return false;
  }

  bool missing = false;
  try {
    manager.cam_tool_delete("tool-99");
  } catch (const std::runtime_error& error) {
    missing = std::string(error.what()).find("not found") !=
              std::string::npos;
  }
  if (!expect(missing, "tool delete: unknown id throws")) {
    return false;
  }

  const DocumentState afterDelete = manager.cam_tool_delete("tool-1");
  return expect(afterDelete.cam.tool_library.size() == 1,
                "tool delete: removes the tool");
}

bool test_operation_add_validates_tool() {
  DocumentManager manager;
  manager.create_document();
  manager.cam_setup_create(make_setup());

  // No tool library yet — any operation must fail.
  bool threw = false;
  try {
    manager.cam_operation_add(make_op("face_milling", "tool-99"));
  } catch (const std::runtime_error& error) {
    threw = std::string(error.what()).find("Unknown tool") !=
            std::string::npos;
  }
  return expect(threw, "op add: unknown tool throws");
}

bool test_laser_operation_requires_laser_machine() {
  DocumentManager manager;
  manager.create_document();
  manager.cam_setup_create(make_setup("3_axis_mill"));
  const DocumentState doc = manager.cam_tool_add(make_tool("laser"));
  const std::string laserToolId = doc.cam.tool_library[0].tool_id;

  bool threw = false;
  try {
    manager.cam_operation_add(make_op("laser_cut", laserToolId));
  } catch (const std::runtime_error& error) {
    threw = std::string(error.what()).find("laser") != std::string::npos;
  }
  if (!expect(threw, "laser op on a mill machine throws")) {
    return false;
  }

  // Same operation on a laser setup succeeds.
  manager.cam_setup_update([&] {
    CamSetup laserSetup = make_setup("laser");
    laserSetup.setup_id = "cam-setup-1";
    return laserSetup;
  }());
  const DocumentState created =
      manager.cam_operation_add(make_op("laser_cut", laserToolId));
  return expect(created.cam.operations.size() == 1 &&
                    created.cam.operations[0].op_id == "cam-op-1" &&
                    created.cam.operations[0].status == "pending",
                "laser op on a laser machine succeeds with assigned id");
}

bool test_operation_update_delete_undo() {
  DocumentManager manager;
  manager.create_document();
  manager.cam_setup_create(make_setup("laser"));
  DocumentState doc = manager.cam_tool_add(make_tool("laser"));
  const std::string laserToolId = doc.cam.tool_library[0].tool_id;

  CamOperation op = make_op("laser_cut", laserToolId);
  op.parameters.laser = LaserCutParameters{};
  doc = manager.cam_operation_add(op);
  if (!expect(doc.cam.operations.size() == 1, "op add: one operation")) {
    return false;
  }

  const DocumentState beforeUndo = doc;
  doc = manager.undo();
  if (!expect(doc.cam.operations.empty(), "undo: removes the operation")) {
    return false;
  }
  doc = manager.redo();
  if (!expect(doc.cam.operations.size() == 1, "redo: restores the operation")) {
    return false;
  }

  CamOperation updated = doc.cam.operations[0];
  updated.parameters.laser = LaserCutParameters{};
  updated.parameters.laser->power_percent = 60.0;
  doc = manager.cam_operation_update("cam-op-1", updated);
  if (!expect(doc.cam.operations[0].parameters.laser.has_value() &&
                  doc.cam.operations[0].parameters.laser->power_percent == 60.0,
              "op update: parameters replaced")) {
    return false;
  }
  if (!expect(doc.cam.operations[0].status == "needs_regenerate",
              "op update: status resets to needs_regenerate")) {
    return false;
  }

  doc = manager.cam_operation_delete("cam-op-1");
  if (!expect(doc.cam.operations.empty(), "op delete: removes the operation")) {
    return false;
  }

  bool missing = false;
  try {
    manager.cam_operation_delete("cam-op-1");
  } catch (const std::runtime_error& error) {
    missing = std::string(error.what()).find("not found") !=
              std::string::npos;
  }
  return expect(missing, "op delete: unknown id throws");
}

bool test_operation_add_creates_default_tool() {
  DocumentManager manager;
  manager.create_document();
  CamSetup setup = make_setup("laser");
  manager.cam_setup_create(setup);

  // First laser operation with no tool_id: a default laser tool is
  // created on the spot.
  CamOperation op;
  op.name = "2D Cut";
  op.type = "laser_cut";
  const DocumentState created = manager.cam_operation_add(op);
  if (!expect(created.cam.operations.size() == 1 &&
                  !created.cam.operations[0].tool_id.empty() &&
                  created.cam.tool_library.size() == 1 &&
                  created.cam.tool_library[0].type == "laser",
              "default tool: laser tool auto-created")) {
    return false;
  }

  // A second operation with no tool_id reuses the library tool.
  CamOperation second;
  second.name = "2D Cut 2";
  second.type = "laser_cut";
  const DocumentState afterSecond = manager.cam_operation_add(second);
  if (!expect(afterSecond.cam.tool_library.size() == 1 &&
                  afterSecond.cam.operations[1].tool_id ==
                      afterSecond.cam.tool_library[0].tool_id,
              "default tool: existing tool reused")) {
    return false;
  }

  // One undo step removes the operation AND its default tool.
  const DocumentState undone = manager.undo();
  if (!expect(undone.cam.operations.size() == 1,
              "default tool: undo removes the operation")) {
    return false;
  }
  const DocumentState undoneTwice = manager.undo();
  return expect(undoneTwice.cam.operations.empty() &&
                    undoneTwice.cam.tool_library.empty(),
                "default tool: undo removes the auto-created tool too");
}

bool test_id_counters_survive_reload() {
  DocumentManager manager;
  manager.create_document();
  CamSetup setup = make_setup();
  setup.setup_id = "cam-setup-7";
  manager.cam_setup_create(setup);
  DocumentState doc = manager.cam_tool_add(make_tool());
  const std::string toolId = doc.cam.tool_library[0].tool_id;  // "tool-1"
  CamOperation op = make_op("face_milling", toolId);
  op.op_id = "cam-op-23";
  manager.cam_operation_add(op);

  // Serialize + reload through the protocol path, then create new
  // entities — their ids must not collide with the loaded ones.
  const auto payload =
      polysmith::protocol::to_payload(manager.get_document().value(), true);

  DocumentManager reloaded;
  reloaded.create_document();
  // Reuse the same load path the app takes for files.
  const auto path = std::filesystem::temp_directory_path() /
                    "polysmith_cam_ids_reload_test.json";
  {
    std::ofstream stream(path.string());
    stream << payload.dump(2);
  }
  const DocumentState loaded = reloaded.load_document_from_path(path.string());
  if (!expect(loaded.cam.operations.size() == 1 &&
                  loaded.cam.operations[0].op_id == "cam-op-23",
              "reload: explicit op id restored")) {
    return false;
  }

  const DocumentState added = reloaded.cam_operation_add(make_op("face_milling", toolId));
  if (!expect(added.cam.operations.size() == 2 &&
                  added.cam.operations[1].op_id == "cam-op-24",
              "reload: new op id continues past the loaded maximum")) {
    return false;
  }
  const DocumentState setupAdded = reloaded.cam_setup_create(make_setup());
  if (!expect(setupAdded.cam.setups.size() == 2 &&
                  setupAdded.cam.setups[1].setup_id == "cam-setup-8",
              "reload: new setup id continues past the loaded maximum")) {
    return false;
  }
  const DocumentState toolAdded = reloaded.cam_tool_add(make_tool());
  if (!expect(toolAdded.cam.tool_library.size() == 2 &&
                  toolAdded.cam.tool_library[1].tool_id == "tool-2",
              "reload: new tool id continues past the loaded maximum")) {
    return false;
  }
  return true;
}

bool test_operation_set_scope_sketch() {
  DocumentManager manager;
  manager.create_document();
  manager.cam_setup_create(make_setup("laser"));
  DocumentState doc = manager.cam_tool_add(make_tool("laser"));
  const std::string laserToolId = doc.cam.tool_library[0].tool_id;

  // A sketch with two closed rectangles: two profile regions.
  manager.start_sketch_on_plane("ref-plane-xy");
  doc = manager.add_sketch_rectangle(0.0, 0.0, 20.0, 10.0);
  doc = manager.add_sketch_rectangle(30.0, 0.0, 50.0, 10.0);

  std::string sketchId;
  for (const auto& feature : doc.feature_history) {
    if (feature.kind == "sketch") {
      sketchId = feature.id;
      break;
    }
  }
  if (!expect(!sketchId.empty(), "scope fixture: sketch feature exists")) {
    return false;
  }

  doc = manager.cam_operation_add(make_op("laser_cut", laserToolId));
  if (!expect(doc.cam.operations.size() == 1 &&
                  doc.cam.operations[0]
                      .geometry_references.machining_regions.empty(),
              "scope fixture: op starts with no regions")) {
    return false;
  }

  doc = manager.cam_operation_set_scope("cam-op-1", sketchId);
  const auto& regions =
      doc.cam.operations[0].geometry_references.machining_regions;
  if (!expect(regions.size() == 2,
              "set scope: both profile regions captured")) {
    return false;
  }
  bool allSketch = true;
  for (const auto& region : regions) {
    if (!std::holds_alternative<SketchProfileAttestation>(
            region.attestation)) {
      allSketch = false;
      break;
    }
    const auto& attestation =
        std::get<SketchProfileAttestation>(region.attestation);
    if (attestation.sketch_feature_id != sketchId) {
      allSketch = false;
      break;
    }
  }
  if (!expect(allSketch,
              "set scope: every region attests the target sketch")) {
    return false;
  }
  if (!expect(doc.cam.operations[0].status == "needs_regenerate",
              "set scope: status resets to needs_regenerate")) {
    return false;
  }

  // One undo step restores the previous (empty) geometry.
  const DocumentState undone = manager.undo();
  return expect(undone.cam.operations[0]
                    .geometry_references.machining_regions.empty(),
                "set scope: undo restores the previous regions");
}

bool test_operation_set_scope_errors() {
  DocumentManager manager;
  manager.create_document();
  manager.cam_setup_create(make_setup("laser"));
  DocumentState doc = manager.cam_tool_add(make_tool("laser"));
  const std::string laserToolId = doc.cam.tool_library[0].tool_id;
  manager.cam_operation_add(make_op("laser_cut", laserToolId));

  bool unknownOp = false;
  try {
    manager.cam_operation_set_scope("cam-op-99", "sketch-1");
  } catch (const std::runtime_error& error) {
    unknownOp = std::string(error.what()).find("not found") !=
                std::string::npos;
  }
  if (!expect(unknownOp, "set scope: unknown op throws")) {
    return false;
  }

  bool unknownSketch = false;
  try {
    manager.cam_operation_set_scope("cam-op-1", "sketch-99");
  } catch (const std::runtime_error& error) {
    unknownSketch = std::string(error.what()).find("not found") !=
                    std::string::npos;
  }
  if (!expect(unknownSketch, "set scope: unknown sketch throws")) {
    return false;
  }
  return expect(manager.get_document()->cam.operations[0]
                    .geometry_references.machining_regions.empty(),
                "set scope: failed scope leaves the op untouched");
}

bool test_select_sketch_profile_by_entity() {
  using polysmith::core::SketchProfileRegion;
  DocumentManager manager;
  manager.create_document();
  manager.start_sketch_on_plane("ref-plane-xy");
  DocumentState doc = manager.add_sketch_rectangle(0.0, 0.0, 20.0, 10.0);
  doc = manager.add_sketch_circle(10.0, 5.0, 2.0);

  const polysmith::core::SketchFeatureParameters* sketch = nullptr;
  for (const auto& feature : doc.feature_history) {
    if (feature.kind == "sketch" && feature.sketch_parameters.has_value()) {
      sketch = &feature.sketch_parameters.value();
      break;
    }
  }
  if (!expect(sketch != nullptr && sketch->lines.size() == 4 &&
                  sketch->circles.size() == 1 && sketch->profiles.size() == 2,
              "entity fixture: rect + circle sketch with two regions")) {
    return false;
  }
  const std::string rectLineId = sketch->lines[0].id;
  const std::string circleId = sketch->circles[0].id;

  // IMPORTANT: every `manager.<mutation>` reassigns `doc`, destroying
  // the previous copy — never keep pointers into an older snapshot
  // (a dangling FeatureEntry* here reads freed memory).
  const auto sketch_params_of = [](const DocumentState& state)
      -> const polysmith::core::SketchFeatureParameters* {
    for (const auto& feature : state.feature_history) {
      if (feature.kind == "sketch" && feature.sketch_parameters.has_value()) {
        return &feature.sketch_parameters.value();
      }
    }
    return nullptr;
  };
  const auto region_owns_line = [](const SketchProfileRegion& region,
                                   const std::string& lineId) {
    const auto in = [&](const std::vector<std::string>& ids) {
      return std::find(ids.begin(), ids.end(), lineId) != ids.end();
    };
    return in(region.line_ids) || in(region.ordered_edge_ids) ||
           std::any_of(region.boundary_edges.begin(),
                       region.boundary_edges.end(),
                       [&](const auto& edge) {
                         return edge.entity_id == lineId;
                       });
  };

  // Clicking a rectangle edge selects the outer region only.
  doc = manager.select_sketch_profile_by_entity(rectLineId, true);
  const auto& selection = doc.selected_sketch_profile_ids;
  if (!expect(selection.size() == 1,
              "by entity: rectangle edge selects exactly one profile")) {
    return false;
  }
  const auto* liveSketch = sketch_params_of(doc);
  const SketchProfileRegion* pickedRegion = nullptr;
  if (liveSketch != nullptr) {
    for (const auto& region : liveSketch->profiles) {
      if (region.id == selection[0]) {
        pickedRegion = &region;
        break;
      }
    }
  }
  if (!expect(pickedRegion != nullptr &&
                  region_owns_line(*pickedRegion, rectLineId),
              "by entity: the selected region owns the clicked line")) {
    return false;
  }

  // Toggle: clicking the same edge again removes it.
  doc = manager.select_sketch_profile_by_entity(rectLineId, true);
  if (!expect(doc.selected_sketch_profile_ids.empty(),
              "by entity: second click toggles the profile off")) {
    return false;
  }

  // Clicking the circle selects the circle-sourced region.
  doc = manager.select_sketch_profile_by_entity(circleId, false);
  if (!expect(doc.selected_sketch_profile_ids.size() == 1,
              "by entity: circle selects one profile")) {
    return false;
  }
  liveSketch = sketch_params_of(doc);
  const SketchProfileRegion* circleRegion = nullptr;
  if (liveSketch != nullptr) {
    for (const auto& region : liveSketch->profiles) {
      if (region.id == doc.selected_sketch_profile_ids[0]) {
        circleRegion = &region;
        break;
      }
    }
  }
  if (!expect(circleRegion != nullptr &&
                  circleRegion->kind == "circle" &&
                  std::find(circleRegion->line_ids.begin(),
                            circleRegion->line_ids.end(),
                            circleId) != circleRegion->line_ids.end(),
              "by entity: circle selects the exact circle region")) {
    return false;
  }

  // A construction line lies on no profile boundary → throws.
  doc = manager.add_sketch_line(1.0, 5.0, 19.0, 5.0, /*is_construction=*/true);
  const std::string constructionId =
      sketch_params_of(doc)->lines.back().id;
  bool constructionThrew = false;
  try {
    manager.select_sketch_profile_by_entity(constructionId, true);
  } catch (const std::runtime_error& error) {
    constructionThrew =
        std::string(error.what()).find("not on a profile boundary") !=
        std::string::npos;
  }
  if (!expect(constructionThrew,
              "by entity: construction line throws a boundary error")) {
    return false;
  }

  // Unknown entity → throws.
  bool unknownThrew = false;
  try {
    manager.select_sketch_profile_by_entity("line-999", true);
  } catch (const std::runtime_error& error) {
    unknownThrew = std::string(error.what()).find("not found") !=
                   std::string::npos;
  }
  return expect(unknownThrew, "by entity: unknown entity throws");
}

bool test_capture_from_profile_ids_no_fallback() {
  DocumentManager manager;
  manager.create_document();
  manager.start_sketch_on_plane("ref-plane-xy");
  DocumentState doc = manager.add_sketch_rectangle(0.0, 0.0, 20.0, 10.0);
  doc = manager.add_sketch_rectangle(30.0, 0.0, 50.0, 10.0);

  // An EMPTY explicit selection captures nothing — the re-pick gesture
  // must not silently fall back to the whole sketch.
  CamOperation op;
  op.type = "laser_cut";
  if (!expect(!polysmith::core::capture_profile_references_from_profile_ids(
                  doc, {}, op),
              "capture ids: empty selection captures nothing")) {
    return false;
  }
  if (!expect(op.geometry_references.machining_regions.empty(),
              "capture ids: no fallback regions appended")) {
    return false;
  }

  // Explicit ids capture exactly those regions.
  const std::vector<polysmith::core::SketchProfileRegion>* profiles = nullptr;
  for (const auto& feature : doc.feature_history) {
    if (feature.kind == "sketch" && feature.sketch_parameters.has_value()) {
      profiles = &feature.sketch_parameters->profiles;
      break;
    }
  }
  if (!expect(profiles != nullptr && profiles->size() == 2,
              "capture ids fixture: two regions")) {
    return false;
  }
  const std::vector<std::string> oneId = {(*profiles)[0].id};
  if (!expect(polysmith::core::capture_profile_references_from_profile_ids(
                  doc, oneId, op),
              "capture ids: explicit id captures")) {
    return false;
  }
  return expect(op.geometry_references.machining_regions.size() == 1,
                "capture ids: exactly the requested region captured");
}

bool test_setup_delete() {
  DocumentManager manager;
  manager.create_document();
  manager.cam_setup_create(make_setup("laser"));
  DocumentState doc = manager.cam_tool_add(make_tool("laser"));
  const std::string laserToolId = doc.cam.tool_library[0].tool_id;
  doc = manager.cam_operation_add(make_op("laser_cut", laserToolId));
  if (!expect(doc.cam.setups.size() == 1 && doc.cam.operations.size() == 1,
              "setup delete fixture: one setup with one operation")) {
    return false;
  }

  // Deleting the setup removes it AND its operations.
  doc = manager.cam_setup_delete("cam-setup-1");
  if (!expect(doc.cam.setups.empty() && doc.cam.operations.empty(),
              "setup delete: setup and its operations removed")) {
    return false;
  }

  // One undo restores both.
  doc = manager.undo();
  return expect(doc.cam.setups.size() == 1 && doc.cam.operations.size() == 1,
                "setup delete: undo restores setup and operations");
}

bool test_setup_delete_legacy_and_isolation() {
  DocumentManager manager;
  manager.create_document();
  manager.cam_setup_create(make_setup("3_axis_mill"));  // cam-setup-1
  DocumentState doc = manager.cam_setup_create(make_setup("3_axis_mill"));
  doc = manager.cam_tool_add(make_tool());
  const std::string toolId = doc.cam.tool_library[0].tool_id;

  // A legacy operation (no setup_id) joins the FIRST setup by the
  // core's join rule, so it must die with that setup.
  doc = manager.cam_operation_add(make_op("face_milling", toolId));

  // An operation explicitly on setup 2 must survive deleting setup 1.
  CamOperation second = make_op("face_milling", toolId);
  second.setup_id = "cam-setup-2";
  doc = manager.cam_operation_add(second);

  doc = manager.cam_setup_delete("cam-setup-1");
  if (!expect(doc.cam.setups.size() == 1 &&
                  doc.cam.setups[0].setup_id == "cam-setup-2" &&
                  doc.cam.operations.size() == 1 &&
                  doc.cam.operations[0].setup_id == "cam-setup-2",
              "setup delete: legacy op dies with the first setup, "
              "setup-2 op survives")) {
    return false;
  }

  // Deleting the remaining setup removes its operation too.
  doc = manager.cam_setup_delete("cam-setup-2");
  if (!expect(doc.cam.setups.empty() && doc.cam.operations.empty(),
              "setup delete: last setup removes everything")) {
    return false;
  }

  bool threw = false;
  try {
    manager.cam_setup_delete("cam-setup-99");
  } catch (const std::runtime_error& error) {
    threw = std::string(error.what()).find("not found") !=
            std::string::npos;
  }
  return expect(threw, "setup delete: unknown id throws");
}

bool test_cam_setup_find_resolves_by_id() {
  DocumentManager manager;
  manager.create_document();
  manager.cam_setup_create(make_setup());  // cam-setup-1
  manager.cam_setup_create(make_setup());  // cam-setup-2

  const auto byId = manager.cam_setup_find("cam-setup-2");
  if (!expect(byId.has_value() && byId->setup_id == "cam-setup-2",
              "setup find: resolves by id")) {
    return false;
  }
  const auto legacy = manager.cam_setup_find("");
  if (!expect(legacy.has_value() && legacy->setup_id == "cam-setup-1",
              "setup find: empty id means the first setup")) {
    return false;
  }
  return expect(!manager.cam_setup_find("cam-setup-99").has_value(),
                "setup find: unknown id returns nullopt");
}

bool test_wcs_face_update_targets_named_setup() {
  DocumentManager manager;
  manager.create_document();
  manager.cam_setup_create(make_setup());  // cam-setup-1
  manager.cam_setup_create(make_setup());  // cam-setup-2

  // The handler's write path: resolve the target setup, mutate the
  // WCS witness, and update by id — setups[0] must stay untouched.
  auto target = manager.cam_setup_find("cam-setup-2");
  if (!expect(target.has_value(), "wcs face update fixture: setup found")) {
    return false;
  }
  target->wcs_origin.feature_id = "body-1";
  target->wcs_origin.face_reference.persistent_id = "body-1:face:3";
  const DocumentState after = manager.cam_setup_update(target.value());
  if (!expect(after.cam.setups.size() == 2,
              "wcs face update: document keeps both setups")) {
    return false;
  }
  const auto byId = manager.cam_setup_find("cam-setup-2");
  if (!expect(byId.has_value() &&
                  byId->wcs_origin.feature_id == "body-1" &&
                  byId->wcs_origin.face_reference.persistent_id ==
                      "body-1:face:3",
              "wcs face update: named setup received the WCS witness")) {
    return false;
  }
  const auto first = manager.cam_setup_find("cam-setup-1");
  if (!expect(first.has_value() &&
                  first->wcs_origin.feature_id.empty() &&
                  first->wcs_origin.face_reference.persistent_id.empty(),
              "wcs face update: first setup untouched")) {
    return false;
  }

  // Stock-face anchor (the "stock:top" pick path): the WCS carries the
  // anchor + face name instead of a body witness; the update must
  // round-trip them on the targeted setup only.
  auto stockTarget = manager.cam_setup_find("cam-setup-2");
  if (!expect(stockTarget.has_value(), "wcs stock update: setup found")) {
    return false;
  }
  stockTarget->wcs_origin.anchor = "stock_face";
  stockTarget->wcs_origin.stock_face = "top";
  stockTarget->wcs_origin.feature_id.clear();
  stockTarget->wcs_origin.face_reference = GeometryReference{};
  stockTarget->wcs_origin.position.reset();
  const DocumentState afterStock = manager.cam_setup_update(stockTarget.value());
  if (!expect(afterStock.cam.setups.size() == 2,
              "wcs stock update: document keeps both setups")) {
    return false;
  }
  const auto byIdStock = manager.cam_setup_find("cam-setup-2");
  if (!expect(byIdStock.has_value() &&
                  byIdStock->wcs_origin.anchor == "stock_face" &&
                  byIdStock->wcs_origin.stock_face == "top" &&
                  byIdStock->wcs_origin.feature_id.empty() &&
                  byIdStock->wcs_origin.face_reference.persistent_id.empty(),
              "wcs stock update: named setup received the stock anchor")) {
    return false;
  }
  const auto firstAfter = manager.cam_setup_find("cam-setup-1");
  return expect(firstAfter.has_value() && firstAfter->wcs_origin.anchor.empty(),
                "wcs stock update: first setup untouched");
}

bool test_cam_capture_point() {
  DocumentManager manager;
  manager.create_document();

  const GeometryReference first = manager.cam_capture_point({1.0, 2.0, 3.0});
  if (!expect(first.persistent_id == "pt-1",
              "capture point: first id is pt-1")) {
    return false;
  }
  if (!expect(std::holds_alternative<polysmith::core::PointAttestation>(
                  first.attestation),
              "capture point: PointAttestation variant")) {
    return false;
  }
  const auto& att =
      std::get<polysmith::core::PointAttestation>(first.attestation);
  if (!expect(att.point[0] == 1.0 && att.point[1] == 2.0 &&
                  att.point[2] == 3.0,
              "capture point: coordinates stored verbatim")) {
    return false;
  }
  const GeometryReference second = manager.cam_capture_point({4.0, 5.0, 6.0});
  if (!expect(second.persistent_id == "pt-2",
              "capture point: ids increment monotonically")) {
    return false;
  }
  // The mutator only mints the id — the document is untouched (the
  // caller appends the reference to the operation).
  if (!expect(manager.get_document()->cam.operations.empty(),
              "capture point: document state untouched")) {
    return false;
  }

  DocumentManager bare;
  bool threw = false;
  try {
    bare.cam_capture_point({0.0, 0.0, 0.0});
  } catch (const std::runtime_error&) {
    threw = true;
  }
  return expect(threw, "capture point: no active document throws");
}

bool test_drilling_creates_default_tool() {
  DocumentManager manager;
  manager.create_document();
  manager.cam_setup_create(make_setup());  // 3_axis_mill

  CamOperation op;
  op.name = "Drill 1";
  op.type = "drilling";
  const DocumentState created = manager.cam_operation_add(op);
  if (!expect(created.cam.operations.size() == 1 &&
                  !created.cam.operations[0].tool_id.empty() &&
                  created.cam.tool_library.size() == 1 &&
                  created.cam.tool_library[0].type == "drill" &&
                  created.cam.tool_library[0].name == "3mm drill (default)" &&
                  created.cam.tool_library[0].diameter_mm == 3.0,
              "default drill tool: auto-created for drilling")) {
    return false;
  }
  if (!expect(created.cam.operations[0].tool_id ==
                  created.cam.tool_library[0].tool_id,
              "default drill tool: op references the new tool")) {
    return false;
  }

  // A second drilling op reuses the library drill.
  CamOperation second;
  second.name = "Drill 2";
  second.type = "drilling";
  const DocumentState afterSecond = manager.cam_operation_add(second);
  return expect(afterSecond.cam.tool_library.size() == 1 &&
                    afterSecond.cam.operations[1].tool_id ==
                        afterSecond.cam.tool_library[0].tool_id,
                "default drill tool: existing drill reused");
}

bool test_drilling_requires_mill_machine() {
  DocumentManager manager;
  manager.create_document();
  manager.cam_setup_create(make_setup("laser"));

  bool threw = false;
  try {
    manager.cam_operation_add(make_op("drilling", /*toolId=*/""));
  } catch (const std::runtime_error& error) {
    threw = std::string(error.what()).find("milling machine") !=
            std::string::npos;
  }
  return expect(threw, "drilling op on a laser machine throws");
}

// A point attestation round-tripped through a lenient client-side
// parse can arrive carrying leftover FACE keys (defaults injected by
// the parse).  The point branch must win — the coordinate is the
// identity that must survive, not the injected empty witness (which
// would fail face resolution with "the face geometry was lost or
// changed").
bool test_point_attestation_wins_over_face_keys() {
  const nlohmann::json corrupted = {
      {"machining_regions",
       nlohmann::json::array({{
           {"persistent_id", "pt-11"},
           {"attestation",
            {{"point", {35.0, -20.0, 10.0}},
             {"bounds",
              {{"min_x", 0.0}, {"min_y", 0.0}, {"min_z", 0.0},
               {"max_x", 0.0}, {"max_y", 0.0}, {"max_z", 0.0}}},
             {"area", 0.0},
             {"normal", {0.0, 0.0, 1.0}},
             {"sample_points", nlohmann::json::array()}}},
       }})},
      {"avoidance_regions", nlohmann::json::array()},
      {"guide_curves", nlohmann::json::array()},
      {"check_surfaces", nlohmann::json::array()},
  };
  const auto parsed =
      polysmith::protocol::cam_geometry_references_from_payload(corrupted);
  if (!expect(parsed.machining_regions.size() == 1,
              "mixed point/face payload: one region parses")) {
    return false;
  }
  const auto& ref = parsed.machining_regions[0];
  if (!expect(std::holds_alternative<polysmith::core::PointAttestation>(
                  ref.attestation),
              "mixed point/face payload: parses as PointAttestation")) {
    return false;
  }
  const auto& att =
      std::get<polysmith::core::PointAttestation>(ref.attestation);
  return expect(att.point[0] == 35.0 && att.point[1] == -20.0 &&
                    att.point[2] == 10.0,
                "mixed point/face payload: coordinates survive verbatim");
}

// A genuine face attestation (no point key) must still parse as a
// face — the reordering above must not regress the face branch.
bool test_face_attestation_still_parses() {
  const nlohmann::json face_payload = {
      {"machining_regions",
       nlohmann::json::array({{
           {"persistent_id", "body-1:face:3"},
           {"attestation",
            {{"bounds",
              {{"min_x", 0.0}, {"min_y", 0.0}, {"min_z", 0.0},
               {"max_x", 100.0}, {"max_y", 70.0}, {"max_z", 10.0}}},
             {"area", 7000.0},
             {"normal", {0.0, 0.0, 1.0}},
             {"sample_points", nlohmann::json::array({nlohmann::json::array({1.0, 2.0, 3.0})})}}},
       }})},
      {"avoidance_regions", nlohmann::json::array()},
      {"guide_curves", nlohmann::json::array()},
      {"check_surfaces", nlohmann::json::array()},
  };
  const auto parsed =
      polysmith::protocol::cam_geometry_references_from_payload(face_payload);
  if (!expect(parsed.machining_regions.size() == 1,
              "face payload: one region parses")) {
    return false;
  }
  const auto& ref = parsed.machining_regions[0];
  if (!expect(std::holds_alternative<polysmith::core::FaceAttestation>(
                  ref.attestation),
              "face payload: parses as FaceAttestation")) {
    return false;
  }
  const auto& att =
      std::get<polysmith::core::FaceAttestation>(ref.attestation);
  return expect(att.area == 7000.0 && att.sample_points.size() == 1 &&
                    att.normal[2] == 1.0,
                "face payload: witness fields parse");
}

// An edge attestation parses through the edge fallback branch of the
// discriminator; the circle witness keys are optional (a line edge
// carries none) and must survive verbatim when present.
bool test_edge_attestation_parses() {
  const nlohmann::json rim_payload = {
      {"machining_regions",
       nlohmann::json::array({{
           {"persistent_id", "body-1:edge:4"},
           {"attestation",
            {{"start_point", {10.0, 5.0, 10.0}},
             {"end_point", {10.0, 5.0, 10.0}},
             {"length", 12.566370614359172},
             {"tangent", {1.0, 0.0, 0.0}},
             {"center", {10.0, 5.0, 10.0}},
             {"axis", {0.0, 0.0, 1.0}},
             {"radius", 2.0}}},
       }})},
      {"avoidance_regions", nlohmann::json::array()},
      {"guide_curves", nlohmann::json::array()},
      {"check_surfaces", nlohmann::json::array()},
  };
  const auto parsed =
      polysmith::protocol::cam_geometry_references_from_payload(rim_payload);
  if (!expect(parsed.machining_regions.size() == 1,
              "edge payload: one region parses")) {
    return false;
  }
  const auto& ref = parsed.machining_regions[0];
  if (!expect(std::holds_alternative<polysmith::core::EdgeAttestation>(
                  ref.attestation),
              "edge payload: parses as EdgeAttestation")) {
    return false;
  }
  const auto& att =
      std::get<polysmith::core::EdgeAttestation>(ref.attestation);
  if (!expect(att.center.has_value() && att.axis.has_value() &&
                  att.radius.has_value(),
              "edge payload: circle witness parses")) {
    return false;
  }
  if (!expect(att.center.value()[0] == 10.0 && att.center.value()[1] == 5.0 &&
                  att.center.value()[2] == 10.0 &&
                  att.axis.value()[2] == 1.0 &&
                  near(att.radius.value(), 2.0) &&
                  near(att.length, 12.566370614359172),
              "edge payload: witness values survive verbatim")) {
    return false;
  }

  // A line edge (no circle keys) parses with the witness unset.
  const nlohmann::json line_payload = {
      {"machining_regions",
       nlohmann::json::array({{
           {"persistent_id", "body-1:edge:0"},
           {"attestation",
            {{"start_point", {0.0, 0.0, 10.0}},
             {"end_point", {20.0, 0.0, 10.0}},
             {"length", 20.0},
             {"tangent", {1.0, 0.0, 0.0}}}},
       }})},
      {"avoidance_regions", nlohmann::json::array()},
      {"guide_curves", nlohmann::json::array()},
      {"check_surfaces", nlohmann::json::array()},
  };
  const auto lineParsed =
      polysmith::protocol::cam_geometry_references_from_payload(line_payload);
  if (!expect(lineParsed.machining_regions.size() == 1 &&
                  std::holds_alternative<polysmith::core::EdgeAttestation>(
                      lineParsed.machining_regions[0].attestation),
              "line payload: parses as EdgeAttestation")) {
    return false;
  }
  const auto& lineAtt = std::get<polysmith::core::EdgeAttestation>(
      lineParsed.machining_regions[0].attestation);
  return expect(!lineAtt.center.has_value() && !lineAtt.axis.has_value() &&
                    !lineAtt.radius.has_value(),
                "line payload: circle witness stays unset");
}

// capture_edge_reference stores line endpoints/length/tangent with no
// circle witness, and the full circle witness (center/axis/radius) for
// closed rim edges — the fields drilling resolution scores on.
bool test_capture_edge_reference_witness() {
  DocumentManager manager;
  manager.create_document();
  DocumentState document = manager.add_box_feature(
      {.width = 20.0, .height = 20.0, .depth = 10.0});
  const auto compiled = polysmith::core::compile_bodies(document);
  if (!expect(compiled.bodies.size() == 1,
              "capture: box body compiled")) {
    return false;
  }
  const auto& box = compiled.bodies[0];
  // Line edge: a box top/bottom edge (the box has 12 line edges — 8 of
  // length 20, 4 vertical of length 10; take a length-20 one so the
  // assertion below is deterministic).
  int lineIndex = -1;
  {
    NCollection_IndexedMap<TopoDS_Shape, TopTools_ShapeMapHasher> edgeMap;
    TopExp::MapShapes(box.shape, TopAbs_EDGE, edgeMap);
    for (int i = 1; i <= edgeMap.Extent(); ++i) {
      try {
        BRepAdaptor_Curve curve(TopoDS::Edge(edgeMap(i)));
        if (curve.GetType() != GeomAbs_Line) {
          continue;
        }
        const double length =
            std::abs(curve.LastParameter() - curve.FirstParameter());
        if (near(length, 20.0)) {
          lineIndex = i - 1;
          break;
        }
      } catch (const std::exception&) {
        continue;
      }
    }
  }
  if (!expect(lineIndex >= 0, "capture: box line edge found")) {
    return false;
  }
  const auto lineRef = polysmith::core::capture_edge_reference(
      box.id, box.shape, lineIndex);
  if (!expect(lineRef.has_value(), "capture: line edge captured")) {
    return false;
  }
  if (!expect(near(lineRef->length, 20.0) && !lineRef->center.has_value() &&
                  !lineRef->axis.has_value() && !lineRef->radius.has_value(),
              "capture: line stores endpoints/length, no circle witness")) {
    return false;
  }

  // Full-circle rim: a cylinder's top edge.  The document also holds
  // the box — locate the cylinder by its feature id.
  DocumentState cylinderDoc = manager.add_cylinder_feature(
      polysmith::core::CylinderFeatureParameters{.radius = 10.0,
                                                 .height = 10.0});
  const std::string cylinderId = cylinderDoc.feature_history.back().id;
  const auto cylinderCompiled =
      polysmith::core::compile_bodies(cylinderDoc);
  const polysmith::core::CompiledBody* cylinder = nullptr;
  for (const auto& candidate : cylinderCompiled.bodies) {
    if (candidate.id == cylinderId) {
      cylinder = &candidate;
    }
  }
  if (!expect(cylinder != nullptr, "capture: cylinder body compiled")) {
    return false;
  }
  int rimIndex = -1;
  {
    NCollection_IndexedMap<TopoDS_Shape, TopTools_ShapeMapHasher> edgeMap;
    TopExp::MapShapes(cylinder->shape, TopAbs_EDGE, edgeMap);
    for (int i = 1; i <= edgeMap.Extent(); ++i) {
      try {
        const auto edge = TopoDS::Edge(edgeMap(i));
        BRepAdaptor_Curve curve(edge);
        if (curve.GetType() != GeomAbs_Circle) {
          continue;
        }
        TopoDS_Vertex firstVertex;
        TopoDS_Vertex lastVertex;
        TopExp::Vertices(edge, firstVertex, lastVertex);
        const double radius = curve.Circle().Radius();
        if (BRep_Tool::Pnt(firstVertex).Distance(BRep_Tool::Pnt(lastVertex)) <=
            std::max(1e-7, radius * 1e-7)) {
          rimIndex = i - 1;
          break;
        }
      } catch (const std::exception&) {
        continue;
      }
    }
  }
  if (!expect(rimIndex >= 0, "capture: cylinder rim edge found")) {
    return false;
  }
  const auto rimRef = polysmith::core::capture_edge_reference(
      cylinder->id, cylinder->shape, rimIndex);
  if (!expect(rimRef.has_value(), "capture: rim edge captured")) {
    return false;
  }
  if (!expect(rimRef->center.has_value() && rimRef->axis.has_value() &&
                  rimRef->radius.has_value(),
              "capture: rim stores the circle witness")) {
    return false;
  }
  if (!expect(near(rimRef->radius.value(), 10.0) &&
                  near(rimRef->axis.value()[2], 1.0),
              "capture: rim radius and vertical axis")) {
    return false;
  }
  return expect(rimRef->startPoint == rimRef->endPoint,
                "capture: closed rim collapses both endpoints");
}

// Partial arcs are not drilling inputs — capture rejects them outright
// (the full-circle test) so a bad pick never becomes a stored op.
bool test_capture_edge_reference_rejects_arc() {
  DocumentManager manager;
  manager.create_document();
  DocumentState document = manager.add_cylinder_feature(
      polysmith::core::CylinderFeatureParameters{.radius = 10.0,
                                                 .height = 10.0});
  const std::string cylinderId = document.feature_history.back().id;
  // Cut the right half away: a rectangle prism over x >= 0 leaves a
  // half cylinder whose circular rims are now arcs.
  manager.start_sketch_on_plane("ref-plane-xy");
  document = manager.add_sketch_rectangle(0.0, -12.0, 12.0, 12.0);
  const std::string sketchId = document.feature_history.back().id;
  std::string rectProfile;
  for (const auto& feature : document.feature_history) {
    if (feature.id == sketchId && feature.sketch_parameters.has_value()) {
      rectProfile = feature.sketch_parameters->profiles[0].id;
    }
  }
  if (!expect(!rectProfile.empty(), "arc: rectangle profile found")) {
    return false;
  }
  document = manager.extrude_profile(rectProfile, 10.0, "cut", cylinderId);
  const auto compiled = polysmith::core::compile_bodies(document);
  const polysmith::core::CompiledBody* body = nullptr;
  for (const auto& candidate : compiled.bodies) {
    if (candidate.id == cylinderId) {
      body = &candidate;
    }
  }
  if (!expect(body != nullptr, "arc: cut body compiled")) {
    return false;
  }
  int arcIndex = -1;
  NCollection_IndexedMap<TopoDS_Shape, TopTools_ShapeMapHasher> edgeMap;
  TopExp::MapShapes(body->shape, TopAbs_EDGE, edgeMap);
  for (int i = 1; i <= edgeMap.Extent(); ++i) {
    try {
      const auto edge = TopoDS::Edge(edgeMap(i));
      BRepAdaptor_Curve curve(edge);
      if (curve.GetType() != GeomAbs_Circle) {
        continue;
      }
      TopoDS_Vertex firstVertex;
      TopoDS_Vertex lastVertex;
      TopExp::Vertices(edge, firstVertex, lastVertex);
      const double radius = curve.Circle().Radius();
      if (BRep_Tool::Pnt(firstVertex).Distance(BRep_Tool::Pnt(lastVertex)) >
          std::max(1e-7, radius * 1e-7)) {
        arcIndex = i - 1;
        break;
      }
    } catch (const std::exception&) {
      continue;
    }
  }
  if (!expect(arcIndex >= 0, "arc: partial rim edge found")) {
    return false;
  }
  return expect(!polysmith::core::capture_edge_reference(
                    body->id, body->shape, arcIndex)
                     .has_value(),
                "arc: capture rejects the partial rim");
}

}  // namespace

int main() {
  bool allPassed = true;

  std::cout << "cam_commands_test\n";
  std::cout << "  Test 1: setup CRUD... ";
  if (test_setup_crud()) {
    std::cout << "PASS\n";
  } else {
    std::cout << "FAIL\n";
    allPassed = false;
  }

  std::cout << "  Test 2: stock access... ";
  if (test_stock_access()) {
    std::cout << "PASS\n";
  } else {
    std::cout << "FAIL\n";
    allPassed = false;
  }

  std::cout << "  Test 3: tool library CRUD... ";
  if (test_tool_library_crud()) {
    std::cout << "PASS\n";
  } else {
    std::cout << "FAIL\n";
    allPassed = false;
  }

  std::cout << "  Test 4: operation add validates tool... ";
  if (test_operation_add_validates_tool()) {
    std::cout << "PASS\n";
  } else {
    std::cout << "FAIL\n";
    allPassed = false;
  }

  std::cout << "  Test 5: laser op requires laser machine... ";
  if (test_laser_operation_requires_laser_machine()) {
    std::cout << "PASS\n";
  } else {
    std::cout << "FAIL\n";
    allPassed = false;
  }

  std::cout << "  Test 6: operation update/delete/undo... ";
  if (test_operation_update_delete_undo()) {
    std::cout << "PASS\n";
  } else {
    std::cout << "FAIL\n";
    allPassed = false;
  }

  std::cout << "  Test 7: default tool auto-creation... ";
  if (test_operation_add_creates_default_tool()) {
    std::cout << "PASS\n";
  } else {
    std::cout << "FAIL\n";
    allPassed = false;
  }

  std::cout << "  Test 8: id counters survive reload... ";
  if (test_id_counters_survive_reload()) {
    std::cout << "PASS\n";
  } else {
    std::cout << "FAIL\n";
    allPassed = false;
  }

  std::cout << "  Test 9: operation set scope on a sketch... ";
  if (test_operation_set_scope_sketch()) {
    std::cout << "PASS\n";
  } else {
    std::cout << "FAIL\n";
    allPassed = false;
  }

  std::cout << "  Test 10: operation set scope errors... ";
  if (test_operation_set_scope_errors()) {
    std::cout << "PASS\n";
  } else {
    std::cout << "FAIL\n";
    allPassed = false;
  }

  std::cout << "  Test 11: select profile by entity... ";
  if (test_select_sketch_profile_by_entity()) {
    std::cout << "PASS\n";
  } else {
    std::cout << "FAIL\n";
    allPassed = false;
  }

  std::cout << "  Test 12: capture from profile ids (no fallback)... ";
  if (test_capture_from_profile_ids_no_fallback()) {
    std::cout << "PASS\n";
  } else {
    std::cout << "FAIL\n";
    allPassed = false;
  }

  std::cout << "  Test 13: setup delete removes ops + undo... ";
  if (test_setup_delete()) {
    std::cout << "PASS\n";
  } else {
    std::cout << "FAIL\n";
    allPassed = false;
  }

  std::cout << "  Test 14: setup delete legacy + isolation... ";
  if (test_setup_delete_legacy_and_isolation()) {
    std::cout << "PASS\n";
  } else {
    std::cout << "FAIL\n";
    allPassed = false;
  }

  std::cout << "  Test 15: setup find resolves by id... ";
  if (test_cam_setup_find_resolves_by_id()) {
    std::cout << "PASS\n";
  } else {
    std::cout << "FAIL\n";
    allPassed = false;
  }

  std::cout << "  Test 16: wcs face update targets named setup... ";
  if (test_wcs_face_update_targets_named_setup()) {
    std::cout << "PASS\n";
  } else {
    std::cout << "FAIL\n";
    allPassed = false;
  }

  std::cout << "  Test 17: cam_capture_point mints point refs... ";
  if (test_cam_capture_point()) {
    std::cout << "PASS\n";
  } else {
    std::cout << "FAIL\n";
    allPassed = false;
  }

  std::cout << "  Test 18: drilling default drill tool... ";
  if (test_drilling_creates_default_tool()) {
    std::cout << "PASS\n";
  } else {
    std::cout << "FAIL\n";
    allPassed = false;
  }

  std::cout << "  Test 19: drilling requires a mill machine... ";
  if (test_drilling_requires_mill_machine()) {
    std::cout << "PASS\n";
  } else {
    std::cout << "FAIL\n";
    allPassed = false;
  }

  std::cout << "  Test 20: point attestation wins over injected face keys... ";
  if (test_point_attestation_wins_over_face_keys()) {
    std::cout << "PASS\n";
  } else {
    std::cout << "FAIL\n";
    allPassed = false;
  }

  std::cout << "  Test 21: genuine face attestation still parses... ";
  if (test_face_attestation_still_parses()) {
    std::cout << "PASS\n";
  } else {
    std::cout << "FAIL\n";
    allPassed = false;
  }

  std::cout << "  Test 22: edge attestation parses with/without witness... ";
  if (test_edge_attestation_parses()) {
    std::cout << "PASS\n";
  } else {
    std::cout << "FAIL\n";
    allPassed = false;
  }

  std::cout << "  Test 23: capture_edge_reference witness fields... ";
  if (test_capture_edge_reference_witness()) {
    std::cout << "PASS\n";
  } else {
    std::cout << "FAIL\n";
    allPassed = false;
  }

  std::cout << "  Test 24: capture rejects partial arcs... ";
  if (test_capture_edge_reference_rejects_arc()) {
    std::cout << "PASS\n";
  } else {
    std::cout << "FAIL\n";
    allPassed = false;
  }

  if (allPassed) {
    std::cout << "cam_commands_test passed\n";
    return 0;
  }
  return 1;
}
