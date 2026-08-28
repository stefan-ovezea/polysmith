#include "core/cam/post_processor.h"

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <map>
#include <sstream>

#include <nlohmann/json.hpp>

#include "core/cam/post_definition.h"
#include "core/cam/toolpath_geometry.h"

namespace polysmith::core {

namespace {

using json = nlohmann::json;

// ── Built-in post definitions (the seed files) ────────────────────

const char* kGrblDefinition = R"JSON({
  "units_mm": "G21",
  "units_inch": "G20",
  "header_lines": ["(op: {op_name})", "{units_word}", "G90", "G94", "G17", "M5"],
  "rapid": "G0 X{x} Y{y}",
  "feed": "G1 X{x} Y{y}",
  "arc_cw": "G2 X{x} Y{y} I{i} J{j}",
  "arc_ccw": "G3 X{x} Y{y} I{i} J{j}",
  "dwell": "G4 P{seconds}",
  "laser_on_dynamic": "M4 S{power}",
  "laser_on_constant": "M3 S{power}",
  "laser_off": "M5",
  "spindle_on": "M3 S{rpm}",
  "spindle_off": "M5",
  "footer_lines": ["M5", "G0 Z{safety_z}", "M2"],
  "power_max": 1000,
  "line_numbers": false,
  "use_arcs": true,
  "decimal_places": 3
})JSON";

const char* kLinuxcncDefinition = R"JSON({
  "units_mm": "G21",
  "units_inch": "G20",
  "header_lines": ["(op: {op_name})", "{units_word}", "G90", "G94", "G17", "M5"],
  "rapid": "G0 X{x} Y{y}",
  "feed": "G1 X{x} Y{y}",
  "arc_cw": "G2 X{x} Y{y} I{i} J{j}",
  "arc_ccw": "G3 X{x} Y{y} I{i} J{j}",
  "dwell": "G4 P{seconds}",
  "laser_on_dynamic": "M4 S{power}",
  "laser_on_constant": "M3 S{power}",
  "laser_off": "M5",
  "spindle_on": "M3 S{rpm}",
  "spindle_off": "M5",
  "footer_lines": ["M5", "G0 Z{safety_z}", "M2"],
  "power_max": 1000,
  "line_numbers": true,
  "use_arcs": true,
  "decimal_places": 3
})JSON";

const char* kMach3Definition = R"JSON({
  "units_mm": "G21",
  "units_inch": "G20",
  "header_lines": ["(op: {op_name})", "{units_word}", "G90", "G94", "G17", "M5"],
  "rapid": "G0 X{x} Y{y}",
  "feed": "G1 X{x} Y{y}",
  "arc_cw": "G2 X{x} Y{y} I{i} J{j}",
  "arc_ccw": "G3 X{x} Y{y} I{i} J{j}",
  "dwell": "G4 P{seconds}",
  "laser_on_dynamic": "M4 S{power}",
  "laser_on_constant": "M3 S{power}",
  "laser_off": "M5",
  "spindle_on": "M3 S{rpm}",
  "spindle_off": "M5",
  "footer_lines": ["M5", "M30"],
  "power_max": 1000,
  "line_numbers": true,
  "use_arcs": true,
  "decimal_places": 3
})JSON";

const char* kMach4Definition = R"JSON({
  "units_mm": "G21",
  "units_inch": "G20",
  "header_lines": ["(op: {op_name})", "{units_word}", "G90", "G94", "G17", "M5"],
  "rapid": "G0 X{x} Y{y}",
  "feed": "G1 X{x} Y{y}",
  "arc_cw": "G2 X{x} Y{y} I{i} J{j}",
  "arc_ccw": "G3 X{x} Y{y} I{i} J{j}",
  "dwell": "G4 P{seconds}",
  "laser_on_dynamic": "M4 S{power}",
  "laser_on_constant": "M3 S{power}",
  "laser_off": "M5",
  "spindle_on": "M3 S{rpm}",
  "spindle_off": "M5",
  "footer_lines": ["M5", "G0 Z{safety_z}", "M30"],
  "power_max": 1000,
  "line_numbers": false,
  "use_arcs": true,
  "decimal_places": 3
})JSON";

