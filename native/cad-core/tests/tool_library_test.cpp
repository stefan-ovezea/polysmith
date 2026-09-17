// Tool library test.
//
// Covers the shared tool library end to end: built-in generic-catalog
// seeding (idempotent, user files win), save/load round trip,
// validation, directory override via POLYSMITH_TOOLS_DIR, and that
// the built-in catalog parses into well-formed tools.

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <set>
#include <string>
#include <vector>

#include "core/cam/tool_library.h"

namespace {

using polysmith::core::ToolEntry;
using polysmith::core::is_generic_catalog_tool;
using polysmith::core::load_tool_library;
using polysmith::core::save_tool_entry;

bool expect(bool condition, const char* message) {
  if (condition) {
    return true;
  }
  std::cerr << "FAIL: " << message << "\n";
  return false;
}

// Points POLYSMITH_TOOLS_DIR at `dir` (empty string = unset).
void set_tools_dir(const std::filesystem::path& dir) {
#ifdef _WIN32
  _putenv_s("POLYSMITH_TOOLS_DIR", dir.string().c_str());
#else
  if (dir.empty()) {
    unsetenv("POLYSMITH_TOOLS_DIR");
  } else {
    setenv("POLYSMITH_TOOLS_DIR", dir.string().c_str(), 1);
  }
#endif
}

const ToolEntry* find_tool(const std::vector<ToolEntry>& tools,
                           const std::string& name) {
  for (const auto& tool : tools) {
    if (tool.name == name) {
      return &tool;
    }
  }
  return nullptr;
}

ToolEntry make_test_endmill() {
  ToolEntry tool;
  tool.tool_id = "tool-99";
  tool.name = "Test Endmill 4F";
  tool.type = "endmill_flat";
  tool.tool_number = 99;
  tool.diameter_mm = 4.0;
  tool.flute_length_mm = 10.0;
  tool.overall_length_mm = 45.0;
  tool.shank_diameter_mm = 4.0;
  tool.flutes = 4;
  tool.guid = "test-guid-001";
  tool.vendor = "Acme";
  tool.description = "test fixture";
  return tool;
}

std::filesystem::path make_temp_dir() {
  const auto dir = std::filesystem::temp_directory_path() /
                   "polysmith_tools_test";
  std::filesystem::remove_all(dir);
  std::filesystem::create_directories(dir);
  return dir;
}

bool test_builtin_catalog_parses() {
  const auto dir = make_temp_dir();
  set_tools_dir(dir);
  const auto tools = load_tool_library();
  if (!expect(tools.size() == 42, "catalog: 42 built-in tools")) {
    return false;
  }
  std::set<int> numbers;
  for (const auto& tool : tools) {
    if (!expect(tool.tool_number >= 1 && tool.tool_number <= 42,
                "catalog: tool numbers within 1..42")) {
      return false;
    }
    if (!expect(is_generic_catalog_tool(tool),
                "catalog: generic guid recognized")) {
      return false;
    }
    numbers.insert(tool.tool_number);
  }
  if (!expect(numbers.size() == 42, "catalog: tool numbers unique")) {
    return false;
  }
  if (!expect(find_tool(tools, "Flat End Mill Ø6 2F") != nullptr,
              "catalog: flat end mill present") ||
      !expect(find_tool(tools, "Ball End Mill Ø6") != nullptr,
              "catalog: ball end mill present") ||
      !expect(find_tool(tools, "V-Bit 60° Ø6") != nullptr,
              "catalog: v-bit present") ||
      !expect(find_tool(tools, "Spot Drill 90° Ø6") != nullptr,
              "catalog: spot drill present") ||
      !expect(find_tool(tools, "Twist Drill Ø6.0") != nullptr,
              "catalog: twist drill present")) {
    return false;
  }
  // Spot-check one entry's geometry.
  const auto* ball = find_tool(tools, "Ball End Mill Ø6");
  return expect(ball != nullptr && ball->type == "endmill_ball" &&
                    ball->corner_radius_mm == 3.0 && ball->flutes == 2,
                "catalog: ball endmill geometry correct");
}

bool test_seed_idempotent_and_respects_user_files() {
  const auto dir = make_temp_dir();
  set_tools_dir(dir);

  const auto first = load_tool_library();
  if (!expect(first.size() == 42, "seed: catalog present after first load")) {
    return false;
  }
  if (!expect(std::filesystem::exists(dir / "t7-flat-end-mill-6-2f.tool.json"),
              "seed: tool file written to the directory")) {
    return false;
  }

  // A user edit of the seeded file overrides the built-in entry.
  {
    std::ofstream stream(dir / "t7-flat-end-mill-6-2f.tool.json",
                         std::ios::trunc);
    stream << R"({
      "tool_number": 7,
      "guid": "generic-t07",
      "name": "Flat End Mill Ø6 2F",
      "type": "endmill_flat",
      "diameter_mm": 6.35,
      "flute_length_mm": 18.0,
      "overall_length_mm": 50.0,
      "shank_diameter_mm": 6.0,
      "flutes": 2
    })";
  }
  const auto edited = load_tool_library();
  const auto* t7 = find_tool(edited, "Flat End Mill Ø6 2F");
  if (!expect(t7 != nullptr && t7->diameter_mm == 6.35,
              "user file overrides the built-in")) {
    return false;
  }
  if (!expect(edited.size() == 42,
              "seed: user edit does not duplicate the entry")) {
    return false;
  }

  // Re-seeding never overwrites the user's edit.
  const auto reseeded = load_tool_library();
  const auto* again = find_tool(reseeded, "Flat End Mill Ø6 2F");
  return expect(again != nullptr && again->diameter_mm == 6.35,
                "seed: idempotent — user edit survives re-loads");
}

