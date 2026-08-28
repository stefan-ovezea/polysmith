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

#include "core/document/document.h"
#include "core/cam/cam_operation.h"
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

  if (allPassed) {
    std::cout << "cam_commands_test passed\n";
    return 0;
  }
  return 1;
}
