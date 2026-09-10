// LinuxCNC post-processor golden test (5-axis scaffolding).
//
// Builds small mill toolpath IRs by hand and asserts the exact G-code
// the writer emits for rotary words: modal A emitted once then
// suppressed, no rotary words when the toolpath is 3-axis only,
// inverse-time feed (G93, F = feedrate / path length) with the G94
// restore, and rotary words on rapids (no G93 for non-feed moves).

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include "core/cam/post_processor.h"

namespace {

using polysmith::core::CamSetup;
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

CamSetup make_setup() {
  CamSetup setup;
  setup.name = "Setup";
  setup.safety_height = 5.0;
  return setup;
}

ToolEntry make_mill_tool() {
  ToolEntry tool;
  tool.name = "Endmill";
  tool.type = "endmill_flat";
  return tool;
}

ToolpathMove rapid(double x, double y, double z) {
  ToolpathMove move;
  move.kind = ToolpathMoveKind::Rapid;
  move.x = x;
  move.y = y;
  move.z = z;
  move.laser_on = false;
  return move;
}

ToolpathMove feed(double x, double y, double z, double feedrate) {
  ToolpathMove move;
  move.kind = ToolpathMoveKind::FeedLinear;
  move.x = x;
  move.y = y;
  move.z = z;
  move.feedrate_mm_per_min = feedrate;
  move.laser_on = false;
  return move;
}

ToolpathMove drill(double x, double y, double z, double r_plane, double peck,
                   double feedrate) {
  ToolpathMove move;
  move.kind = ToolpathMoveKind::DrillCycle;
  move.x = x;
  move.y = y;
  move.z = z;
  move.r_plane_z = r_plane;
  move.peck_depth_mm = peck;
  move.feedrate_mm_per_min = feedrate;
  move.laser_on = false;
  return move;
}

std::string joined(const std::vector<std::string>& lines) {
  std::string result;
  for (const auto& line : lines) {
    result += line + "\n";
  }
  return result;
}

// Counts non-overlapping occurrences of `needle` in `hay`.
int count_occurrences(const std::string& hay, const std::string& needle) {
  int count = 0;
  size_t pos = 0;
  while ((pos = hay.find(needle, pos)) != std::string::npos) {
    ++count;
    pos += needle.size();
  }
  return count;
}

// A changes 0 → 0 → 45 → 45: exactly one A0.000 and one A45.000, and
// no B/C words anywhere.
bool test_modal_rotary_words() {
  Toolpath path;
  path.moves.push_back(rapid(0.0, 0.0, 5.0));
  auto move = feed(1.0, 0.0, 0.0, 500.0);
  move.a = 0.0;
  path.moves.push_back(move);
  move = feed(2.0, 0.0, 0.0, 500.0);
  move.a = 0.0;
  path.moves.push_back(move);
  move = feed(3.0, 0.0, 0.0, 500.0);
  move.a = 45.0;
  path.moves.push_back(move);
  move = feed(4.0, 0.0, 0.0, 500.0);
  move.a = 45.0;
  path.moves.push_back(move);

  PostContext context{
      .toolpath = path,
      .setup = make_setup(),
      .tool = make_mill_tool(),
      .op_name = "Rotary 1",
  };
  const std::string gcode = joined(post_process("linuxcnc", context));
  return expect(count_occurrences(gcode, "A0.000") == 1,
                "linuxcnc: A0.000 emitted once (modal)") &&
         expect(count_occurrences(gcode, "A45.000") == 1,
                "linuxcnc: A45.000 emitted once (modal)") &&
         expect(gcode.find(" B") == std::string::npos,
                "linuxcnc: no B word when B absent") &&
         expect(gcode.find(" C") == std::string::npos,
                "linuxcnc: no C word when C absent");
}

// A pure 3-axis toolpath through the linuxcnc post (which declares
// feed_inverse_time) must not emit G93 or any rotary word.
bool test_no_rotary_no_g93() {
  Toolpath path;
  path.moves.push_back(rapid(0.0, 0.0, 5.0));
  path.moves.push_back(rapid(0.0, 0.0, 0.0));
  path.moves.push_back(feed(10.0, 0.0, 0.0, 500.0));
  path.moves.push_back(feed(10.0, 10.0, 0.0, 500.0));

  PostContext context{
      .toolpath = path,
      .setup = make_setup(),
      .tool = make_mill_tool(),
      .op_name = "Pocket 1",
  };
  const std::string gcode = joined(post_process("linuxcnc", context));
  return expect(gcode.find("G93") == std::string::npos,
                "linuxcnc: no G93 for a 3-axis toolpath") &&
         expect(gcode.find(" A") == std::string::npos &&
                     gcode.find(" B") == std::string::npos &&
                     gcode.find(" C") == std::string::npos,
                "linuxcnc: no rotary words for a 3-axis toolpath");
}

// G93 pricing: feed 500 mm/min over a 10 mm rotary move → F50.000,
// emitted once; the modal repeat does not re-emit; the next plain
// 3-axis move restores G94 with F in mm/min.
bool test_inverse_time_feed() {
  Toolpath path;
  path.moves.push_back(rapid(0.0, 0.0, 5.0));
  path.moves.push_back(rapid(0.0, 0.0, 0.0));
  auto move = feed(10.0, 0.0, 0.0, 500.0);
  move.a = 90.0;
  path.moves.push_back(move);
  move = feed(10.0, 10.0, 0.0, 500.0);
  move.a = 90.0;
  path.moves.push_back(move);
  path.moves.push_back(feed(20.0, 10.0, 0.0, 500.0));

  PostContext context{
      .toolpath = path,
      .setup = make_setup(),
      .tool = make_mill_tool(),
      .op_name = "Rotary 2",
  };
  const std::string gcode = joined(post_process("linuxcnc", context));
  if (!expect(count_occurrences(gcode, "F50.000") == 1,
              "linuxcnc: inverse-time F emitted once")) {
    std::cerr << gcode;
    return false;
  }
  if (!expect(count_occurrences(gcode, "A90.000") == 1,
              "linuxcnc: A90.000 emitted once (modal)")) {
    std::cerr << gcode;
    return false;
  }
  const size_t inverseFeed = gcode.find("F50.000");
  if (!expect(inverseFeed != std::string::npos &&
                  gcode.find("G93") < inverseFeed,
              "linuxcnc: G93 precedes the inverse-time F")) {
    std::cerr << gcode;
    return false;
  }
  const size_t restore = gcode.find("G94", inverseFeed);
  if (!expect(restore != std::string::npos &&
                  gcode.find("F500.000", restore) != std::string::npos,
              "linuxcnc: G94 restores mm/min feed after rotary moves")) {
    std::cerr << gcode;
    return false;
  }
  // The rotary word rides the same G1 line as the inverse-time F.
  const size_t aWord = gcode.find("A90.000");
  const size_t lineStart = gcode.rfind("\n", aWord) + 1;
  const size_t lineEnd = gcode.find("\n", aWord);
  return expect(aWord != std::string::npos && lineEnd != std::string::npos &&
                    gcode.find("F50.000", lineStart) < lineEnd,
                "linuxcnc: rotary word on the same line as the inverse-time F");
}

// A rapid carrying a rotary word gets the A word appended, but rapids
// are not feed moves — no G93 may appear.
bool test_rapid_carries_rotary_word() {
  Toolpath path;
  path.moves.push_back(rapid(0.0, 0.0, 5.0));
  auto move = rapid(10.0, 0.0, 5.0);
  move.a = 30.0;
  path.moves.push_back(move);

  PostContext context{
      .toolpath = path,
      .setup = make_setup(),
      .tool = make_mill_tool(),
      .op_name = "Rotary 3",
  };
  const std::string gcode = joined(post_process("linuxcnc", context));
  return expect(gcode.find("A30.000") != std::string::npos,
                "linuxcnc: rapid carries the rotary word") &&
         expect(gcode.find("G93") == std::string::npos,
                "linuxcnc: no G93 for a rapid-only toolpath");
}

// Canned G81: the linuxcnc built-in declares canned_cycles — one
// G81 line per hole with X/Y/Z/R/F, no longhand plunge, no
// duplicated rapid (the cycle starts from the preceding rapid).
bool test_canned_g81() {
  Toolpath path;
  path.moves.push_back(rapid(10.0, 5.0, 20.0));
  path.moves.push_back(drill(10.0, 5.0, -5.0, 20.0, 0.0, 200.0));

  PostContext context{
      .toolpath = path,
      .setup = make_setup(),
      .tool = make_mill_tool(),
      .op_name = "Drill 1",
      .spindle_rpm = 12000.0,
  };
  const std::string gcode = joined(post_process("linuxcnc", context));
  if (!expect(gcode.find("G81 X10.000 Y5.000 Z-5.000 R20.000 F200.000") !=
                  std::string::npos,
              "linuxcnc: canned G81 with X/Y/Z/R/F")) {
    std::cerr << gcode;
    return false;
  }
  return expect(count_occurrences(gcode, "Z-5.000") == 1 &&
                    gcode.find("G1 X10.000") == std::string::npos,
                "linuxcnc: canned cycle replaces the longhand plunge");
}

// Canned G83: the peck template carries Q for the peck depth.
bool test_canned_g83_peck() {
  Toolpath path;
  path.moves.push_back(rapid(10.0, 5.0, 20.0));
  path.moves.push_back(drill(10.0, 5.0, -5.0, 20.0, 2.5, 200.0));

  PostContext context{
      .toolpath = path,
      .setup = make_setup(),
      .tool = make_mill_tool(),
      .op_name = "Drill 2",
  };
  const std::string gcode = joined(post_process("linuxcnc", context));
  return expect(
      gcode.find("G83 X10.000 Y5.000 Z-5.000 R20.000 Q2.500 F200.000") !=
          std::string::npos,
      "linuxcnc: canned G83 carries Q for the peck depth");
}

// The canned capability is per-dialect: linuxcnc/mach3/mach4/fanuc
// emit canned cycles; grbl/marlin/smoothieware stay longhand.
bool test_builtin_canned_matrix() {
  Toolpath path;
  path.moves.push_back(rapid(10.0, 5.0, 20.0));
  path.moves.push_back(drill(10.0, 5.0, -5.0, 20.0, 0.0, 200.0));

  PostContext context{
      .toolpath = path,
      .setup = make_setup(),
      .tool = make_mill_tool(),
      .op_name = "Drill 3",
  };
  for (const char* canned : {"linuxcnc", "mach3", "mach4", "fanuc"}) {
    const std::string gcode = joined(post_process(canned, context));
    if (gcode.find("G81 X10.000 Y5.000 Z-5.000 R20.000") ==
        std::string::npos) {
      std::cerr << "FAIL: " << canned << " missing the canned G81\n";
      return false;
    }
  }
  for (const char* longhand : {"grbl", "marlin", "smoothieware"}) {
    const std::string gcode = joined(post_process(longhand, context));
    if (gcode.find("G81") != std::string::npos ||
        gcode.find("Z-5") == std::string::npos) {
      std::cerr << "FAIL: " << longhand << " must drill longhand\n";
      return false;
    }
  }
  return true;
}

// A stale user post file (seeded before canned_cycles existed) has no
// cycle keys: drilling must degrade to longhand.  Even a post that
// declares canned_cycles WITHOUT a template stays longhand — the
// capability requires both.
bool test_stale_post_drilling_longhand() {
  const auto dir = std::filesystem::temp_directory_path() /
                   "polysmith_posts_stale_drill_test";
  std::filesystem::create_directories(dir);
  {
    std::ofstream stream(dir / "linuxcnc.json");
    stream << R"JSON({
      "rapid": "G0 X{x} Y{y}",
      "feed": "G1 X{x} Y{y}",
      "spindle_on": "M3 S{rpm}",
      "spindle_off": "M5",
      "footer_lines": ["M5", "M2"],
      "decimal_places": 3,
      "line_numbers": false
    })JSON";
  }
