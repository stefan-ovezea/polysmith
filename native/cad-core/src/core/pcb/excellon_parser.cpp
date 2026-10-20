#include "core/pcb/excellon_parser.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <optional>

namespace polysmith::core::pcb {
namespace {

// Untrusted-input bounds.  The fixture board has 511 holes; these caps only
// ever trip on malformed or hostile files.
constexpr size_t kMaxHits = 2'000'000;
constexpr size_t kMaxTools = 4096;
constexpr int kMaxRepeat = 100'000;

std::string trim(const std::string& text) {
  size_t begin = 0;
  size_t end = text.size();
  while (begin < end && std::isspace(static_cast<unsigned char>(text[begin]))) {
    ++begin;
  }
  while (end > begin && std::isspace(static_cast<unsigned char>(text[end - 1]))) {
    --end;
  }
  return text.substr(begin, end - begin);
}

bool starts_with(const std::string& text, const char* prefix) {
  const size_t length = std::char_traits<char>::length(prefix);
  return text.size() >= length && text.compare(0, length, prefix) == 0;
}

std::optional<double> parse_double(const std::string& raw) {
  const std::string text = trim(raw);
  if (text.empty()) {
    return std::nullopt;
  }
  const char* begin = text.c_str();
  char* end = nullptr;
  const double value = std::strtod(begin, &end);
  if (end == begin || *end != '\0') {
    return std::nullopt;
  }
  return value;
}

/// How a file writes coordinates that carry no decimal point.  KiCad's
/// header comment states it (`; FORMAT={3:3/ absolute / metric / leading}`),
/// which is the only reliable source — Excellon itself leaves it ambiguous.
struct FormatHint {
  int decimals = 0;          // fractional digits when the point is implicit
  bool decimal = false;      // coordinates always carry an explicit point
  bool known = false;
};

/// Parses KiCad's `; FORMAT={-:-/ absolute / metric / decimal}` comment.
FormatHint parse_format_comment(const std::string& comment) {
  FormatHint hint;
  const std::string::size_type open = comment.find('{');
  const std::string::size_type close = comment.find('}', open == std::string::npos ? 0 : open);
  if (open == std::string::npos || close == std::string::npos) {
    return hint;
  }
  const std::string body = comment.substr(open + 1, close - open - 1);
  const std::string::size_type slash = body.find('/');
  const std::string digits = slash == std::string::npos ? body : body.substr(0, slash);
  const std::string::size_type colon = digits.find(':');
  if (colon != std::string::npos) {
    const std::optional<double> decimals = parse_double(digits.substr(colon + 1));
    if (decimals && *decimals >= 0.0 && *decimals <= 9.0) {
      hint.decimals = static_cast<int>(*decimals);
      hint.known = true;
    }
  }
  const std::string lower = [&comment] {
    std::string copy = comment;
    std::transform(copy.begin(), copy.end(), copy.begin(), [](unsigned char c) {
      return static_cast<char>(std::tolower(c));
    });
    return copy;
  }();
  if (lower.find("decimal") != std::string::npos) {
    hint.decimal = true;
    hint.known = true;
  }
  return hint;
}

/// Harvests the KiCad-style `#@! TA.AperFunction,…` / `TF.FileFunction,…`
/// value out of a comment body.
std::string attribute_value(const std::string& comment, const char* prefix) {
  const std::string::size_type marker = comment.find(prefix);
  if (marker == std::string::npos) {
    return std::string();
  }
  std::string value = trim(comment.substr(marker + std::char_traits<char>::length(prefix)));
  // Commas inside the value are meaningful ("Plated,PTH,ViaDrill").
  return value;
}

class ExcellonParser {
 public:
  explicit ExcellonParser(const std::string& text) : text_(text) {}

  DrillFile run() {
    const std::vector<std::string> lines = split_lines();
    if (lines.empty()) {
      file_.ok = false;
      file_.diagnostics.push_back({0, "empty drill file"});
      return file_;
    }
    bool saw_program = false;
    for (size_t index = 0; index < lines.size(); ++index) {
      const int line = static_cast<int>(index) + 1;
      const std::string& raw = lines[index];
      if (raw.empty()) {
        continue;
      }
      if (handle(raw, line)) {
        saw_program = true;
      }
      if (ended_) {
        break;  // M30
      }
      if (file_.hits.size() >= kMaxHits) {
        file_.diagnostics.push_back({line, "hit limit reached — rest of file ignored"});
        break;
      }
    }
    if (!saw_program) {
      file_.ok = false;
      file_.diagnostics.push_back({0, "no drill program found"});
    }
    return file_;
  }

