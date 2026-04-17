#include "core/sketch_profile.h"

#include <algorithm>
#include <cmath>
#include <deque>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <vector>

namespace polysmith::core {
namespace {

constexpr double kProfileTolerance = 0.01;

bool nearly_equal(double left, double right) {
  return std::abs(left - right) <= kProfileTolerance;
}

bool points_match(const SketchProfilePoint& left, const SketchProfilePoint& right) {
  return nearly_equal(left.x, right.x) && nearly_equal(left.y, right.y);
}

long long quantize_coordinate(double value) {
  return std::llround(value / kProfileTolerance);
}

std::string make_node_key(const SketchProfilePoint& point) {
  return std::to_string(quantize_coordinate(point.x)) + ":" +
         std::to_string(quantize_coordinate(point.y));
}

double polygon_signed_area(const std::vector<SketchProfilePoint>& points) {
  if (points.size() < 3) {
    return 0.0;
  }

  double area = 0.0;
  for (size_t index = 0; index < points.size(); ++index) {
    const auto& current = points[index];
    const auto& next = points[(index + 1) % points.size()];
    area += current.x * next.y - next.x * current.y;
  }

  return area * 0.5;
}

std::string make_polygon_profile_id(const std::vector<std::string>& line_ids) {
  std::vector<std::string> sorted_ids = line_ids;
  std::sort(sorted_ids.begin(), sorted_ids.end());

  std::ostringstream stream;
  stream << "profile-poly";
  for (const auto& id : sorted_ids) {
    stream << "-" << id;
  }
  return stream.str();
}

struct LineLoopCandidate {
  std::vector<SketchProfilePoint> points;
  std::vector<std::string> line_ids;
};

std::optional<LineLoopCandidate> detect_line_loop(
    const std::vector<SketchLine>& lines) {
  if (lines.size() < 3) {
    return std::nullopt;
  }

  std::map<std::string, SketchProfilePoint> nodes;
  std::map<std::string, std::vector<size_t>> adjacency;
  std::vector<std::pair<std::string, std::string>> line_nodes;

  for (size_t index = 0; index < lines.size(); ++index) {
    const auto& line = lines[index];
    const SketchProfilePoint start{.x = line.start_x, .y = line.start_y};
    const SketchProfilePoint end{.x = line.end_x, .y = line.end_y};

    if (points_match(start, end)) {
      return std::nullopt;
    }

    const std::string start_key = make_node_key(start);
    const std::string end_key = make_node_key(end);
    nodes.emplace(start_key, start);
    nodes.emplace(end_key, end);
    adjacency[start_key].push_back(index);
    adjacency[end_key].push_back(index);
    line_nodes.push_back({start_key, end_key});
  }

  if (nodes.size() != lines.size()) {
    return std::nullopt;
  }

  for (const auto& [node_key, incident_lines] : adjacency) {
    if (incident_lines.size() != 2) {
      return std::nullopt;
    }
  }

  std::vector<SketchProfilePoint> ordered_points;
  std::vector<std::string> ordered_line_ids;
  std::set<size_t> visited_lines;

  std::string current_node = line_nodes.front().first;

  while (visited_lines.size() < lines.size()) {
    const auto adjacency_it = adjacency.find(current_node);
    if (adjacency_it == adjacency.end()) {
      return std::nullopt;
    }

    const auto next_line_it = std::find_if(
        adjacency_it->second.begin(),
        adjacency_it->second.end(),
        [&](size_t line_index) { return !visited_lines.contains(line_index); });

    if (next_line_it == adjacency_it->second.end()) {
      return std::nullopt;
    }

    const size_t line_index = *next_line_it;
    visited_lines.insert(line_index);
    ordered_points.push_back(nodes.at(current_node));
    ordered_line_ids.push_back(lines[line_index].id);

    const auto& [start_key, end_key] = line_nodes[line_index];
    current_node = start_key == current_node ? end_key : start_key;
  }

  if (!points_match(nodes.at(line_nodes.front().first), nodes.at(current_node))) {
    return std::nullopt;
  }

  const double area = polygon_signed_area(ordered_points);
  if (std::abs(area) <= kProfileTolerance) {
    return std::nullopt;
  }

  if (area < 0.0) {
    std::reverse(ordered_points.begin(), ordered_points.end());
  }

  return LineLoopCandidate{
      .points = ordered_points,
      .line_ids = ordered_line_ids,
  };
}

std::vector<std::vector<SketchLine>> split_line_components(
    const std::vector<SketchLine>& lines) {
  std::map<std::string, std::vector<size_t>> node_to_lines;
  std::vector<std::pair<std::string, std::string>> line_nodes;

  for (size_t index = 0; index < lines.size(); ++index) {
    const auto& line = lines[index];
    const std::string start_key = make_node_key({
        .x = line.start_x,
        .y = line.start_y,
    });
    const std::string end_key = make_node_key({
        .x = line.end_x,
        .y = line.end_y,
    });
    node_to_lines[start_key].push_back(index);
    node_to_lines[end_key].push_back(index);
    line_nodes.push_back({start_key, end_key});
  }

  std::set<size_t> visited_lines;
  std::vector<std::vector<SketchLine>> components;

  for (size_t start_index = 0; start_index < lines.size(); ++start_index) {
    if (visited_lines.contains(start_index)) {
      continue;
    }

    std::deque<size_t> frontier = {start_index};
    std::vector<SketchLine> component;

    while (!frontier.empty()) {
      const size_t line_index = frontier.front();
      frontier.pop_front();

      if (visited_lines.contains(line_index)) {
        continue;
      }

      visited_lines.insert(line_index);
      component.push_back(lines[line_index]);

      const auto& [start_key, end_key] = line_nodes[line_index];
      for (const auto& node_key : {start_key, end_key}) {
        const auto adjacency_it = node_to_lines.find(node_key);
        if (adjacency_it == node_to_lines.end()) {
          continue;
        }

        for (size_t adjacent_line_index : adjacency_it->second) {
          if (!visited_lines.contains(adjacent_line_index)) {
            frontier.push_back(adjacent_line_index);
          }
        }
      }
    }

    components.push_back(component);
  }

  return components;
}

}  // namespace

DetectedSketchProfiles detect_sketch_profiles(const FeatureEntry& feature) {
  DetectedSketchProfiles profiles;

  if (feature.kind != "sketch" || !feature.sketch_parameters.has_value()) {
    return profiles;
  }

  const auto& sketch = feature.sketch_parameters.value();

  for (const auto& component : split_line_components(sketch.lines)) {
    if (const auto loop = detect_line_loop(component); loop.has_value()) {
      profiles.polygons.push_back(PolygonSketchProfile{
          .id = make_polygon_profile_id(loop->line_ids),
          .plane_id = sketch.plane_id,
          .plane_frame = sketch.plane_frame,
          .points = loop->points,
      });
    }
  }

  for (const auto& circle : sketch.circles) {
    profiles.circles.push_back(CircleSketchProfile{
        .id = "profile-circle-" + circle.id,
        .plane_id = sketch.plane_id,
        .plane_frame = sketch.plane_frame,
        .center_x = circle.center_x,
        .center_y = circle.center_y,
        .radius = circle.radius,
    });
  }

  return profiles;
}

}  // namespace polysmith::core
