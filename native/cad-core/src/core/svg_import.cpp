#include "core/svg_import.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <fstream>
#include <regex>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace polysmith::core {
namespace {

constexpr double kCurveStep = 0.05;
constexpr double kPi = 3.14159265358979323846;
constexpr std::size_t kMaxSvgSegments = 20000;

struct Transform {
  double a = 1.0;
  double b = 0.0;
  double c = 0.0;
  double d = 1.0;
  double e = 0.0;
  double f = 0.0;
};

struct SvgContext {
  Transform transform;
  bool hidden = false;
  std::size_t layer_index = 0;
  int group_depth = 0;
};

Transform multiply(const Transform& left, const Transform& right) {
  return Transform{
      .a = left.a * right.a + left.c * right.b,
      .b = left.b * right.a + left.d * right.b,
      .c = left.a * right.c + left.c * right.d,
      .d = left.b * right.c + left.d * right.d,
      .e = left.a * right.e + left.c * right.f + left.e,
      .f = left.b * right.e + left.d * right.f + left.f,
  };
}

std::pair<double, double> apply_transform(const Transform& transform,
                                          double x,
                                          double y) {
  return {transform.a * x + transform.c * y + transform.e,
          transform.b * x + transform.d * y + transform.f};
}

std::string read_file(const std::string& file_path) {
  std::ifstream stream(file_path, std::ios::binary);
  if (!stream) {
    throw std::runtime_error("Unable to read SVG: " + file_path);
  }
  std::ostringstream buffer;
  buffer << stream.rdbuf();
  return buffer.str();
}

double read_number(const std::string& text, double fallback) {
  std::smatch match;
  static const std::regex number_re(R"(^\s*(-?\d+(?:\.\d+)?(?:[eE][+-]?\d+)?))");
  if (std::regex_search(text, match, number_re)) {
    return std::stod(match[1].str());
  }
  return fallback;
}

std::string attr(const std::string& tag, const std::string& name) {
  auto is_name_char = [](char c) {
    return std::isalnum(static_cast<unsigned char>(c)) || c == '_' ||
           c == '-' || c == ':';
  };

  std::size_t search_from = 0;
  while (search_from < tag.size()) {
    const std::size_t found = tag.find(name, search_from);
    if (found == std::string::npos) {
      return "";
    }
    const bool has_left_boundary =
        found == 0 || !is_name_char(tag[found - 1]);
    std::size_t i = found + name.size();
    const bool has_right_boundary =
        i >= tag.size() || !is_name_char(tag[i]);
    if (!has_left_boundary || !has_right_boundary) {
      search_from = found + 1;
      continue;
    }
    while (i < tag.size() && std::isspace(static_cast<unsigned char>(tag[i]))) {
      ++i;
    }
    if (i >= tag.size() || tag[i] != '=') {
      search_from = found + 1;
      continue;
    }
    ++i;
    while (i < tag.size() && std::isspace(static_cast<unsigned char>(tag[i]))) {
      ++i;
    }
    if (i >= tag.size() || (tag[i] != '"' && tag[i] != '\'')) {
      search_from = found + 1;
      continue;
    }
    const char quote = tag[i++];
    const std::size_t value_start = i;
    const std::size_t value_end = tag.find(quote, value_start);
    if (value_end == std::string::npos) {
      return "";
    }
    return tag.substr(value_start, value_end - value_start);
  }
  return "";
}

std::string lowercase(std::string value) {
  std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
    return static_cast<char>(std::tolower(c));
  });
  return value;
}

std::string trim(std::string value) {
  const auto first = value.find_first_not_of(" \t\r\n");
  if (first == std::string::npos) {
    return "";
  }
  const auto last = value.find_last_not_of(" \t\r\n");
  return value.substr(first, last - first + 1);
}

std::string layer_name_from_tag(const std::string& tag, std::size_t fallback_index) {
  const std::array<std::string, 4> names = {
      attr(tag, "inkscape:label"),
      attr(tag, "aria-label"),
      attr(tag, "label"),
      attr(tag, "id"),
  };
  for (const auto& name : names) {
    const std::string cleaned = trim(name);
    if (!cleaned.empty()) {
      return cleaned;
    }
  }
  return "Layer " + std::to_string(fallback_index);
}

std::vector<double> parse_numbers(const std::string& text);

