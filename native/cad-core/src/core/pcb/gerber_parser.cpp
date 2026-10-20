#include "core/pcb/gerber_parser.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>

namespace polysmith::core::pcb {
namespace {

// Untrusted input guard: a Gerber file is user-supplied, so the command
// stream is bounded.  A real 4-layer board peak is ~16k operations per
// layer, so this only ever trips on malformed or hostile input.
constexpr size_t kMaxOperations = 4'000'000;
constexpr size_t kMaxStatements = 4'000'000;
constexpr size_t kMaxMacroStatements = 4096;

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

std::vector<std::string> split(const std::string& text, char separator) {
  std::vector<std::string> parts;
  std::string current;
  for (char c : text) {
    if (c == separator) {
      parts.push_back(current);
      current.clear();
    } else {
      current.push_back(c);
    }
  }
  parts.push_back(current);
  return parts;
}

bool all_digits(const std::string& text) {
  if (text.empty()) {
    return false;
  }
  return std::all_of(text.begin(), text.end(), [](unsigned char c) {
    return std::isdigit(c) != 0;
  });
}

/// Parses a plain decimal number (sign, digits, optional fraction).
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

/// A Gerber macro arithmetic expression: numbers, `$n` variables,
/// `+ - x /`, parentheses, unary minus.  `x` (or `X`) multiplies.
class ExpressionParser {
 public:
  ExpressionParser(const std::string& text, const std::vector<double>& variables)
      : text_(text), variables_(variables) {}

  std::optional<double> parse() {
    const std::optional<double> value = parse_expression();
    skip_spaces();
    if (!value || position_ != text_.size()) {
      return std::nullopt;
    }
    return value;
  }

 private:
  void skip_spaces() {
    while (position_ < text_.size() &&
           std::isspace(static_cast<unsigned char>(text_[position_]))) {
      ++position_;
    }
  }

  std::optional<double> parse_expression() {
    std::optional<double> left = parse_term();
    if (!left) {
      return std::nullopt;
    }
    for (;;) {
      skip_spaces();
      if (position_ >= text_.size()) {
        return left;
      }
      const char op = text_[position_];
      if (op != '+' && op != '-') {
        return left;
      }
      ++position_;
      const std::optional<double> right = parse_term();
      if (!right) {
        return std::nullopt;
      }
      *left = (op == '+') ? (*left + *right) : (*left - *right);
    }
  }

  std::optional<double> parse_term() {
    std::optional<double> left = parse_factor();
    if (!left) {
      return std::nullopt;
    }
    for (;;) {
      skip_spaces();
      if (position_ >= text_.size()) {
        return left;
      }
      const char op = text_[position_];
      if (op == 'x' || op == 'X' || op == '*') {
        ++position_;
        const std::optional<double> right = parse_factor();
        if (!right) {
          return std::nullopt;
        }
        *left = (*left) * (*right);
      } else if (op == '/') {
        ++position_;
        const std::optional<double> right = parse_factor();
        if (!right || *right == 0.0) {
          return std::nullopt;
        }
        *left = (*left) / (*right);
      } else {
        return left;
      }
    }
  }

  std::optional<double> parse_factor() {
    skip_spaces();
    if (position_ >= text_.size()) {
      return std::nullopt;
    }
    const char c = text_[position_];
    if (c == '+') {
      ++position_;
      return parse_factor();
    }
    if (c == '-') {
      ++position_;
      const std::optional<double> value = parse_factor();
      if (!value) {
        return std::nullopt;
      }
      return -*value;
    }
    if (c == '(') {
      ++position_;
      const std::optional<double> value = parse_expression();
      if (!value) {
        return std::nullopt;
      }
      skip_spaces();
      if (position_ >= text_.size() || text_[position_] != ')') {
        return std::nullopt;
      }
      ++position_;
      return value;
    }
    if (c == '$') {
      ++position_;
      const size_t begin = position_;
      while (position_ < text_.size() &&
             std::isdigit(static_cast<unsigned char>(text_[position_]))) {
        ++position_;
      }
      const std::string digits = text_.substr(begin, position_ - begin);
      if (digits.empty()) {
        return std::nullopt;
      }
      const size_t index = static_cast<size_t>(std::strtoul(digits.c_str(), nullptr, 10));
      if (index == 0 || index > variables_.size()) {
        return std::nullopt;
      }
      return variables_[index - 1];
    }
    if (std::isdigit(static_cast<unsigned char>(c)) || c == '.') {
      const size_t begin = position_;
      while (position_ < text_.size() &&
             (std::isdigit(static_cast<unsigned char>(text_[position_])) ||
              text_[position_] == '.')) {
        ++position_;
      }
      return parse_double(text_.substr(begin, position_ - begin));
    }
    return std::nullopt;
  }