const char* kMarlinDefinition = R"JSON({
  "units_mm": "G21",
  "units_inch": "G20",
  "header_lines": ["(op: {op_name})", "{units_word}", "G90", "M5"],
  "rapid": "G0 X{x} Y{y}",
  "feed": "G1 X{x} Y{y}",
  "arc_cw": "G2 X{x} Y{y} I{i} J{j}",
  "arc_ccw": "G3 X{x} Y{y} I{i} J{j}",
  "dwell": "G4 P{seconds}",
  "laser_on_dynamic": "M4 S{power}",
  "laser_on_constant": "M3 S{power}",
  "laser_off": "M5",
  "spindle_on": "M3 S{rpm}",
  "spindle_off": "M5",
  "footer_lines": ["M5", "M2"],
  "power_max": 255,
  "line_numbers": false,
  "use_arcs": true,
  "decimal_places": 2
})JSON";

const char* kFanucDefinition = R"JSON({
  "units_mm": "G21",
  "units_inch": "G20",
  "header_lines": ["(op: {op_name})", "{units_word}", "G90", "G94", "G17", "M5"],
  "rapid": "G0 X{x} Y{y}",
  "feed": "G1 X{x} Y{y}",
  "arc_cw": "G2 X{x} Y{y} I{i} J{j}",
  "arc_ccw": "G3 X{x} Y{y} I{i} J{j}",
  "dwell": "G4 P{seconds}",
  "laser_on_dynamic": "M4 S{power}",
  "laser_on_constant": "M3 S{power}",
  "laser_off": "M5",
  "spindle_on": "M3 S{rpm}",
  "spindle_off": "M5",
  "footer_lines": ["M5", "G0 Z{safety_z}", "M30"],
  "power_max": 1000,
  "line_numbers": true,
  "use_arcs": true,
  "decimal_places": 3
})JSON";

// ── Directory handling ────────────────────────────────────────────

std::filesystem::path posts_directory() {
  const char* env = std::getenv("POLYSMITH_POSTS_DIR");
  if (env == nullptr || env[0] == '\0') {
    return {};
  }
  return std::filesystem::path(env);
}

// Seeds the user's posts directory with the built-in definitions when
// a file is missing.  Idempotent; user edits are never overwritten.
void seed_builtin_posts() {
  const auto dir = posts_directory();
  if (dir.empty()) {
    return;
  }
  std::error_code ec;
  std::filesystem::create_directories(dir, ec);
  for (const auto& [name, text] : builtin_post_definitions()) {
    const auto target = dir / (name + ".json");
    if (!std::filesystem::exists(target)) {
      std::ofstream stream(target);
      if (stream.is_open()) {
        stream << text;
      }
    }
  }
}

// ── Definition parsing ────────────────────────────────────────────

std::string read_optional_template(const json& payload, const char* key,
                                   const std::string& fallback) {
  if (payload.contains(key) && payload.at(key).is_string()) {
    return payload.at(key).get<std::string>();
  }
  return fallback;
}

std::vector<std::string> read_optional_lines(const json& payload,
                                             const char* key,
                                             const std::vector<std::string>& fallback) {
  if (!payload.contains(key) || !payload.at(key).is_array()) {
    return fallback;
  }
  std::vector<std::string> lines;
  for (const auto& entry : payload.at(key)) {
    if (entry.is_string()) {
      lines.push_back(entry.get<std::string>());
    }
  }
  return lines.empty() ? fallback : lines;
}

// ── Template rendering ────────────────────────────────────────────

std::string fmt_number(double value, int decimals) {
  char buffer[64];
  std::snprintf(buffer, sizeof(buffer), "%.*f", decimals, value);
  return buffer;
}

