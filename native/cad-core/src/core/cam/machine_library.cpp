#include "core/cam/machine_library.h"

#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <optional>
#include <set>
#include <sstream>

#include <nlohmann/json.hpp>

#include "core/diagnostics/logger.h"

namespace polysmith::core {

namespace {

using json = nlohmann::json;

// ── Built-in machine definitions (the seed files) ─────────────────

const char* kGrblLaserDefinition = R"JSON({
  "name": "GRBL Laser",
  "machine_type": "laser",
  "post_processor": { "type": "grbl", "filename": "" },
  "work_area_x_mm": 400.0,
  "work_area_y_mm": 400.0,
  "pointer_offset_x_mm": 0.0,
  "pointer_offset_y_mm": 0.0
})JSON";

const char* kSmoothiewareLaserDefinition = R"JSON({
  "name": "Smoothieware Laser",
  "machine_type": "laser",
  "post_processor": { "type": "smoothieware", "filename": "" },
  "work_area_x_mm": 400.0,
  "work_area_y_mm": 400.0,
  "pointer_offset_x_mm": 0.0,
  "pointer_offset_y_mm": 0.0
})JSON";

const char* kGenericMillDefinition = R"JSON({
  "name": "Generic 3-Axis Mill",
  "machine_type": "3_axis_mill",
  "post_processor": { "type": "grbl", "filename": "" },
  "work_area_x_mm": 400.0,
  "work_area_y_mm": 400.0,
  "pointer_offset_x_mm": 0.0,
  "pointer_offset_y_mm": 0.0
})JSON";

std::vector<std::pair<std::string, std::string>> builtin_machine_definitions() {
  return {
      {"grbl-laser", kGrblLaserDefinition},
      {"smoothieware-laser", kSmoothiewareLaserDefinition},
      {"generic-3-axis-mill", kGenericMillDefinition},
  };
}

// ── Directory handling ────────────────────────────────────────────

std::filesystem::path machines_directory() {
  const char* env = std::getenv("POLYSMITH_MACHINES_DIR");
  if (env == nullptr || env[0] == '\0') {
    return {};
  }
  return std::filesystem::path(env);
}

// Seeds the user's machines directory with the built-in definitions
// when a file is missing.  Idempotent; user edits are never overwritten.
void seed_builtin_machines() {
  const auto dir = machines_directory();
  if (dir.empty()) {
    return;
  }
  std::error_code ec;
  std::filesystem::create_directories(dir, ec);
  for (const auto& [filename, text] : builtin_machine_definitions()) {
    const auto target = dir / (filename + ".json");
    if (!std::filesystem::exists(target)) {
      std::ofstream stream(target);
      if (stream.is_open()) {
        stream << text;
      }
    }
  }
}

// ── Validation ────────────────────────────────────────────────────

// Same machine-type set as the CAM setup validation
// (session_cam_commands.inc) — kept local so the library stays
// self-contained.
bool is_supported_machine_type(const std::string& type) {
  static const std::set<std::string> kSupported = {
      "3_axis_mill", "4_axis_mill", "5_axis_mill", "lathe_2_axis",
      "lathe_live_tooling", "laser", "plasma", "printer"};
  return kSupported.count(type) > 0;
}

bool validate_machine(const MachineDefinition& machine, std::string& error) {
  if (machine.name.empty()) {
    error = "the machine needs a name";
    return false;
  }
  if (!is_supported_machine_type(machine.machine_type)) {
    error = "unknown machine type: " + machine.machine_type;
    return false;
  }
  if (machine.machine_type == "laser" &&
      (machine.work_area_x_mm <= 0.0 || machine.work_area_y_mm <= 0.0)) {
    error = "laser machines need a positive work area";
    return false;
  }
  return true;
}

// ── Parsing ───────────────────────────────────────────────────────

double read_number(const json& payload, const char* key, double fallback) {
  if (!payload.contains(key) || payload.at(key).is_null()) {
    return fallback;
  }
  return payload.at(key).get<double>();
}

std::string read_string(const json& payload, const char* key,
                        const std::string& fallback) {
  if (!payload.contains(key) || payload.at(key).is_null()) {
    return fallback;
  }
  return payload.at(key).get<std::string>();
}