  const std::string& text_;
  const std::vector<double>& variables_;
  size_t position_ = 0;
};

/// One `*`-terminated statement, tagged with its source line and the
/// `%…%` block it came from (macro bodies span several statements).
struct RawStatement {
  std::string text;
  int line = 0;
  bool extended = false;
  size_t block = 0;
};

std::vector<RawStatement> scan_statements(const std::string& text) {
  std::vector<RawStatement> statements;
  size_t position = 0;
  size_t block = 0;
  int line = 1;
  while (position < text.size() && statements.size() < kMaxStatements) {
    const char c = text[position];
    if (c == '\r' || c == ' ' || c == '\t') {
      ++position;
      continue;
    }
    if (c == '\n') {
      ++line;
      ++position;
      continue;
    }
    if (c == '%') {
      const int block_line = line;
      ++position;
      std::string body;
      while (position < text.size() && text[position] != '%') {
        if (text[position] == '\n') {
          ++line;
        }
        body.push_back(text[position]);
        ++position;
      }
      if (position < text.size()) {
        ++position;  // closing '%'
      }
      ++block;
      for (const std::string& part : split(body, '*')) {
        const std::string trimmed = trim(part);
        if (!trimmed.empty()) {
          statements.push_back(RawStatement{trimmed, block_line, true, block});
        }
      }
      continue;
    }
    std::string word;
    while (position < text.size() && text[position] != '*' && text[position] != '\n') {
      word.push_back(text[position]);
      ++position;
    }
    if (position < text.size() && text[position] == '*') {
      ++position;
    }
    const std::string trimmed = trim(word);
    if (!trimmed.empty()) {
      statements.push_back(RawStatement{trimmed, line, false, 0});
    }
  }
  return statements;
}

/// Coordinates are written as fixed-point digit strings whose shape comes
/// from `%FS`: leading zeros omitted (most tools), trailing zeros omitted,
/// or explicit width.  Returns millimetres.
std::optional<double> coordinate_to_mm(const std::string& raw,
                                       int integer_digits,
                                       int decimal_digits,
                                       ZeroSuppression suppression,
                                       PcbUnit unit) {
  std::string digits = trim(raw);
  if (digits.empty()) {
    return std::nullopt;
  }
  double sign = 1.0;
  if (digits[0] == '+') {
    digits.erase(0, 1);
  } else if (digits[0] == '-') {
    sign = -1.0;
    digits.erase(0, 1);
  }
  if (digits.empty()) {
    return std::nullopt;
  }
  double magnitude = 0.0;
  if (digits.find('.') != std::string::npos) {
    // Some writers emit explicit decimals regardless of %FS.
    const std::optional<double> literal = parse_double(digits);
    if (!literal) {
      return std::nullopt;
    }
    magnitude = *literal;
  } else {
    if (suppression == ZeroSuppression::Trailing) {
      const std::string::size_type width =
          static_cast<std::string::size_type>(integer_digits + decimal_digits);
      while (digits.size() < width) {
        digits.push_back('0');
      }
    }
    const std::optional<double> scaled = parse_double(digits);
    if (!scaled) {
      return std::nullopt;
    }
    magnitude = *scaled / std::pow(10.0, decimal_digits);
  }
  const double millimetres = sign * magnitude * (unit == PcbUnit::Inch ? 25.4 : 1.0);
  return millimetres;
}

class GerberParser {
 public:
  explicit GerberParser(const std::string& text) : text_(text) {}

  GerberFile run() {
    for (const RawStatement& statement : scan_statements(text_)) {
      if (file_.operations.size() >= kMaxOperations) {
        file_.diagnostics.push_back(
            {statement.line, "operation limit reached — rest of file ignored"});
        break;
      }
      if (statement.extended) {
        handle_extended(statement);
      } else {
        handle_word(statement);
      }
      if (ended_) {
        break;
      }
    }
    if (!file_.format.defined) {
      file_.ok = false;
      file_.diagnostics.push_back({0, "no %FS format statement — coordinates are unreadable"});
    }
    return file_;
  }

