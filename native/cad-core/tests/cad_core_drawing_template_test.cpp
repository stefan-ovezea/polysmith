// Drawing template test (CREATE DRAWING dialog save/load format).
//
// Pins the setup-only template shape end-to-end:
//   - round-trip: name + per-sheet settings + title block (revision
//     rows included) survive to_json → from_json
//   - setup-only: sheet_id / view_ids never appear in the JSON (ids
//     are minted at drawing_create time)
//   - validation: the exact drawing_sheet_update enums (A0–A4,
//     portrait/landscape, first/third angle) plus name/sheets rules
//   - lenient parsing: missing keys take the payload defaults
//   - file round-trip: the pretty dump parses back

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

#include <nlohmann/json.hpp>

#include "core/drawing/drawing_templates.h"
#include "protocol/serialization.h"

namespace {

using polysmith::core::DrawingSheet;
using polysmith::core::DrawingTemplate;
using polysmith::core::drawing_template_from_json;
using polysmith::core::drawing_template_to_json;
using polysmith::core::drawing_template_validate;

bool expect(bool condition, const char* message) {
  if (condition) {
    return true;
  }
  std::cerr << "FAIL: " << message << "\n";
  return false;
}

DrawingTemplate make_template() {
  DrawingTemplate template_;
  template_.name = "My Company Standard";
  DrawingSheet first;
  first.name = "Sheet 1";
  first.paper_size = "A3";
  first.orientation = "portrait";
  first.projection_angle = "first_angle";
  first.title_block.legal_owner = "Acme GmbH";
  first.title_block.identification = "ACME-0001";
  first.title_block.title = "Widget";
  first.title_block.revision_rows = {
      {"A", "1", "Initial release", "2026-09-20", "JD"},
  };
  DrawingSheet second;
  second.name = "Sheet 2";
  second.paper_size = "A4";
  second.orientation = "landscape";
  second.projection_angle = "third_angle";
  template_.sheets = {first, second};
  return template_;
}

bool test_round_trip() {
  const DrawingTemplate original = make_template();
  const auto json = drawing_template_to_json(original);

  // Setup-only: ids never leave the template.
  const auto sheets = json.at("sheets");
  if (!expect(sheets.size() == 2, "two sheets serialized")) {
    return false;
  }
  for (const auto& sheet : sheets) {
    if (!expect(!sheet.contains("sheet_id") && !sheet.contains("view_ids"),
                "sheet_id/view_ids stripped from the template JSON")) {
      return false;
    }
  }

  std::string error = "unset";
  const DrawingTemplate restored = drawing_template_from_json(json, error);
  if (!expect(error.empty(), "round-trip validates clean")) {
    return false;
  }
  if (!expect(restored.name == original.name, "name survives")) {
    return false;
  }
  if (!expect(restored.sheets.size() == 2, "sheet count survives")) {
    return false;
  }
  const DrawingSheet& first = restored.sheets[0];
  if (!expect(first.name == "Sheet 1" && first.paper_size == "A3" &&
                  first.orientation == "portrait" &&
                  first.projection_angle == "first_angle",
              "sheet 1 settings survive")) {
    return false;
  }
  if (!expect(first.title_block.legal_owner == "Acme GmbH" &&
                  first.title_block.identification == "ACME-0001" &&
                  first.title_block.title == "Widget" &&
                  first.title_block.revision_rows.size() == 1 &&
                  first.title_block.revision_rows[0][2] == "Initial release",
              "title block + revision rows survive")) {
    return false;
  }
  const DrawingSheet& second = restored.sheets[1];
  return expect(second.paper_size == "A4" &&
                    second.orientation == "landscape" &&
                    second.projection_angle == "third_angle",
                "sheet 2 settings survive");
}

bool test_enum_rejections() {
  // Unknown paper size.
  DrawingTemplate bad = make_template();
  bad.sheets[0].paper_size = "A5";
  if (!expect(!drawing_template_validate(bad).empty(),
              "paper size A5 rejected")) {
    return false;
  }
  // Unknown orientation.
  bad = make_template();
  bad.sheets[0].orientation = "square";
  if (!expect(!drawing_template_validate(bad).empty(),
              "orientation 'square' rejected")) {
    return false;
  }
  // Unknown projection angle.
  bad = make_template();
  bad.sheets[0].projection_angle = "second_angle";
  if (!expect(!drawing_template_validate(bad).empty(),
              "projection angle 'second_angle' rejected")) {
    return false;
  }
  // Empty name.
  bad = make_template();
  bad.name = "";
  if (!expect(!drawing_template_validate(bad).empty(),
              "empty name rejected")) {
    return false;
  }
  // Zero sheets.
  bad = make_template();
  bad.sheets.clear();
  return expect(!drawing_template_validate(bad).empty(),
                "zero sheets rejected");
}

bool test_missing_key_defaults() {
  // A minimal template parses with the payload defaults and then
  // validates clean (the defaults ARE valid enum values).
  const auto json = nlohmann::json{
      {"name", "Minimal"},
      {"sheets", nlohmann::json::array({nlohmann::json::object()})},
  };
  std::string error = "unset";
  const DrawingTemplate restored = drawing_template_from_json(json, error);
  if (!expect(error.empty(), "minimal template validates")) {
    return false;
  }
  return expect(restored.sheets.size() == 1 &&
                    restored.sheets[0].paper_size == "A4" &&
                    restored.sheets[0].orientation == "landscape" &&
                    restored.sheets[0].projection_angle == "first_angle",
                "minimal sheet takes A4/landscape/first_angle defaults");
}

bool test_file_round_trip() {
  const auto path = std::filesystem::temp_directory_path() /
                    "polysmith_drawing_template_test.json";
  {
    std::ofstream stream(path.string());
    stream << drawing_template_to_json(make_template()).dump(2) << "\n";
  }
  std::ifstream stream(path.string());
  if (!expect(stream.good(), "template file readable")) {
    return false;
  }
  nlohmann::json payload;
  stream >> payload;
  std::string error = "unset";
  const DrawingTemplate restored = drawing_template_from_json(payload, error);
  if (!expect(error.empty(), "file round-trip validates")) {
    return false;
  }
  return expect(restored.name == "My Company Standard" &&
                    restored.sheets.size() == 2,
                "file round-trip restores the template");
}

}  // namespace

int main() {
  bool allPassed = true;

  std::cout << "cad_core_drawing_template_test\n";
  std::cout << "  Test 1: template round-trip + setup-only shape... ";
  if (test_round_trip()) {
    std::cout << "PASS\n";
  } else {
    std::cout << "FAIL\n";
    allPassed = false;
  }

  std::cout << "  Test 2: enum rejections... ";
  if (test_enum_rejections()) {
    std::cout << "PASS\n";
  } else {
    std::cout << "FAIL\n";
    allPassed = false;
  }

  std::cout << "  Test 3: missing-key defaults... ";
  if (test_missing_key_defaults()) {
    std::cout << "PASS\n";
  } else {
    std::cout << "FAIL\n";
    allPassed = false;
  }

  std::cout << "  Test 4: file round-trip... ";
  if (test_file_round_trip()) {
    std::cout << "PASS\n";
  } else {
    std::cout << "FAIL\n";
    allPassed = false;
  }

  if (allPassed) {
    std::cout << "cad_core_drawing_template_test passed\n";
    return 0;
  }
  return 1;
}
