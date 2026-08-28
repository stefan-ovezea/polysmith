#pragma once

#include <string>
#include <vector>

namespace polysmith::core {

// ── Post-processor definition ─────────────────────────────────────
//
// A post processor is a JSON FILE the user owns.  Every dialect is a
// definition in the user's posts directory (one <name>.json per
// machine), seeded with the built-in definitions on first use, and
// re-read from disk on every export — edits in an external editor
// apply immediately, and importing a definition is copying a file.
//
// The definition controls the OUTPUT SHAPE via line templates with
// {placeholders}.  The engine keeps the modal state (Z on change,
// feed on change, laser/spindle on-off transitions, arc
// linearization); the templates decide the actual syntax, so dialects
// genuinely differ without new code.

struct PostDefinition {
  // Template placeholders: {x} {y} {z} {feed} {i} {j} {power} {rpm}
  // {seconds} {op_name} {units_word}.  Missing templates fall back to
  // the GRBL-compatible defaults.
  std::string units_mm = "G21";
  std::string units_inch = "G20";
  std::vector<std::string> header_lines = {"(op: {op_name})", "{units_word}",
                                           "G90", "G94", "G17", "M5"};
  std::string rapid = "G0 X{x} Y{y}";
  std::string feed = "G1 X{x} Y{y}";
  std::string arc_cw = "G2 X{x} Y{y} I{i} J{j}";
  std::string arc_ccw = "G3 X{x} Y{y} I{i} J{j}";
  std::string dwell = "G4 P{seconds}";
  std::string laser_on_dynamic = "M4 S{power}";
  std::string laser_on_constant = "M3 S{power}";
  std::string laser_off = "M5";
  std::string spindle_on = "M3 S{rpm}";
  std::string spindle_off = "M5";
  std::vector<std::string> footer_lines = {"M5", "G0 Z{safety_z}", "M2"};

  // S value at 100% power (GRBL: 1000; other boards differ).
  double power_max = 1000.0;
  bool line_numbers = true;
  bool use_arcs = true;
  int decimal_places = 3;
};

// Parses a definition from JSON.  Unknown keys are ignored; missing
// keys keep the defaults.  Returns false (with a message) when the
// JSON is not an object.
bool parse_post_definition(const std::string& json_text,
                           PostDefinition& definition, std::string& error);

// The built-in definitions (the seed for the user's posts directory).
std::vector<std::pair<std::string, std::string>> builtin_post_definitions();

}  // namespace polysmith::core