Transform parse_transform(const std::string& text,
                          std::vector<std::string>& warnings) {
  Transform transform;
  static const std::regex command_re(
      R"((matrix|translate|scale|rotate)\s*\(([^)]*)\))",
      std::regex::icase);
  auto begin = std::sregex_iterator(text.begin(), text.end(), command_re);
  auto end = std::sregex_iterator();
  for (auto it = begin; it != end; ++it) {
    const std::string name = lowercase((*it)[1].str());
    const auto values = parse_numbers((*it)[2].str());
    Transform next;
    if (name == "matrix" && values.size() >= 6) {
      next = Transform{values[0], values[1], values[2], values[3], values[4], values[5]};
    } else if (name == "translate" && !values.empty()) {
      next.e = values[0];
      next.f = values.size() >= 2 ? values[1] : 0.0;
    } else if (name == "scale" && !values.empty()) {
      next.a = values[0];
      next.d = values.size() >= 2 ? values[1] : values[0];
    } else if (name == "rotate" && !values.empty()) {
      const double angle = values[0] * kPi / 180.0;
      const double ca = std::cos(angle);
      const double sa = std::sin(angle);
      Transform rotation{ca, sa, -sa, ca, 0.0, 0.0};
      if (values.size() >= 3) {
        const Transform to_origin{1.0, 0.0, 0.0, 1.0, -values[1], -values[2]};
        const Transform back{1.0, 0.0, 0.0, 1.0, values[1], values[2]};
        next = multiply(back, multiply(rotation, to_origin));
      } else {
        next = rotation;
      }
    } else {
      warnings.push_back("Unsupported SVG transform skipped");
      continue;
    }
    transform = multiply(transform, next);
  }
  return transform;
}

bool hidden_by_display_or_paint(const std::string& tag) {
  const std::string display = lowercase(attr(tag, "display"));
  const std::string visibility = lowercase(attr(tag, "visibility"));
  const std::string style = lowercase(attr(tag, "style"));
  if (display == "none" || visibility == "hidden" ||
      style.find("display:none") != std::string::npos ||
      style.find("visibility:hidden") != std::string::npos) {
    return true;
  }
  const std::string fill = lowercase(attr(tag, "fill"));
  const std::string stroke = lowercase(attr(tag, "stroke"));
  return fill == "none" && stroke == "none";
}

void add_segment(std::vector<SvgLineSegment>& segments,
                 double x1,
                 double y1,
                 double x2,
                 double y2,
                 const Transform& transform = Transform{}) {
  const auto [tx1, ty1] = apply_transform(transform, x1, y1);
  const auto [tx2, ty2] = apply_transform(transform, x2, y2);
  x1 = tx1;
  y1 = ty1;
  x2 = tx2;
  y2 = ty2;
  const double dx = x2 - x1;
  const double dy = y2 - y1;
  if (std::sqrt(dx * dx + dy * dy) <= 0.001) {
    return;
  }
  if (segments.size() >= kMaxSvgSegments) {
    return;
  }
  segments.push_back({x1, y1, x2, y2});
}

std::vector<double> parse_numbers(const std::string& text) {
  std::vector<double> values;
  static const std::regex number_re(R"([-+]?\d*\.?\d+(?:[eE][-+]?\d+)?)");
  auto begin = std::sregex_iterator(text.begin(), text.end(), number_re);
  auto end = std::sregex_iterator();
  for (auto it = begin; it != end; ++it) {
    values.push_back(std::stod((*it).str()));
  }
  return values;
}

void add_poly_points(std::vector<SvgLineSegment>& segments,
                     const std::string& points,
                     bool close,
                     const Transform& transform) {
  const auto values = parse_numbers(points);
  if (values.size() < 4) {
    return;
  }
  for (std::size_t i = 2; i + 1 < values.size(); i += 2) {
    add_segment(segments,
                values[i - 2],
                values[i - 1],
                values[i],
                values[i + 1],
                transform);
  }
  if (close) {
    add_segment(segments,
                values[values.size() - 2],
                values[values.size() - 1],
                values[0],
                values[1],
                transform);
  }
}

void add_ellipse(std::vector<SvgLineSegment>& segments,
                 double cx,
                 double cy,
                 double rx,
                 double ry,
                 const Transform& transform) {
  if (rx <= 0.0 || ry <= 0.0) {
    return;
  }
  constexpr int steps = 96;
  double prev_x = cx + rx;
  double prev_y = cy;
  for (int i = 1; i <= steps; ++i) {
    const double a = (static_cast<double>(i) / steps) * 2.0 * kPi;
    const double x = cx + std::cos(a) * rx;
    const double y = cy + std::sin(a) * ry;
    add_segment(segments, prev_x, prev_y, x, y, transform);
    prev_x = x;
    prev_y = y;
  }
}

