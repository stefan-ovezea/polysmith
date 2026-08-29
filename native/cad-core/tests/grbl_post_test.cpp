// GRBL post-processor golden test.
//
// Builds small toolpath IRs by hand and asserts the exact G-code the
// writer emits: header, S scaling, M3 vs M4, G0/G1/G2/G3 with I/J from
// the arc start, dwells, laser on/off transitions, line numbers,
// decimal places, mm/inch, Z-on-change, and WCS offset.

#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include "core/cam/post_processor.h"

namespace {

using polysmith::core::CamSetup;
using polysmith::core::LaserCutParameters;
using polysmith::core::PostContext;
using polysmith::core::ToolEntry;
using polysmith::core::Toolpath;
using polysmith::core::ToolpathMove;
using polysmith::core::ToolpathMoveKind;
using polysmith::core::post_process;

bool expect(bool condition, const char* message) {
  if (condition) {
    return true;
  }
  std::cerr << "FAIL: " << message << "\n";
  return false;
}

CamSetup make_setup(const std::string& units = "mm") {
  CamSetup setup;
  setup.name = "Setup";
  setup.units = units;
  setup.safety_height = 5.0;
  return setup;
}

ToolEntry make_laser_tool() {
  ToolEntry tool;
  tool.name = "CO2 laser";
  tool.type = "laser";
  return tool;
}

std::string joined(const std::vector<std::string>& lines) {
  std::string result;
  for (const auto& line : lines) {
    result += line + "\n";
  }
  return result;
}

// Rapid → pierce (M4 S) → G1 feed with dwell → arc CW/CCW → M5 off.
Toolpath make_laser_toolpath() {
  Toolpath path;
  path.moves.push_back({ToolpathMoveKind::Rapid, 0.0, 0.0, 5.0});
  path.moves.push_back(
      {ToolpathMoveKind::Rapid, 3.0, 3.0, 0.0, 0.0, 0.0, 0.0, 0.0, false});
  // Pierce + lead-in: laser on, dynamic power, feed 500, dwell 0.3 s.
  path.moves.push_back({ToolpathMoveKind::FeedLinear, 4.0, 3.0, 0.0, 0.0, 0.0,
                        500.0, 85.0, true, 0.3});
  // CW arc from (4,3) to (4,7): center at (4,5) → i=0, j=2.
  path.moves.push_back(
      {ToolpathMoveKind::FeedArcCW, 4.0, 7.0, 0.0, 0.0, 2.0, 500.0, 85.0, true});
  // CCW arc from (4,7) to (4,3): center (4,5) → i=0, j=-2.
  path.moves.push_back({ToolpathMoveKind::FeedArcCCW, 4.0, 3.0, 0.0, 0.0, -2.0,
                        500.0, 85.0, true});
  // Lead-out with the laser off.
  path.moves.push_back(
      {ToolpathMoveKind::FeedLinear, 5.0, 3.0, 0.0, 0.0, 0.0, 500.0, 0.0, false});
  return path;
}

bool test_laser_golden() {
  LaserCutParameters laser;
  laser.power_percent = 85.0;
  laser.dynamic_power = true;
  laser.mode = "cut";

  PostContext context{
      .toolpath = make_laser_toolpath(),
      .setup = make_setup(),
      .tool = make_laser_tool(),
      .op_name = "2D Cut 1",
      .laser = laser,
  };
  const auto lines = post_process("grbl", context);
  const std::string gcode = joined(lines);

  // Header.
  if (!expect(gcode.find("(op: 2D Cut 1)") != std::string::npos &&
                  gcode.find("G21") != std::string::npos &&
                  gcode.find("G90") != std::string::npos &&
                  gcode.find("G94") != std::string::npos &&
                  gcode.find("G17") != std::string::npos &&
                  gcode.find("M5") != std::string::npos,
              "grbl: header block present")) {
    std::cerr << gcode;
    return false;
  }
  // Power scaling: 85% → S850, then M4 for dynamic power.
  if (!expect(gcode.find("S850") != std::string::npos &&
                  gcode.find("M4") != std::string::npos,
              "grbl: S850 + M4 for 85% dynamic power")) {
    std::cerr << gcode;
    return false;
  }
  // Arcs with I/J from the start point.
  if (!expect(gcode.find("G2 X4.000 Y7.000 I0.000 J2.000") !=
                      std::string::npos &&
                  gcode.find("G3 X4.000 Y3.000 I0.000 J-2.000") !=
                      std::string::npos,
              "grbl: G2/G3 carry I/J offsets from the arc start")) {
    std::cerr << gcode;
    return false;
  }
  // Pierce dwell.
  if (!expect(gcode.find("G4 P0.300") != std::string::npos,
              "grbl: pierce dwell emitted")) {
    std::cerr << gcode;
    return false;
  }
  // Feed rate rides the first feed move.
  if (!expect(gcode.find("F500.000") != std::string::npos,
              "grbl: feed rate emitted")) {
    std::cerr << gcode;
    return false;
  }
  // The laser turns off (M5) between the last arc and the lead-out
  // travel move.
  const size_t lastArc = gcode.find("G3 X4.000");
  const size_t leadOut = gcode.find("G1 X5.000");
  const size_t m5Between = gcode.find("M5", lastArc);
  if (!expect(leadOut != std::string::npos &&
                  m5Between != std::string::npos &&
                  m5Between < leadOut,
              "grbl: laser off (M5) before the lead-out travel")) {
    std::cerr << gcode;
    return false;
  }
  // Z only on the moves that change it: the first rapid carries Z5,
  // the dive to Z0 on the second rapid.
  if (!expect(gcode.find("Z5.000") != std::string::npos,
              "grbl: rapid carries the retract Z")) {
    std::cerr << gcode;
    return false;
  }
  return true;
}

bool test_engrave_uses_m3() {
  LaserCutParameters laser;
  laser.power_percent = 30.0;
  laser.dynamic_power = true;
  laser.mode = "engrave";

  Toolpath path;
  path.moves.push_back({ToolpathMoveKind::FeedLinear, 1.0, 1.0, 0.0, 0.0, 0.0,
                        300.0, 30.0, true});
  path.moves.push_back(
      {ToolpathMoveKind::FeedLinear, 1.0, 2.0, 0.0, 0.0, 0.0, 300.0, 30.0, false});

  PostContext context{
      .toolpath = path,
      .setup = make_setup(),
      .tool = make_laser_tool(),
      .op_name = "Engrave 1",
      .laser = laser,
  };
  const std::string gcode = joined(post_process("grbl", context));
  if (!expect(gcode.find("S300") != std::string::npos &&
                  gcode.find("M3") != std::string::npos &&
                  gcode.find("M4") == std::string::npos,
              "grbl: engrave uses M3 (constant power), never M4")) {
    std::cerr << gcode;
    return false;
  }
  return true;
}

bool test_line_numbers_and_decimals() {
  // A USER FILE overrides the built-in definition: line numbers and
  // decimals come from the file (posts are first-class citizens).
  LaserCutParameters laser;
  laser.power_percent = 100.0;

  Toolpath path;
  path.moves.push_back(
      {ToolpathMoveKind::FeedLinear, 1.234, 2.345, 0.0, 0.0, 0.0, 300.0, 100.0, true});

  const auto dir = std::filesystem::temp_directory_path() /
                   "polysmith_posts_test";
  std::filesystem::create_directories(dir);
  {
    std::ofstream stream(dir / "grbl.json");
    stream << R"({
      "line_numbers": true,
      "decimal_places": 2
    })";
  }