 private:
  enum class Interpolation { Linear, Clockwise, CounterClockwise };

  void note_unsupported(const std::string& what, int line) {
    if (std::find(file_.unsupported.begin(), file_.unsupported.end(), what) ==
        file_.unsupported.end()) {
      file_.unsupported.push_back(what);
    }
    file_.diagnostics.push_back({line, "unsupported construct skipped: " + what});
  }

  void track_bounds(double x, double y) {
    if (!file_.bounds.valid) {
      file_.bounds = GerberBounds{x, y, x, y, true};
      return;
    }
    file_.bounds.min_x = std::min(file_.bounds.min_x, x);
    file_.bounds.min_y = std::min(file_.bounds.min_y, y);
    file_.bounds.max_x = std::max(file_.bounds.max_x, x);
    file_.bounds.max_y = std::max(file_.bounds.max_y, y);
  }

  void emit(GerberOperation operation) {
    track_bounds(operation.x, operation.y);
    file_.operations.push_back(operation);
  }

  // ── extended (%…%) statements ─────────────────────────────────────

  void handle_extended(const RawStatement& statement) {
    const std::string& text = statement.text;
    // Continue the macro body started by an `AM` statement in the same block.
    if (current_macro_ != nullptr && statement.block == macro_block_) {
      add_macro_statement(text, statement.line);
      return;
    }
    current_macro_ = nullptr;

    if (starts_with(text, "FS")) {
      parse_format(text, statement.line);
    } else if (starts_with(text, "MO")) {
      const std::string mode = trim(text.substr(2));
      if (mode == "MM") {
        file_.format.unit = PcbUnit::Millimeter;
      } else if (mode == "IN") {
        file_.format.unit = PcbUnit::Inch;
      } else {
        file_.diagnostics.push_back({statement.line, "unknown unit mode: " + mode});
      }
    } else if (starts_with(text, "AD")) {
      parse_aperture(text, statement.line);
    } else if (starts_with(text, "AM")) {
      const std::string name = trim(text.substr(2));
      if (name.empty()) {
        file_.diagnostics.push_back({statement.line, "aperture macro without a name"});
        return;
      }
      GerberMacro macro;
      macro.name = name;
      current_macro_ = &(file_.macros[name] = macro);
      macro_block_ = statement.block;
    } else if (starts_with(text, "LP")) {
      const std::string polarity = trim(text.substr(2));
      if (polarity == "D") {
        dark_ = true;
      } else if (polarity == "C") {
        dark_ = false;
      } else {
        file_.diagnostics.push_back({statement.line, "unknown LP polarity: " + polarity});
      }
    } else if (starts_with(text, "TF")) {
      parse_file_attribute(text.substr(2));
    } else if (starts_with(text, "TA")) {
      const std::string value = attribute_value(text.substr(2));
      if (!value.empty()) {
        aperture_function_ = value;
      }
    } else if (starts_with(text, "TD")) {
      aperture_function_.clear();
    } else if (starts_with(text, "TO")) {
      // Object attributes carry no geometry — ignored by design.
    } else if (starts_with(text, "IP")) {
      const std::string polarity = trim(text.substr(2));
      if (polarity == "NEG") {
        note_unsupported("%IPNEG (negative image)", statement.line);
      }
    } else if (starts_with(text, "SR")) {
      note_unsupported("%SR (step and repeat)", statement.line);
    } else if (starts_with(text, "AB")) {
      note_unsupported("%AB (block aperture)", statement.line);
    } else if (starts_with(text, "IN") || starts_with(text, "LN") || starts_with(text, "AS")) {
      // Image name / layer name / axis select — metadata, no geometry.
    } else {
      note_unsupported("%" + text.substr(0, std::min<size_t>(text.size(), 2)),
                       statement.line);
    }
  }