#ifdef _WIN32
  _putenv_s("POLYSMITH_POSTS_DIR", dir.string().c_str());
#else
  setenv("POLYSMITH_POSTS_DIR", dir.string().c_str(), 1);
#endif

  Toolpath path;
  path.moves.push_back(rapid(10.0, 5.0, 20.0));
  path.moves.push_back(drill(10.0, 5.0, -5.0, 20.0, 0.0, 200.0));
  PostContext context{
      .toolpath = path,
      .setup = make_setup(),
      .tool = make_mill_tool(),
      .op_name = "Drill 4",
  };
  const std::string stale = joined(post_process("linuxcnc", context));
  if (!expect(stale.find("G81") == std::string::npos &&
                  stale.find("G1 X10.000 Y5.000 Z-5.000") !=
                      std::string::npos,
              "stale post: drilling degrades to longhand")) {
    std::cerr << stale;
    return false;
  }

  // Declares the flag but no template: still longhand.
  {
    std::ofstream stream(dir / "linuxcnc.json", std::ios::trunc);
    stream << R"JSON({
      "rapid": "G0 X{x} Y{y}",
      "feed": "G1 X{x} Y{y}",
      "canned_cycles": true,
      "footer_lines": ["M5", "M2"],
      "decimal_places": 3,
      "line_numbers": false
    })JSON";
  }
  const std::string flaggedOnly = joined(post_process("linuxcnc", context));
#ifdef _WIN32
  _putenv_s("POLYSMITH_POSTS_DIR", "");
#else
  unsetenv("POLYSMITH_POSTS_DIR");
#endif
  return expect(flaggedOnly.find("G81") == std::string::npos &&
                    flaggedOnly.find("G1 X10.000 Y5.000 Z-5.000") !=
                        std::string::npos,
                "stale post: canned flag without a template stays longhand");
}

}  // namespace

int main() {
  bool ok = true;
  ok = test_modal_rotary_words() && ok;
  ok = test_no_rotary_no_g93() && ok;
  ok = test_inverse_time_feed() && ok;
  ok = test_rapid_carries_rotary_word() && ok;
  ok = test_canned_g81() && ok;
  ok = test_canned_g83_peck() && ok;
  ok = test_builtin_canned_matrix() && ok;
  ok = test_stale_post_drilling_longhand() && ok;
  if (ok) {
    std::cout << "linuxcnc_post_test: all tests passed\n";
    return 0;
  }
  std::cerr << "linuxcnc_post_test: FAILURES\n";
  return 1;
}
