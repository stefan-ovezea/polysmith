// Tool table I/O test.
//
// Covers the LinuxCNC tool.tbl parser/writer (the lingua franca:
// LinuxCNC, FreeCAD export, PathPilot) and the lossless PolySmith
// tools JSON round trip.  Pins the documented gotchas: order-
// independent fields, the ";" header line, T0 = empty spindle,
// "+"-signed values, lathe I/J/Q + X/Z offsets, comment
// name/description idempotence, and version-guarded JSON.

#include <iostream>
#include <string>
#include <vector>

#include "core/cam/tool_table_io.h"

namespace {

using polysmith::core::ToolEntry;
using polysmith::core::parse_linuxcnc_tool_table;
using polysmith::core::parse_polysmith_tools_json;
using polysmith::core::serialize_linuxcnc_tool_table;
using polysmith::core::serialize_polysmith_tools_json;

bool expect(bool condition, const char* message) {
  if (condition) {
    return true;
  }
  std::cerr << "FAIL: " << message << "\n";
  return false;
}

ToolEntry make_mill() {
  ToolEntry tool;
  tool.tool_id = "tool-1";
  tool.name = "6mm End Mill";
  tool.type = "endmill_flat";
  tool.tool_number = 3;
  tool.diameter_mm = 6.0;
  tool.flute_length_mm = 18.0;
  tool.overall_length_mm = 50.0;
  tool.shank_diameter_mm = 6.0;
  tool.guid = "abc123";
  return tool;
}

bool test_parse_classic_table() {
  // Field order is free; the header is a lone ";"; values may carry
  // an explicit '+' (shipped configs show both orderings).
  const std::string table =
      ";\n"
      "T1 P1 D0.125000 Z+0.511000 ;1/8 end mill\n"
      "T2 P2 Z0.100000 D0.062500 ;1/16 end mill\n"
      "; a comment-only line\n"
      "T3 P3 Z1.273000 D0.201000 ;#7 tap drill\n";
  std::vector<ToolEntry> tools;
  std::vector<std::string> warnings;
  std::string error;
  parse_linuxcnc_tool_table(table, tools, warnings, error);
  if (!expect(error.empty(), "parse: no hard error")) {
    return false;
  }
  if (!expect(tools.size() == 3, "parse: three tools")) {
    return false;
  }
  if (!expect(tools[0].tool_number == 1 &&
                  tools[0].pocket_number == 1 &&
                  tools[0].diameter_mm == 0.125 &&
                  tools[0].z_offset_mm.has_value() &&
                  tools[0].z_offset_mm.value() == 0.511 &&
                  tools[0].name == "1/8 end mill",
              "parse: T1 fields + comment become the name")) {
    return false;
  }
  if (!expect(tools[1].diameter_mm == 0.0625 &&
                  tools[1].z_offset_mm.value() == 0.1,
              "parse: Z-before-D order accepted")) {
    return false;
  }
  return expect(tools[2].name == "#7 tap drill",
                "parse: comment-only line ignored, T3 parsed");
}

bool test_parse_skips_t0_and_junk() {
  const std::string table =
      ";\n"
      "T0 P0 Z0.0 ;the empty spindle\n"
      "T2 P2 D6.0 ;fine\n"
      "garbage line without fields\n"
      "T3 D6.0\n"       // no P — defaults to T
      "T4JUNK D6.0\n";  // missing separator — malformed
  std::vector<ToolEntry> tools;
  std::vector<std::string> warnings;
  std::string error;
  parse_linuxcnc_tool_table(table, tools, warnings, error);
  if (!expect(error.empty(), "skip: no hard error")) {
    return false;
  }
  if (!expect(tools.size() == 2, "skip: T0 and junk lines skipped")) {
    return false;
  }
  if (!expect(tools[0].tool_number == 2,
              "skip: valid line after T0 parsed")) {
    return false;
  }
  if (!expect(tools[1].tool_number == 3 && tools[1].pocket_number == 3,
              "skip: missing P defaults to the tool number")) {
    return false;
  }
  if (!expect(warnings.size() == 3, "skip: three warnings emitted")) {
    return false;
  }
  bool t0Warning = false;
  for (const auto& warning : warnings) {
    if (warning.find("T0") != std::string::npos) {
      t0Warning = true;
    }
  }
  return expect(t0Warning, "skip: T0 warned");
}

bool test_lathe_fields_round_trip() {
  const std::string table =
      ";\n"
      "T5 P5 X-1.5 Z22.3 D0.4 I95.0 J155.0 Q1 ;60 deg turning tool\n";
  std::vector<ToolEntry> tools;
  std::vector<std::string> warnings;
  std::string error;
  parse_linuxcnc_tool_table(table, tools, warnings, error);
  if (!expect(error.empty() && tools.size() == 1, "lathe: parsed")) {
    return false;
  }
  const ToolEntry& tool = tools[0];
  if (!expect(tool.type == "turning_insert" && tool.orientation == 1 &&
                  tool.front_angle_deg == 95.0 &&
                  tool.back_angle_deg == 155.0 &&
                  tool.x_offset_mm.has_value() &&
                  tool.x_offset_mm.value() == -1.5 &&
                  tool.z_offset_mm.has_value() &&
                  tool.z_offset_mm.value() == 22.3,
              "lathe: I/J/Q + X/Z offsets captured")) {
    return false;
  }
  // And back out.
  const std::string out = serialize_linuxcnc_tool_table(tools);
  const std::string expected =
      ";\n"
      "T5 P5 X-1.500000 Z22.300000 D0.400000 I95.000000 J155.000000 Q1 "
      ";60 deg turning tool\n";
  return expect(out == expected, "lathe: canonical serialization");
}

bool test_serialize_canonical_mill() {
  std::vector<ToolEntry> tools;
  ToolEntry mill = make_mill();
  mill.description = "roughing";
  tools.push_back(mill);

  const std::string out = serialize_linuxcnc_tool_table(tools);
  const std::string expected =
      ";\n"
      "T3 P3 D6.000000 ;6mm End Mill — roughing\n";
  return expect(out == expected,
                "mill: canonical line, no lathe fields, header present");
}

bool test_serialize_parse_round_trip() {
  std::vector<ToolEntry> tools;
  ToolEntry mill = make_mill();
  mill.description = "roughing";
  mill.z_offset_mm = 12.5;
  tools.push_back(mill);
  ToolEntry lathe;
  lathe.name = "60 deg";
  lathe.type = "turning_insert";
  lathe.tool_number = 8;
  lathe.diameter_mm = 0.4;
  lathe.front_angle_deg = 95.0;
  lathe.back_angle_deg = 155.0;
  lathe.orientation = 1;
  lathe.x_offset_mm = -1.5;
  lathe.z_offset_mm = 22.3;
  tools.push_back(lathe);

  const std::string out = serialize_linuxcnc_tool_table(tools);
  std::vector<ToolEntry> parsed;
  std::vector<std::string> warnings;
  std::string error;
  parse_linuxcnc_tool_table(out, parsed, warnings, error);
  if (!expect(error.empty() && warnings.empty() && parsed.size() == 2,
              "round trip: clean parse")) {
    return false;
  }
  const ToolEntry& mill2 = parsed[0];
  if (!expect(mill2.tool_number == 3 && mill2.diameter_mm == 6.0 &&
                  mill2.name == "6mm End Mill" &&
                  mill2.description == "roughing" &&
                  mill2.z_offset_mm.has_value() &&
                  mill2.z_offset_mm.value() == 12.5,
              "round trip: mill fields survive")) {
    return false;
  }
  const ToolEntry& lathe2 = parsed[1];
  return expect(lathe2.orientation == 1 && lathe2.front_angle_deg == 95.0 &&
                    lathe2.x_offset_mm.value() == -1.5,
                "round trip: lathe fields survive");
}

bool test_json_round_trip_lossless() {
  std::vector<ToolEntry> tools;
  ToolEntry tool = make_mill();
  tool.description = "long description";
  tool.vendor = "Acme";
  tool.product_id = "P-1";
  tool.flutes = 4;
  tool.helix_angle_deg = 38.0;
  tool.point_angle_deg = 90.0;
  tool.tip_diameter_mm = 0.2;
  tool.shoulder_length_mm = 18.0;
  tool.length_below_holder_mm = 48.0;
  tool.surface_speed_m_per_min = 250.0;
  tool.feed_per_tooth_mm = 0.03;
  tool.material = "hss";
  tool.coating = "tialn";
  tool.coolant_through = true;
  tool.x_offset_mm = 1.0;
  tool.z_offset_mm = 2.0;
  tools.push_back(tool);

  const std::string out = serialize_polysmith_tools_json(tools);
  std::vector<std::string> warnings;
  std::string error;
  const auto parsed = parse_polysmith_tools_json(out, warnings, error);
  if (!expect(error.empty() && warnings.empty() && parsed.size() == 1,
              "json: clean parse")) {
    return false;
  }
  const ToolEntry& round = parsed[0];
  return expect(round.tool_number == tool.tool_number &&
                    round.name == tool.name &&
                    round.description == tool.description &&
                    round.vendor == tool.vendor &&
                    round.product_id == tool.product_id &&
                    round.guid == tool.guid &&
                    round.diameter_mm == tool.diameter_mm &&
                    round.flutes == tool.flutes &&
                    round.helix_angle_deg == tool.helix_angle_deg &&
                    round.point_angle_deg == tool.point_angle_deg &&
                    round.tip_diameter_mm == tool.tip_diameter_mm &&
                    round.shoulder_length_mm == tool.shoulder_length_mm &&
                    round.length_below_holder_mm ==
                        tool.length_below_holder_mm &&
                    round.surface_speed_m_per_min ==
                        tool.surface_speed_m_per_min &&
                    round.feed_per_tooth_mm == tool.feed_per_tooth_mm &&
                    round.material == tool.material &&
                    round.coating == tool.coating &&
                    round.coolant_through == tool.coolant_through &&
                    round.x_offset_mm == tool.x_offset_mm &&
                    round.z_offset_mm == tool.z_offset_mm,
                "json: every field round-trips");
}

bool test_json_version_rejected() {
  std::vector<std::string> warnings;
  std::string error;
  const auto parsed =
      parse_polysmith_tools_json("{\"version\": 99, \"tools\": []}",
                                 warnings, error);
  return expect(parsed.empty() && !error.empty() &&
                    error.find("99") != std::string::npos,
                "json: unknown version rejected with a message");
}

bool test_json_missing_version_rejected() {
  std::vector<std::string> warnings;
  std::string error;
  const auto parsed =
      parse_polysmith_tools_json("{\"tools\": []}", warnings, error);
  return expect(parsed.empty() && !error.empty(),
                "json: missing version rejected");
}

}  // namespace