// Missing fields fall back to the struct defaults so machine files
// saved before a field existed still load (same convention as the
// protocol deserializers).
std::optional<MachineDefinition> parse_machine_definition(
    const std::string& json_text, std::string& error) {
  try {
    const json payload = json::parse(json_text);
    if (!payload.is_object()) {
      error = "machine definition must be a JSON object";
      return std::nullopt;
    }
    MachineDefinition machine;
    machine.name = read_string(payload, "name", "");
    machine.machine_type = read_string(payload, "machine_type", "laser");
    if (payload.contains("post_processor") &&
        payload.at("post_processor").is_object()) {
      const auto& post = payload.at("post_processor");
      machine.post_processor.type = read_string(post, "type", "fanuc");
      machine.post_processor.filename = read_string(post, "filename", "");
    }
    machine.work_area_x_mm = read_number(payload, "work_area_x_mm", 400.0);
    machine.work_area_y_mm = read_number(payload, "work_area_y_mm", 400.0);
    machine.pointer_offset_x_mm =
        read_number(payload, "pointer_offset_x_mm", 0.0);
    machine.pointer_offset_y_mm =
        read_number(payload, "pointer_offset_y_mm", 0.0);
    return machine;
  } catch (const std::exception& exception) {
    error = std::string("invalid machine JSON: ") + exception.what();
    return std::nullopt;
  }
}

// "My Laser!" → "my-laser".  Non-alphanumerics become '-', runs are
// collapsed, and a name that is nothing but punctuation falls back to
// "machine" so the file is never created as ".json".
std::string slugify(const std::string& name) {
  std::string slug;
  slug.reserve(name.size());
  for (const char c : name) {
    const bool alnum = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                       (c >= '0' && c <= '9');
    slug.push_back(alnum ? static_cast<char>(std::tolower(
                               static_cast<unsigned char>(c)))
                         : '-');
  }
  std::string collapsed;
  for (const char c : slug) {
    if (c == '-' && !collapsed.empty() && collapsed.back() == '-') {
      continue;
    }
    collapsed.push_back(c);
  }
  while (!collapsed.empty() && collapsed.front() == '-') {
    collapsed.erase(collapsed.begin());
  }
  while (!collapsed.empty() && collapsed.back() == '-') {
    collapsed.pop_back();
  }
  return collapsed.empty() ? "machine" : collapsed;
}

json to_json(const MachineDefinition& machine) {
  return {
      {"name", machine.name},
      {"machine_type", machine.machine_type},
      {"post_processor",
       {{"type", machine.post_processor.type},
        {"filename", machine.post_processor.filename}}},
      {"work_area_x_mm", machine.work_area_x_mm},
      {"work_area_y_mm", machine.work_area_y_mm},
      {"pointer_offset_x_mm", machine.pointer_offset_x_mm},
      {"pointer_offset_y_mm", machine.pointer_offset_y_mm},
  };
}

}  // namespace

// ── Library entry points ──────────────────────────────────────────

std::vector<MachineDefinition> load_machine_library() {
  seed_builtin_machines();
  std::vector<MachineDefinition> machines;
  // Built-ins first; a user file with the same name overrides.
  for (const auto& [filename, text] : builtin_machine_definitions()) {
    (void)filename;
    std::string error;
    const auto parsed = parse_machine_definition(text, error);
    if (parsed.has_value()) {
      machines.push_back(parsed.value());
    } else {
      log_warn("cam",
               "built-in machine definition failed to parse: " + error);
    }
  }
  const auto dir = machines_directory();
  if (dir.empty()) {
    return machines;
  }
  std::error_code ec;
  for (const auto& file : std::filesystem::directory_iterator(dir, ec)) {
    if (file.path().extension() != ".json") {
      continue;
    }
    std::ifstream stream(file.path());
    std::stringstream buffer;
    buffer << stream.rdbuf();
    std::string error;
    const auto parsed = parse_machine_definition(buffer.str(), error);
    if (!parsed.has_value() || !validate_machine(parsed.value(), error)) {
      log_warn("cam",
               "skipping machine file '" +
                   file.path().filename().string() + "': " + error);
      continue;
    }
    bool replaced = false;
    for (auto& existing : machines) {
      if (existing.name == parsed.value().name) {
        existing = parsed.value();
        replaced = true;
        break;
      }
    }
    if (!replaced) {
      machines.push_back(parsed.value());
    }
  }
  return machines;
}

std::string save_machine_definition(const MachineDefinition& machine,
                                    std::string& error) {
  if (!validate_machine(machine, error)) {
    return "";
  }
  const auto dir = machines_directory();
  if (dir.empty()) {
    error = "the machines directory is not configured";
    return "";
  }
  std::error_code ec;
  std::filesystem::create_directories(dir, ec);
  if (ec) {
    error = "failed to create the machines directory: " + ec.message();
    return "";
  }
  const std::string slug = slugify(machine.name);
  const auto target = dir / (slug + ".json");
  std::ofstream stream(target);
  if (!stream.is_open()) {
    error = "failed to write machine file '" + target.string() + "'";
    return "";
  }
  // Overwrites an existing file with the same slug — save = user
  // intent, same semantics as post import.
  stream << to_json(machine).dump(2);
  return slug;
}

}  // namespace polysmith::core
