// CAM document mutator test.
//
// Exercises the DocumentManager CAM CRUD methods against the
// target-schema types: setup, stock, tool library, and operations.
// Also proves error paths throw human-readable messages and leave the
// document untouched, ids are assigned monotonically, and undo/redo
// covers CAM mutations.

#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <variant>

#include "core/document/document.h"
#include "core/cam/cam_operation.h"
#include "core/cam/cam_profile_reference.h"
#include "protocol/serialization.h"

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
                  circleRegion->source_circle_id.has_value() &&
                  circleRegion->source_circle_id.value() == circleId,
              "by entity: circle selects the circle-sourced region")) {
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

  if (allPassed) {
    std::cout << "cam_commands_test passed\n";
    return 0;
  }
  return 1;
}
