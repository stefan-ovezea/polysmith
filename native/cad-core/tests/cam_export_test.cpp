// CAM export text/file parity test.
//
// Pins the in-memory posting pipeline (post_cam_gcode_text) used by
// the GRBL workspace handoff: a minimal laser test-pattern document
// posts real G-code (operation header + laser footer), the file
// export writes byte-identical text, and the no-setup error path
// throws.

#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>

#include "core/cam/cam_export.h"
#include "core/cam/cam_generator.h"
#include "core/cam/cam_types.h"
#include "core/document/document.h"

namespace {

using polysmith::core::CamOperation;
using polysmith::core::CamSetup;
using polysmith::core::DocumentManager;
using polysmith::core::DocumentState;
using polysmith::core::LaserTestPatternParameters;
using polysmith::core::ToolEntry;
using polysmith::core::export_cam_gcode;
using polysmith::core::post_cam_gcode_text;
using polysmith::core::register_builtin_cam_generators;

bool expect(bool condition, const char* message) {
  if (condition) {
    return true;
  }
  std::cerr << "FAIL: " << message << "\n";
  return false;
}

ToolEntry make_laser_tool() {
  ToolEntry tool;
  tool.name = "CO2 laser";
  tool.type = "laser";
  return tool;
}

bool test_post_text_matches_file_export() {
  DocumentManager manager;
  manager.create_document();

  CamSetup setup;
  setup.name = "Laser Setup";
  setup.machine_type = "laser";
  const auto withSetup = manager.cam_setup_create(setup);
  const std::string setup_id = withSetup.cam.setups[0].setup_id;

  const auto withTool = manager.cam_tool_add(make_laser_tool());
  const std::string tool_id = withTool.cam.tool_library[0].tool_id;

  // A laser test pattern generates in machine coordinates — no sketch
  // geometry, no OCCT — so it exercises the full export pipeline with
  // the smallest possible fixture.
  CamOperation op;
  op.name = "Test Pattern";
  op.type = "laser_test_pattern";
  op.setup_id = setup_id;
  op.tool_id = tool_id;
  op.parameters.test_pattern = LaserTestPatternParameters{};
  manager.cam_operation_add(op);

  // Copy, never bind a reference: get_document() returns the optional
  // by value — a reference would dangle once the temporary dies.
  const DocumentState document = manager.get_document().value();

  const auto posted = post_cam_gcode_text(document);
  if (!expect(!posted.text.empty(), "post text: non-empty")) {
    return false;
  }
  if (!expect(posted.text.find("(operation: Test Pattern)") !=
                  std::string::npos,
              "post text: operation header present")) {
    return false;
  }
  if (!expect(posted.text.find("M5") != std::string::npos,
              "post text: laser footer (M5) present")) {
    return false;
  }
  if (!expect(posted.exported_feature_count == 1,
              "post text: one exported feature")) {
    return false;
  }

  // The two pipelines must stay byte-identical — the file export is
  // what LaserGRBL sees, the text is what the workspace streams.
  const auto temp = std::filesystem::temp_directory_path() /
                    "polysmith_cam_export_test.nc";
  const auto exported = export_cam_gcode(document, temp.string());
  // Windows keeps open files locked — read and close before removing.
  std::string file_content;
  {
    std::ifstream stream(temp);
    std::stringstream file_text;
    file_text << stream.rdbuf();
    file_content = file_text.str();
  }
  std::filesystem::remove(temp);
  if (!expect(file_content == posted.text,
              "file export byte-equals the in-memory post")) {
    return false;
  }
  return expect(exported.exported_feature_count == 1,
                "file export: one exported feature");
}

bool test_post_text_requires_setup() {
  DocumentManager manager;
  manager.create_document();
  bool threw = false;
  try {
    post_cam_gcode_text(manager.get_document().value());
  } catch (const std::runtime_error& error) {
    threw = std::string(error.what()).find("setup") != std::string::npos;
  }
  return expect(threw, "post text: document without a CAM setup throws");
}

}  // namespace

int main() {
  // The generators register from CadCoreApp::run() — test processes
  // never run the app, so register them here.
  register_builtin_cam_generators();
  try {
    bool ok = true;
    ok = test_post_text_matches_file_export() && ok;
    ok = test_post_text_requires_setup() && ok;
    if (ok) {
      std::cout << "cam_export_test: all tests passed\n";
      return 0;
    }
    std::cerr << "cam_export_test: FAILURES\n";
    return 1;
  } catch (const std::exception& error) {
    // Uncaught exceptions reach std::terminate with no output on this
    // toolchain — surface the real throw site instead.
    std::cerr << "cam_export_test: UNCAUGHT EXCEPTION: " << error.what()
              << "\n";
    return 1;
  }
}