  void parse_format(const std::string& text, int line) {
    file_.format.defined = true;
    size_t position = 2;
    int* integer_digits = &file_.format.x_integer_digits;
    int* decimal_digits = &file_.format.x_decimal_digits;
    while (position < text.size()) {
      const char c = text[position];
      if (c == 'L') {
        file_.format.zero_suppression = ZeroSuppression::Leading;
        ++position;
      } else if (c == 'T') {
        file_.format.zero_suppression = ZeroSuppression::Trailing;
        ++position;
      } else if (c == 'D') {
        file_.format.zero_suppression = ZeroSuppression::Explicit;
        ++position;
      } else if (c == 'A') {
        file_.format.mode = CoordinateMode::Absolute;
        ++position;
      } else if (c == 'I') {
        file_.format.mode = CoordinateMode::Incremental;
        ++position;
      } else if (c == 'X' || c == 'Y') {
        if (c == 'Y') {
          integer_digits = &file_.format.y_integer_digits;
          decimal_digits = &file_.format.y_decimal_digits;
        }
        ++position;
        std::string digits;
        while (position < text.size() &&
               std::isdigit(static_cast<unsigned char>(text[position]))) {
          digits.push_back(text[position]);
          ++position;
        }
        if (digits.size() != 2) {
          file_.diagnostics.push_back({line, "malformed %FS digit specification"});
          return;
        }
        *integer_digits = digits[0] - '0';
        *decimal_digits = digits[1] - '0';
      } else if (c == 'N') {
        position += std::min<size_t>(2, text.size() - position);
      } else {
        ++position;
      }
    }
    if (file_.format.y_integer_digits == 0 && file_.format.y_decimal_digits == 0) {
      file_.format.y_integer_digits = file_.format.x_integer_digits;
      file_.format.y_decimal_digits = file_.format.x_decimal_digits;
    }
  }

  void parse_file_attribute(const std::string& body) {
    const std::string::size_type comma = body.find(',');
    const std::string name = trim(comma == std::string::npos ? body : body.substr(0, comma));
    const std::string value =
        comma == std::string::npos ? std::string() : trim(body.substr(comma + 1));
    if (name == "FileFunction") {
      file_.file_function = value;
    } else if (name == "FilePolarity") {
      file_.file_polarity = value;
    }
  }

  static std::string attribute_value(const std::string& body) {
    const std::string::size_type comma = body.find(',');
    if (comma == std::string::npos) {
      return std::string();
    }
    return trim(body.substr(comma + 1));
  }

  void parse_aperture(const std::string& text, int line) {
    size_t position = 2;
    // The command is spelled `ADD<code>` — skip the trailing 'D' before
    // reading the code digits.  Reading them straight after "AD" silently
    // rejected every aperture in the file (the parser's first real bug).
    if (position < text.size() && (text[position] == 'D' || text[position] == 'd')) {
      ++position;
    }
    std::string digits;
    while (position < text.size() &&
           std::isdigit(static_cast<unsigned char>(text[position]))) {
      digits.push_back(text[position]);
      ++position;
    }
    if (digits.empty()) {
      file_.diagnostics.push_back({line, "aperture definition without a D-code"});
      return;
    }
    GerberAperture aperture;
    aperture.code = std::atoi(digits.c_str());
    aperture.line = line;
    aperture.function = aperture_function_;

    std::string name;
    while (position < text.size() && text[position] != ',') {
      name.push_back(text[position]);
      ++position;
    }
    if (position < text.size()) {
      ++position;  // ','
    }
    const std::vector<std::string> raw_params = split(text.substr(position), 'X');
    std::vector<double> params;
    for (const std::string& raw : raw_params) {
      const std::string trimmed = trim(raw);
      if (trimmed.empty()) {
        continue;
      }
      const std::optional<double> value = parse_double(trimmed);
      if (!value) {
        file_.diagnostics.push_back(
            {line, "aperture D" + std::to_string(aperture.code) +
                       " has a non-numeric parameter: " + trimmed});
        return;
      }
      params.push_back(*value);
    }

    if (name == "C") {
      aperture.kind = ApertureKind::Circle;
      aperture.params = params;
    } else if (name == "R") {
      aperture.kind = ApertureKind::Rectangle;
      aperture.params = params;
    } else if (name == "O") {
      aperture.kind = ApertureKind::Obround;
      aperture.params = params;
    } else if (name == "P") {
      aperture.kind = ApertureKind::Polygon;
      aperture.params = params;
    } else {
      // A macro invocation — resolved by the polygonizer, which reports a
      // missing definition rather than the parser failing the file.
      aperture.kind = ApertureKind::Macro;
      aperture.macro_name = name;
      aperture.macro_args = params;
    }
    file_.apertures[aperture.code] = aperture;
  }

