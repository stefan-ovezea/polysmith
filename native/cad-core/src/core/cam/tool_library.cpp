#include "core/cam/tool_library.h"

#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <set>
#include <sstream>

#include <nlohmann/json.hpp>

#include "core/diagnostics/logger.h"

namespace polysmith::core {

namespace {

using json = nlohmann::json;

// ── Built-in generic catalog (the seed set) ────────────────────────
//
// A starter library of realistic round-metric tools (FreeCAD's nine
// built-in toolbits taken further: flat/ball/bull end mills, chamfer
// mills, V-bits, spot drills and DIN 338 jobber twist drills).  Only
// the fields that differ from the ToolEntry struct defaults are
// listed; tool_from_json fills the rest.  guids are the stable
// "generic-tNN" identity used for import reconciliation.

const char* kGenericCatalog = R"JSON([
{"tool_number":1,"guid":"generic-t01","name":"Flat End Mill Ø1 2F","type":"endmill_flat","diameter_mm":1.0,"flute_length_mm":3.0,"overall_length_mm":38.0,"shank_diameter_mm":3.0,"flutes":2},
{"tool_number":2,"guid":"generic-t02","name":"Flat End Mill Ø1.5 2F","type":"endmill_flat","diameter_mm":1.5,"flute_length_mm":4.0,"overall_length_mm":38.0,"shank_diameter_mm":3.0,"flutes":2},
{"tool_number":3,"guid":"generic-t03","name":"Flat End Mill Ø2 2F","type":"endmill_flat","diameter_mm":2.0,"flute_length_mm":6.0,"overall_length_mm":38.0,"shank_diameter_mm":3.0,"flutes":2},
{"tool_number":4,"guid":"generic-t04","name":"Flat End Mill Ø3 2F","type":"endmill_flat","diameter_mm":3.0,"flute_length_mm":8.0,"overall_length_mm":38.0,"shank_diameter_mm":3.0,"flutes":2},
{"tool_number":5,"guid":"generic-t05","name":"Flat End Mill Ø4 2F","type":"endmill_flat","diameter_mm":4.0,"flute_length_mm":11.0,"overall_length_mm":50.0,"shank_diameter_mm":4.0,"flutes":2},
{"tool_number":6,"guid":"generic-t06","name":"Flat End Mill Ø5 2F","type":"endmill_flat","diameter_mm":5.0,"flute_length_mm":13.0,"overall_length_mm":50.0,"shank_diameter_mm":5.0,"flutes":2},
{"tool_number":7,"guid":"generic-t07","name":"Flat End Mill Ø6 2F","type":"endmill_flat","diameter_mm":6.0,"flute_length_mm":18.0,"overall_length_mm":50.0,"shank_diameter_mm":6.0,"flutes":2},
{"tool_number":8,"guid":"generic-t08","name":"Flat End Mill Ø6 4F","type":"endmill_flat","diameter_mm":6.0,"flute_length_mm":18.0,"overall_length_mm":50.0,"shank_diameter_mm":6.0,"flutes":4},
{"tool_number":9,"guid":"generic-t09","name":"Flat End Mill Ø8 2F","type":"endmill_flat","diameter_mm":8.0,"flute_length_mm":20.0,"overall_length_mm":60.0,"shank_diameter_mm":8.0,"flutes":2},
{"tool_number":10,"guid":"generic-t10","name":"Flat End Mill Ø8 4F","type":"endmill_flat","diameter_mm":8.0,"flute_length_mm":20.0,"overall_length_mm":60.0,"shank_diameter_mm":8.0,"flutes":4},
{"tool_number":11,"guid":"generic-t11","name":"Ball End Mill Ø1","type":"endmill_ball","diameter_mm":1.0,"flute_length_mm":2.0,"overall_length_mm":38.0,"shank_diameter_mm":3.0,"flutes":2,"corner_radius_mm":0.5},
{"tool_number":12,"guid":"generic-t12","name":"Ball End Mill Ø2","type":"endmill_ball","diameter_mm":2.0,"flute_length_mm":4.0,"overall_length_mm":38.0,"shank_diameter_mm":3.0,"flutes":2,"corner_radius_mm":1.0},
{"tool_number":13,"guid":"generic-t13","name":"Ball End Mill Ø3","type":"endmill_ball","diameter_mm":3.0,"flute_length_mm":6.0,"overall_length_mm":38.0,"shank_diameter_mm":3.0,"flutes":2,"corner_radius_mm":1.5},
{"tool_number":14,"guid":"generic-t14","name":"Ball End Mill Ø4","type":"endmill_ball","diameter_mm":4.0,"flute_length_mm":8.0,"overall_length_mm":50.0,"shank_diameter_mm":4.0,"flutes":2,"corner_radius_mm":2.0},
{"tool_number":15,"guid":"generic-t15","name":"Ball End Mill Ø6","type":"endmill_ball","diameter_mm":6.0,"flute_length_mm":12.0,"overall_length_mm":50.0,"shank_diameter_mm":6.0,"flutes":2,"corner_radius_mm":3.0},
{"tool_number":16,"guid":"generic-t16","name":"Ball End Mill Ø8","type":"endmill_ball","diameter_mm":8.0,"flute_length_mm":16.0,"overall_length_mm":60.0,"shank_diameter_mm":8.0,"flutes":2,"corner_radius_mm":4.0},
{"tool_number":17,"guid":"generic-t17","name":"Bull Nose Ø6 R1","type":"endmill_bull","diameter_mm":6.0,"flute_length_mm":18.0,"overall_length_mm":50.0,"shank_diameter_mm":6.0,"flutes":2,"corner_radius_mm":1.0},
{"tool_number":18,"guid":"generic-t18","name":"Bull Nose Ø6 R2","type":"endmill_bull","diameter_mm":6.0,"flute_length_mm":18.0,"overall_length_mm":50.0,"shank_diameter_mm":6.0,"flutes":2,"corner_radius_mm":2.0},
{"tool_number":19,"guid":"generic-t19","name":"Bull Nose Ø8 R1","type":"endmill_bull","diameter_mm":8.0,"flute_length_mm":20.0,"overall_length_mm":60.0,"shank_diameter_mm":8.0,"flutes":2,"corner_radius_mm":1.0},
{"tool_number":20,"guid":"generic-t20","name":"Chamfer Mill 60° Ø6","type":"chamfer","diameter_mm":6.0,"flute_length_mm":12.0,"overall_length_mm":50.0,"shank_diameter_mm":6.0,"flutes":2,"point_angle_deg":60.0,"tip_diameter_mm":0.2},
{"tool_number":21,"guid":"generic-t21","name":"Chamfer Mill 90° Ø6","type":"chamfer","diameter_mm":6.0,"flute_length_mm":12.0,"overall_length_mm":50.0,"shank_diameter_mm":6.0,"flutes":2,"point_angle_deg":90.0,"tip_diameter_mm":0.2},
{"tool_number":22,"guid":"generic-t22","name":"Chamfer Mill 90° Ø10","type":"chamfer","diameter_mm":10.0,"flute_length_mm":16.0,"overall_length_mm":60.0,"shank_diameter_mm":10.0,"flutes":2,"point_angle_deg":90.0,"tip_diameter_mm":0.2},
{"tool_number":23,"guid":"generic-t23","name":"Chamfer Mill 120° Ø6","type":"chamfer","diameter_mm":6.0,"flute_length_mm":12.0,"overall_length_mm":50.0,"shank_diameter_mm":6.0,"flutes":2,"point_angle_deg":120.0,"tip_diameter_mm":0.2},
{"tool_number":24,"guid":"generic-t24","name":"V-Bit 30° Ø6","type":"v_bit","diameter_mm":6.0,"flute_length_mm":12.0,"overall_length_mm":50.0,"shank_diameter_mm":6.0,"flutes":2,"point_angle_deg":30.0,"tip_diameter_mm":0.1},
{"tool_number":25,"guid":"generic-t25","name":"V-Bit 60° Ø6","type":"v_bit","diameter_mm":6.0,"flute_length_mm":12.0,"overall_length_mm":50.0,"shank_diameter_mm":6.0,"flutes":2,"point_angle_deg":60.0,"tip_diameter_mm":0.1},
{"tool_number":26,"guid":"generic-t26","name":"V-Bit 90° Ø6","type":"v_bit","diameter_mm":6.0,"flute_length_mm":12.0,"overall_length_mm":50.0,"shank_diameter_mm":6.0,"flutes":2,"point_angle_deg":90.0,"tip_diameter_mm":0.1},
{"tool_number":27,"guid":"generic-t27","name":"Engraving Bit 30° Ø3.175","type":"v_bit","diameter_mm":3.175,"flute_length_mm":8.0,"overall_length_mm":38.0,"shank_diameter_mm":3.175,"flutes":2,"point_angle_deg":30.0,"tip_diameter_mm":0.1},
{"tool_number":28,"guid":"generic-t28","name":"Spot Drill 90° Ø3","type":"spot_drill","diameter_mm":3.0,"flute_length_mm":6.0,"overall_length_mm":40.0,"shank_diameter_mm":3.0,"flutes":2,"point_angle_deg":90.0},
{"tool_number":29,"guid":"generic-t29","name":"Spot Drill 90° Ø6","type":"spot_drill","diameter_mm":6.0,"flute_length_mm":12.0,"overall_length_mm":50.0,"shank_diameter_mm":6.0,"flutes":2,"point_angle_deg":90.0},
{"tool_number":30,"guid":"generic-t30","name":"Spot Drill 90° Ø8","type":"spot_drill","diameter_mm":8.0,"flute_length_mm":16.0,"overall_length_mm":60.0,"shank_diameter_mm":8.0,"flutes":2,"point_angle_deg":90.0},
{"tool_number":31,"guid":"generic-t31","name":"Twist Drill Ø1.0","type":"drill","diameter_mm":1.0,"flute_length_mm":12.0,"overall_length_mm":34.0,"shank_diameter_mm":1.0,"flutes":2,"point_angle_deg":118.0},
{"tool_number":32,"guid":"generic-t32","name":"Twist Drill Ø1.5","type":"drill","diameter_mm":1.5,"flute_length_mm":18.0,"overall_length_mm":40.0,"shank_diameter_mm":1.5,"flutes":2,"point_angle_deg":118.0},
{"tool_number":33,"guid":"generic-t33","name":"Twist Drill Ø2.0","type":"drill","diameter_mm":2.0,"flute_length_mm":24.0,"overall_length_mm":49.0,"shank_diameter_mm":2.0,"flutes":2,"point_angle_deg":118.0},
{"tool_number":34,"guid":"generic-t34","name":"Twist Drill Ø2.5","type":"drill","diameter_mm":2.5,"flute_length_mm":30.0,"overall_length_mm":57.0,"shank_diameter_mm":2.5,"flutes":2,"point_angle_deg":118.0},
{"tool_number":35,"guid":"generic-t35","name":"Twist Drill Ø3.0","type":"drill","diameter_mm":3.0,"flute_length_mm":33.0,"overall_length_mm":61.0,"shank_diameter_mm":3.0,"flutes":2,"point_angle_deg":118.0},
{"tool_number":36,"guid":"generic-t36","name":"Twist Drill Ø3.5","type":"drill","diameter_mm":3.5,"flute_length_mm":39.0,"overall_length_mm":70.0,"shank_diameter_mm":3.5,"flutes":2,"point_angle_deg":118.0},
{"tool_number":37,"guid":"generic-t37","name":"Twist Drill Ø4.0","type":"drill","diameter_mm":4.0,"flute_length_mm":43.0,"overall_length_mm":75.0,"shank_diameter_mm":4.0,"flutes":2,"point_angle_deg":118.0},
{"tool_number":38,"guid":"generic-t38","name":"Twist Drill Ø4.5","type":"drill","diameter_mm":4.5,"flute_length_mm":47.0,"overall_length_mm":80.0,"shank_diameter_mm":4.5,"flutes":2,"point_angle_deg":118.0},
{"tool_number":39,"guid":"generic-t39","name":"Twist Drill Ø5.0","type":"drill","diameter_mm":5.0,"flute_length_mm":52.0,"overall_length_mm":86.0,"shank_diameter_mm":5.0,"flutes":2,"point_angle_deg":118.0},
{"tool_number":40,"guid":"generic-t40","name":"Twist Drill Ø6.0","type":"drill","diameter_mm":6.0,"flute_length_mm":57.0,"overall_length_mm":93.0,"shank_diameter_mm":6.0,"flutes":2,"point_angle_deg":118.0},
{"tool_number":41,"guid":"generic-t41","name":"Twist Drill Ø8.0","type":"drill","diameter_mm":8.0,"flute_length_mm":75.0,"overall_length_mm":117.0,"shank_diameter_mm":8.0,"flutes":2,"point_angle_deg":118.0},
{"tool_number":42,"guid":"generic-t42","name":"Twist Drill Ø10.0","type":"drill","diameter_mm":10.0,"flute_length_mm":87.0,"overall_length_mm":133.0,"shank_diameter_mm":10.0,"flutes":2,"point_angle_deg":118.0}
])JSON";