// Replaces {placeholders} in a template.  Unknown placeholders are
// left verbatim so a broken template is visible in the output.
std::string render_template(const std::string& templ,
                            const std::map<std::string, std::string>& vars) {
  std::string out = templ;
  for (const auto& [key, value] : vars) {
    const std::string placeholder = "{" + key + "}";
    size_t pos = 0;
    while ((pos = out.find(placeholder, pos)) != std::string::npos) {
      out.replace(pos, placeholder.size(), value);
      pos += value.size();
    }
  }
  return out;
}

std::map<std::string, std::string> common_vars(const PostContext& context) {
  return {
      {"op_name", context.op_name},
      {"units_word", context.setup.units == "inch" ? context.definition.units_inch
                                                   : context.definition.units_mm},
      {"safety_z", fmt_number(context.setup.safety_height - context.wcs_origin[2],
                              context.definition.decimal_places)},
  };
}

std::vector<std::string> render_post(const PostContext& context) {
  const auto& def = context.definition;
  std::vector<std::string> lines;
  int sequence = 0;

  const auto emit = [&](const std::string& line) {
    if (def.line_numbers) {
      lines.push_back("N" + std::to_string(++sequence * 10) + " " + line);
    } else {
      lines.push_back(line);
    }
  };

  const double originX = context.wcs_origin[0];
  const double originY = context.wcs_origin[1];
  const double originZ = context.wcs_origin[2];
  const int decimals = def.decimal_places;

  // Header.
  for (const auto& templ : def.header_lines) {
    emit(render_template(templ, common_vars(context)));
  }

  bool laserOn = false;
  bool spindleOn = false;
  double currentPower = -1.0;
  double lastFeed = -1.0;
  double currentZ = 0.0;
  bool haveZ = false;

  const auto ensure_power_state = [&](const ToolpathMove& move) {
    if (!context.laser.has_value()) {
      if (!spindleOn) {
        emit(render_template(def.spindle_on,
                             {{"rpm", std::to_string(static_cast<int>(context.spindle_rpm))}}));
        spindleOn = true;
      }
      return;
    }
    if (move.laser_on && !laserOn) {
      const double power =
          std::round(move.power_percent / 100.0 * def.power_max);
      const bool dynamic =
          context.laser->dynamic_power && context.laser->mode != "engrave";
      emit(render_template(dynamic ? def.laser_on_dynamic
                                   : def.laser_on_constant,
                           {{"power", std::to_string(static_cast<int>(power))}}));
      currentPower = power;
      laserOn = true;
    } else if (!move.laser_on && laserOn) {
      emit(render_template(def.laser_off, {}));
      laserOn = false;
    }
  };

  ToolpathMove previous;
  bool havePrevious = false;
  for (const auto& move : context.toolpath.moves) {
    const double x = move.x - originX;
    const double y = move.y - originY;
    const double z = move.z - originZ;
    const bool zChanged = !haveZ || z != currentZ;
    const bool feedChanged = move.feedrate_mm_per_min > 0.0 &&
                             move.feedrate_mm_per_min != lastFeed;
    const auto moveVars = [&]() {
      auto vars = std::map<std::string, std::string>{
          {"x", fmt_number(x, decimals)},
          {"y", fmt_number(y, decimals)},
          {"z", fmt_number(z, decimals)},
      };
      if (feedChanged) {
        vars["feed"] = fmt_number(move.feedrate_mm_per_min, decimals);
      }
      return vars;
    };

    if (move.kind == ToolpathMoveKind::Rapid) {
      auto vars = moveVars();
      std::string line = render_template(def.rapid, vars);
      if (zChanged) {
        line += " Z" + fmt_number(z, decimals);
        currentZ = z;
        haveZ = true;
      }
      emit(line);
    } else if (move.kind == ToolpathMoveKind::FeedArcCW ||
               move.kind == ToolpathMoveKind::FeedArcCCW) {
      const double radius = std::hypot(move.i, move.j);
      if (def.use_arcs && radius >= 0.001) {
        ensure_power_state(move);
        auto vars = moveVars();
        vars["i"] = fmt_number(move.i, decimals);
        vars["j"] = fmt_number(move.j, decimals);
        std::string line = render_template(
            move.kind == ToolpathMoveKind::FeedArcCW ? def.arc_cw
                                                     : def.arc_ccw,
            vars);
        if (feedChanged) {
          line += " F" + fmt_number(move.feedrate_mm_per_min, decimals);
          lastFeed = move.feedrate_mm_per_min;
        }
        if (zChanged) {
          line += " Z" + fmt_number(z, decimals);
          currentZ = z;
          haveZ = true;
        }
        emit(line);
      } else {
        ensure_power_state(move);
        std::vector<std::array<double, 3>> chords;
        linearize_arc_move(previous, move, /*chord_tolerance_mm=*/0.01,
                           chords);
        for (const auto& point : chords) {
          auto vars = std::map<std::string, std::string>{
              {"x", fmt_number(point[0] - originX, decimals)},
              {"y", fmt_number(point[1] - originY, decimals)},
              {"z", fmt_number(z, decimals)},
          };
          std::string line = render_template(def.feed, vars);
          if (feedChanged) {
            line += " F" + fmt_number(move.feedrate_mm_per_min, decimals);
            lastFeed = move.feedrate_mm_per_min;
          }
          emit(line);
        }
      }
    } else {
      ensure_power_state(move);
      std::string line = render_template(def.feed, moveVars());
      if (feedChanged) {
        line += " F" + fmt_number(move.feedrate_mm_per_min, decimals);
        lastFeed = move.feedrate_mm_per_min;
      }
      if (zChanged) {
        line += " Z" + fmt_number(z, decimals);
        currentZ = z;
        haveZ = true;
      }
      emit(line);
      if (move.dwell_seconds > 0.0) {
        emit(render_template(def.dwell,
                             {{"seconds", fmt_number(move.dwell_seconds, decimals)}}));
      }
    }

    previous = move;
    havePrevious = true;
  }

  if (laserOn || spindleOn) {
    emit(render_template(def.spindle_off, {}));
  }
  for (const auto& templ : def.footer_lines) {
    emit(render_template(templ, common_vars(context)));
  }

  return lines;
}

}  // namespace

