// Regression suite for the sketch tool whitelists. There are two
// whitelists that must accept exactly the same tool ids — validate_tool
// (sketch core) and is_supported_sketch_tool (document layer) — and both
// delegate to the canonical list in core/sketch/sketch_tool_ids.h.
// Historically they drifted (one accepted "move", the other rejected it),
// breaking set_sketch_tool end to end.
//
// Also guards the IPC command vocabulary in protocol/schema/
// commands.schema.json: stale command names that were renamed
// (update_sketch_point -> update_sketch_vertex, ...) must stay gone and
// previously-missing dispatched commands must stay listed.

#include <filesystem>
#include <fstream>
#include <iostream>
#include <set>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "core/document/document.h"
#include "core/sketch/sketch_tool_ids.h"

namespace {

using polysmith::core::DocumentManager;
using polysmith::core::DocumentState;
using polysmith::core::kSupportedSketchTools;

bool expect(bool condition, const char* message) {
  if (condition) return true;
  std::cerr << message << "\n";
  return false;
}

// set_sketch_tool exercises BOTH whitelists end to end: the document
// layer's is_supported_sketch_tool rejects first, then the core layer's
// validate_tool runs on the accepted tool.
bool test_canonical_tools_accepted_end_to_end() {
  const auto& tools = kSupportedSketchTools();

  // The canonical list must contain no duplicates — a duplicated id would
  // mask drift between the two whitelist call sites.
  std::set<std::string> unique(tools.begin(), tools.end());
  if (!expect(unique.size() == tools.size(),
              "canonical: tool list has no duplicates")) {
    return false;
  }

  for (const auto& tool : tools) {
    DocumentManager manager;
    manager.create_document();
    manager.start_sketch_on_plane("ref-plane-xy");

    bool threw = false;
    DocumentState document;
    try {
      document = manager.set_sketch_tool(tool);
    } catch (const std::exception&) {
      threw = true;
    }
    if (!expect(!threw,
                ("tool: set_sketch_tool accepts \"" + tool + "\"").c_str())) {
      return false;
    }
    if (!expect(document.active_sketch_tool == tool,
                ("tool: active_sketch_tool records \"" + tool + "\"")
                    .c_str())) {
      return false;
    }
  }
  return true;
}

bool test_unknown_tools_rejected() {
  const std::vector<std::string> invalid = {"bogus", "rectangle2", ""};

  for (const auto& tool : invalid) {
    DocumentManager manager;
    manager.create_document();
    manager.start_sketch_on_plane("ref-plane-xy");

    bool threw = false;
    try {
      (void)manager.set_sketch_tool(tool);
    } catch (const std::exception&) {
      threw = true;
    }
    const std::string label = tool.empty() ? std::string("<empty>") : tool;
    if (!expect(threw,
                ("tool: set_sketch_tool rejects \"" + label + "\"").c_str())) {
      return false;
    }
  }
  return true;
}

// Resolves the repo root from the test source location so the suite runs
// regardless of the working directory (the runner spawns binaries from the
// pnpm root, but direct invocations may differ).
std::filesystem::path schema_path() {
  // tests/tool_whitelist_test.cpp -> tests -> cad-core -> native -> repo root.
  return std::filesystem::path(__FILE__)
             .parent_path()
             .parent_path()
             .parent_path()
             .parent_path() /
         "protocol" / "schema" / "commands.schema.json";
}

bool test_schema_enum_matches_vocabulary() {
  std::ifstream file(schema_path());
  if (!expect(file.good(), "schema: commands.schema.json exists and opens")) {
    return false;
  }
  nlohmann::json schema;
  try {
    file >> schema;
  } catch (const std::exception&) {
    return expect(false, "schema: commands.schema.json is valid JSON");
  }
  if (!expect(schema.contains("properties") &&
                  schema["properties"].contains("type") &&
                  schema["properties"]["type"].contains("enum"),
              "schema: has properties.type.enum")) {
    return false;
  }

  std::set<std::string> commands;
  for (const auto& entry : schema["properties"]["type"]["enum"]) {
    commands.insert(entry.get<std::string>());
  }

  // Renamed command vocabulary: these stale names must NOT come back.
  const std::vector<std::string> stale = {
      "update_sketch_point",
      "set_sketch_point_fixed",
      "select_sketch_point",
      "add_sketch_point_distance_dimension",
  };
  for (const auto& name : stale) {
    if (!expect(commands.count(name) == 0,
                ("schema: stale command \"" + name + "\" absent").c_str())) {
      return false;
    }
  }

  // Commands that are dispatched by the core and were missing from the
  // schema before the alignment pass — must stay listed.
  const std::vector<std::string> required = {
      "add_sketch_arc",
      "add_sketch_arc_radius_dimension",
      "add_sketch_fillet",
      "add_sketch_vertex_distance_dimension",
      "cancel_mirror_preview",
      "clear_sketch_line_constraints",
      "commit_mirror_preview",
      "delete_sketch_coincident_constraint",
      "delete_sketch_fillet",
      "delete_sketch_selection",
      "select_sketch_vertex",
      "set_sketch_line_construction",
      "set_sketch_midpoint_anchor",
      "set_sketch_tangent_constraint",
      "set_sketch_vertex_fixed",
      "set_sketch_vertex_line_anchor",
      "start_mirror_preview",
      "toggle_sketch_dimension_driven",
      "trim_preview",
      "update_sketch_fillet_radius",
      "update_sketch_vertex",
  };
  for (const auto& name : required) {
    if (!expect(commands.count(name) == 1,
                ("schema: dispatched command \"" + name + "\" present")
                    .c_str())) {
      return false;
    }
  }
  return true;
}

}  // namespace

int main() {
  if (!test_canonical_tools_accepted_end_to_end()) return 1;
  if (!test_unknown_tools_rejected()) return 1;
  if (!test_schema_enum_matches_vocabulary()) return 1;

  std::cout << "tool_whitelist_test passed\n";
  return 0;
}