// ── Directory handling ────────────────────────────────────────────

std::filesystem::path tools_directory() {
  const char* env = std::getenv("POLYSMITH_TOOLS_DIR");
  if (env == nullptr || env[0] == '\0') {
    return {};
  }
  return std::filesystem::path(env);
}

// ── Parsing / serializing (payload field names, struct-default
//    fallbacks — same convention as the protocol deserializers) ────

double read_number(const json& payload, const char* key, double fallback) {
  if (!payload.contains(key) || payload.at(key).is_null()) {
    return fallback;
  }
  return payload.at(key).get<double>();
}

int read_int(const json& payload, const char* key, int fallback) {
  if (!payload.contains(key) || payload.at(key).is_null()) {
    return fallback;
  }
  return payload.at(key).get<int>();
}

std::string read_string(const json& payload, const char* key,
                        const std::string& fallback) {
  if (!payload.contains(key) || payload.at(key).is_null()) {
    return fallback;
  }
  return payload.at(key).get<std::string>();
}

bool read_bool(const json& payload, const char* key, bool fallback) {
  if (!payload.contains(key) || payload.at(key).is_null()) {
    return fallback;
  }
  return payload.at(key).get<bool>();
}

ToolEntry tool_from_json(const json& payload) {
  ToolEntry tool;
  tool.tool_id = read_string(payload, "tool_id", "");
  tool.name = read_string(payload, "name", "");
  tool.type = read_string(payload, "type", "endmill_flat");
  tool.tool_number = read_int(payload, "tool_number", 0);
  tool.pocket_number = read_int(payload, "pocket_number", 0);
  tool.description = read_string(payload, "description", "");
  tool.vendor = read_string(payload, "vendor", "");
  tool.product_id = read_string(payload, "product_id", "");
  tool.guid = read_string(payload, "guid", "");
  tool.diameter_mm = read_number(payload, "diameter_mm", 6.0);
  tool.corner_radius_mm = read_number(payload, "corner_radius_mm", 0.0);
  tool.flute_length_mm = read_number(payload, "flute_length_mm", 20.0);
  tool.overall_length_mm = read_number(payload, "overall_length_mm", 60.0);
  tool.shank_diameter_mm = read_number(payload, "shank_diameter_mm", 6.0);
  tool.shoulder_length_mm = read_number(payload, "shoulder_length_mm", 0.0);
  tool.length_below_holder_mm =
      read_number(payload, "length_below_holder_mm", 0.0);
  tool.flutes = read_int(payload, "flutes", 2);
  tool.helix_angle_deg = read_number(payload, "helix_angle_deg", 30.0);
  tool.point_angle_deg = read_number(payload, "point_angle_deg", 118.0);
  tool.tip_diameter_mm = read_number(payload, "tip_diameter_mm", 0.0);
  tool.tip_length_mm = read_number(payload, "tip_length_mm", 0.0);
  tool.taper_angle_deg = read_number(payload, "taper_angle_deg", 0.0);
  tool.front_angle_deg = read_number(payload, "front_angle_deg", 0.0);
  tool.back_angle_deg = read_number(payload, "back_angle_deg", 0.0);
  tool.orientation = read_int(payload, "orientation", 0);
  tool.material = read_string(payload, "material", "carbide");
  const std::string coating = read_string(payload, "coating", "");
  if (!coating.empty()) {
    tool.coating = coating;
  }
  tool.coolant_through = read_bool(payload, "coolant_through", false);
  tool.max_spindle_rpm = read_number(payload, "max_spindle_rpm", 15000.0);
  tool.surface_speed_m_per_min =
      read_number(payload, "surface_speed_m_per_min", 100.0);
  tool.feed_per_tooth_mm = read_number(payload, "feed_per_tooth_mm", 0.05);
  tool.default_feedrate_mm_per_min =
      read_number(payload, "default_feedrate_mm_per_min", 1000.0);
  tool.default_plunge_feedrate_mm_per_min =
      read_number(payload, "default_plunge_feedrate_mm_per_min", 500.0);
  tool.default_stepdown_mm = read_number(payload, "default_stepdown_mm", 1.0);
  tool.default_stepover_percent =
      read_number(payload, "default_stepover_percent", 50.0);
  return tool;
}