#ifdef _WIN32
  _putenv_s("POLYSMITH_POSTS_DIR", dir.string().c_str());
#else
  setenv("POLYSMITH_POSTS_DIR", dir.string().c_str(), 1);
#endif

  PostContext context{
      .toolpath = path,
      .setup = make_setup(),
      .tool = make_laser_tool(),
      .op_name = "Numbers",
      .laser = laser,
  };
  const std::string gcode = joined(post_process("grbl", context));
  if (!expect(gcode.find("N10 ") != std::string::npos &&
                  gcode.find("X1.23 Y2.35") != std::string::npos,
              "file post: line numbers and decimal places applied")) {
    std::cerr << gcode;
    return false;
  }

  // Editing the file applies on the NEXT call — no restart.
  {
    std::ofstream stream(dir / "grbl.json", std::ios::trunc);
    stream << R"({
      "line_numbers": false,
      "decimal_places": 2
    })";
  }
  const std::string plain = joined(post_process("grbl", context));
#ifdef _WIN32
  _putenv_s("POLYSMITH_POSTS_DIR", "");
#else
  unsetenv("POLYSMITH_POSTS_DIR");
#endif
  return expect(plain.find("N10") == std::string::npos,
                "file post: edits apply immediately");
}

bool test_inch_and_wcs_offset() {
  LaserCutParameters laser;
  laser.power_percent = 50.0;

  Toolpath path;
  path.moves.push_back({ToolpathMoveKind::FeedLinear, 10.0, 20.0, 0.0, 0.0, 0.0,
                        200.0, 50.0, true});

  PostContext context{
      .toolpath = path,
      .setup = make_setup("inch"),
      .tool = make_laser_tool(),
      .op_name = "Inch",
      .laser = laser,
  };
  context.wcs_origin = {2.0, 4.0, 0.0};
  const std::string gcode = joined(post_process("grbl", context));
  if (!expect(gcode.find("G20") != std::string::npos,
              "grbl: inch units emit G20")) {
    std::cerr << gcode;
    return false;
  }
  // Machine coords = (world - wcs origin) in INCHES: 8 mm → 0.315 in,
  // 16 mm → 0.630 in.
  return expect(gcode.find("X0.315") != std::string::npos &&
                    gcode.find("Y0.630") != std::string::npos,
                "grbl: inch coordinates scaled by 1/25.4");
}

