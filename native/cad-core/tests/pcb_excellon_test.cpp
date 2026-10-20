// Regression suite for the Excellon drill parser (P1c).
//
// The committed fixtures are KiCad 10.0.4 plots: `pth.drl` is a real 13-tool
// plated file with 511 holes in decimal metric coordinates, and `npth.drl` is
// the degenerate case — a valid header with zero holes, which must parse as a
// *successful* empty program rather than an error.  Synthetic cases cover the
// dialects KiCad does not emit: inch units, fixed-point (point-less)
// coordinates, repeats, and slots.

#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>

#include "core/pcb/excellon_parser.h"

namespace {

using polysmith::core::pcb::DrillFile;
using polysmith::core::pcb::PcbUnit;
using polysmith::core::pcb::parse_excellon;

bool expect(bool condition, const std::string& message) {
  if (condition) {
    return true;
  }
  std::cerr << "  FAIL: " << message << "\n";
  return false;
}

bool near(double actual, double expected, double tolerance = 1e-6) {
  return std::abs(actual - expected) <= tolerance;
}

std::string read_fixture(const std::string& name) {
  const std::string path = "native/cad-core/tests/data/pcb/" + name;
  std::error_code error;
  if (!std::filesystem::exists(path, error)) {
    return std::string();
  }
  std::ifstream stream(path, std::ios::binary);
  return std::string((std::istreambuf_iterator<char>(stream)),
                     std::istreambuf_iterator<char>());
}

bool has_diagnostic(const DrillFile& file, const std::string& needle) {
  for (const auto& diagnostic : file.diagnostics) {
    if (diagnostic.message.find(needle) != std::string::npos) {
      return true;
    }
  }
  return false;
}

bool test_committed_plated_drill_file() {
  const std::string text = read_fixture("pth.drl");
  if (text.empty()) {
    std::cout << "  (skipped: tests/data/pcb/pth.drl not found)\n";
    return true;
  }
  const DrillFile file = parse_excellon(text);
  bool ok = expect(file.ok, "plated drill file parses");
  ok &= expect(file.unit == PcbUnit::Millimeter, "METRIC header");
  ok &= expect(file.plated && file.plated_known, "TF.FileFunction says Plated");
  ok &= expect(file.file_function == "Plated,1,4,PTH", "role attribute is preserved");
  ok &= expect(file.tools.size() == 13, "13 tool definitions");
  ok &= expect(file.hits.size() == 511, "511 holes");
  ok &= expect(file.hits_without_tool == 0, "every hole has a defined tool");
  ok &= expect(file.tools.count(1) == 1 && near(file.tools.at(1).diameter_mm, 0.4),
               "T1 is a 0.4 mm via drill");
  ok &= expect(near(file.tools.at(13).diameter_mm, 4.5), "T13 is 4.5 mm");
  ok &= expect(file.tools.at(1).function == "Plated,PTH,ViaDrill",
               "per-tool function attribute is attached");
  ok &= expect(file.tools.at(12).function == "Plated,PTH,ComponentDrill",
               "component drills are distinguishable from vias");
  ok &= expect(file.diagnostics.empty(), "the real file parses without diagnostics");
  ok &= expect(file.unsupported.empty(), "no unsupported constructs in the real file");
  ok &= expect(file.bounds.valid && file.bounds.max_x > 90.0 && file.bounds.max_y > 90.0,
               "bounds cover the 99.9 mm board");
  return ok;
}

bool test_committed_non_plated_file_is_a_valid_empty_program() {
  const std::string text = read_fixture("npth.drl");
  if (text.empty()) {
    std::cout << "  (skipped: tests/data/pcb/npth.drl not found)\n";
    return true;
  }
  const DrillFile file = parse_excellon(text);
  bool ok = expect(file.ok, "a header-only drill file is a valid empty program");
  ok &= expect(!file.plated && file.plated_known, "TF.FileFunction says NonPlated");
  ok &= expect(file.file_function == "NonPlated,1,4,NPTH", "role attribute");
  ok &= expect(file.tools.empty() && file.hits.empty(), "no tools, no holes");
  ok &= expect(!file.bounds.valid, "no bounds without hits");
  return ok;
}

bool test_decimal_metric_program() {
  const std::string text =
      "M48\n"
      "; DRILL file\n"
      "FMAT,2\n"
      "METRIC\n"
      "T1C0.800\n"
      "%\n"
      "G90\n"
      "G05\n"
      "T1\n"
      "X1.5Y-2.25\n"
      "X10Y20\n"
      "M30\n";
  const DrillFile file = parse_excellon(text);
  bool ok = expect(file.ok, "synthetic program parses");
  ok &= expect(file.hits.size() == 2, "two holes");
  if (file.hits.size() == 2) {
    ok &= expect(near(file.hits[0].x, 1.5) && near(file.hits[0].y, -2.25),
                 "negative decimal coordinates");
    ok &= expect(file.hits[0].tool == 1, "the hole carries its tool");
    ok &= expect(near(file.hits[1].x, 10.0) && near(file.hits[1].y, 20.0),
                 "whole-millimetre coordinates");
  }
  return ok;
}

bool test_inch_units_scale_diameters_and_coordinates() {
  const std::string text =
      "M48\n"
      "INCH,TZ\n"
      "T1C0.032\n"
      "%\n"
      "T1\n"
      "X1.5Y-2.25\n"
      "M30\n";
  const DrillFile file = parse_excellon(text);
  bool ok = expect(file.unit == PcbUnit::Inch, "INCH header");
  ok &= expect(file.tools.size() == 1 && near(file.tools.at(1).diameter_mm, 0.032 * 25.4),
               "tool diameter converts to millimetres");
  if (file.hits.empty()) {
    return false;
  }
  ok &= expect(near(file.hits[0].x, 1.5 * 25.4) && near(file.hits[0].y, -2.25 * 25.4, 1e-6),
               "coordinates convert to millimetres");
  return ok;
}

bool test_fixed_point_coordinates_use_the_format_hint() {
  const std::string text =
      "M48\n"
      "; FORMAT={3:3/ absolute / metric / leading}\n"
      "FMAT,2\n"
      "METRIC\n"
      "T1C0.800\n"
      "%\n"
      "T1\n"
      "X001500Y002250\n"
      "M30\n";
  const DrillFile file = parse_excellon(text);
  bool ok = expect(file.ok, "fixed-point program parses");
  if (file.hits.empty()) {
    return false;
  }
  ok &= expect(near(file.hits[0].x, 1.5) && near(file.hits[0].y, 2.25),
               "the FORMAT hint supplies the fractional digits");
  ok &= expect(file.diagnostics.empty(), "no diagnostic needed when the hint is present");
  return ok;
}

bool test_point_less_coordinates_without_a_hint_warn_once() {
  const std::string text =
      "M48\n"
      "METRIC\n"
      "T1C0.800\n"
      "%\n"
      "T1\n"
      "X1500Y2250\n"
      "X3000Y4500\n"
      "M30\n";
  const DrillFile file = parse_excellon(text);
  if (file.hits.size() != 2) {
    return expect(false, "two holes");
  }
  bool ok = expect(near(file.hits[0].x, 1.5) && near(file.hits[1].x, 3.0),
                   "metric default of three fractional digits");
  int warnings = 0;
  for (const auto& diagnostic : file.diagnostics) {
    if (diagnostic.message.find("FORMAT hint") != std::string::npos) {
      ++warnings;
    }
  }
  ok &= expect(warnings == 1, "the assumption is reported exactly once");
  return ok;
}

bool test_incremental_mode() {
  const std::string text =
      "M48\n"
      "; FORMAT={-:-/ absolute / metric / decimal}\n"
      "METRIC\n"
      "T1C0.500\n"
      "%\n"
      "G91\n"
      "T1\n"
      "X1.5Y2.5\n"
      "X1Y0\n"
      "M30\n";
  const DrillFile file = parse_excellon(text);
  bool ok = expect(file.hits.size() == 2, "two incremental holes");
  if (file.hits.size() != 2) {
    return false;
  }
  ok &= expect(near(file.hits[0].x, 1.5) && near(file.hits[0].y, 2.5), "first hole");
  ok &= expect(near(file.hits[1].x, 2.5) && near(file.hits[1].y, 2.5),
               "incremental move accumulates on X and leaves Y untouched");
  return ok;
}

bool test_repeat_repeats_the_previous_hole_with_a_step() {
  // KiCad semantics (qa/tests/gerbview/test_excellon_repeat.cpp): `R<n>` with
  // an X/Y step repeats the PREVIOUS hole n more times, stepping each time —
  // `R07X5000` after a hole gives 8 holes 5 mm apart.
  const std::string text =
      "M48\n"
      "; FORMAT={-:-/ absolute / metric / decimal}\n"
      "METRIC\n"
      "T1C0.500\n"
      "%\n"
      "G90\n"
      "T1\n"
      "X10Y10\n"
      "R2\n"
      "X10Y20\n"
      "R3Y5\n"
      "M30\n";
  const DrillFile file = parse_excellon(text);
  bool ok = expect(file.hits.size() == 7, "1 + R2 + 1 + R3 = 7 holes");
  if (file.hits.size() != 7) {
    return false;
  }
  ok &= expect(near(file.hits[1].x, 10.0) && near(file.hits[2].x, 10.0),
               "R2 without a step repeats in place");
  ok &= expect(near(file.hits[3].x, 10.0) && near(file.hits[3].y, 20.0),
               "the explicit hole after a repeat is unaffected");
  ok &= expect(near(file.hits[4].y, 25.0) && near(file.hits[5].y, 30.0) &&
                   near(file.hits[6].y, 35.0),
               "R3Y5 steps 5 mm in Y per repeat");
  ok &= expect(file.hits_without_tool == 0, "repeats inherit the selected tool");
  return ok;
}

bool test_slots_and_route_mode_degrade() {
  const std::string text =
      "M48\n"
      "METRIC\n"
      "T1C0.800\n"
      "%\n"
      "T1\n"
      "X1Y1G85X2Y2\n"
      "M15\n"
      "G00X5Y5\n"
      "X3Y3\n"
      "M30\n";
  const DrillFile file = parse_excellon(text);
  bool ok = expect(file.ok, "a file with slots still parses");
  bool slot = false;
  bool route = false;
  for (const auto& entry : file.unsupported) {
    slot = slot || entry.find("G85") != std::string::npos;
    route = route || entry.find("route") != std::string::npos;
  }
  ok &= expect(slot, "slots are reported as unsupported");
  ok &= expect(route, "route mode is reported as unsupported");
  ok &= expect(file.hits.size() == 1, "the plain hole after them still loads");
  return ok;
}

bool test_bad_input_is_reported_not_thrown() {
  bool ok = true;
  const DrillFile empty = parse_excellon("");
  ok &= expect(!empty.ok, "empty input is not a program");
  ok &= expect(!empty.diagnostics.empty(), "empty input explains itself");

  const DrillFile junk = parse_excellon("\x01\x02\x03 nonsense\n\x7f\x80");
  ok &= expect(!junk.ok, "binary junk is rejected");
  ok &= expect(!junk.diagnostics.empty(), "junk is reported");

  const DrillFile undefined_tool = parse_excellon(
      "M48\nMETRIC\n%\nT7\nX1Y1\nM30\n");
  ok &= expect(has_diagnostic(undefined_tool, "T7"), "an undefined tool is named");
  ok &= expect(undefined_tool.hits_without_tool == 1,
               "the hole is counted as having no tool rather than dropped silently");
  return ok;
}

}  // namespace