json to_json(const ToolEntry& tool) {
  json j{
      {"tool_id", tool.tool_id},
      {"name", tool.name},
      {"type", tool.type},
      {"tool_number", tool.tool_number},
      {"pocket_number", tool.pocket_number},
      {"description", tool.description},
      {"vendor", tool.vendor},
      {"product_id", tool.product_id},
      {"guid", tool.guid},
      {"diameter_mm", tool.diameter_mm},
      {"corner_radius_mm", tool.corner_radius_mm},
      {"flute_length_mm", tool.flute_length_mm},
      {"overall_length_mm", tool.overall_length_mm},
      {"shank_diameter_mm", tool.shank_diameter_mm},
      {"shoulder_length_mm", tool.shoulder_length_mm},
      {"length_below_holder_mm", tool.length_below_holder_mm},
      {"flutes", tool.flutes},
      {"helix_angle_deg", tool.helix_angle_deg},
      {"point_angle_deg", tool.point_angle_deg},
      {"tip_diameter_mm", tool.tip_diameter_mm},
      {"tip_length_mm", tool.tip_length_mm},
      {"taper_angle_deg", tool.taper_angle_deg},
      {"front_angle_deg", tool.front_angle_deg},
      {"back_angle_deg", tool.back_angle_deg},
      {"orientation", tool.orientation},
      {"material", tool.material},
      {"coolant_through", tool.coolant_through},
      {"max_spindle_rpm", tool.max_spindle_rpm},
      {"surface_speed_m_per_min", tool.surface_speed_m_per_min},
      {"feed_per_tooth_mm", tool.feed_per_tooth_mm},
      {"default_feedrate_mm_per_min", tool.default_feedrate_mm_per_min},
      {"default_plunge_feedrate_mm_per_min",
       tool.default_plunge_feedrate_mm_per_min},
      {"default_stepdown_mm", tool.default_stepdown_mm},
      {"default_stepover_percent", tool.default_stepover_percent},
  };
  if (tool.coating.has_value()) j["coating"] = tool.coating.value();
  return j;
}

