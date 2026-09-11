// LaserGRBL post-processor golden test.
//
// LaserGRBL is a GRBL 1.1 laser-mode host.  This suite pins the
// built-in "lasergrbl" definition: the GRBL dialect plus M8/M9 air
// assist on the coolant relay and S0-before-M5 (the diode driver
// latches the last S value).  It also pins the LaserGRBL machine seed
// and the opt-in boundary: the built-in "grbl" post is unchanged.

#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

#include "core/cam/machine_library.h"
#include "core/cam/post_processor.h"

namespace {

using polysmith::core::CamSetup;
using polysmith::core::LaserCutParameters;
using polysmith::core::MachineDefinition;
using polysmith::core::PostContext;
using polysmith::core::ToolEntry;
using polysmith::core::Toolpath;
using polysmith::core::ToolpathMove;
using polysmith::core::ToolpathMoveKind;
using polysmith::core::load_machine_library;
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

int count_occurrences(const std::string& haystack, const std::string& needle) {
  int count = 0;
  size_t pos = 0;
  while ((pos = haystack.find(needle, pos)) != std::string::npos) {
    ++count;
    pos += needle.size();
  }
  return count;
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

PostContext make_context(const Toolpath& path, LaserCutParameters laser,
                         const std::string& op_name = "2D Cut 1") {
  return PostContext{
      .toolpath = path,
      .setup = make_setup(),
      .tool = make_laser_tool(),
      .op_name = op_name,
      .laser = laser,
  };
}

bool test_golden_laser_cut() {
  LaserCutParameters laser;
  laser.power_percent = 85.0;
  laser.dynamic_power = true;
  laser.mode = "cut";

  const std::string gcode = joined(post_process(
      "lasergrbl", make_context(make_laser_toolpath(), laser)));

  // Header + power/arc/dwell/feed basics.
  if (!expect(gcode.find("(op: 2D Cut 1)") != std::string::npos &&
                  gcode.find("G21") != std::string::npos &&
                  gcode.find("G90") != std::string::npos &&
                  gcode.find("G94") != std::string::npos &&
                  gcode.find("G17") != std::string::npos &&
                  gcode.find("S850.000") != std::string::npos &&
                  gcode.find("M4") != std::string::npos,
              "lasergrbl: header + S850 + M4 for 85% dynamic power")) {
    std::cerr << gcode;
    return false;
  }
  if (!expect(gcode.find("G2 X4.000 Y7.000 I0.000 J2.000") !=
                      std::string::npos &&
                  gcode.find("G3 X4.000 Y3.000 I0.000 J-2.000") !=
                      std::string::npos &&
                  gcode.find("G4 P0.300") != std::string::npos &&
                  gcode.find("F500.000") != std::string::npos,
              "lasergrbl: G2/G3 I/J arcs + pierce dwell + feed")) {
    std::cerr << gcode;
    return false;
  }
  // LaserGRBL conventions: no Z words, no line numbers, S0 before the
  // lead-out M5, footer M5/M2, exactly one M2.
  if (!expect(gcode.find('Z') == std::string::npos &&
                  gcode.find("N10") == std::string::npos &&
                  gcode.find("S0.000\nM5") != std::string::npos &&
                  gcode.find("M5\nM2\n") != std::string::npos &&
                  count_occurrences(gcode, "M2") == 1,
              "lasergrbl: no Z, no N, S0-before-M5, single M5/M2 footer")) {
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

  const std::string gcode = joined(
      post_process("lasergrbl", make_context(path, laser, "Engrave 1")));
  return expect(gcode.find("S300.000") != std::string::npos &&
                    gcode.find("M3") != std::string::npos &&
                    gcode.find("M4") == std::string::npos,
                "lasergrbl: engrave uses M3 (constant), never M4");
}

bool test_s0_at_program_end() {
  // Beam still on at the last move → S0/M5/M2 with no duplicate M5.
  LaserCutParameters laser;
  laser.power_percent = 85.0;

  Toolpath path;
  path.moves.push_back({ToolpathMoveKind::FeedLinear, 1.0, 1.0, 0.0, 0.0, 0.0,
                        300.0, 85.0, true});

  const std::string gcode = joined(
      post_process("lasergrbl", make_context(path, laser, "S0End")));
  return expect(gcode.find("S0.000\nM5\nM2") != std::string::npos &&
                    gcode.find("M5\nM5") == std::string::npos,
                "lasergrbl: program end reads S0/M5/M2, no duplicate M5");
}

bool test_air_assist() {
  LaserCutParameters laser;
  laser.power_percent = 50.0;
  laser.air_assist = true;

  Toolpath path;
  path.moves.push_back({ToolpathMoveKind::FeedLinear, 1.0, 1.0, 0.0, 0.0, 0.0,
                        300.0, 50.0, true});
  path.moves.push_back(
      {ToolpathMoveKind::FeedLinear, 1.0, 2.0, 0.0, 0.0, 0.0, 300.0, 50.0, false});

  const std::string gcode = joined(
      post_process("lasergrbl", make_context(path, laser, "Air")));
  const size_t airOn = gcode.find("M8");
  const size_t powerOn = gcode.find("M4 S500.000");
  const size_t firstFeed = gcode.find("G1 X1.000");
  const size_t airOff = gcode.find("M9");
  return expect(airOn != std::string::npos && airOn < powerOn &&
                    airOn < firstFeed && airOff != std::string::npos &&
                    airOff > gcode.find("G1 X1.000 Y2.000"),
                "lasergrbl: M8 before the cut, M9 after it");
}

bool test_power_scale() {
  LaserCutParameters laser;
  laser.power_percent = 50.0;

  Toolpath path;
  path.moves.push_back({ToolpathMoveKind::FeedLinear, 1.0, 1.0, 0.0, 0.0, 0.0,
                        300.0, 50.0, true});

  const std::string gcode = joined(
      post_process("lasergrbl", make_context(path, laser, "Scale")));
  return expect(gcode.find("S500.000") != std::string::npos,
                "lasergrbl: 50% scales to S500 (power_max 1000)");
}

bool test_machine_seed() {
  const auto dir = std::filesystem::temp_directory_path() /
                   "polysmith_machines_lasergrbl_test";
  std::filesystem::remove_all(dir);
  std::filesystem::create_directories(dir);
#ifdef _WIN32
  _putenv_s("POLYSMITH_MACHINES_DIR", dir.string().c_str());
#else
  setenv("POLYSMITH_MACHINES_DIR", dir.string().c_str(), 1);
#endif

  const auto machines = load_machine_library();
#ifdef _WIN32
  _putenv_s("POLYSMITH_MACHINES_DIR", "");
#else
  unsetenv("POLYSMITH_MACHINES_DIR");
#endif

  const MachineDefinition* found = nullptr;
  for (const auto& machine : machines) {
    if (machine.name == "LaserGRBL") {
      found = &machine;
      break;
    }
  }
  return expect(found != nullptr &&
                    found->machine_type == "laser" &&
                    found->post_processor.type == "lasergrbl" &&
                    found->work_area_x_mm == 430.0 &&
                    found->work_area_y_mm == 430.0,
                "machine seed: LaserGRBL carries the lasergrbl post + 430x430");
}

bool test_grbl_builtin_unchanged() {
  // The S0-before-M5 behavior is opt-in per post — the built-in grbl
  // must stay byte-identical for existing users.
  LaserCutParameters laser;
  laser.power_percent = 85.0;

  const std::string gcode = joined(post_process(
      "grbl", make_context(make_laser_toolpath(), laser, "GrblPin")));
  return expect(gcode.find("S0.000") == std::string::npos &&
                    gcode.find("M5\nM2\n") != std::string::npos,
                "grbl regression pin: no S0, plain M5/M2 footer");
}

bool test_laser_off_before_travel_rapid() {
  // A rapid between regions must travel with the beam OFF — GRBL
  // latches the last S, so an unguarded rapid keeps firing across
  // the part.  Pins the post engine's rapid branch running the power
  // state machine (bug: the beam stayed on during inter-region
  // travel).  The lasergrbl post emits S0 before the M5.
  LaserCutParameters laser;
  laser.power_percent = 85.0;

  Toolpath path;
  path.moves.push_back({ToolpathMoveKind::FeedLinear, 1.0, 1.0, 0.0, 0.0, 0.0,
                        500.0, 85.0, true});  // region 1 cut
  path.moves.push_back({ToolpathMoveKind::Rapid, 10.0, 10.0, 0.0, 0.0, 0.0,
                        0.0, 0.0, false});  // travel, beam off
  path.moves.push_back({ToolpathMoveKind::FeedLinear, 11.0, 10.0, 0.0, 0.0, 0.0,
                        500.0, 85.0, true});  // region 2 pierce

  const std::string gcode = joined(
      post_process("lasergrbl", make_context(path, laser, "Travel")));
  const size_t travel = gcode.find("G0 X10.000 Y10.000");
  const size_t reOn = gcode.find("M4 S850.000", travel);  // after the travel
  const size_t feed2 = gcode.find("G1 X11.000 Y10.000", travel);
  if (!expect(gcode.find("S0.000\nM5\nG0 X10.000 Y10.000") !=
                      std::string::npos &&
                  reOn != std::string::npos && reOn < feed2,
              "travel: S0/M5 before the inter-region rapid, M4 re-arms")) {
    std::cerr << gcode;
    return false;
  }
  return true;
}

}  // namespace

int main() {
  bool allPassed = true;
  const auto run = [&](const char* label, bool (*test)()) {
    std::cout << "  " << label << "... ";
    if (test()) {
      std::cout << "PASS\n";
    } else {
      std::cout << "FAIL\n";
      allPassed = false;
    }
  };

  std::cout << "lasergrbl_post_test\n";
  run("Test 1: golden laser cut", test_golden_laser_cut);
  run("Test 2: engrave uses M3", test_engrave_uses_m3);
  run("Test 3: S0 at program end", test_s0_at_program_end);
  run("Test 4: air assist M8/M9", test_air_assist);
  run("Test 5: power scale", test_power_scale);
  run("Test 6: LaserGRBL machine seed", test_machine_seed);
  run("Test 7: built-in grbl unchanged", test_grbl_builtin_unchanged);
  run("Test 9: laser off before inter-region travel rapid",
      test_laser_off_before_travel_rapid);

  if (allPassed) {
    std::cout << "lasergrbl_post_test passed\n";
    return 0;
  }
  return 1;
}