struct PathToken {
  bool command = false;
  char c = '\0';
  double value = 0.0;
};

std::vector<PathToken> tokenize_path(const std::string& d) {
  std::vector<PathToken> tokens;
  static const std::regex token_re(
      R"(([AaCcHhLlMmQqSsTtVvZz])|([-+]?\d*\.?\d+(?:[eE][-+]?\d+)?))");
  auto begin = std::sregex_iterator(d.begin(), d.end(), token_re);
  auto end = std::sregex_iterator();
  for (auto it = begin; it != end; ++it) {
    if ((*it)[1].matched) {
      tokens.push_back({true, (*it)[1].str()[0], 0.0});
    } else {
      tokens.push_back({false, '\0', std::stod((*it)[2].str())});
    }
  }
  return tokens;
}

bool has_number(const std::vector<PathToken>& tokens, std::size_t i) {
  return i < tokens.size() && !tokens[i].command;
}

double cubic(double p0, double p1, double p2, double p3, double t) {
  const double u = 1.0 - t;
  return u * u * u * p0 + 3.0 * u * u * t * p1 + 3.0 * u * t * t * p2 +
         t * t * t * p3;
}

double quad(double p0, double p1, double p2, double t) {
  const double u = 1.0 - t;
  return u * u * p0 + 2.0 * u * t * p1 + t * t * p2;
}

void flatten_path(std::vector<SvgLineSegment>& segments,
                  std::vector<std::string>& warnings,
                  const std::string& d,
                  const Transform& transform) {
  const auto tokens = tokenize_path(d);
  bool warned_segment_limit = false;
  auto reached_segment_limit = [&]() {
    if (segments.size() < kMaxSvgSegments) {
      return false;
    }
    if (!warned_segment_limit) {
      warnings.push_back("SVG import reached the sketch segment limit; remaining geometry was skipped");
      warned_segment_limit = true;
    }
    return true;
  };
  std::size_t i = 0;
  char cmd = '\0';
  double x = 0.0;
  double y = 0.0;
  double sx = 0.0;
  double sy = 0.0;
  while (i < tokens.size()) {
    if (reached_segment_limit()) {
      break;
    }
    if (tokens[i].command) {
      cmd = tokens[i++].c;
    }
    if (cmd == '\0') {
      break;
    }
    const bool rel = std::islower(static_cast<unsigned char>(cmd));
    const char op = static_cast<char>(std::toupper(static_cast<unsigned char>(cmd)));
    auto read = [&]() -> double { return tokens[i++].value; };
    if (op == 'M') {
      if (!has_number(tokens, i + 1)) break;
      x = read();
      y = read();
      if (rel) {
        x += sx;
        y += sy;
      }
      sx = x;
      sy = y;
      cmd = rel ? 'l' : 'L';
      continue;
    }
    if (op == 'L') {
      while (has_number(tokens, i + 1)) {
        if (reached_segment_limit()) break;
        double nx = read();
        double ny = read();
        if (rel) {
          nx += x;
          ny += y;
        }
        add_segment(segments, x, y, nx, ny, transform);
        x = nx;
        y = ny;
      }
      continue;
    }
    if (op == 'H') {
      while (has_number(tokens, i)) {
        if (reached_segment_limit()) break;
        double nx = read();
        if (rel) nx += x;
        add_segment(segments, x, y, nx, y, transform);
        x = nx;
      }
      continue;
    }
    if (op == 'V') {
      while (has_number(tokens, i)) {
        if (reached_segment_limit()) break;
        double ny = read();
        if (rel) ny += y;
        add_segment(segments, x, y, x, ny, transform);
        y = ny;
      }
      continue;
    }
    if (op == 'C') {
      while (has_number(tokens, i + 5)) {
        if (reached_segment_limit()) break;
        double x1 = read(), y1 = read(), x2 = read(), y2 = read();
        double x3 = read(), y3 = read();
        if (rel) {
          x1 += x; y1 += y; x2 += x; y2 += y; x3 += x; y3 += y;
        }
        double px = x;
        double py = y;
        for (double t = kCurveStep; t <= 1.0001; t += kCurveStep) {
          if (reached_segment_limit()) break;
          const double nx = cubic(x, x1, x2, x3, std::min(t, 1.0));
          const double ny = cubic(y, y1, y2, y3, std::min(t, 1.0));
          add_segment(segments, px, py, nx, ny, transform);
          px = nx;
          py = ny;
        }
        x = x3;
        y = y3;
      }
      continue;
    }
    if (op == 'Q') {
      while (has_number(tokens, i + 3)) {
        if (reached_segment_limit()) break;
        double x1 = read(), y1 = read(), x2 = read(), y2 = read();
        if (rel) {
          x1 += x; y1 += y; x2 += x; y2 += y;
        }
        double px = x;
        double py = y;
        for (double t = kCurveStep; t <= 1.0001; t += kCurveStep) {
          if (reached_segment_limit()) break;
          const double nx = quad(x, x1, x2, std::min(t, 1.0));
          const double ny = quad(y, y1, y2, std::min(t, 1.0));
          add_segment(segments, px, py, nx, ny, transform);
          px = nx;
          py = ny;
        }
        x = x2;
        y = y2;
      }
      continue;
    }
    if (op == 'A') {
      warnings.push_back("SVG arc path commands were approximated as straight chords");
      while (has_number(tokens, i + 6)) {
        if (reached_segment_limit()) break;
        const double rx = read();
        const double ry = read();
        (void)rx;
        (void)ry;
        (void)read(); // x-axis-rotation
        (void)read(); // large-arc-flag
        (void)read(); // sweep-flag
        double nx = read();
        double ny = read();
        if (rel) {
          nx += x;
          ny += y;
        }
        add_segment(segments, x, y, nx, ny, transform);
        x = nx;
        y = ny;
      }
      continue;
    }
    if (op == 'Z') {
      add_segment(segments, x, y, sx, sy, transform);
      x = sx;
      y = sy;
      cmd = '\0';
      continue;
    }
    warnings.push_back(std::string("Unsupported SVG path command skipped: ") + op);
    while (has_number(tokens, i)) {
      ++i;
    }
  }
}

}  // namespace