  void add_macro_statement(const std::string& text, int line) {
    if (current_macro_ == nullptr || current_macro_->statements.size() >= kMaxMacroStatements) {
      return;
    }
    if (text[0] == '$') {
      const std::string::size_type equals = text.find('=');
      if (equals == std::string::npos) {
        file_.diagnostics.push_back({line, "malformed macro assignment: " + text});
        return;
      }
      GerberMacro::Statement statement;
      statement.is_assignment = true;
      statement.variable = trim(text.substr(0, equals));
      statement.expression = trim(text.substr(equals + 1));
      statement.line = line;
      current_macro_->statements.push_back(statement);
      return;
    }
    const std::vector<std::string> parts = split(text, ',');
    const std::optional<double> code = parse_double(trim(parts[0]));
    if (!code) {
      file_.diagnostics.push_back({line, "macro primitive without a code: " + text});
      return;
    }
    const int primitive_code = static_cast<int>(*code);
    if (primitive_code == 0) {
      return;  // comment primitive
    }
    GerberMacro::Statement statement;
    statement.code = primitive_code;
    statement.line = line;
    if (parts.size() > 1) {
      statement.exposure_dark = trim(parts[1]) != "0";
    }
    for (size_t index = 2; index < parts.size(); ++index) {
      statement.param_exprs.push_back(trim(parts[index]));
    }
    current_macro_->statements.push_back(statement);
  }

  // ── word commands (G/D/M/X/Y/I/J) ─────────────────────────────────

  void handle_word(const RawStatement& statement) {
    const std::string& word = statement.text;
    // `G04`/`G4` is a comment (and the carrier of KiCad's X2 attributes).
    if (starts_with(word, "G04")) {
      handle_comment(word.substr(3), statement.line);
      return;
    }
    if (starts_with(word, "G4") &&
        (word.size() == 2 || !std::isdigit(static_cast<unsigned char>(word[2])))) {
      handle_comment(word.substr(2), statement.line);
      return;
    }

    bool has_x = false;
    bool has_y = false;
    double x = current_x_;
    double y = current_y_;
    double i = 0.0;
    double j = 0.0;
    bool has_i = false;
    bool has_j = false;
    int d_code = 0;

    size_t position = 0;
    while (position < word.size()) {
      const char letter = static_cast<char>(std::toupper(static_cast<unsigned char>(word[position])));
      ++position;
      const size_t begin = position;
      while (position < word.size() &&
             (std::isdigit(static_cast<unsigned char>(word[position])) ||
              word[position] == '+' || word[position] == '-' || word[position] == '.')) {
        ++position;
      }
      const std::string raw = word.substr(begin, position - begin);
      switch (letter) {
        case 'G': {
          if (raw.empty()) {
            break;
          }
          handle_g_code(std::atoi(raw.c_str()), statement.line);
          break;
        }
        case 'M': {
          if (!raw.empty() && std::atoi(raw.c_str()) == 2) {
            ended_ = true;
          }
          break;
        }
        case 'D': {
          if (raw.empty()) {
            break;
          }
          const int code = std::atoi(raw.c_str());
          if (code == 1 || code == 2 || code == 3) {
            d_code = code;
          } else if (code >= 10) {
            if (file_.apertures.find(code) == file_.apertures.end()) {
              file_.diagnostics.push_back(
                  {statement.line, "D" + std::to_string(code) + " selected but never defined"});
            }
            current_aperture_ = code;
          }
          break;
        }
        case 'X': {
          const std::optional<double> value =
              coordinate_to_mm(raw, file_.format.x_integer_digits,
                               file_.format.x_decimal_digits,
                               file_.format.zero_suppression, file_.format.unit);
          if (!value) {
            file_.diagnostics.push_back({statement.line, "unreadable X coordinate: " + raw});
            break;
          }
          x = file_.format.mode == CoordinateMode::Incremental ? current_x_ + *value : *value;
          has_x = true;
          break;
        }
        case 'Y': {
          const std::optional<double> value =
              coordinate_to_mm(raw, file_.format.y_integer_digits,
                               file_.format.y_decimal_digits,
                               file_.format.zero_suppression, file_.format.unit);
          if (!value) {
            file_.diagnostics.push_back({statement.line, "unreadable Y coordinate: " + raw});
            break;
          }
          y = file_.format.mode == CoordinateMode::Incremental ? current_y_ + *value : *value;
          has_y = true;
          break;
        }
        case 'I': {
          const std::optional<double> value =
              coordinate_to_mm(raw, file_.format.x_integer_digits,
                               file_.format.x_decimal_digits,
                               file_.format.zero_suppression, file_.format.unit);
          if (value) {
            i = *value;
            has_i = true;
          }
          break;
        }
        case 'J': {
          const std::optional<double> value =
              coordinate_to_mm(raw, file_.format.y_integer_digits,
                               file_.format.y_decimal_digits,
                               file_.format.zero_suppression, file_.format.unit);
          if (value) {
            j = *value;
            has_j = true;
          }
          break;
        }
        default:
          break;
      }
    }

    if (d_code == 0) {
      // Coordinates without a D-code: RS-274X expects one per operation, but
      // some writers keep D01 modal.  Continue with the previous operation
      // kind instead of dropping geometry.
      if (!has_x && !has_y) {
        return;
      }
      d_code = last_d_code_;
      if (d_code == 0) {
        return;
      }
    }
    if (!has_x && !has_y) {
      return;  // e.g. a bare "D10*" aperture select
    }

    GerberOperation operation;
    operation.x = x;
    operation.y = y;
    operation.i = i;
    operation.j = j;
    operation.dark = dark_;
    operation.aperture_code = current_aperture_;
    operation.line = statement.line;
    switch (d_code) {
      case 1:
        if (interpolation_ == Interpolation::Linear) {
          operation.kind = GerberOperation::Kind::Line;
        } else {
          operation.kind = GerberOperation::Kind::Arc;
          operation.clockwise = interpolation_ == Interpolation::Clockwise;
          if (!has_i && !has_j) {
            file_.diagnostics.push_back(
                {statement.line, "arc without I/J centre offset — treated as a line"});
            operation.kind = GerberOperation::Kind::Line;
          } else if (!arc_mode_set_ && !warned_default_arc_mode_) {
            // Gerber's literal default is single-quadrant, but every modern
            // writer emits G75; assume multi-quadrant and say so once.
            warned_default_arc_mode_ = true;
            file_.diagnostics.push_back(
                {statement.line, "arc mode not specified — assuming multi-quadrant (G75)"});
          }
        }
        break;
      case 3:
        operation.kind = GerberOperation::Kind::Flash;
        break;
      default:
        operation.kind = GerberOperation::Kind::Move;
        break;
    }
    last_d_code_ = d_code;
    current_x_ = x;
    current_y_ = y;
    emit(operation);
  }