 private:
  std::vector<std::string> split_lines() const {
    std::vector<std::string> lines;
    std::string current;
    for (const char c : text_) {
      if (c == '\n') {
        lines.push_back(trim(current));
        current.clear();
      } else if (c != '\r') {
        current.push_back(c);
      }
    }
    lines.push_back(trim(current));
    while (!lines.empty() && lines.back().empty()) {
      lines.pop_back();
    }
    return lines;
  }

  void note_unsupported(const std::string& what, int line) {
    if (std::find(file_.unsupported.begin(), file_.unsupported.end(), what) ==
        file_.unsupported.end()) {
      file_.unsupported.push_back(what);
    }
    file_.diagnostics.push_back({line, "unsupported construct skipped: " + what});
  }

  /// Returns true when the line carried drill-program content.
  bool handle(const std::string& line, int line_number) {
    if (line[0] == ';') {
      handle_comment(line, line_number);
      return false;
    }
    if (starts_with(line, "M48")) {
      return true;  // header start
    }
    if (starts_with(line, "FMAT")) {
      return true;
    }
    if (starts_with(line, "METRIC")) {
      file_.unit = PcbUnit::Millimeter;
      return true;
    }
    if (starts_with(line, "INCH")) {
      file_.unit = PcbUnit::Inch;
      return true;
    }
    if (line == "%" || starts_with(line, "M95")) {
      return true;  // header end / rewind
    }
    if (starts_with(line, "M71")) {
      file_.unit = PcbUnit::Millimeter;
      return true;
    }
    if (starts_with(line, "M72")) {
      file_.unit = PcbUnit::Inch;
      return true;
    }
    if (starts_with(line, "M30")) {
      ended_ = true;
      return true;
    }
    if (starts_with(line, "G90")) {
      incremental_ = false;
      return true;
    }
    if (starts_with(line, "G91")) {
      incremental_ = true;
      return true;
    }
    if (starts_with(line, "G05")) {
      return true;  // drill mode: the only mode we implement, nothing to do
    }
    if (starts_with(line, "G85") || line.find("G85") != std::string::npos) {
      note_unsupported("G85 (slot)", line_number);
      return true;
    }
    if (starts_with(line, "M15") || starts_with(line, "M16") || starts_with(line, "G00") ||
        starts_with(line, "G01")) {
      note_unsupported("route mode", line_number);
      return true;
    }
    if (line[0] == 'T') {
      return handle_tool(line, line_number);
    }
    if (line[0] == 'R') {
      handle_repeat(line, line_number);
      return true;
    }
    if (line[0] == 'X' || line[0] == 'Y') {
      handle_coordinates(line, line_number);
      return true;
    }
    if (line[0] == 'I' || line[0] == 'J') {
      // Slot/route offsets — already reported through their construct.
      return true;
    }
    if (line[0] == 'S' && line.size() > 1) {
      // Spindle speed (some exporters write it per tool change).
      return true;
    }
    file_.diagnostics.push_back({line_number, "unrecognised drill line: " + line});
    return false;
  }

  void handle_comment(const std::string& line, int line_number) {
    (void)line_number;
    const std::string function = attribute_value(line, "TF.FileFunction,");
    if (!function.empty()) {
      file_.file_function = function;
      const std::string lower = to_lower(function);
      if (lower.find("nonplated") != std::string::npos) {
        file_.plated = false;
        file_.plated_known = true;
      } else if (lower.find("plated") != std::string::npos) {
        file_.plated = true;
        file_.plated_known = true;
      }
      return;
    }
    const std::string aperture = attribute_value(line, "TA.AperFunction,");
    if (!aperture.empty()) {
      pending_tool_function_ = aperture;
      return;
    }
    if (line.find("FORMAT=") != std::string::npos) {
      format_hint_ = parse_format_comment(line);
    }
  }