// ── Validation (same rules as the document-side cam_tool_add) ─────

bool is_supported_tool_type(const std::string& type) {
  static const std::set<std::string> kSupported = {
      "endmill_flat", "endmill_ball", "endmill_bull", "drill",
      "facemill", "chamfer", "threadmill", "turning_insert",
      "laser", "plasma", "v_bit", "spot_drill"};
  return kSupported.count(type) > 0;
}

bool validate_tool_library_entry(const ToolEntry& tool, std::string& error) {
  if (tool.name.empty()) {
    error = "the tool needs a name";
    return false;
  }
  if (!is_supported_tool_type(tool.type)) {
    error = "unknown tool type: " + tool.type;
    return false;
  }
  if (tool.diameter_mm <= 0.0 || tool.shank_diameter_mm <= 0.0 ||
      tool.flute_length_mm <= 0.0) {
    error = "diameter, shank diameter and flute length must be positive";
    return false;
  }
  if (tool.overall_length_mm < tool.flute_length_mm) {
    error = "the overall length must be at least the flute length";
    return false;
  }
  if (tool.corner_radius_mm < 0.0 ||
      tool.corner_radius_mm > tool.diameter_mm / 2.0) {
    error = "the corner radius must be between 0 and half the diameter";
    return false;
  }
  return true;
}