// ── Definition parsing ────────────────────────────────────────────

bool parse_post_definition(const std::string& json_text,
                           PostDefinition& definition, std::string& error) {
  try {
    const json payload = json::parse(json_text);
    if (!payload.is_object()) {
      error = "post definition must be a JSON object";
      return false;
    }
    definition.units_mm = read_optional_template(payload, "units_mm", definition.units_mm);
    definition.units_inch = read_optional_template(payload, "units_inch", definition.units_inch);
    definition.header_lines = read_optional_lines(payload, "header_lines", definition.header_lines);
    definition.rapid = read_optional_template(payload, "rapid", definition.rapid);
    definition.feed = read_optional_template(payload, "feed", definition.feed);
    definition.arc_cw = read_optional_template(payload, "arc_cw", definition.arc_cw);
    definition.arc_ccw = read_optional_template(payload, "arc_ccw", definition.arc_ccw);
    definition.dwell = read_optional_template(payload, "dwell", definition.dwell);
    definition.laser_on_dynamic = read_optional_template(payload, "laser_on_dynamic", definition.laser_on_dynamic);
    definition.laser_on_constant = read_optional_template(payload, "laser_on_constant", definition.laser_on_constant);
    definition.laser_off = read_optional_template(payload, "laser_off", definition.laser_off);
    definition.spindle_on = read_optional_template(payload, "spindle_on", definition.spindle_on);
    definition.spindle_off = read_optional_template(payload, "spindle_off", definition.spindle_off);
    definition.footer_lines = read_optional_lines(payload, "footer_lines", definition.footer_lines);
    if (payload.contains("power_max") && payload.at("power_max").is_number()) {
      definition.power_max = payload.at("power_max").get<double>();
    }
    if (payload.contains("line_numbers") && payload.at("line_numbers").is_boolean()) {
      definition.line_numbers = payload.at("line_numbers").get<bool>();
    }
    if (payload.contains("use_arcs") && payload.at("use_arcs").is_boolean()) {
      definition.use_arcs = payload.at("use_arcs").get<bool>();
    }
    if (payload.contains("decimal_places") && payload.at("decimal_places").is_number_integer()) {
      definition.decimal_places = payload.at("decimal_places").get<int>();
    }
    return true;
  } catch (const std::exception& exception) {
    error = std::string("invalid post definition JSON: ") + exception.what();
    return false;
  }
}

