#include "core/cam/tool_table_io.h"

#include <algorithm>
#include <cctype>
#include <iomanip>
#include <sstream>

#include <nlohmann/json.hpp>

#include "core/cam/tool_library.h"

namespace polysmith::core {

namespace {

using json = nlohmann::json;

// ── LinuxCNC tool table ────────────────────────────────────────────
//
// Grammar (LinuxCNC docs): one tool per line; fields are
// order-independent KEY+value pairs ("the only format requirement is
// at least one space or tab after each entry"); text after ';' is a
// comment; lines beginning with ';' are ignored; values may carry an
// explicit '+'.  T/P/Q are unsigned integers, the rest are floats.
// A conventional file begins with a line containing only ';'.

constexpr char kFieldLetters[] = "TPXYZABCUVWDIJQ";

bool is_field_letter(char c) {
  for (const char letter : kFieldLetters) {
    if (c == letter) {
      return true;
    }
  }
  return false;
}

bool is_double_field(char c) {
  return c == 'X' || c == 'Y' || c == 'Z' || c == 'A' || c == 'B' ||
         c == 'C' || c == 'U' || c == 'V' || c == 'W' || c == 'D' ||
         c == 'I' || c == 'J';
}

struct TableLine {
  int tool_number = -1;
  int pocket_number = -1;
  std::optional<double> x, y, z, a, b, c, u, v, w, d, front, back;
  std::optional<int> q;
  std::string comment;
};

// Parses ONE line into a TableLine.  Returns false when the line is
// malformed (missing separators / junk letters) so the caller can
// warn and skip.
bool parse_table_line(const std::string& line, TableLine& result) {
  size_t i = 0;
  const size_t n = line.size();
  // Skip leading whitespace.
  while (i < n && (line[i] == ' ' || line[i] == '\t')) {
    ++i;
  }
  bool saw_field = false;
  while (i < n) {
    const char c = line[i];
    if (c == ';') {
      result.comment = line.substr(i + 1);
      // Trim trailing whitespace/newline.
      while (!result.comment.empty() &&
             (result.comment.back() == ' ' || result.comment.back() == '\t' ||
              result.comment.back() == '\r' || result.comment.back() == '\n')) {
        result.comment.pop_back();
      }
      return true;
    }
    if (c == ' ' || c == '\t') {
      ++i;
      continue;
    }
    if (!is_field_letter(c)) {
      return false;  // junk before ';'
    }
    const char key = c;
    ++i;
    const size_t value_start = i;
    while (i < n && line[i] != ' ' && line[i] != '\t' && line[i] != ';') {
      ++i;
    }
    const std::string raw = line.substr(value_start, i - value_start);
    // "at least one space or tab after each entry" — the next
    // character must be a separator or the comment marker.
    if (i < n && line[i] != ' ' && line[i] != '\t' && line[i] != ';') {
      return false;
    }
    // The value is required; ';' directly after the letter is junk.
    if (raw.empty()) {
      return false;
    }
    std::string value = raw;
    if (!value.empty() && value.front() == '+') {
      value.erase(value.begin());
    }
    try {
      size_t used = 0;
      if (key == 'T' || key == 'P' || key == 'Q') {
        const long parsed = std::stol(value, &used);
        if (used != value.size()) {
          return false;
        }
        if (key == 'T') result.tool_number = static_cast<int>(parsed);
        if (key == 'P') result.pocket_number = static_cast<int>(parsed);
        if (key == 'Q') result.q = static_cast<int>(parsed);
      } else if (is_double_field(key)) {
        const double parsed = std::stod(value, &used);
        if (used != value.size()) {
          return false;
        }
        switch (key) {
          case 'X': result.x = parsed; break;
          case 'Y': result.y = parsed; break;
          case 'Z': result.z = parsed; break;
          case 'A': result.a = parsed; break;
          case 'B': result.b = parsed; break;
          case 'C': result.c = parsed; break;
          case 'U': result.u = parsed; break;
          case 'V': result.v = parsed; break;
          case 'W': result.w = parsed; break;
          case 'D': result.d = parsed; break;
          case 'I': result.front = parsed; break;
          case 'J': result.back = parsed; break;
          default: return false;
        }
      }
    } catch (const std::exception&) {
      return false;
    }
    saw_field = true;
  }
  return saw_field;
}

std::string format_double(double value) {
  std::ostringstream out;
  out << std::fixed << std::setprecision(6) << value;
  return out.str();
}

void trim(std::string& text) {
  size_t begin = 0;
  while (begin < text.size() && std::isspace(
                                    static_cast<unsigned char>(text[begin]))) {
    ++begin;
  }
  size_t end = text.size();
  while (end > begin && std::isspace(static_cast<unsigned char>(text[end - 1]))) {
    --end;
  }
  text = text.substr(begin, end - begin);
}

// Comment → name/description split used by BOTH directions so the
// round-trip is idempotent: export writes "name — description",
// import splits on the first " — ".
constexpr char kCommentSeparator[] = " — ";

std::string comment_for_tool(const ToolEntry& tool) {
  if (tool.description.empty()) {
    return tool.name;
  }
  return tool.name + kCommentSeparator + tool.description;
}

void apply_comment(ToolEntry& tool, const std::string& comment) {
  const size_t split = comment.find(kCommentSeparator);
  if (split == std::string::npos) {
    tool.name = comment.size() > 60 ? comment.substr(0, 60) : comment;
    tool.description.clear();
    return;
  }
  // Skip the separator's actual byte length (UTF-8 em-dash = 3 bytes).
  const std::string separator(kCommentSeparator);
  tool.name = comment.substr(0, split);
  tool.description = comment.substr(split + separator.size());
}

}  // namespace

// ── LinuxCNC .tbl ─────────────────────────────────────────────────

void parse_linuxcnc_tool_table(const std::string& text,
                               std::vector<ToolEntry>& tools,
                               std::vector<std::string>& warnings,
                               std::string& error) {
  std::istringstream stream(text);
  std::string line;
  int line_number = 0;
  while (std::getline(stream, line)) {
    ++line_number;
    trim(line);
    if (line.empty() || line.front() == ';') {
      continue;  // comment lines are preserved on export, ignored here
    }
    TableLine parsed;
    if (!parse_table_line(line, parsed)) {
      warnings.push_back("line " + std::to_string(line_number) +
                         ": skipped (malformed tool line)");
      continue;
    }
    if (parsed.tool_number < 0) {
      warnings.push_back("line " + std::to_string(line_number) +
                         ": skipped (no tool number)");
      continue;
    }
    if (parsed.tool_number == 0) {
      warnings.push_back("line " + std::to_string(line_number) +
                         ": skipped (T0 is the empty spindle, never a tool)");
      continue;
    }

    ToolEntry tool;
    tool.tool_number = parsed.tool_number;
    tool.pocket_number = parsed.pocket_number > 0 ? parsed.pocket_number
                                                  : parsed.tool_number;
    if (parsed.d.has_value()) {
      tool.diameter_mm = parsed.d.value();
    }
    if (parsed.z.has_value()) {
      tool.z_offset_mm = parsed.z.value();
    }
    if (parsed.x.has_value()) {
      tool.x_offset_mm = parsed.x.value();
    }
    // Not representable in the PolySmith model — say so, drop them.
    if (parsed.y.has_value() || parsed.a.has_value() ||
        parsed.b.has_value() || parsed.c.has_value() ||
        parsed.u.has_value() || parsed.v.has_value() ||
        parsed.w.has_value()) {
      warnings.push_back("T" + std::to_string(parsed.tool_number) +
                         ": axis offsets other than X/Z are not "
                         "representable and were dropped");
    }
    if (parsed.front.has_value()) {
      tool.front_angle_deg = parsed.front.value();
    }
    if (parsed.back.has_value()) {
      tool.back_angle_deg = parsed.back.value();
    }
    if (parsed.q.has_value()) {
      tool.orientation = parsed.q.value();
    }
    if (parsed.front.has_value() || parsed.back.has_value() ||
        parsed.q.has_value()) {
      tool.type = "turning_insert";  // lathe table
    }
    if (!parsed.comment.empty()) {
      apply_comment(tool, parsed.comment);
    }
    if (tool.name.empty()) {
      tool.name = "Tool " + std::to_string(parsed.tool_number);
    }
    tools.push_back(tool);
  }
}

std::string serialize_linuxcnc_tool_table(
    const std::vector<ToolEntry>& tools) {
  // Ascending tool-number order, like tooledit expects.
  std::vector<ToolEntry> ordered = tools;
  std::sort(ordered.begin(), ordered.end(), [](const ToolEntry& a,
                                               const ToolEntry& b) {
    return a.tool_number < b.tool_number;
  });

  std::ostringstream out;
  out << ";\n";
  for (const auto& tool : ordered) {
    if (tool.tool_number <= 0) {
      continue;  // T0 must never be written
    }
    out << "T" << tool.tool_number << " ";
    out << "P" << (tool.pocket_number > 0 ? tool.pocket_number
                                          : tool.tool_number)
        << " ";
    if (tool.x_offset_mm.has_value()) {
      out << "X" << format_double(tool.x_offset_mm.value()) << " ";
    }
    if (tool.z_offset_mm.has_value()) {
      out << "Z" << format_double(tool.z_offset_mm.value()) << " ";
    }
    out << "D" << format_double(tool.diameter_mm);
    // Lathe-only fields (display data); mills stay clean.
    if (tool.type == "turning_insert") {
      out << " I" << format_double(tool.front_angle_deg);
      out << " J" << format_double(tool.back_angle_deg);
      out << " Q" << tool.orientation;
    }
    out << " ;" << comment_for_tool(tool) << "\n";
  }
  return out.str();
}

// ── PolySmith tools JSON ───────────────────────────────────────────

std::vector<ToolEntry> parse_polysmith_tools_json(
    const std::string& text, std::vector<std::string>& warnings,
    std::string& error) {
  std::vector<ToolEntry> tools;
  json payload;
  try {
    payload = json::parse(text);
  } catch (const std::exception& exception) {
    error = std::string("invalid JSON: ") + exception.what();
    return tools;
  }
  if (!payload.is_object()) {
    error = "the tools file must be a JSON object";
    return tools;
  }
  if (!payload.contains("version") || !payload.at("version").is_number()) {
    error = "the tools file has no version — is this a PolySmith tools file?";
    return tools;
  }
  const int version = payload.at("version").get<int>();
  if (version != 1) {
    error = "unsupported tools file version " + std::to_string(version);
    return tools;
  }
  if (!payload.contains("tools") || !payload.at("tools").is_array()) {
    error = "the tools file has no tools array";
    return tools;
  }
  for (const auto& entry : payload.at("tools")) {
    if (!entry.is_object()) {
      warnings.push_back("skipped a non-object tools entry");
      continue;
    }
    std::string tool_error;
    const auto tool = parse_tool_file(entry.dump(), tool_error);
    if (!tool.has_value()) {
      warnings.push_back("skipped an unreadable tool entry: " + tool_error);
      continue;
    }
    tools.push_back(tool.value());
  }
  return tools;
}

std::string serialize_polysmith_tools_json(
    const std::vector<ToolEntry>& tools) {
  json payload;
  payload["version"] = 1;
  json entries = json::array();
  for (const auto& tool : tools) {
    entries.push_back(json::parse(serialize_tool_file(tool)));
  }
  payload["tools"] = entries;
  return payload.dump(2);
}

}  // namespace polysmith::core