// ── Seeding ────────────────────────────────────────────────────────

// "My Endmill!" → "my-endmill".  Non-alphanumerics become '-', runs
// are collapsed, and a name that is nothing but punctuation falls
// back to "tool" so the file is never created as ".tool.json".
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
  return collapsed.empty() ? "tool" : collapsed;
}

// t<N> prefix keeps files sorted by tool number and lets two tools
// share a name without colliding.
std::string tool_file_stem(const ToolEntry& tool) {
  const std::string number =
      tool.tool_number > 0 ? std::to_string(tool.tool_number) : "0";
  return "t" + number + "-" + slugify(tool.name);
}

std::vector<ToolEntry> parse_catalog() {
  std::vector<ToolEntry> tools;
  try {
    const json payload = json::parse(kGenericCatalog);
    if (payload.is_array()) {
      for (const auto& entry : payload) {
        if (entry.is_object()) {
          tools.push_back(tool_from_json(entry));
        }
      }
    }
  } catch (const std::exception& exception) {
    log_warn("cam",
             std::string("built-in tool catalog failed to parse: ") +
                 exception.what());
  }
  return tools;
}

// Seeds the user's tools directory with the generic catalog when a
// file is missing.  Idempotent; user edits are never overwritten.
void seed_builtin_tools() {
  const auto dir = tools_directory();
  if (dir.empty()) {
    return;
  }
  std::error_code ec;
  std::filesystem::create_directories(dir, ec);
  for (const auto& tool : parse_catalog()) {
    const std::string filename = tool_file_stem(tool) + ".tool.json";
    const auto target = dir / filename;
    if (!std::filesystem::exists(target)) {
      std::ofstream stream(target);
      if (stream.is_open()) {
        stream << to_json(tool).dump(2);
      }
    }
  }
}

}  // namespace

