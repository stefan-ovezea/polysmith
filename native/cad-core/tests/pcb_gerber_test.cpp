// Regression suite for the Gerber parser (native/cad-core/src/core/pcb).
//
// The snippets below are lifted from a real KiCad 10.0.4 plot of a
// 4-layer board (F_Cu header, the RoundRect aperture macro KiCad emits
// for every SMD pad, an Edge_Cuts arc).  Synthetic cases cover what that
// board does not exercise: inch units, trailing-zero suppression,
// incremental coordinates, and malformed input — the parser is a
// user-input surface, so "no crash, diagnostics instead" is part of the
// contract, not a nicety.

#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <vector>

#include "core/pcb/gerber_parser.h"

namespace {

using polysmith::core::pcb::ApertureKind;
using polysmith::core::pcb::GerberFile;
using polysmith::core::pcb::GerberOperation;
using polysmith::core::pcb::PcbUnit;
using polysmith::core::pcb::ZeroSuppression;
using polysmith::core::pcb::evaluate_macro_expression;
using polysmith::core::pcb::parse_gerber;
using polysmith::core::pcb::resolve_macro;

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

bool has_diagnostic(const GerberFile& file, const std::string& needle) {
  for (const auto& diagnostic : file.diagnostics) {
    if (diagnostic.message.find(needle) != std::string::npos) {
      return true;
    }
  }
  return false;
}

bool has_unsupported(const GerberFile& file, const std::string& needle) {
  for (const auto& entry : file.unsupported) {
    if (entry.find(needle) != std::string::npos) {
      return true;
    }
  }
  return false;
}

// The F_Cu header, verbatim structure from the fixture: KiCad writes its
// X2 attributes as `G04 #@! …` comments rather than %TF…% statements.
const char* kCopperHeader =
    "G04 #@! TF.GenerationSoftware,KiCad,Pcbnew,10.0.4*\n"
    "G04 #@! TF.FileFunction,Copper,L1,Top*\n"
    "G04 #@! TF.FilePolarity,Positive*\n"
    "%FSLAX46Y46*%\n"
    "G04 Gerber Fmt 4.6, Leading zero omitted, Abs format (unit mm)*\n"
    "%MOMM*%\n"
    "%LPD*%\n"
    "G01*\n";

const char* kRoundRectMacro =
    "%AMRoundRect*\n"
    "0 Rectangle with rounded corners*\n"
    "0 $1 Rounding radius*\n"
    "0 $2 $3 $4 $5 $6 $7 $8 $9 X,Y pos of 4 corners*\n"
    "0 Add a 4 corners polygon primitive as box body*\n"
    "4,1,4,$2,$3,$4,$5,$6,$7,$8,$9,$2,$3,0*\n"
    "0 Add four circle primitives for the rounded corners*\n"
    "1,1,$1+$1,$2,$3*\n"
    "1,1,$1+$1,$4,$5*\n"
    "1,1,$1+$1,$6,$7*\n"
    "1,1,$1+$1,$8,$9*\n"
    "0 Add four rect primitives between the rounded corners*\n"
    "20,1,$1+$1,$2,$3,$4,$5,0*\n"
    "20,1,$1+$1,$4,$5,$6,$7,0*\n"
    "20,1,$1+$1,$6,$7,$8,$9,0*\n"
    "20,1,$1+$1,$8,$9,$2,$3,0*%\n";

bool test_format_units_and_attributes() {
  const GerberFile file = parse_gerber(std::string(kCopperHeader) + "%ADD10C,1.600000*%\n");

  bool ok = expect(file.ok, "header parses");
  ok &= expect(file.format.defined, "%FS was seen");
  ok &= expect(file.format.x_integer_digits == 4 && file.format.x_decimal_digits == 6,
               "X format is 4.6");
  ok &= expect(file.format.y_integer_digits == 4 && file.format.y_decimal_digits == 6,
               "Y format defaults to the X format when unspecified");
  ok &= expect(file.format.zero_suppression == ZeroSuppression::Leading, "leading zero omitted");
  ok &= expect(file.format.unit == PcbUnit::Millimeter, "%MOMM sets millimetres");
  ok &= expect(file.file_function == "Copper,L1,Top", "role comes from the G04 attribute");
  ok &= expect(file.file_polarity == "Positive", "polarity comes from the G04 attribute");
  ok &= expect(file.apertures.size() == 1, "one aperture defined");
  const auto circle = file.apertures.find(10);
  ok &= expect(circle != file.apertures.end(), "D10 exists");
  if (circle != file.apertures.end()) {
    ok &= expect(circle->second.kind == ApertureKind::Circle, "D10 is a circle");
    ok &= expect(!circle->second.params.empty() && near(circle->second.params[0], 1.6),
                 "D10 diameter is 1.6 mm");
  }
  return ok;
}

bool test_macro_aperture_resolves() {
  const std::string text =
      std::string(kCopperHeader) + kRoundRectMacro +
      "G04 #@! TA.AperFunction,SMDPad,CuDef*\n"
      "%ADD11RoundRect,0.250000X-1.500000X-0.900000X1.500000X-0.900000X1.500000X0.900000X-1.500000X0.900000X0*%\n"
      "G04 #@! TD*\n";

  const GerberFile file = parse_gerber(text);
  bool ok = expect(file.macros.count("RoundRect") == 1, "the RoundRect macro is defined");
  const auto aperture = file.apertures.find(11);
  ok &= expect(aperture != file.apertures.end(), "D11 exists");
  if (aperture == file.apertures.end()) {
    return false;
  }
  ok &= expect(aperture->second.kind == ApertureKind::Macro, "D11 is a macro aperture");
  ok &= expect(aperture->second.macro_name == "RoundRect", "macro name parsed");
  ok &= expect(aperture->second.macro_args.size() == 10, "all ten macro arguments parsed");
  ok &= expect(aperture->second.function == "SMDPad,CuDef",
               "the TA attribute in effect is attached to the aperture");

  const auto macro = file.macros.find("RoundRect");
  if (macro == file.macros.end()) {
    return false;
  }
  std::vector<polysmith::core::pcb::PcbDiagnostic> diagnostics;
  const auto resolved = resolve_macro(macro->second, aperture->second.macro_args, &diagnostics, 1);
  ok &= expect(resolved.supported, "RoundRect resolves");
  ok &= expect(diagnostics.empty(), "no diagnostics while resolving");
  ok &= expect(resolved.primitives.size() == 9, "outline + 4 circles + 4 lines = 9 primitives");
  if (resolved.primitives.size() == 9) {
    ok &= expect(resolved.primitives[0].code == 4, "first primitive is the 4-corner outline");
    ok &= expect(resolved.primitives[1].code == 1, "then four circles");
    ok &= expect(near(resolved.primitives[1].params[0], 0.5),
                 "$1+$1 evaluates to twice the rounding radius");
    ok &= expect(near(resolved.primitives[0].params[1], -1.5) &&
                     near(resolved.primitives[0].params[2], -0.9),
                 "outline takes the corner coordinates from $2/$3");
    ok &= expect(resolved.primitives[5].code == 20, "then four vector lines");
    ok &= expect(near(resolved.primitives[5].params[5], 0.0),
                 "the line rotation parameter is preserved in order");
  }
  return ok;
}

bool test_macro_expression_evaluator() {
  bool ok = true;
  ok &= expect(near(*evaluate_macro_expression("$1+$1", {0.25}), 0.5), "$1+$1");
  ok &= expect(near(*evaluate_macro_expression("$1x0.5", {0.4}), 0.2), "x multiplies");
  ok &= expect(near(*evaluate_macro_expression("($1+$2)/2", {1.0, 3.0}), 2.0), "parentheses");
  ok &= expect(near(*evaluate_macro_expression("-$1", {2.0}), -2.0), "unary minus");
  ok &= expect(near(*evaluate_macro_expression(" 4 ", {}), 4.0), "bare number");
  ok &= expect(!evaluate_macro_expression("$2", {1.0}).has_value(), "out-of-range variable");
  ok &= expect(!evaluate_macro_expression("$1/0", {1.0}).has_value(), "division by zero");
  ok &= expect(!evaluate_macro_expression("1+", {}).has_value(), "dangling operator");
  ok &= expect(!evaluate_macro_expression("(1+2", {}).has_value(), "unbalanced parenthesis");
  return ok;
}

bool test_strokes_flashes_and_modal_state() {
  // F_Cu coordinates are 4.6 format: X18750000 → 18.75 mm.
  const std::string text = std::string(kCopperHeader) +
                           "%ADD10C,1.600000*%\n"
                           "D10*\n"
                           "X18750000Y43550000D03*\n"
                           "X1000000Y2000000D02*\n"
                           "X3000000Y2000000D01*\n"
                           "M02*\n";
  const GerberFile file = parse_gerber(text);
  bool ok = expect(file.operations.size() == 3, "flash + move + draw");
  if (file.operations.size() != 3) {
    return false;
  }
  ok &= expect(file.operations[0].kind == GerberOperation::Kind::Flash, "D03 flashes");
  ok &= expect(near(file.operations[0].x, 18.75) && near(file.operations[0].y, 43.55),
               "flash position scaled by the 4.6 format");
  ok &= expect(file.operations[0].aperture_code == 10, "flash carries the selected aperture");
  ok &= expect(file.operations[1].kind == GerberOperation::Kind::Move, "D02 moves");
  // No explicit G01 after the header's G01 — interpolation is modal.
  ok &= expect(file.operations[2].kind == GerberOperation::Kind::Line,
               "D01 stays linear from the modal G01");
  ok &= expect(file.operations[2].dark, "LPD is the default polarity");
  ok &= expect(file.bounds.valid && near(file.bounds.max_x, 18.75), "bounds cover the flash");
  return ok;
}

bool test_arcs_carry_offsets_and_clear_polarity() {
  // Edge_Cuts lines 20-24 of the fixture, including KiCad's -3 rounding
  // artifact (J-3 → -0.000003 mm).
  const std::string text = std::string(kCopperHeader) +
                           "%ADD10C,0.100000*%\n"
                           "D10*\n"
                           "X-863827Y95037773D02*\n"
                           "%LPC*%\n"
                           "G75*\n"
                           "G02*\n"
                           "X3136173Y99037767I3999997J-3D01*\n"
                           "G01*\n"
                           "%LPD*%\n"
                           "X3145373Y-762227D02*\n";
  const GerberFile file = parse_gerber(text);
  bool ok = expect(file.multi_quadrant_arcs, "G75 selects multi-quadrant arcs");
  ok &= expect(file.operations.size() == 3, "move + arc + move");
  if (file.operations.size() != 3) {
    return false;
  }
  ok &= expect(near(file.operations[0].x, -0.863827), "negative coordinates parse");
  const GerberOperation& arc = file.operations[1];
  ok &= expect(arc.kind == GerberOperation::Kind::Arc, "G02 D01 is an arc");
  ok &= expect(arc.clockwise, "G02 is clockwise");
  ok &= expect(near(arc.i, 3.999997) && near(arc.j, -0.000003),
               "I/J offsets scale like coordinates");
  ok &= expect(!arc.dark, "%LPC makes the arc a clear (erase) operation");
  ok &= expect(file.operations[2].dark, "%LPD restores dark polarity");
  return ok;
}

bool test_regions_are_bracketed() {
  const std::string text = std::string(kCopperHeader) +
                           "%ADD10C,0.100000*%\n"
                           "G36*\n"
                           "X0Y0D02*\n"
                           "X1000000Y0D01*\n"
                           "X1000000Y1000000D01*\n"
                           "G37*\n";
  const GerberFile file = parse_gerber(text);
  bool ok = expect(file.operations.size() == 5, "region begin + move + 2 lines + region end");
  if (file.operations.size() != 5) {
    return false;
  }
  ok &= expect(file.operations.front().kind == GerberOperation::Kind::RegionBegin, "G36 opens");
  ok &= expect(file.operations.back().kind == GerberOperation::Kind::RegionEnd, "G37 closes");
  ok &= expect(file.operations[1].kind == GerberOperation::Kind::Move, "contour start");
  ok &= expect(file.operations[2].kind == GerberOperation::Kind::Line, "contour edge");
  return ok;
}

bool test_inch_units_convert_to_millimetres() {
  const std::string text =
      "%FSLAX24Y24*%\n"
      "%MOIN*%\n"
      "G90*\n"
      "G01*\n"
      "%ADD10C,0.010000*%\n"
      "D10*\n"
      "X10000Y20000D02*\n";
  const GerberFile file = parse_gerber(text);
  bool ok = expect(file.format.unit == PcbUnit::Inch, "inch units selected");
  ok &= expect(file.format.x_integer_digits == 2 && file.format.x_decimal_digits == 4, "2.4");
  if (file.operations.empty()) {
    return false;
  }
  ok &= expect(near(file.operations[0].x, 25.4) && near(file.operations[0].y, 50.8),
               "1.0000 in / 2.0000 in convert to millimetres");
  return ok;
}

bool test_trailing_zero_suppression_pads_on_the_right() {
  const std::string text =
      "%FSTAX24Y24*%\n"
      "%MOMM*%\n"
      "G01*\n"
      "%ADD10C,0.100000*%\n"
      "D10*\n"
      "X123Y45D02*\n";
  const GerberFile file = parse_gerber(text);
  bool ok = expect(file.format.zero_suppression == ZeroSuppression::Trailing, "T mode parsed");
  if (file.operations.empty()) {
    return false;
  }
  ok &= expect(near(file.operations[0].x, 12.3) && near(file.operations[0].y, 45.0),
               "omitted trailing zeros pad to the full 2.4 width");
  return ok;
}

bool test_incremental_coordinates_accumulate() {
  const std::string text = std::string(kCopperHeader) +
                           "%ADD10C,0.100000*%\n"
                           "D10*\n"
                           "G91*\n"
                           "X1000000Y2000000D02*\n"
                           "X1000000Y0D01*\n";
  const GerberFile file = parse_gerber(text);
  bool ok = expect(file.operations.size() == 2, "move + draw");
  if (file.operations.size() != 2) {
    return false;
  }
  ok &= expect(near(file.operations[0].x, 1.0) && near(file.operations[0].y, 2.0), "first delta");
  ok &= expect(near(file.operations[1].x, 2.0) && near(file.operations[1].y, 2.0),
               "second delta accumulates on X and leaves Y untouched");
  return ok;
}

bool test_unsupported_constructs_degrade_without_failing() {
  const std::string text = std::string(kCopperHeader) +
                           "%SRX2Y2I5.0J5.0*%\n"
                           "%ADD10C,0.100000*%\n"
                           "D10*\n"
                           "G74*\n"
                           "X0Y0D02*\n"
                           "X1000000Y0D01*\n";
  const GerberFile file = parse_gerber(text);
  bool ok = expect(has_unsupported(file, "%SR"), "step-and-repeat recorded as unsupported");
  ok &= expect(has_unsupported(file, "G74"), "single-quadrant arcs recorded as unsupported");
  ok &= expect(file.ok, "the file still parses");
  ok &= expect(file.operations.size() == 2, "geometry after the unsupported constructs still loads");
  return ok;
}

bool test_bad_input_returns_diagnostics_never_crashes() {
  bool ok = true;
  const GerberFile empty = parse_gerber("");
  ok &= expect(!empty.ok, "empty input is not usable");
  ok &= expect(!empty.diagnostics.empty(), "empty input explains itself");

  const GerberFile no_format = parse_gerber("D10*\nX100Y100D02*\n");
  ok &= expect(!no_format.ok, "coordinates without %FS are unusable");
  ok &= expect(has_diagnostic(no_format, "%FS"), "the missing format is named");

  const GerberFile undefined = parse_gerber(std::string(kCopperHeader) + "D99*\n");
  ok &= expect(has_diagnostic(undefined, "D99"), "selecting an undefined aperture is reported");

  std::string junk;
  for (int index = 0; index < 512; ++index) {
    junk.push_back(static_cast<char>(index % 256));
  }
  const GerberFile binary = parse_gerber(junk);
  ok &= expect(!binary.ok, "binary junk is rejected, not parsed");
  return ok;
}

// ── Real-board fixture (optional) ──────────────────────────────────
// The full KiCad plot is not committed, so this case skips when the
// fixture is absent and still asserts hard when it is there.  Point
// POLYSMITH_PCB_FIXTURE_DIR at any directory of Gerber files to run it
// against a different board.

std::string fixture_directory() {
  if (const char* from_env = std::getenv("POLYSMITH_PCB_FIXTURE_DIR")) {
    return std::string(from_env);
  }
  return "projects/EDM";
}

std::string read_text(const std::string& path) {
  std::error_code error;
  if (!std::filesystem::exists(path, error)) {
    return std::string();
  }
  std::ifstream stream(path, std::ios::binary);
  if (!stream) {
    return std::string();
  }
  return std::string((std::istreambuf_iterator<char>(stream)),
                     std::istreambuf_iterator<char>());
}

int count_kind(const GerberFile& file, GerberOperation::Kind kind) {
  int count = 0;
  for (const auto& operation : file.operations) {
    if (operation.kind == kind) {
      ++count;
    }
  }
  return count;
}

bool test_real_board_fixture_when_present() {
  const std::string directory = fixture_directory();
  const std::string copper = read_text(directory + "/gapstorm-retro-F_Cu.gbr");
  if (copper.empty()) {
    std::cout << "  (skipped: no fixture at " << directory << ")\n";
    return true;
  }

  const GerberFile layer = parse_gerber(copper);
  bool ok = expect(layer.ok, "the real layer parses");
  ok &= expect(layer.file_function == "Copper,L1,Top", "outer copper role");
  ok &= expect(layer.apertures.size() == 71, "all 71 aperture definitions load");
  ok &= expect(layer.macros.size() == 1 && layer.macros.count("RoundRect") == 1,
               "the RoundRect macro loads");
  ok &= expect(layer.unsupported.empty(), "nothing in this layer is unsupported");
  ok &= expect(count_kind(layer, GerberOperation::Kind::RegionBegin) == 12,
               "12 copper regions (pours)");
  ok &= expect(count_kind(layer, GerberOperation::Kind::Flash) == 716, "716 pad flashes");
  ok &= expect(layer.operations.size() >= 16000 && layer.operations.size() <= 17000,
               "the ~16.5k command stream is complete");
  ok &= expect(layer.bounds.valid && layer.bounds.max_x > 90.0 && layer.bounds.max_y > 90.0,
               "bounds cover the 99.9 mm board");
  ok &= expect(!layer.operations.empty() && layer.operations.front().dark,
               "the layer starts dark (positive)");

  const std::string profile = read_text(directory + "/gapstorm-retro-Edge_Cuts.gbr");
  if (!profile.empty()) {
    const GerberFile edge = parse_gerber(profile);
    ok &= expect(edge.ok, "the profile layer parses");
    // KiCad's raw attribute value is preserved verbatim, modifiers and all
    // ("Profile,NP" = non-plated profile); role classification is the job
    // of the resolution layer, which splits on commas.
    ok &= expect(edge.file_function == "Profile,NP", "profile role");
    ok &= expect(count_kind(edge, GerberOperation::Kind::Arc) == 10, "10 profile arcs");
    ok &= expect(edge.operations.size() == 40, "20 draws + 20 travels");
    ok &= expect(edge.multi_quadrant_arcs, "profile arcs are multi-quadrant");
  }
  return ok;
}

}  // namespace

int main() {
  struct Suite {
    const char* name;
    bool (*run)();
  };
  const Suite suites[] = {
      {"format_units_and_attributes", test_format_units_and_attributes},
      {"macro_aperture_resolves", test_macro_aperture_resolves},
      {"macro_expression_evaluator", test_macro_expression_evaluator},
      {"strokes_flashes_and_modal_state", test_strokes_flashes_and_modal_state},
      {"arcs_carry_offsets_and_clear_polarity", test_arcs_carry_offsets_and_clear_polarity},
      {"regions_are_bracketed", test_regions_are_bracketed},
      {"inch_units_convert_to_millimetres", test_inch_units_convert_to_millimetres},
      {"trailing_zero_suppression_pads_on_the_right",
       test_trailing_zero_suppression_pads_on_the_right},
      {"incremental_coordinates_accumulate", test_incremental_coordinates_accumulate},
      {"unsupported_constructs_degrade_without_failing",
       test_unsupported_constructs_degrade_without_failing},
      {"bad_input_returns_diagnostics_never_crashes",
       test_bad_input_returns_diagnostics_never_crashes},
      {"real_board_fixture_when_present", test_real_board_fixture_when_present},
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
  std::cout << "pcb gerber parser: all cases passed\n";
  return 0;
}