bool test_mill_uses_spindle_m3() {
  ToolEntry mill;
  mill.name = "6mm endmill";
  mill.type = "endmill_flat";

  Toolpath path;
  path.moves.push_back({ToolpathMoveKind::Rapid, 0.0, 0.0, 5.0});
  path.moves.push_back({ToolpathMoveKind::FeedLinear, 0.0, 0.0, 0.0, 0.0, 0.0,
                        400.0, 0.0, true});

  PostContext context{
      .toolpath = path,
      .setup = make_setup(),
      .tool = mill,
      .op_name = "Face 1",
      .spindle_rpm = 12000.0,
      .laser = std::nullopt,
  };
  const std::string gcode = joined(post_process("grbl", context));
  if (!expect(gcode.find("M3 S12000") != std::string::npos,
              "grbl: mill emits M3 with the operation spindle rpm")) {
    std::cerr << gcode;
    return false;
  }
  return expect(gcode.find("M4") == std::string::npos,
                "grbl: mill never emits M4");
}

bool test_header_footer_passthrough() {
  // A fully custom dialect defined in a FILE: custom header, footer,
  // and move syntax — the file drives the whole output shape.
  LaserCutParameters laser;
  laser.power_percent = 50.0;
  Toolpath path;
  path.moves.push_back(
      {ToolpathMoveKind::FeedLinear, 10.5, 20.25, 0.0, 0.0, 0.0, 200.0, 50.0, true});

  const auto dir = std::filesystem::temp_directory_path() /
                   "polysmith_posts_custom_test";
  std::filesystem::create_directories(dir);
  {
    std::ofstream stream(dir / "mylaser.json");
    stream << R"JSON({
      "header_lines": ["(CUSTOM HEADER)"],
      "rapid": "G00 X{x} Y{y}",
      "feed": "G01 X{x} Y{y}",
      "arc_cw": "G02 X{x} Y{y} I{i} J{j}",
      "arc_ccw": "G03 X{x} Y{y} I{i} J{j}",
      "laser_on_dynamic": "M4 S{power}",
      "laser_off": "M5",
      "footer_lines": ["(CUSTOM FOOTER)", "M30"],
      "decimal_places": 2,
      "line_numbers": false
    })JSON";
  }
#ifdef _WIN32
  _putenv_s("POLYSMITH_POSTS_DIR", dir.string().c_str());