  bool handle_tool(const std::string& line, int line_number) {
    const std::string::size_type c = line.find('C');
    std::string digits = c == std::string::npos ? line.substr(1) : line.substr(1, c - 1);
    const std::optional<double> number = parse_double(digits);
    if (!number) {
      file_.diagnostics.push_back({line_number, "unreadable tool number: " + line});
      return false;
    }
    const int tool_number = static_cast<int>(*number);
    if (c == std::string::npos) {
      // Bare `Tn` — a tool selection.
      if (tool_number == 0) {
        current_tool_ = 0;
      } else {
        if (file_.tools.find(tool_number) == file_.tools.end()) {
          file_.diagnostics.push_back(
              {line_number, "T" + std::to_string(tool_number) +
                                " selected but never defined"});
        }
        current_tool_ = tool_number;
      }
      return true;
    }
    if (file_.tools.size() >= kMaxTools) {
      return true;
    }
    std::string diameter_text = line.substr(c + 1);
    // Feed/speed suffixes (F…S…) belong to the tool, not its diameter.
    for (const char stop : {'F', 'S'}) {
      const std::string::size_type position = diameter_text.find(stop);
      if (position != std::string::npos) {
        diameter_text = diameter_text.substr(0, position);
      }
    }
    const std::optional<double> diameter = parse_double(diameter_text);
    if (!diameter || *diameter <= 0.0) {
      file_.diagnostics.push_back({line_number,
                                   "tool T" + std::to_string(tool_number) +
                                       " has an unreadable diameter: " + line});
      return false;
    }
    DrillTool tool;
    tool.number = tool_number;
    tool.diameter_mm = *diameter * unit_scale();
    tool.function = pending_tool_function_;
    tool.line = line_number;
    pending_tool_function_.clear();
    file_.tools[tool_number] = tool;
    return true;
  }

  double unit_scale() const {
    return file_.unit == PcbUnit::Inch ? 25.4 : 1.0;
  }

  /// One coordinate word, possibly several `X…Y…` pairs plus an optional
  /// trailing repeat count.
  void handle_coordinates(const std::string& line, int line_number) {
    double x = last_x_;
    double y = last_y_;
    bool has_x = false;
    bool has_y = false;
    size_t position = 0;
    while (position < line.size()) {
      const char letter = static_cast<char>(std::toupper(static_cast<unsigned char>(line[position])));
      if (letter != 'X' && letter != 'Y') {
        ++position;
        continue;
      }
      ++position;
      const size_t begin = position;
      while (position < line.size() &&
             (std::isdigit(static_cast<unsigned char>(line[position])) ||
              line[position] == '+' || line[position] == '-' || line[position] == '.')) {
        ++position;
      }
      const std::string raw = line.substr(begin, position - begin);
      const std::optional<double> value = coordinate_to_mm(raw, line_number);
      if (!value) {
        continue;
      }
      if (letter == 'X') {
        x = incremental_ ? last_x_ + *value : *value;
        has_x = true;
      } else {
        y = incremental_ ? last_y_ + *value : *value;
        has_y = true;
      }
    }
    if (!has_x && !has_y) {
      file_.diagnostics.push_back({line_number, "coordinate line without values: " + line});
      return;
    }
    last_x_ = x;
    last_y_ = y;

    DrillHit hit;
    hit.x = x;
    hit.y = y;
    hit.tool = current_tool_;
    hit.line = line_number;
    if (current_tool_ == 0 || file_.tools.find(current_tool_) == file_.tools.end()) {
      ++file_.hits_without_tool;
    }
    track_bounds(hit.x, hit.y);
    file_.hits.push_back(hit);
  }