  void handle_g_code(int code, int line) {
    switch (code) {
      case 1:
        interpolation_ = Interpolation::Linear;
        break;
      case 2:
        interpolation_ = Interpolation::Clockwise;
        break;
      case 3:
        interpolation_ = Interpolation::CounterClockwise;
        break;
      case 36: {
        GerberOperation operation;
        operation.kind = GerberOperation::Kind::RegionBegin;
        operation.x = current_x_;
        operation.y = current_y_;
        operation.dark = dark_;
        operation.aperture_code = current_aperture_;
        operation.line = line;
        emit(operation);
        break;
      }
      case 37: {
        GerberOperation operation;
        operation.kind = GerberOperation::Kind::RegionEnd;
        operation.x = current_x_;
        operation.y = current_y_;
        operation.dark = dark_;
        operation.aperture_code = current_aperture_;
        operation.line = line;
        emit(operation);
        break;
      }
      case 70:
        file_.format.unit = PcbUnit::Inch;
        break;
      case 71:
        file_.format.unit = PcbUnit::Millimeter;
        break;
      case 74:
        file_.multi_quadrant_arcs = false;
        arc_mode_set_ = true;
        note_unsupported("G74 (single-quadrant arcs)", line);
        break;
      case 75:
        file_.multi_quadrant_arcs = true;
        arc_mode_set_ = true;
        break;
      case 90:
        file_.format.mode = CoordinateMode::Absolute;
        break;
      case 91:
        file_.format.mode = CoordinateMode::Incremental;
        break;
      default:
        break;
    }
  }