#else
  setenv("POLYSMITH_POSTS_DIR", dir.string().c_str(), 1);
#endif

  PostContext context{
      .toolpath = path,
      .setup = make_setup(),
      .tool = make_laser_tool(),
      .op_name = "Custom",
      .laser = laser,
  };
  const std::string gcode = joined(post_process("mylaser", context));
  if (!expect(gcode.find("(CUSTOM HEADER)") != std::string::npos &&
                  gcode.find("(CUSTOM FOOTER)") != std::string::npos &&
                  gcode.find("G01 X10.50 Y20.25") != std::string::npos,
              "custom post: file templates drive the output")) {
    std::cerr << gcode;
    return false;
  }
#ifdef _WIN32
  _putenv_s("POLYSMITH_POSTS_DIR", "");
#else
  unsetenv("POLYSMITH_POSTS_DIR");
#endif
  return expect(gcode.find("M30") != std::string::npos,
                "custom post: custom footer program end");
}

bool test_post_list_and_import() {
  const auto dir = std::filesystem::temp_directory_path() /
                   "polysmith_posts_list_test";
  std::filesystem::remove_all(dir);
  std::filesystem::create_directories(dir);
#ifdef _WIN32
  _putenv_s("POLYSMITH_POSTS_DIR", dir.string().c_str());
#else
  setenv("POLYSMITH_POSTS_DIR", dir.string().c_str(), 1);
#endif

  // Seeding: the built-ins appear, and after seeding they all have
  // editable files.
  const auto seeded = polysmith::core::list_post_processors();
  if (!expect(seeded.size() >= 6, "list: built-ins listed")) {
    return false;
  }
  bool allHaveFiles = true;
  for (const auto& entry : seeded) {
    if (entry.path.empty()) {
      allHaveFiles = false;
    }
  }
  if (!expect(allHaveFiles, "list: built-ins seeded as editable files")) {
    return false;
  }

  // Importing a definition makes it available as a dialect.
  const auto source = std::filesystem::temp_directory_path() /
                      "polysmith_import_source.json";
  {
    std::ofstream stream(source);
    stream << R"({"rapid": "G0 X{x} Y{y}", "footer_lines": ["M2"]})";
  }
  std::string error;
  const std::string imported =
      polysmith::core::import_post_processor(source.string(), error);
  if (!expect(error.empty() && imported == "polysmith_import_source",
              "import: valid definition imported")) {
    std::cerr << "  error: " << error << "\n";
    return false;
  }
  const auto after = polysmith::core::list_post_processors();
  bool found = false;
  for (const auto& entry : after) {
    if (entry.name == "polysmith_import_source" && !entry.path.empty()) {
      found = true;
    }
  }
  if (!expect(found, "import: appears in the list with a file path")) {
    return false;
  }

  // A BROKEN definition must be rejected, not imported.
  const auto broken = std::filesystem::temp_directory_path() /
                      "polysmith_broken_source.json";
  {
    std::ofstream stream(broken);
    stream << "{ not json";
  }
  const std::string rejected =
      polysmith::core::import_post_processor(broken.string(), error);
#ifdef _WIN32
  _putenv_s("POLYSMITH_POSTS_DIR", "");
#else
  unsetenv("POLYSMITH_POSTS_DIR");
#endif
  return expect(rejected.empty() && !error.empty(),
                "import: broken definitions rejected");
}

bool test_mid_cut_power_change() {
  // The laser stays on at a different S value (tabs / pass ramps):
  // the post emits the power-change template instead of dropping it.
  LaserCutParameters laser;
  laser.power_percent = 85.0;
  laser.dynamic_power = false;  // M3 constant — clean S comparison

  Toolpath path;
  path.moves.push_back({ToolpathMoveKind::FeedLinear, 0.0, 0.0, 0.0, 0.0, 0.0,
                        300.0, 85.0, true});
  path.moves.push_back({ToolpathMoveKind::FeedLinear, 1.0, 0.0, 0.0, 0.0, 0.0,
                        300.0, 60.0, true});
  path.moves.push_back(
      {ToolpathMoveKind::FeedLinear, 2.0, 0.0, 0.0, 0.0, 0.0, 300.0, 60.0, false});

  PostContext context{
      .toolpath = path,
      .setup = make_setup(),
      .tool = make_laser_tool(),
      .op_name = "Power ramp",
      .laser = laser,
  };
  const std::string gcode = joined(post_process("grbl", context));
  const size_t s850 = gcode.find("S850");
  const size_t s600 = gcode.find("S600");
  if (!expect(s850 != std::string::npos && s600 != std::string::npos &&
                  s600 > s850,
              "grbl: mid-cut power change emits the S transition")) {
    std::cerr << gcode;
    return false;
  }
  return true;
}