std::vector<std::pair<std::string, std::string>> builtin_post_definitions() {
  return {
      {"grbl", kGrblDefinition},
      {"linuxcnc", kLinuxcncDefinition},
      {"mach3", kMach3Definition},
      {"mach4", kMach4Definition},
      {"marlin", kMarlinDefinition},
      {"fanuc", kFanucDefinition},
  };
}

// ── Engine entry points ───────────────────────────────────────────

bool load_post_definition(const std::string& name, PostDefinition& definition,
                          std::string& error) {
  // User file wins over the built-in — the whole point of the posts
  // directory is that edits/imports apply.
  const auto dir = posts_directory();
  if (!dir.empty()) {
    const auto file = dir / (name + ".json");
    std::error_code ec;
    if (std::filesystem::exists(file, ec)) {
      std::ifstream stream(file);
      std::stringstream buffer;
      buffer << stream.rdbuf();
      return parse_post_definition(buffer.str(), definition, error);
    }
  }
  for (const auto& [builtin, text] : builtin_post_definitions()) {
    if (builtin == name) {
      return parse_post_definition(text, definition, error);
    }
  }
  error = "unknown post processor '" + name + "'";
  return false;
}

std::vector<std::string> post_process(const std::string& type,
                                      const PostContext& context) {
  seed_builtin_posts();
  PostDefinition definition;
  std::string error;
  if (!load_post_definition(type, definition, error)) {
    return {"(error: " + error + ")"};
  }
  PostContext effective = context;
  effective.definition = definition;
  return render_post(effective);
}

std::vector<PostListEntry> list_post_processors() {
  seed_builtin_posts();
  std::vector<PostListEntry> entries;
  for (const auto& [name, text] : builtin_post_definitions()) {
    (void)text;
    entries.push_back({name, ""});
  }
  const auto dir = posts_directory();
  if (!dir.empty()) {
    std::error_code ec;
    for (const auto& file : std::filesystem::directory_iterator(dir, ec)) {
      if (file.path().extension() == ".json") {
        const std::string name = file.path().stem().string();
        bool found = false;
        for (auto& entry : entries) {
          if (entry.name == name) {
            entry.path = file.path().string();
            found = true;
            break;
          }
        }
        if (!found) {
          entries.push_back({name, file.path().string()});
        }
      }
    }
  }
  return entries;
}

std::string import_post_processor(const std::string& source_path,
                                  std::string& error) {
  seed_builtin_posts();
  const auto dir = posts_directory();
  if (dir.empty()) {
    error = "the posts directory is not configured";
    return "";
  }
  // Validate BEFORE copying — a broken definition must not shadow a
  // built-in.
  {
    std::ifstream stream(source_path);
    std::stringstream buffer;
    buffer << stream.rdbuf();
    PostDefinition probe;
    if (!parse_post_definition(buffer.str(), probe, error)) {
      return "";
    }
  }
  const std::string name =
      std::filesystem::path(source_path).stem().string();
  const auto target = dir / (name + ".json");
  std::error_code ec;
  std::filesystem::copy_file(source_path, target,
                             std::filesystem::copy_options::overwrite_existing,
                             ec);
  if (ec) {
    error = "failed to import post processor: " + ec.message();
    return "";
  }
  return name;
}

}  // namespace polysmith::core