SvgImportResult flatten_svg_file(const std::string& file_path) {
  SvgImportResult result;
  result.layers.push_back(SvgLayer{.name = "SVG"});
  const std::string text = read_file(file_path);

  std::vector<SvgContext> contexts{SvgContext{}};
  bool read_root_svg_size = false;
  std::size_t search_from = 0;
  while (search_from < text.size()) {
    const std::size_t tag_start = text.find('<', search_from);
    if (tag_start == std::string::npos) {
      break;
    }
    const std::size_t tag_end = text.find('>', tag_start + 1);
    if (tag_end == std::string::npos) {
      break;
    }
    search_from = tag_end + 1;
    const std::string tag = text.substr(tag_start, tag_end - tag_start + 1);
    if (tag.rfind("<!--", 0) == 0 || tag.rfind("<!", 0) == 0 ||
        tag.rfind("<?", 0) == 0) {
      continue;
    }

    std::size_t cursor = 1;
    while (cursor < tag.size() &&
           std::isspace(static_cast<unsigned char>(tag[cursor]))) {
      ++cursor;
    }
    bool closing = false;
    if (cursor < tag.size() && tag[cursor] == '/') {
      closing = true;
      ++cursor;
    }
    while (cursor < tag.size() &&
           std::isspace(static_cast<unsigned char>(tag[cursor]))) {
      ++cursor;
    }
    const std::size_t name_start = cursor;
    while (cursor < tag.size() &&
           (std::isalnum(static_cast<unsigned char>(tag[cursor])) ||
            tag[cursor] == ':' || tag[cursor] == '_' || tag[cursor] == '-')) {
      ++cursor;
    }
    if (cursor == name_start) {
      continue;
    }
    std::string name = lowercase(tag.substr(name_start, cursor - name_start));
    const std::size_t prefix = name.find(':');
    if (prefix != std::string::npos) {
      name = name.substr(prefix + 1);
    }
    const std::string attrs_text =
        cursor < tag.size() ? tag.substr(cursor, tag.size() - cursor - 1) : "";
    const bool self_closing =
        !attrs_text.empty() &&
        attrs_text.find_last_not_of(" \t\r\n") != std::string::npos &&
        attrs_text[attrs_text.find_last_not_of(" \t\r\n")] == '/';

    if (!closing && name == "svg" && !read_root_svg_size) {
      read_root_svg_size = true;
      result.width = read_number(attr(tag, "width"), result.width);
      result.height = read_number(attr(tag, "height"), result.height);
      const auto vb = parse_numbers(attr(tag, "viewBox"));
      if (vb.size() == 4 && vb[2] > 0.0 && vb[3] > 0.0) {
        if (attr(tag, "width").empty()) result.width = vb[2];
        if (attr(tag, "height").empty()) result.height = vb[3];
      }
    }

    if (closing) {
      if ((name == "g" || name == "svg" || name == "defs" ||
           name == "symbol" || name == "filter" || name == "mask" ||
           name == "clippath") &&
          contexts.size() > 1) {
        contexts.pop_back();
      }
      continue;
    }

    const SvgContext parent_context = contexts.back();
    SvgContext context = parent_context;
    context.transform =
        multiply(context.transform, parse_transform(attr(tag, "transform"), result.warnings));
    context.hidden = context.hidden || hidden_by_display_or_paint(tag);
    const bool container = name == "g" || name == "svg" || name == "defs" ||
                           name == "symbol" || name == "filter" ||
                           name == "mask" || name == "clippath";
    if (name == "defs" || name == "symbol" || name == "filter" ||
        name == "mask" || name == "clippath") {
      context.hidden = true;
    }
    if (name == "g" && !context.hidden) {
      const bool explicit_layer =
          lowercase(attr(tag, "inkscape:groupmode")) == "layer" ||
          !attr(tag, "inkscape:label").empty();
      if (explicit_layer || parent_context.group_depth == 0) {
        result.layers.push_back(SvgLayer{
            .name = layer_name_from_tag(tag, result.layers.size()),
        });
        context.layer_index = result.layers.size() - 1;
      }
      context.group_depth = parent_context.group_depth + 1;
    }
    if (container && !self_closing) {
      contexts.push_back(context);
      continue;
    }
    if (context.hidden) {
      continue;
    }

    const Transform& transform = context.transform;
    auto& layer_segments = result.layers[context.layer_index].segments;
    if (name == "line") {
      add_segment(layer_segments,
                  read_number(attr(tag, "x1"), 0.0),
                  read_number(attr(tag, "y1"), 0.0),
                  read_number(attr(tag, "x2"), 0.0),
                  read_number(attr(tag, "y2"), 0.0),
                  transform);
    } else if (name == "rect") {
      const double x = read_number(attr(tag, "x"), 0.0);
      const double y = read_number(attr(tag, "y"), 0.0);
      const double w = read_number(attr(tag, "width"), 0.0);
      const double h = read_number(attr(tag, "height"), 0.0);
      add_segment(layer_segments, x, y, x + w, y, transform);
      add_segment(layer_segments, x + w, y, x + w, y + h, transform);
      add_segment(layer_segments, x + w, y + h, x, y + h, transform);
      add_segment(layer_segments, x, y + h, x, y, transform);
    } else if (name == "polyline") {
      add_poly_points(layer_segments, attr(tag, "points"), false, transform);
    } else if (name == "polygon") {
      add_poly_points(layer_segments, attr(tag, "points"), true, transform);
    } else if (name == "circle") {
      const double r = read_number(attr(tag, "r"), 0.0);
      add_ellipse(layer_segments,
                  read_number(attr(tag, "cx"), 0.0),
                  read_number(attr(tag, "cy"), 0.0),
                  r,
                  r,
                  transform);
    } else if (name == "ellipse") {
      add_ellipse(layer_segments,
                  read_number(attr(tag, "cx"), 0.0),
                  read_number(attr(tag, "cy"), 0.0),
                  read_number(attr(tag, "rx"), 0.0),
                  read_number(attr(tag, "ry"), 0.0),
                  transform);
    } else if (name == "path") {
      flatten_path(layer_segments, result.warnings, attr(tag, "d"), transform);
    }
  }

  std::vector<SvgLayer> non_empty_layers;
  for (auto& layer : result.layers) {
    if (!layer.segments.empty()) {
      non_empty_layers.push_back(std::move(layer));
    }
  }
  result.layers = std::move(non_empty_layers);
  result.segments.clear();
  for (const auto& layer : result.layers) {
    result.segments.insert(result.segments.end(),
                           layer.segments.begin(),
                           layer.segments.end());
  }

  if (std::regex_search(text, std::regex(R"(<image\b)", std::regex::icase))) {
    result.warnings.push_back("Embedded raster images are not converted to sketch geometry");
  }
  if (std::regex_search(text, std::regex(R"(<text\b)", std::regex::icase))) {
    result.warnings.push_back("SVG text is not converted to sketch geometry");
  }
  if (std::regex_search(text, std::regex(R"(<(filter|mask|clipPath)\b)", std::regex::icase))) {
    result.warnings.push_back("SVG filters, masks, and clipping effects are skipped");
  }
  if (result.segments.empty()) {
    throw std::runtime_error("SVG did not contain usable vector geometry");
  }
  return result;
}

}  // namespace polysmith::core