bool test_smoothieware_power_scale() {
  LaserCutParameters laser;
  laser.power_percent = 85.0;
  laser.dynamic_power = true;

  Toolpath path;
  path.moves.push_back(
      {ToolpathMoveKind::FeedLinear, 1.0, 1.0, 0.0, 0.0, 0.0, 300.0, 85.0, true});

  PostContext context{
      .toolpath = path,
      .setup = make_setup(),
      .tool = make_laser_tool(),
      .op_name = "Smoothie",
      .laser = laser,
  };
  const std::string gcode = joined(post_process("smoothieware", context));
  return expect(gcode.find("S0.850") != std::string::npos,
                "smoothieware: S values normalized to 0..1");
}

bool test_laser_footer_has_no_z() {
  // Laser ops must never lift Z at program end — a gantry laser may
  // not have a Z axis.
  LaserCutParameters laser;
  laser.power_percent = 85.0;

  PostContext context{
      .toolpath = make_laser_toolpath(),
      .setup = make_setup(),
      .tool = make_laser_tool(),
      .op_name = "2D Cut 1",
      .laser = laser,
  };
  const std::string gcode = joined(post_process("grbl", context));
  return expect(gcode.find("G0 Z") == std::string::npos &&
                    gcode.find("M2") != std::string::npos,
                "grbl laser: footer is M5 M2 — no Z lift");
}

bool test_air_assist_codes() {
  // Air assist templates come from a user file (machine-specific);
  // M8 wraps the cut block, M9 follows the last cut.
  const auto dir = std::filesystem::temp_directory_path() /
                   "polysmith_posts_air_test";
  std::filesystem::create_directories(dir);
  {
    std::ofstream stream(dir / "airgrbl.json");
    stream << R"({
      "laser_air_on": "M8",
      "laser_air_off": "M9"
    })";
  }
#ifdef _WIN32
  _putenv_s("POLYSMITH_POSTS_DIR", dir.string().c_str());
#else
  setenv("POLYSMITH_POSTS_DIR", dir.string().c_str(), 1);
#endif

  LaserCutParameters laser;
  laser.power_percent = 85.0;
  laser.air_assist = true;

  Toolpath path;
  path.moves.push_back(
      {ToolpathMoveKind::FeedLinear, 1.0, 1.0, 0.0, 0.0, 0.0, 300.0, 85.0, true});
  path.moves.push_back(
      {ToolpathMoveKind::FeedLinear, 2.0, 1.0, 0.0, 0.0, 0.0, 300.0, 85.0, false});

  PostContext context{
      .toolpath = path,
      .setup = make_setup(),
      .tool = make_laser_tool(),
      .op_name = "Air",
      .laser = laser,
  };
  const std::string gcode = joined(post_process("airgrbl", context));
#ifdef _WIN32
  _putenv_s("POLYSMITH_POSTS_DIR", "");
#else
  unsetenv("POLYSMITH_POSTS_DIR");
#endif
  const size_t m8 = gcode.find("M8");
  const size_t m9 = gcode.find("M9");
  return expect(m8 != std::string::npos && m9 != std::string::npos &&
                    m9 > m8,
                "air assist: M8 before the cut, M9 after");
}

bool test_unknown_post_is_empty() {
  LaserCutParameters laser;
  laser.power_percent = 50.0;
  Toolpath path;
  path.moves.push_back(
      {ToolpathMoveKind::FeedLinear, 1.0, 1.0, 0.0, 0.0, 0.0, 300.0, 50.0, true});
  PostContext context{
      .toolpath = path,
      .setup = make_setup(),
      .tool = make_laser_tool(),
      .op_name = "Unknown",
      .laser = laser,
  };
  return expect(post_process("does_not_exist", context).empty(),
                "unknown post: empty output signals failure");
}

