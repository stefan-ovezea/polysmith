// Regression suite for the Gerber polygonizer (P1b): command stream →
// closed rings.
//
// Areas are asserted rather than vertex counts, so the tessellation
// tolerance can be tuned without rewriting the suite — a wrong shape
// changes the area, a different sampling density does not.  The macro
// case is the fixture's real RoundRect definition, which is what every
// SMD pad on the board is flashed with.

#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <vector>

#include "core/cam/cam2d.h"
#include "core/pcb/gerber_polygonize.h"
#include "core/pcb/gerber_parser.h"

namespace {

using polysmith::core::pcb::ApertureKind;
using polysmith::core::pcb::GerberFile;
using polysmith::core::pcb::PcbLayerGeometry;
using polysmith::core::pcb::PcbRing;
using polysmith::core::pcb::parse_gerber;
using polysmith::core::pcb::polygonize_gerber;

bool expect(bool condition, const std::string& message) {
  if (condition) {
    return true;
  }
  std::cerr << "  FAIL: " << message << "\n";
  return false;
}

double area(const PcbRing& ring) {
  return std::abs(polysmith::core::cam2d::xy_signed_area(ring.points));
}

bool near(double actual, double expected, double relative = 1e-3) {
  const double scale = std::max(1.0, std::abs(expected));
  return std::abs(actual - expected) <= relative * scale;
}

const char* kHeader =
    "%FSLAX46Y46*%\n"
    "%MOMM*%\n"
    "%LPD*%\n"
    "G01*\n";

/// Wraps a body in a minimal but valid Gerber file.
std::string file_with(const std::string& body) {
  return std::string(kHeader) + body;
}

bool test_circle_and_rectangle_flashes() {
  const GerberFile file = parse_gerber(file_with(
      "%ADD10C,2.000000*%\n"
      "%ADD11R,1.600000X0.900000*%\n"
      "D10*\n"
      "X0Y0D03*\n"
      "D11*\n"
      "X10000000Y0D03*\n"));
  const PcbLayerGeometry geometry = polygonize_gerber(file, 0.001);
  bool ok = expect(geometry.rings.size() == 2, "two flashed pads");
  if (geometry.rings.size() != 2) {
    return false;
  }
  ok &= expect(near(area(geometry.rings[0]), 3.14159265, 1e-2),
               "circle pad area is pi*r^2");
  ok &= expect(near(area(geometry.rings[1]), 1.6 * 0.9), "rect pad area is w*h");
  ok &= expect(geometry.rings[1].points.size() == 4, "rect pad is exactly four corners");
  ok &= expect(near(geometry.bounds.max_x, 10.8, 1e-6),
               "bounds follow the flash position");
  return ok;
}

bool test_obround_pad_area() {
  const GerberFile file = parse_gerber(file_with(
      "%ADD10O,2.000000X1.000000*%\n"
      "D10*\n"
      "X0Y0D03*\n"));
  const PcbLayerGeometry geometry = polygonize_gerber(file, 0.001);
  if (!expect(geometry.rings.size() == 1, "one obround pad")) {
    return false;
  }
  // Stadium: w*h minus the two half-disc corners that are cut away.
  const double expected = 2.0 * 1.0 - (4.0 - 3.14159265358979) * 0.25;
  return expect(near(area(geometry.rings[0]), expected, 1e-2),
                "obround area = w*h - (4-pi)*r^2");
}

bool test_round_rect_macro_flash_is_a_compound() {
  // The fixture's real macro: one 4-corner outline, four corner circles
  // and four edge lines per pad.
  const std::string macro =
      "%AMRoundRect*\n"
      "4,1,4,$2,$3,$4,$5,$6,$7,$8,$9,$2,$3,0*\n"
      "1,1,$1+$1,$2,$3*\n"
      "1,1,$1+$1,$4,$5*\n"
      "1,1,$1+$1,$6,$7*\n"
      "1,1,$1+$1,$8,$9*\n"
      "20,1,$1+$1,$2,$3,$4,$5,0*\n"
      "20,1,$1+$1,$4,$5,$6,$7,0*\n"
      "20,1,$1+$1,$6,$7,$8,$9,0*\n"
      "20,1,$1+$1,$8,$9,$2,$3,0*%\n";
  const GerberFile file = parse_gerber(file_with(
      macro +
      "%ADD11RoundRect,0.250000X-1.500000X-0.900000X1.500000X-0.900000X1.500000X0.900000X"
      "-1.500000X0.900000X0*%\n"
      "D11*\n"
      "X0Y0D03*\n"));
  const PcbLayerGeometry geometry = polygonize_gerber(file, 0.001);

  int outlines = 0;
  int circles = 0;
  int lines = 0;
  double outline_area = 0.0;
  double circle_area = 0.0;
  double longest_line_area = 0.0;
  for (const PcbRing& ring : geometry.rings) {
    if (ring.macro_primitive == 4) {
      ++outlines;
      outline_area = area(ring);
    } else if (ring.macro_primitive == 1) {
      ++circles;
      circle_area = area(ring);
    } else if (ring.macro_primitive == 20) {
      ++lines;
      longest_line_area = std::max(longest_line_area, area(ring));
    }
  }
  bool ok = expect(geometry.rings.size() == 9, "outline + 4 circles + 4 lines");
  ok &= expect(outlines == 1 && circles == 4 && lines == 4, "primitive mix");
  ok &= expect(near(outline_area, 3.0 * 1.8), "outline spans the two corners");
  ok &= expect(near(circle_area, 3.14159265358979 * 0.25 * 0.25, 1e-2),
               "corner circle diameter is $1+$1");
  ok &= expect(near(longest_line_area, 3.0 * 0.5), "edge line is width x span");
  ok &= expect(geometry.diagnostics.empty(), "no diagnostics for a clean macro");
  return ok;
}

bool test_circle_aperture_stroke_is_a_capsule() {
  const GerberFile file = parse_gerber(file_with(
      "%ADD10C,1.000000*%\n"
      "D10*\n"
      "X0Y0D02*\n"
      "X10000000Y0D01*\n"));
  const PcbLayerGeometry geometry = polygonize_gerber(file, 0.001);
  if (!expect(geometry.rings.size() == 1, "one stroked trace")) {
    return false;
  }
  const double expected = 10.0 * 1.0 + 3.14159265358979 * 0.25;
  return expect(near(area(geometry.rings[0]), expected, 1e-2),
                "capsule area = length*width + pi*r^2");
}

bool test_rectangle_aperture_stroke_sweeps_flat_ends() {
  const GerberFile file = parse_gerber(file_with(
      "%ADD10R,1.000000X0.500000*%\n"
      "D10*\n"
      "X0Y0D02*\n"
      "X4000000Y0D01*\n"));
  const PcbLayerGeometry geometry = polygonize_gerber(file, 0.001);
  if (!expect(geometry.rings.size() == 1, "one swept rectangle")) {
    return false;
  }
  // A rectangular aperture sweeps to a hexagon: 4x0.5 body + a 1x0.5 cap.
  return expect(near(area(geometry.rings[0]), 4.0 * 0.5 + 1.0 * 0.5),
                "rect sweep area");
}

bool test_region_becomes_one_ring() {
  const GerberFile file = parse_gerber(file_with(
      "%ADD10C,0.100000*%\n"
      "D10*\n"
      "G36*\n"
      "X0Y0D02*\n"
      "X1000000Y0D01*\n"
      "X1000000Y1000000D01*\n"
      "X0Y1000000D01*\n"
      "G37*\n"));
  const PcbLayerGeometry geometry = polygonize_gerber(file, 0.001);
  if (!expect(geometry.rings.size() == 1, "one region ring")) {
    return false;
  }
  bool ok = expect(geometry.rings[0].from_region, "ring is flagged as a region");
  ok &= expect(near(area(geometry.rings[0]), 1.0), "1x1 mm region area");
  ok &= expect(geometry.rings[0].dark, "region inherits LPD");
  return ok;
}

bool test_arc_is_sampled_on_the_true_radius() {
  // Quarter circle, counter-clockwise, centred on the origin: start (1,0),
  // end (0,1), centre offset I=-1 J=0.
  const GerberFile file = parse_gerber(file_with(
      "%ADD10C,0.100000*%\n"
      "D10*\n"
      "X1000000Y0D02*\n"
      "G75*\n"
      "G03*\n"
      "X0Y1000000I-1000000J0D01*\n"
      "G01*\n"));
  const PcbLayerGeometry geometry = polygonize_gerber(file, 0.001);
  if (!expect(geometry.rings.size() == 1, "arc strokes into one ring")) {
    return false;
  }
  const PcbRing& ring = geometry.rings[0];
  bool ok = expect(ring.points.size() > 8, "the arc is sampled, not a chord");
  double worst_radius_error = 0.0;
  for (const auto& point : ring.points) {
    // The capsule is offset by the 0.05 mm aperture radius, so the centre
    // line sits at 1.0 mm from the origin.
    const double distance = std::hypot(point.x, point.y);
    worst_radius_error = std::max(worst_radius_error, std::abs(distance - 1.05));
    worst_radius_error = std::min(worst_radius_error, std::abs(distance - 0.95));
  }
  ok &= expect(worst_radius_error < 0.02, "sampled points stay on the arc");
  return ok;
}

bool test_clear_polarity_is_tagged_not_dropped() {
  const GerberFile file = parse_gerber(file_with(
      "%ADD10C,1.000000*%\n"
      "D10*\n"
      "X0Y0D03*\n"
      "%LPC*%\n"
      "X2000000Y0D03*\n"
      "%LPD*%\n"
      "X4000000Y0D03*\n"));
  const PcbLayerGeometry geometry = polygonize_gerber(file, 0.001);
  bool ok = expect(geometry.rings.size() == 3, "clear rings are kept, not removed");
  if (geometry.rings.size() != 3) {
    return false;
  }
  ok &= expect(geometry.rings[0].dark, "first pad is dark");
  ok &= expect(!geometry.rings[1].dark, "second pad is clear");
  ok &= expect(geometry.rings[2].dark, "third pad is dark again");
  return ok;
}

bool test_unsupported_shapes_degrade_with_diagnostics() {
  const GerberFile file = parse_gerber(file_with(
      "%AMOdd*\n"
      "7,1,0*%\n"
      "%ADD10Odd,1.0*%\n"
      "%ADD11C,0.500000*%\n"
      "D10*\n"
      "X0Y0D03*\n"
      "X0Y0D02*\n"
      "X5000000Y0D01*\n"
      "D11*\n"
      "X0Y5000000D02*\n"
      "X5000000Y5000000D01*\n"));
  const PcbLayerGeometry geometry = polygonize_gerber(file, 0.001);
  bool ok = expect(!geometry.diagnostics.empty(),
                   "an unsupported macro primitive is reported");
  ok &= expect(geometry.skipped_operations >= 2,
               "the unsupported flash and the macro stroke are counted as skipped");
  // The round-aperture stroke after them must still produce geometry.
  ok &= expect(geometry.rings.size() == 1, "the rest of the file still polygonizes");
  return ok;
}

// ── Real-board fixture (optional), mirroring the parser suite ──────

std::string fixture_directory() {
  if (const char* from_env = std::getenv("POLYSMITH_PCB_FIXTURE_DIR")) {
    return std::string(from_env);
  }
  return "projects/EDM";
}

bool test_real_board_fixture_when_present() {
  const std::string path = fixture_directory() + "/gapstorm-retro-F_Cu.gbr";
  std::error_code error;
  if (!std::filesystem::exists(path, error)) {
    std::cout << "  (skipped: no fixture at " << path << ")\n";
    return true;
  }
  std::ifstream stream(path, std::ios::binary);
  const std::string text((std::istreambuf_iterator<char>(stream)),
                         std::istreambuf_iterator<char>());
  const GerberFile file = parse_gerber(text);
  const PcbLayerGeometry geometry = polygonize_gerber(file);
  std::cout << "  rings=" << geometry.rings.size()
            << " skipped=" << geometry.skipped_operations
            << " diagnostics=" << geometry.diagnostics.size() << "\n";

  // Note: the great majority of F_Cu's ~15.6k D01 commands are *region
  // outline* vertices (two pours of several thousand points each), not
  // independent traces — so the ring count is far below the command
  // count.  The invariant that matters is that nothing was skipped.
  bool ok = expect(geometry.skipped_operations == 0,
                   "every stroke, flash and region on the real layer produced geometry");
  ok &= expect(geometry.rings.size() >= 1000, "the layer produces its ring set");
  ok &= expect(geometry.bounds.valid && geometry.bounds.max_x > 90.0,
               "bounds cover the board");
  size_t dark = 0;
  for (const PcbRing& ring : geometry.rings) {
    if (ring.dark) {
      ++dark;
    }
  }
  // Negative copper planes would show up as mostly-clear rings; KiCad's
  // positive copper must stay overwhelmingly dark.
  ok &= expect(dark * 10 > geometry.rings.size() * 9, "the layer is positive copper");
  return ok;
}

}  // namespace

int main() {
  struct Suite {
    const char* name;
    bool (*run)();
  };
  const Suite suites[] = {
      {"circle_and_rectangle_flashes", test_circle_and_rectangle_flashes},
      {"obround_pad_area", test_obround_pad_area},
      {"round_rect_macro_flash_is_a_compound", test_round_rect_macro_flash_is_a_compound},
      {"circle_aperture_stroke_is_a_capsule", test_circle_aperture_stroke_is_a_capsule},
      {"rectangle_aperture_stroke_sweeps_flat_ends",
       test_rectangle_aperture_stroke_sweeps_flat_ends},
      {"region_becomes_one_ring", test_region_becomes_one_ring},
      {"arc_is_sampled_on_the_true_radius", test_arc_is_sampled_on_the_true_radius},
      {"clear_polarity_is_tagged_not_dropped", test_clear_polarity_is_tagged_not_dropped},
      {"unsupported_shapes_degrade_with_diagnostics",
       test_unsupported_shapes_degrade_with_diagnostics},
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
  std::cout << "pcb gerber polygonizer: all cases passed\n";
  return 0;
}