int main() {
  struct Suite {
    const char* name;
    bool (*run)();
  };
  const Suite suites[] = {
      {"committed_plated_drill_file", test_committed_plated_drill_file},
      {"committed_non_plated_file_is_a_valid_empty_program",
       test_committed_non_plated_file_is_a_valid_empty_program},
      {"decimal_metric_program", test_decimal_metric_program},
      {"inch_units_scale_diameters_and_coordinates",
       test_inch_units_scale_diameters_and_coordinates},
      {"fixed_point_coordinates_use_the_format_hint",
       test_fixed_point_coordinates_use_the_format_hint},
      {"point_less_coordinates_without_a_hint_warn_once",
       test_point_less_coordinates_without_a_hint_warn_once},
      {"incremental_mode", test_incremental_mode},
      {"repeat_repeats_the_previous_hole_with_a_step",
       test_repeat_repeats_the_previous_hole_with_a_step},
      {"slots_and_route_mode_degrade", test_slots_and_route_mode_degrade},
      {"bad_input_is_reported_not_thrown", test_bad_input_is_reported_not_thrown},
  };

  int failures = 0;
  for (const Suite& suite : suites) {
    std::cout << "case " << suite.name << "\n";
    if (!suite.run()) {
      ++failures;
    }
  }
  if (failures != 0) {
    std::cerr << failures << " case(s) failed\n";
    return 1;
  }
  std::cout << "pcb excellon parser: all cases passed\n";
  return 0;
}