  /// `R<n>[X<step>][Y<step>]` — repeats the **previous** hole `n` more times,
  /// adding an incremental (X, Y) step each time.  Semantics follow KiCad's
  /// own regression test (`qa/tests/gerbview/test_excellon_repeat.cpp`):
  /// `R07X5000` after a hole yields 8 holes spaced 5 mm apart.  Getting this
  /// wrong drills real holes in the wrong place, so the stepping form is
  /// implemented rather than treated as "repeat in situ".
  void handle_repeat(const std::string& line, int line_number) {
    size_t position = 1;
    const size_t count_begin = position;
    while (position < line.size() &&
           (std::isdigit(static_cast<unsigned char>(line[position])) ||
            line[position] == '+' || line[position] == '-' || line[position] == '.')) {
      ++position;
    }
    const std::optional<double> count =
        parse_double(line.substr(count_begin, position - count_begin));
    if (!count || *count < 0.0) {
      file_.diagnostics.push_back({line_number, "unreadable repeat count: " + line});
      return;
    }
    double step_x = 0.0;
    double step_y = 0.0;
    while (position < line.size()) {
      const char letter =
          static_cast<char>(std::toupper(static_cast<unsigned char>(line[position])));
      if (letter != 'X' && letter != 'Y') {
        ++position;
        continue;
      }
      ++position;
      const size_t value_begin = position;
      while (position < line.size() &&
             (std::isdigit(static_cast<unsigned char>(line[position])) ||
              line[position] == '+' || line[position] == '-' || line[position] == '.')) {
        ++position;
      }
      const std::optional<double> value =
          coordinate_to_mm(line.substr(value_begin, position - value_begin), line_number);
      if (!value) {
        continue;
      }
      // Repeat steps are always incremental, whatever G90/G91 said.
      if (letter == 'X') {
        step_x = *value;
      } else {
        step_y = *value;
      }
    }
    if (file_.hits.empty()) {
      file_.diagnostics.push_back({line_number, "repeat with no previous hole: " + line});
      return;
    }
    const int extra = std::min(kMaxRepeat, static_cast<int>(*count));
    for (int copy = 1; copy <= extra; ++copy) {
      DrillHit hit;
      hit.x = last_x_ + copy * step_x;
      hit.y = last_y_ + copy * step_y;
      hit.tool = current_tool_;
      hit.line = line_number;
      if (current_tool_ == 0 || file_.tools.find(current_tool_) == file_.tools.end()) {
        ++file_.hits_without_tool;
      }
      track_bounds(hit.x, hit.y);
      file_.hits.push_back(hit);
      if (file_.hits.size() >= kMaxHits) {
        return;
      }
    }
    // The machine now sits on the last repeated hole.
    last_x_ += extra * step_x;
    last_y_ += extra * step_y;
  }

  std::optional<double> coordinate_to_mm(const std::string& raw, int line_number) {
    if (raw.empty()) {
      return std::nullopt;
    }
    const bool explicit_point = raw.find('.') != std::string::npos;
    const std::optional<double> value = parse_double(raw);
    if (!value) {
      file_.diagnostics.push_back({line_number, "unreadable coordinate: " + raw});
      return std::nullopt;
    }
    if (explicit_point) {
      seen_decimal_point_ = true;
    }
    double millimetres = 0.0;
    // A file that has written one explicit decimal point is a decimal file:
    // a whole-millimetre hole appears as `X10`, not as a fixed-point field.
    if (explicit_point || format_hint_.decimal || seen_decimal_point_) {
      millimetres = *value * unit_scale();
    } else {
      // Fixed-point integer coordinates: the fractional digit count comes
      // from the header's FORMAT comment when present, otherwise from the
      // conventional unit resolution (micrometres / 0.1 mil).
      if (!format_hint_.known && !warned_implicit_format_) {
        warned_implicit_format_ = true;
        file_.diagnostics.push_back(
            {line_number, "coordinates carry no decimal point and the header "
                          "gives no FORMAT hint — assuming metric 3 / inch 4 "
                          "fractional digits"});
      }
      const int decimals =
          format_hint_.known ? format_hint_.decimals
                             : (file_.unit == PcbUnit::Inch ? 4 : 3);
      millimetres = *value / std::pow(10.0, decimals) * unit_scale();
    }
    return millimetres;
  }

  void track_bounds(double x, double y) {
    if (!file_.bounds.valid) {
      file_.bounds = PcbBounds{x, y, x, y, true};
      return;
    }
    file_.bounds.min_x = std::min(file_.bounds.min_x, x);
    file_.bounds.min_y = std::min(file_.bounds.min_y, y);
    file_.bounds.max_x = std::max(file_.bounds.max_x, x);
    file_.bounds.max_y = std::max(file_.bounds.max_y, y);
  }

  static std::string to_lower(const std::string& text) {
    std::string copy = text;
    std::transform(copy.begin(), copy.end(), copy.begin(), [](unsigned char c) {
      return static_cast<char>(std::tolower(c));
    });
    return copy;
  }

  const std::string& text_;
  DrillFile file_;
  FormatHint format_hint_;
  std::string pending_tool_function_;
  bool incremental_ = false;
  bool ended_ = false;
  bool warned_implicit_format_ = false;
  bool seen_decimal_point_ = false;
  int current_tool_ = 0;
  double last_x_ = 0.0;
  double last_y_ = 0.0;
};

}  // namespace

DrillFile parse_excellon(const std::string& text) {
  ExcellonParser parser(text);
  return parser.run();
}

}  // namespace polysmith::core::pcb