int main() {
  bool allPassed = true;

  std::cout << "tool_table_io_test\n";
  std::cout << "  Test 1: parse classic table... ";
  if (test_parse_classic_table()) {
    std::cout << "PASS\n";
  } else {
    std::cout << "FAIL\n";
    allPassed = false;
  }

  std::cout << "  Test 2: T0/junk lines skipped with warnings... ";
  if (test_parse_skips_t0_and_junk()) {
    std::cout << "PASS\n";
  } else {
    std::cout << "FAIL\n";
    allPassed = false;
  }

  std::cout << "  Test 3: lathe fields round trip... ";
  if (test_lathe_fields_round_trip()) {
    std::cout << "PASS\n";
  } else {
    std::cout << "FAIL\n";
    allPassed = false;
  }

  std::cout << "  Test 4: canonical mill serialization... ";
  if (test_serialize_canonical_mill()) {
    std::cout << "PASS\n";
  } else {
    std::cout << "FAIL\n";
    allPassed = false;
  }

  std::cout << "  Test 5: serialize/parse round trip... ";
  if (test_serialize_parse_round_trip()) {
    std::cout << "PASS\n";
  } else {
    std::cout << "FAIL\n";
    allPassed = false;
  }

  std::cout << "  Test 6: JSON lossless round trip... ";
  if (test_json_round_trip_lossless()) {
    std::cout << "PASS\n";
  } else {
    std::cout << "FAIL\n";
    allPassed = false;
  }

  std::cout << "  Test 7: JSON version guard... ";
  if (test_json_version_rejected()) {
    std::cout << "PASS\n";
  } else {
    std::cout << "FAIL\n";
    allPassed = false;
  }

  std::cout << "  Test 8: JSON missing version guard... ";
  if (test_json_missing_version_rejected()) {
    std::cout << "PASS\n";
  } else {
    std::cout << "FAIL\n";
    allPassed = false;
  }

  if (allPassed) {
    std::cout << "tool_table_io_test passed\n";
    return 0;
  }
  return 1;
}