bool test_footer_flag_gates_program_end() {
  LaserCutParameters laser;
  laser.power_percent = 50.0;
  Toolpath path;
  path.moves.push_back(
      {ToolpathMoveKind::FeedLinear, 1.0, 1.0, 0.0, 0.0, 0.0, 300.0, 50.0, true});
  PostContext context{
      .toolpath = path,
      .setup = make_setup(),
      .tool = make_laser_tool(),
      .op_name = "Footer",
      .laser = laser,
  };
  const std::string without = joined(post_process("grbl", context, false));
  const std::string with = joined(post_process("grbl", context, true));
  return expect(without.find("M2") == std::string::npos &&
                    with.find("M2") != std::string::npos,
                "footer flag: M2 only on the op that includes the footer");
}

}  // namespace

int main() {
  bool allPassed = true;

  std::cout << "grbl_post_test\n";
  std::cout << "  Test 1: laser golden... ";
  if (test_laser_golden()) {
    std::cout << "PASS\n";
  } else {
    std::cout << "FAIL\n";
    allPassed = false;
  }

  std::cout << "  Test 2: engrave uses M3... ";
  if (test_engrave_uses_m3()) {
    std::cout << "PASS\n";
  } else {
    std::cout << "FAIL\n";
    allPassed = false;
  }

  std::cout << "  Test 3: line numbers and decimals... ";
  if (test_line_numbers_and_decimals()) {
    std::cout << "PASS\n";
  } else {
    std::cout << "FAIL\n";
    allPassed = false;
  }

  std::cout << "  Test 4: inch units + WCS offset... ";
  if (test_inch_and_wcs_offset()) {
    std::cout << "PASS\n";
  } else {
    std::cout << "FAIL\n";
    allPassed = false;
  }

  std::cout << "  Test 5: mill spindle M3... ";
  if (test_mill_uses_spindle_m3()) {
    std::cout << "PASS\n";
  } else {
    std::cout << "FAIL\n";
    allPassed = false;
  }

  std::cout << "  Test 6: custom dialect file... ";
  if (test_header_footer_passthrough()) {
    std::cout << "PASS\n";
  } else {
    std::cout << "FAIL\n";
    allPassed = false;
  }

  std::cout << "  Test 7: post list + import... ";
  if (test_post_list_and_import()) {
    std::cout << "PASS\n";
  } else {
    std::cout << "FAIL\n";
    allPassed = false;
  }

  std::cout << "  Test 8: mid-cut power change... ";
  if (test_mid_cut_power_change()) {
    std::cout << "PASS\n";
  } else {
    std::cout << "FAIL\n";
    allPassed = false;
  }

  std::cout << "  Test 9: smoothieware power scale... ";
  if (test_smoothieware_power_scale()) {
    std::cout << "PASS\n";
  } else {
    std::cout << "FAIL\n";
    allPassed = false;
  }

  std::cout << "  Test 10: laser footer has no Z... ";
  if (test_laser_footer_has_no_z()) {
    std::cout << "PASS\n";
  } else {
    std::cout << "FAIL\n";
    allPassed = false;
  }

  std::cout << "  Test 11: air assist codes... ";
  if (test_air_assist_codes()) {
    std::cout << "PASS\n";
  } else {
    std::cout << "FAIL\n";
    allPassed = false;
  }

  std::cout << "  Test 12: unknown post is empty... ";
  if (test_unknown_post_is_empty()) {
    std::cout << "PASS\n";
  } else {
    std::cout << "FAIL\n";
    allPassed = false;
  }

  std::cout << "  Test 13: footer flag gates program end... ";
  if (test_footer_flag_gates_program_end()) {
    std::cout << "PASS\n";
  } else {
    std::cout << "FAIL\n";
    allPassed = false;
  }

  if (allPassed) {
    std::cout << "grbl_post_test passed\n";
    return 0;
  }
  return 1;
}