  /// `G04` comment text.  KiCad writes the X2 role attributes here
  /// (`G04 #@! TF.FileFunction,Copper,L1,Top*`), so they are harvested
  /// even though a strict X1 parser would ignore the line.
  void handle_comment(const std::string& text, int line) {
    std::string body = trim(text);
    const std::string::size_type marker = body.find("#@!");
    if (marker != std::string::npos) {
      body = trim(body.substr(marker + 3));
    }
    if (starts_with(body, "TF.")) {
      parse_file_attribute(body.substr(3));
    } else if (starts_with(body, "TA.")) {
      const std::string value = attribute_value(body.substr(3));
      if (!value.empty()) {
        aperture_function_ = value;
      }
    } else if (starts_with(body, "TD")) {
      aperture_function_.clear();
    } else if (starts_with(body, "TO.")) {
      // Object attribute — no geometry.
    } else if (!body.empty() && body[0] == 'T' && body.size() > 2 && body[2] == '.') {
      // Unknown attribute family: recorded once so the UI can list it.
      if (std::find(file_.unsupported.begin(), file_.unsupported.end(),
                    "unknown attribute") == file_.unsupported.end()) {
        file_.unsupported.push_back("unknown attribute");
      }
    }
    (void)line;
  }

  const std::string& text_;
  GerberFile file_;
  Interpolation interpolation_ = Interpolation::Linear;
  bool dark_ = true;
  bool arc_mode_set_ = false;
  bool warned_default_arc_mode_ = false;
  bool ended_ = false;
  int current_aperture_ = 0;
  int last_d_code_ = 0;
  double current_x_ = 0.0;
  double current_y_ = 0.0;
  std::string aperture_function_;
  GerberMacro* current_macro_ = nullptr;
  size_t macro_block_ = 0;
};

}  // namespace

std::optional<double> evaluate_macro_expression(const std::string& expression,
                                                const std::vector<double>& variables) {
  ExpressionParser parser(expression, variables);
  return parser.parse();
}

ResolvedMacro resolve_macro(const GerberMacro& macro, const std::vector<double>& args,
                            std::vector<PcbDiagnostic>* diagnostics, int line) {
  ResolvedMacro resolved;
  std::vector<double> variables = args;
  for (const GerberMacro::Statement& statement : macro.statements) {
    if (statement.is_assignment) {
      const std::optional<double> value =
          evaluate_macro_expression(statement.expression, variables);
      if (!value) {
        resolved.supported = false;
        if (diagnostics != nullptr) {
          diagnostics->push_back({line, "macro " + macro.name + ": cannot evaluate " +
                                            statement.variable + "=" + statement.expression});
        }
        return resolved;
      }
      const std::string& variable = statement.variable;
      if (variable.size() < 2 || !all_digits(variable.substr(1))) {
        resolved.supported = false;
        if (diagnostics != nullptr) {
          diagnostics->push_back({line, "macro " + macro.name + ": malformed variable " +
                                            variable});
        }
        return resolved;
      }
      const size_t index =
          static_cast<size_t>(std::strtoul(variable.c_str() + 1, nullptr, 10));
      if (index == 0) {
        resolved.supported = false;
        if (diagnostics != nullptr) {
          diagnostics->push_back({line, "macro " + macro.name + ": $0 is not assignable"});
        }
        return resolved;
      }
      if (variables.size() < index) {
        variables.resize(index, 0.0);
      }
      variables[index - 1] = *value;
      continue;
    }

    if (!(statement.code == 1 || statement.code == 4 || statement.code == 5 ||
          statement.code == 20 || statement.code == 21 || statement.code == 22)) {
      resolved.supported = false;
      if (diagnostics != nullptr) {
        diagnostics->push_back({line, "macro " + macro.name + ": primitive " +
                                          std::to_string(statement.code) + " unsupported"});
      }
      return resolved;
    }

    MacroPrimitive primitive;
    primitive.code = statement.code;
    primitive.exposure_dark = statement.exposure_dark;
    for (const std::string& expression : statement.param_exprs) {
      const std::optional<double> value = evaluate_macro_expression(expression, variables);
      if (!value) {
        resolved.supported = false;
        if (diagnostics != nullptr) {
          diagnostics->push_back({line, "macro " + macro.name + ": cannot evaluate " +
                                            expression});
        }
        return resolved;
      }
      primitive.params.push_back(*value);
    }
    resolved.primitives.push_back(primitive);
  }
  return resolved;
}

GerberFile parse_gerber(const std::string& text) {
  GerberParser parser(text);
  return parser.run();
}

}  // namespace polysmith::core::pcb