bool test_save_load_round_trip() {
  const auto dir = make_temp_dir();
  set_tools_dir(dir);
  load_tool_library();  // seed first

  std::string error;
  const std::string slug = save_tool_entry(make_test_endmill(), error);
  if (!expect(error.empty() && !slug.empty(), "save: accepted")) {
    return false;
  }
  if (!expect(std::filesystem::exists(dir / "t99-test-endmill-4f.tool.json"),
              "save: file written with t-numbered slug")) {
    return false;
  }

  const auto tools = load_tool_library();
  const auto* loaded = find_tool(tools, "Test Endmill 4F");
  if (!expect(loaded != nullptr, "save: tool listed after save")) {
    return false;
  }
  if (!expect(loaded->tool_number == 99 && loaded->diameter_mm == 4.0 &&
                  loaded->flutes == 4 && loaded->guid == "test-guid-001" &&
                  loaded->vendor == "Acme" &&
                  loaded->description == "test fixture",
              "save: all fields round-trip")) {
    return false;
  }
  return expect(!is_generic_catalog_tool(*loaded),
                "save: user tool is not flagged as catalog");
}

bool test_save_rejects_invalid() {
  const auto dir = make_temp_dir();
  set_tools_dir(dir);

  bool badType = false;
  std::string error;
  ToolEntry badTypeTool = make_test_endmill();
  badTypeTool.type = "wrench";
  save_tool_entry(badTypeTool, error);
  badType = !error.empty();
  if (!expect(badType, "save: unknown type rejected")) {
    return false;
  }

  bool zeroDiameter = false;
  ToolEntry zeroDiaTool = make_test_endmill();
  zeroDiaTool.diameter_mm = 0.0;
  error.clear();
  save_tool_entry(zeroDiaTool, error);
  zeroDiameter = !error.empty();
  if (!expect(zeroDiameter, "save: zero diameter rejected")) {
    return false;
  }

  return expect(!std::filesystem::exists(dir / "t99-test-endmill-4f.tool.json"),
                "save: rejected tools write no file");
}

bool test_directory_env_override() {
  const auto dir = make_temp_dir();
  set_tools_dir({});  // unset
  const auto withoutDir = load_tool_library();
  if (!expect(withoutDir.size() == 42 &&
                  std::filesystem::is_empty(dir),
              "env: unset dir lists catalog only, seeds nothing")) {
    return false;
  }

  set_tools_dir(dir);
  const auto withDir = load_tool_library();
  return expect(withDir.size() == 42 &&
                    std::filesystem::exists(
                        dir / "t1-flat-end-mill-1-2f.tool.json"),
                "env: configured dir seeds the catalog");
}

}  // namespace

int main() {
  bool allPassed = true;

  std::cout << "tool_library_test\n";
  std::cout << "  Test 1: built-in catalog parses... ";
  if (test_builtin_catalog_parses()) {
    std::cout << "PASS\n";
  } else {
    std::cout << "FAIL\n";
    allPassed = false;
  }

  std::cout << "  Test 2: seed idempotent + user files win... ";
  if (test_seed_idempotent_and_respects_user_files()) {
    std::cout << "PASS\n";
  } else {
    std::cout << "FAIL\n";
    allPassed = false;
  }

  std::cout << "  Test 3: save/load round trip... ";
  if (test_save_load_round_trip()) {
    std::cout << "PASS\n";
  } else {
    std::cout << "FAIL\n";
    allPassed = false;
  }

  std::cout << "  Test 4: save rejects invalid tools... ";
  if (test_save_rejects_invalid()) {
    std::cout << "PASS\n";
  } else {
    std::cout << "FAIL\n";
    allPassed = false;
  }

  std::cout << "  Test 5: directory env override... ";
  if (test_directory_env_override()) {
    std::cout << "PASS\n";
  } else {
    std::cout << "FAIL\n";
    allPassed = false;
  }

  // Leave the environment clean for the other suites.
  set_tools_dir({});

  if (allPassed) {
    std::cout << "tool_library_test passed\n";
    return 0;
  }
  return 1;
}