// ── Library entry points ──────────────────────────────────────────

std::vector<ToolEntry> load_tool_library() {
  seed_builtin_tools();
  std::vector<ToolEntry> tools;
  std::vector<std::string> stems;
  // Built-in catalog first; a user file whose stem matches a seeded
  // one overrides it.
  for (const auto& tool : parse_catalog()) {
    tools.push_back(tool);
    stems.push_back(tool_file_stem(tool));
  }
  const auto dir = tools_directory();
  if (dir.empty()) {
    return tools;
  }
  std::error_code ec;
  for (const auto& file : std::filesystem::directory_iterator(dir, ec)) {
    // Only <stem>.tool.json files belong to the library — the tools
    // directory is reserved for tool files.
    const std::string filename = file.path().filename().string();
    if (filename.size() < 10 ||
        filename.compare(filename.size() - 10, 10, ".tool.json") != 0) {
      continue;
    }
    std::ifstream stream(file.path());
    std::stringstream buffer;
    buffer << stream.rdbuf();
    std::string error;
    ToolEntry tool;
    try {
      const json payload = json::parse(buffer.str());
      if (!payload.is_object()) {
        throw std::runtime_error("tool file must be a JSON object");
      }
      tool = tool_from_json(payload);
    } catch (const std::exception& exception) {
      log_warn("cam", "skipping tool file '" +
                          file.path().filename().string() +
                          "': " + exception.what());
      continue;
    }
    if (!validate_tool_library_entry(tool, error)) {
      log_warn("cam", "skipping tool file '" +
                          file.path().filename().string() + "': " + error);
      continue;
    }
    // path::stem() would keep the ".tool" part (it strips only the
    // LAST dot), so strip the suffix from the filename directly.
    const std::string stem = filename.substr(0, filename.size() - 10);
    bool replaced = false;
    for (size_t i = 0; i < stems.size(); ++i) {
      if (stems[i] == stem) {
        tools[i] = tool;
        replaced = true;
        break;
      }
    }
    if (!replaced) {
      tools.push_back(tool);
      stems.push_back(stem);
    }
  }
  return tools;
}

std::string save_tool_entry(const ToolEntry& tool, std::string& error) {
  if (!validate_tool_library_entry(tool, error)) {
    return "";
  }
  const auto dir = tools_directory();
  if (dir.empty()) {
    error = "the tools directory is not configured";
    return "";
  }
  std::error_code ec;
  std::filesystem::create_directories(dir, ec);
  if (ec) {
    error = "failed to create the tools directory: " + ec.message();
    return "";
  }
  const std::string slug = tool_file_stem(tool);
  const auto target = dir / (slug + ".tool.json");
  std::ofstream stream(target);
  if (!stream.is_open()) {
    error = "failed to write tool file '" + target.string() + "'";
    return "";
  }
  // Overwrites an existing file with the same slug — save = user
  // intent, same semantics as machine save.
  stream << to_json(tool).dump(2);
  return slug;
}

bool is_generic_catalog_tool(const ToolEntry& tool) {
  return tool.guid.rfind("generic-t", 0) == 0;
}

}  // namespace polysmith::core
