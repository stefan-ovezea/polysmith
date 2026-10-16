#include "core/pcb/pcb_layer_io.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iterator>

#include "core/pcb/pcb_role.h"

namespace polysmith::core::pcb {
namespace {

std::string lower(const std::string& text) {
  std::string copy = text;
  std::transform(copy.begin(), copy.end(), copy.begin(), [](unsigned char c) {
    return static_cast<char>(std::tolower(c));
  });
  return copy;
}

bool has(const std::string& haystack, const char* needle) {
  return haystack.find(needle) != std::string::npos;
}

/// Short "line N: message" form for the feature summary — the full
/// diagnostics stay available through the loader for tests and the log.
std::string render(const PcbDiagnostic& diagnostic) {
  if (diagnostic.line <= 0) {
    return diagnostic.message;
  }
  return "line " + std::to_string(diagnostic.line) + ": " + diagnostic.message;
}

/// Caps the warnings a feature carries, so one pathological file cannot grow
/// the saved document without bound.  The count is reported in the summary.
std::vector<std::string> collect_warnings(const std::vector<PcbDiagnostic>& diagnostics,
                                          const std::vector<std::string>& unsupported,
                                          int* dropped) {
  constexpr size_t kMaxWarnings = 32;
  std::vector<std::string> warnings;
  for (const PcbDiagnostic& diagnostic : diagnostics) {
    if (warnings.size() >= kMaxWarnings) {
      ++(*dropped);
      continue;
    }
    warnings.push_back(render(diagnostic));
  }
  for (const std::string& entry : unsupported) {
    if (warnings.size() >= kMaxWarnings) {
      ++(*dropped);
      continue;
    }
    warnings.push_back("unsupported: " + entry);
  }
  return warnings;
}

}  // namespace

PcbLayerFormat detect_pcb_layer_format(const std::string& text, const std::string& file_name) {
  // Drill programs announce themselves: `M48` header, or a bare body starting
  // with a unit/tool line.  Gerber always has `%`-delimited extended commands.
  if (has(text, "M48") || has(text, "M95")) {
    return PcbLayerFormat::Excellon;
  }
  const std::string name = lower(file_name);
  const bool drill_extension = name.size() > 4 && name.compare(name.size() - 4, 4, ".drl") == 0;
  if (drill_extension && !has(text, "%FS")) {
    return PcbLayerFormat::Excellon;
  }
  return PcbLayerFormat::Gerber;
}

PcbLayerLoad parse_pcb_layer_text(const std::string& text, const std::string& file_name,
                                  const std::string& file_path, PcbRole role_override) {
  PcbLayerLoad load;
  load.format = detect_pcb_layer_format(text, file_name);
  int dropped_warnings = 0;

  if (load.format == PcbLayerFormat::Excellon) {
    load.drills = parse_excellon(text);
    if (!load.drills.ok) {
      load.error = "not a readable Excellon drill program";
      return load;
    }
    load.file_function = load.drills.file_function;
    load.hit_count = static_cast<int>(load.drills.hits.size());
    load.tool_count = static_cast<int>(load.drills.tools.size());
    load.bounds = load.drills.bounds;
    load.warnings = collect_warnings(load.drills.diagnostics, load.drills.unsupported,
                                     &dropped_warnings);
    if (load.drills.hits_without_tool > 0) {
      load.warnings.push_back(std::to_string(load.drills.hits_without_tool) +
                             " hole(s) use a tool that was never defined");
    }
  } else {
    const GerberFile file = parse_gerber(text);
    if (!file.ok) {
      load.error = "not a readable Gerber file";
      return load;
    }
    load.geometry = polygonize_gerber(file);
    load.file_function = file.file_function;
    load.file_polarity = file.file_polarity;
    load.ring_count = static_cast<int>(load.geometry.rings.size());
    load.bounds = load.geometry.bounds;
    load.warnings = collect_warnings(load.geometry.diagnostics, file.unsupported,
                                     &dropped_warnings);
    if (load.geometry.skipped_operations > 0) {
      load.warnings.push_back(std::to_string(load.geometry.skipped_operations) +
                             " operation(s) produced no geometry");
    }
  }

  if (dropped_warnings > 0) {
    load.warnings.push_back(std::to_string(dropped_warnings) + " further warning(s) suppressed");
  }

  // Role: the user's explicit choice wins, then the file's own attributes, then
  // the filename.
  PcbRoleResolution resolution;
  if (role_override != PcbRole::Unknown) {
    resolution.role = role_override;
    resolution.note = "chosen by the user";
  } else {
    resolution = resolve_pcb_role(load.file_function, file_name);
  }
  load.role = resolution.role;
  load.role_from_attributes = resolution.from_attributes;
  load.role_note = resolution.note;
  if (load.file_polarity.empty() && !pcb_role_is_drill(load.role)) {
    // Mask files describe openings, so their polarity is the opposite of what
    // "dark art" intuition suggests; say so when the file does not.
    load.file_polarity = (load.role == PcbRole::SolderMaskTop ||
                          load.role == PcbRole::SolderMaskBottom)
                             ? "Negative (assumed: mask describes openings)"
                             : "Positive (assumed)";
  }
  (void)file_path;
  load.ok = true;
  return load;
}

PcbLayerLoad load_pcb_layer(const std::string& file_path, PcbRole role_override) {
  PcbLayerLoad load;
  std::error_code error;
  if (!std::filesystem::exists(file_path, error)) {
    load.error = "file not found: " + file_path;
    return load;
  }
  std::ifstream stream(file_path, std::ios::binary);
  if (!stream) {
    load.error = "cannot read: " + file_path;
    return load;
  }
  const std::string text((std::istreambuf_iterator<char>(stream)),
                         std::istreambuf_iterator<char>());
  const std::filesystem::path path(file_path);
  load = parse_pcb_layer_text(text, path.filename().string(), file_path, role_override);
  if (!load.ok && load.error.empty()) {
    load.error = "unreadable layer: " + file_path;
  }
  return load;
}

}  // namespace polysmith::core::pcb
