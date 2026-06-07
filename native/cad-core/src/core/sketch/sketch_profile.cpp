#include "core/sketch_profile.h"

#include <algorithm>
#include <cmath>
#include <deque>
#include <limits>
#include <map>
#include <optional>
#include <set>
#include <sstream>
#include <string>
#include <vector>

namespace polysmith::core {
namespace {

constexpr double kProfileTolerance = 0.01;
// Per-arc sample count when materializing a closed loop into a
// polyline. 16 segments per arc gives a visually smooth approximation
// at typical sketch scales while keeping the resulting polygon cheap
// for OCCT to extrude. We sample uniformly in arc parameter (angle)
// regardless of arc sweep size; tiny arcs end up with tightly-spaced
// samples and large arcs with widely-spaced ones, which is fine.
constexpr int kArcSampleSegments = 16;
constexpr int kCircleProfileSampleSegments = 64;
// Local copy of pi — `M_PI` is non-standard (POSIX-only) and
// `std::numbers::pi` is C++20-only. Defining it here keeps this file
// portable without committing the rest of the codebase to either.
constexpr double kPi = 3.14159265358979323846;

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

std::string make_polygon_profile_id(const std::vector<std::string>& edge_ids) {
  std::vector<std::string> sorted_ids = edge_ids;
  std::sort(sorted_ids.begin(), sorted_ids.end());

  std::ostringstream stream;
  stream << "profile-poly";
  for (const auto& id : sorted_ids) {
    stream << "-" << id;
  }
  return stream.str();
}

// Generic edge wrapper used by the loop detector so a single graph
// algorithm walks both lines and arcs uniformly. `kind == Arc` means
// the cached arc params (center / radius / ccw) are populated; lines
// leave them at zero.
struct ProfileEdge {
  enum class Kind { Line, Arc };

  std::string id;
  Kind kind;
  std::string start_point_id;
  std::string end_point_id;
  double start_x;
  double start_y;
  double end_x;
  double end_y;
  // Arc-only fields (zeroed for lines).
  double center_x = 0.0;
  double center_y = 0.0;
  double radius = 0.0;
  bool ccw = false;
};

ProfileEdge profile_edge_from_line(const SketchLine& line) {
  return ProfileEdge{
      .id = line.id,
      .kind = ProfileEdge::Kind::Line,
      .start_point_id = line.start_point_id,
      .end_point_id = line.end_point_id,
      .start_x = line.start_x,
      .start_y = line.start_y,
      .end_x = line.end_x,
      .end_y = line.end_y,
  };
}

ProfileEdge profile_edge_from_arc(const SketchArc& arc) {
  return ProfileEdge{
      .id = arc.id,
      .kind = ProfileEdge::Kind::Arc,
      .start_point_id = arc.start_point_id,
      .end_point_id = arc.end_point_id,
      .start_x = arc.start_x,
      .start_y = arc.start_y,
      .end_x = arc.end_x,
      .end_y = arc.end_y,
      .center_x = arc.center_x,
      .center_y = arc.center_y,
      .radius = arc.radius,
      .ccw = arc.ccw,
  };
}

std::string edge_node_id(const ProfileEdge& edge, bool is_start) {
  const std::string& point_id =
      is_start ? edge.start_point_id : edge.end_point_id;
  if (!point_id.empty()) {
    return point_id;
  }
  return make_node_key({
      .x = is_start ? edge.start_x : edge.end_x,
      .y = is_start ? edge.start_y : edge.end_y,
  });
}

// Sample interior points of an arc walked from `from` to `to`,
// excluding both endpoints. The result is the (kArcSampleSegments-1)
// intermediate samples in walk order, ready to slot between the
// endpoint vertices in the polygon point list. We parameterize on
// signed sweep angle: the arc's stored ccw direction tells us which
// way start→end goes; if the loop walk crosses the arc end-to-start
// instead, we just reverse `forward` so the sweep direction matches.
std::vector<SketchProfilePoint> sample_arc_interior(
    const ProfileEdge& edge, bool from_start) {
  std::vector<SketchProfilePoint> samples;
  if (edge.kind != ProfileEdge::Kind::Arc) {
    return samples;
  }

  const double start_angle =
      std::atan2(edge.start_y - edge.center_y, edge.start_x - edge.center_x);
  const double end_angle =
      std::atan2(edge.end_y - edge.center_y, edge.end_x - edge.center_x);

  // Compute the signed sweep from start_angle to end_angle along
  // the stored ccw direction. We normalize into (0, 2π) so a full-
  // circle sweep doesn't collapse to zero.
  double sweep = end_angle - start_angle;
  if (edge.ccw) {
    while (sweep <= 0.0) {
      sweep += 2.0 * kPi;
    }
  } else {
    while (sweep >= 0.0) {
      sweep -= 2.0 * kPi;
    }
  }

  // If we're walking the arc in reverse (loop entered at end_point
  // and exits at start_point) we still sample the same geometric
  // points but in reverse order. Compute samples forward, reverse
  // at the end if needed.
  for (int i = 1; i < kArcSampleSegments; ++i) {
    const double t = static_cast<double>(i) / kArcSampleSegments;
    const double angle = start_angle + sweep * t;
    samples.push_back(SketchProfilePoint{
        .x = edge.center_x + edge.radius * std::cos(angle),
        .y = edge.center_y + edge.radius * std::sin(angle),
    });
  }

  if (!from_start) {
    std::reverse(samples.begin(), samples.end());
  }
  return samples;
}

struct EdgeLoopCandidate {
  std::vector<SketchProfilePoint> points;
  std::vector<std::string> point_ids;
  std::vector<std::string> edge_ids;
};

struct LineSegment {
  std::string line_id;
  SketchProfilePoint start;
  SketchProfilePoint end;
};

struct ArrangementEdge {
  std::string line_id;
  int start_node;
  int end_node;
};

struct ArrangementFace {
  std::vector<SketchProfilePoint> points;
  std::vector<std::string> point_ids;
  std::vector<std::string> line_ids;
  double area;
};

std::optional<EdgeLoopCandidate> detect_edge_loop(
    const std::vector<ProfileEdge>& edges) {
  if (edges.size() < 2) {
    // A polygon needs at least 3 edges, but a loop made of two arcs
    // (e.g. a stadium half) is also valid. Two lines can't enclose
    // an area, so 2-line components fail the area test below; arcs
    // can pass on the area test, so we accept >=2 here and let the
    // signed-area gate filter degenerate cases.
    return std::nullopt;
  }

  std::map<std::string, SketchProfilePoint> nodes;
  std::map<std::string, std::vector<size_t>> adjacency;
  std::vector<std::pair<std::string, std::string>> edge_nodes;

  for (size_t index = 0; index < edges.size(); ++index) {
    const auto& edge = edges[index];
    const SketchProfilePoint start{.x = edge.start_x, .y = edge.start_y};
    const SketchProfilePoint end{.x = edge.end_x, .y = edge.end_y};

    if (points_match(start, end)) {
      return std::nullopt;
    }

    const std::string start_key = edge_node_id(edge, true);
    const std::string end_key = edge_node_id(edge, false);
    const auto [start_it, inserted_start] = nodes.emplace(start_key, start);
    const auto [end_it, inserted_end] = nodes.emplace(end_key, end);
    const bool start_uses_fallback_key = edge.start_point_id.empty();
    const bool end_uses_fallback_key = edge.end_point_id.empty();
    if ((!inserted_start && start_uses_fallback_key &&
         !points_match(start_it->second, start)) ||
        (!inserted_end && end_uses_fallback_key &&
         !points_match(end_it->second, end))) {
      return std::nullopt;
    }
    adjacency[start_key].push_back(index);
    adjacency[end_key].push_back(index);
    edge_nodes.push_back({start_key, end_key});
  }

  if (nodes.size() != edges.size()) {
    return std::nullopt;
  }

  for (const auto& [node_key, incident_edges] : adjacency) {
    if (incident_edges.size() != 2) {
      return std::nullopt;
    }
  }

  std::vector<SketchProfilePoint> ordered_points;
  std::vector<std::string> ordered_point_ids;
  std::vector<std::string> ordered_edge_ids;
  std::set<size_t> visited_edges;

  std::string current_node = edge_nodes.front().first;

  while (visited_edges.size() < edges.size()) {
    const auto adjacency_it = adjacency.find(current_node);
    if (adjacency_it == adjacency.end()) {
      return std::nullopt;
    }

    const auto next_edge_it = std::find_if(
        adjacency_it->second.begin(),
        adjacency_it->second.end(),
        [&](size_t edge_index) { return !visited_edges.contains(edge_index); });

    if (next_edge_it == adjacency_it->second.end()) {
      return std::nullopt;
    }

    const size_t edge_index = *next_edge_it;
    visited_edges.insert(edge_index);
    const auto& edge = edges[edge_index];

    // Push the start vertex of this edge in walk direction. The next
    // edge's iteration will push the next vertex, and so on around
    // the loop.
    ordered_points.push_back(nodes.at(current_node));
    ordered_point_ids.push_back(current_node);
    ordered_edge_ids.push_back(edge.id);

    // For arc edges, also push the interior samples so the polygon
    // approximates the arc when extruded. Direction matters: if the
    // loop walk enters at start_point_id we sample forward; if it
    // enters at end_point_id we sample reversed.
    if (edge.kind == ProfileEdge::Kind::Arc) {
      const bool from_start = current_node == edge_nodes[edge_index].first;
      const auto interior = sample_arc_interior(edge, from_start);
      // Interior samples don't have stable point ids — use a synthetic
      // one anchored to the arc id and the sample index so consumers
      // that group by point id can still distinguish them. They're
      // not user-meaningful and don't appear as snap targets.
      for (size_t i = 0; i < interior.size(); ++i) {
        ordered_points.push_back(interior[i]);
        ordered_point_ids.push_back(
            "arc-sample-" + edge.id + "-" + std::to_string(i));
      }
    }

    const auto& [start_key, end_key] = edge_nodes[edge_index];
    current_node = start_key == current_node ? end_key : start_key;
  }

  if (!points_match(nodes.at(edge_nodes.front().first),
                    nodes.at(current_node))) {
    return std::nullopt;
  }

  const double area = polygon_signed_area(ordered_points);
  if (std::abs(area) <= kProfileTolerance) {
    return std::nullopt;
  }

  if (area < 0.0) {
    std::reverse(ordered_points.begin(), ordered_points.end());
  }

  return EdgeLoopCandidate{
      .points = ordered_points,
      .point_ids = ordered_point_ids,
      .edge_ids = ordered_edge_ids,
  };
}

std::vector<std::vector<ProfileEdge>> split_edge_components(
    const std::vector<ProfileEdge>& edges) {
  std::map<std::string, std::vector<size_t>> node_to_edges;
  std::vector<std::pair<std::string, std::string>> edge_nodes;

  for (size_t index = 0; index < edges.size(); ++index) {
    const auto& edge = edges[index];
    const std::string start_key = edge_node_id(edge, true);
    const std::string end_key = edge_node_id(edge, false);
    node_to_edges[start_key].push_back(index);
    node_to_edges[end_key].push_back(index);
    edge_nodes.push_back({start_key, end_key});
  }

  std::set<size_t> visited_edges;
  std::vector<std::vector<ProfileEdge>> components;

  for (size_t start_index = 0; start_index < edges.size(); ++start_index) {
    if (visited_edges.contains(start_index)) {
      continue;
    }

    std::deque<size_t> frontier = {start_index};
    std::vector<ProfileEdge> component;

    while (!frontier.empty()) {
      const size_t edge_index = frontier.front();
      frontier.pop_front();

      if (visited_edges.contains(edge_index)) {
        continue;
      }

      visited_edges.insert(edge_index);
      component.push_back(edges[edge_index]);

      const auto& [start_key, end_key] = edge_nodes[edge_index];
      for (const auto& node_key : {start_key, end_key}) {
        const auto adjacency_it = node_to_edges.find(node_key);
        if (adjacency_it == node_to_edges.end()) {
          continue;
        }

        for (size_t adjacent_edge_index : adjacency_it->second) {
          if (!visited_edges.contains(adjacent_edge_index)) {
            frontier.push_back(adjacent_edge_index);
          }
        }
      }
    }

    components.push_back(component);
  }

  return components;
}

double cross(double ax, double ay, double bx, double by) {
  return ax * by - ay * bx;
}

double segment_parameter(const LineSegment& segment,
                         const SketchProfilePoint& point) {
  const double dx = segment.end.x - segment.start.x;
  const double dy = segment.end.y - segment.start.y;
  const double length_sq = dx * dx + dy * dy;
  if (length_sq <= kProfileTolerance * kProfileTolerance) {
    return 0.0;
  }
  return ((point.x - segment.start.x) * dx + (point.y - segment.start.y) * dy) /
         length_sq;
}

std::optional<SketchProfilePoint> line_intersection(
    const LineSegment& left, const LineSegment& right) {
  const double px = left.start.x;
  const double py = left.start.y;
  const double rx = left.end.x - left.start.x;
  const double ry = left.end.y - left.start.y;
  const double qx = right.start.x;
  const double qy = right.start.y;
  const double sx = right.end.x - right.start.x;
  const double sy = right.end.y - right.start.y;

  const double denominator = cross(rx, ry, sx, sy);
  if (std::abs(denominator) <= kProfileTolerance) {
    return std::nullopt;
  }

  const double qpx = qx - px;
  const double qpy = qy - py;
  const double t = cross(qpx, qpy, sx, sy) / denominator;
  const double u = cross(qpx, qpy, rx, ry) / denominator;
  if (t < -kProfileTolerance || t > 1.0 + kProfileTolerance ||
      u < -kProfileTolerance || u > 1.0 + kProfileTolerance) {
    return std::nullopt;
  }

  return SketchProfilePoint{.x = px + rx * t, .y = py + ry * t};
}

bool point_in_polygon(const SketchProfilePoint& point,
                      const std::vector<SketchProfilePoint>& polygon) {
  bool inside = false;
  for (size_t i = 0, j = polygon.size() - 1; i < polygon.size(); j = i++) {
    const auto& a = polygon[i];
    const auto& b = polygon[j];
    const bool crosses = ((a.y > point.y) != (b.y > point.y)) &&
                         (point.x < (b.x - a.x) * (point.y - a.y) /
                                            (b.y - a.y) +
                                        a.x);
    if (crosses) {
      inside = !inside;
    }
  }
  return inside;
}

SketchProfilePoint polygon_centroid(
    const std::vector<SketchProfilePoint>& polygon) {
  if (polygon.empty()) {
    return SketchProfilePoint{.x = 0.0, .y = 0.0};
  }

  double x = 0.0;
  double y = 0.0;
  for (const auto& point : polygon) {
    x += point.x;
    y += point.y;
  }
  const double count = static_cast<double>(polygon.size());
  return SketchProfilePoint{.x = x / count, .y = y / count};
}

bool circle_contains_circle(const SketchCircle& outer,
                            const SketchCircle& inner) {
  if (outer.id == inner.id ||
      outer.radius <= inner.radius + kProfileTolerance) {
    return false;
  }
  const double dx = inner.center_x - outer.center_x;
  const double dy = inner.center_y - outer.center_y;
  const double center_distance = std::sqrt(dx * dx + dy * dy);
  return center_distance + inner.radius <= outer.radius - kProfileTolerance;
}

double circle_area(const SketchCircle& circle) {
  return kPi * circle.radius * circle.radius;
}

void apply_nested_polygon_holes(std::vector<SketchProfileRegion>& profiles) {
  for (auto& inner : profiles) {
    if (inner.kind != "polygon" || inner.points.size() < 3) {
      continue;
    }

    const SketchProfilePoint inner_center = polygon_centroid(inner.points);
    const double inner_area = std::abs(polygon_signed_area(inner.points));
    SketchProfileRegion* containing_profile = nullptr;
    double containing_area = std::numeric_limits<double>::max();

    for (auto& candidate : profiles) {
      if (&candidate == &inner || candidate.kind != "polygon" ||
          candidate.points.size() < 3) {
        continue;
      }

      const double candidate_area =
          std::abs(polygon_signed_area(candidate.points));
      if (candidate_area <= inner_area + kProfileTolerance ||
          candidate_area >= containing_area) {
        continue;
      }

      if (point_in_polygon(inner_center, candidate.points)) {
        containing_profile = &candidate;
        containing_area = candidate_area;
      }
    }

    if (containing_profile != nullptr) {
      containing_profile->inner_loops.push_back(inner.points);
      containing_profile->id += "-hole-" + inner.id;
    }
  }
}

std::vector<SketchProfilePoint> sample_circle_loop(const SketchCircle& circle) {
  std::vector<SketchProfilePoint> points;
  points.reserve(kCircleProfileSampleSegments);
  for (int index = 0; index < kCircleProfileSampleSegments; ++index) {
    const double angle =
        (static_cast<double>(index) /
         static_cast<double>(kCircleProfileSampleSegments)) *
        2.0 * kPi;
    points.push_back(SketchProfilePoint{
        .x = circle.center_x + circle.radius * std::cos(angle),
        .y = circle.center_y + circle.radius * std::sin(angle),
    });
  }
  return points;
}

std::vector<LineSegment> solid_line_segments(
    const SketchFeatureParameters& parameters) {
  std::map<std::string, SketchProfilePoint> point_by_id;
  for (const auto& line : parameters.lines) {
    if (!line.start_point_id.empty()) {
      point_by_id.emplace(line.start_point_id,
                          SketchProfilePoint{.x = line.start_x,
                                             .y = line.start_y});
    }
    if (!line.end_point_id.empty()) {
      point_by_id.emplace(line.end_point_id,
                          SketchProfilePoint{.x = line.end_x,
                                             .y = line.end_y});
    }
  }

  auto endpoint = [&](const std::string& point_id,
                      double fallback_x,
                      double fallback_y) {
    const auto point_it = point_by_id.find(point_id);
    if (point_it != point_by_id.end()) {
      return point_it->second;
    }
    return SketchProfilePoint{.x = fallback_x, .y = fallback_y};
  };

  std::vector<LineSegment> segments;
  for (const auto& line : parameters.lines) {
    if (line.is_construction) {
      continue;
    }
    const SketchProfilePoint start =
        endpoint(line.start_point_id, line.start_x, line.start_y);
    const SketchProfilePoint end =
        endpoint(line.end_point_id, line.end_x, line.end_y);
    if (points_match(start, end)) {
      continue;
    }
    segments.push_back(LineSegment{.line_id = line.id, .start = start, .end = end});
  }
  return segments;
}

std::vector<LineSegment> circle_boundary_segments(const SketchCircle& circle) {
  std::vector<LineSegment> segments;
  if (circle.radius <= kProfileTolerance) {
    return segments;
  }
  const auto samples = sample_circle_loop(circle);
  segments.reserve(samples.size());
  for (size_t index = 0; index < samples.size(); ++index) {
    segments.push_back(LineSegment{
        .line_id = circle.id,
        .start = samples[index],
        .end = samples[(index + 1) % samples.size()],
    });
  }
  return segments;
}

bool segment_intersects_circle_boundary(const LineSegment& segment,
                                        const SketchCircle& circle) {
  const double dx = segment.end.x - segment.start.x;
  const double dy = segment.end.y - segment.start.y;
  const double fx = segment.start.x - circle.center_x;
  const double fy = segment.start.y - circle.center_y;
  const double a = dx * dx + dy * dy;
  if (a <= kProfileTolerance * kProfileTolerance) {
    return false;
  }

  const double b = 2.0 * (fx * dx + fy * dy);
  const double c = fx * fx + fy * fy - circle.radius * circle.radius;
  const double discriminant = b * b - 4.0 * a * c;
  if (discriminant < -kProfileTolerance) {
    return false;
  }

  const double sqrt_discriminant = std::sqrt(std::max(0.0, discriminant));
  const double t0 = (-b - sqrt_discriminant) / (2.0 * a);
  const double t1 = (-b + sqrt_discriminant) / (2.0 * a);
  return (t0 >= -kProfileTolerance && t0 <= 1.0 + kProfileTolerance) ||
         (t1 >= -kProfileTolerance && t1 <= 1.0 + kProfileTolerance);
}

bool any_line_intersects_circle_boundary(
    const std::vector<LineSegment>& segments,
    const std::vector<const SketchCircle*>& circles) {
  for (const auto& segment : segments) {
    for (const auto* circle : circles) {
      if (segment_intersects_circle_boundary(segment, *circle)) {
        return true;
      }
    }
  }
  return false;
}

std::vector<ArrangementFace> detect_arrangement_faces(
    const std::vector<LineSegment>& segments) {
  std::vector<std::vector<SketchProfilePoint>> split_points(segments.size());
  for (size_t index = 0; index < segments.size(); ++index) {
    split_points[index].push_back(segments[index].start);
    split_points[index].push_back(segments[index].end);
  }

  for (size_t left = 0; left < segments.size(); ++left) {
    for (size_t right = left + 1; right < segments.size(); ++right) {
      if (const auto intersection =
              line_intersection(segments[left], segments[right]);
          intersection.has_value()) {
        split_points[left].push_back(intersection.value());
        split_points[right].push_back(intersection.value());
      }
    }
  }

  std::map<std::string, int> node_by_key;
  std::vector<SketchProfilePoint> nodes;
  auto node_index_for = [&](const SketchProfilePoint& point) {
    const std::string key = make_node_key(point);
    const auto existing = node_by_key.find(key);
    if (existing != node_by_key.end()) {
      return existing->second;
    }
    const int next_index = static_cast<int>(nodes.size());
    node_by_key[key] = next_index;
    nodes.push_back(point);
    return next_index;
  };

  std::vector<ArrangementEdge> edges;
  std::set<std::string> edge_keys;
  for (size_t index = 0; index < segments.size(); ++index) {
    auto points = split_points[index];
    std::sort(points.begin(), points.end(), [&](const auto& left, const auto& right) {
      return segment_parameter(segments[index], left) <
             segment_parameter(segments[index], right);
    });
    points.erase(std::unique(points.begin(), points.end(), points_match),
                 points.end());

    for (size_t point_index = 1; point_index < points.size(); ++point_index) {
      const int start_node = node_index_for(points[point_index - 1]);
      const int end_node = node_index_for(points[point_index]);
      if (start_node == end_node) {
        continue;
      }
      const int low = std::min(start_node, end_node);
      const int high = std::max(start_node, end_node);
      const std::string key = std::to_string(low) + ":" + std::to_string(high);
      if (edge_keys.contains(key)) {
        continue;
      }
      edge_keys.insert(key);
      edges.push_back(ArrangementEdge{
          .line_id = segments[index].line_id,
          .start_node = start_node,
          .end_node = end_node,
      });
    }
  }

  std::vector<std::vector<int>> adjacency(nodes.size());
  for (const auto& edge : edges) {
    adjacency[edge.start_node].push_back(edge.end_node);
    adjacency[edge.end_node].push_back(edge.start_node);
  }
  for (size_t node_index = 0; node_index < adjacency.size(); ++node_index) {
    auto& neighbors = adjacency[node_index];
    std::sort(neighbors.begin(), neighbors.end(), [&](int left, int right) {
      const auto& origin = nodes[node_index];
      const double left_angle =
          std::atan2(nodes[left].y - origin.y, nodes[left].x - origin.x);
      const double right_angle =
          std::atan2(nodes[right].y - origin.y, nodes[right].x - origin.x);
      return left_angle < right_angle;
    });
  }

  std::map<std::pair<int, int>, std::string> line_for_directed_edge;
  for (const auto& edge : edges) {
    line_for_directed_edge[{edge.start_node, edge.end_node}] = edge.line_id;
    line_for_directed_edge[{edge.end_node, edge.start_node}] = edge.line_id;
  }

  std::set<std::pair<int, int>> visited;
  std::vector<ArrangementFace> faces;
  for (const auto& edge : edges) {
    for (const auto& directed :
         {std::pair<int, int>{edge.start_node, edge.end_node},
          std::pair<int, int>{edge.end_node, edge.start_node}}) {
      if (visited.contains(directed)) {
        continue;
      }

      std::vector<int> face_nodes;
      std::vector<std::string> face_lines;
      std::pair<int, int> current = directed;
      bool closed = false;
      for (size_t guard = 0; guard < edges.size() * 4 + 8; ++guard) {
        if (visited.contains(current)) {
          break;
        }
        visited.insert(current);
        face_nodes.push_back(current.first);
        face_lines.push_back(line_for_directed_edge[current]);

        const int at = current.second;
        const int from = current.first;
        const auto& neighbors = adjacency[at];
        const auto reverse_it = std::find(neighbors.begin(), neighbors.end(), from);
        if (reverse_it == neighbors.end() || neighbors.empty()) {
          break;
        }
        const size_t reverse_index =
            static_cast<size_t>(std::distance(neighbors.begin(), reverse_it));
        const size_t next_index =
            (reverse_index + neighbors.size() - 1) % neighbors.size();
        current = {at, neighbors[next_index]};
        if (current == directed) {
          closed = true;
          break;
        }
      }

      if (!closed || face_nodes.size() < 3) {
        continue;
      }

      std::vector<SketchProfilePoint> face_points;
      std::vector<std::string> face_point_ids;
      for (const int node : face_nodes) {
        face_points.push_back(nodes[node]);
        face_point_ids.push_back(make_node_key(nodes[node]));
      }
      const double area = polygon_signed_area(face_points);
      if (area <= kProfileTolerance) {
        continue;
      }
      std::sort(face_lines.begin(), face_lines.end());
      face_lines.erase(std::unique(face_lines.begin(), face_lines.end()),
                       face_lines.end());
      faces.push_back(ArrangementFace{
          .points = face_points,
          .point_ids = face_point_ids,
          .line_ids = face_lines,
          .area = area,
      });
    }
  }

  return faces;
}

std::vector<ArrangementFace> detect_line_arrangement_faces(
    const SketchFeatureParameters& parameters) {
  return detect_arrangement_faces(solid_line_segments(parameters));
}

}  // namespace

std::vector<SketchProfileRegion> build_sketch_profile_regions(
    const SketchFeatureParameters& parameters) {
  std::vector<SketchProfileRegion> profiles;

  // Construction lines are reference geometry only — they should not
  // close profile loops. Arcs don't have a construction-line analogue
  // yet (v1 ships with solid arcs only), but the future flag would
  // be applied here in the same way.
  std::vector<ProfileEdge> solid_edges;
  solid_edges.reserve(parameters.lines.size() + parameters.arcs.size());
  std::vector<const SketchCircle*> solid_circles;
  solid_circles.reserve(parameters.circles.size());
  for (const auto& line : parameters.lines) {
    if (!line.is_construction) {
      solid_edges.push_back(profile_edge_from_line(line));
    }
  }
  for (const auto& arc : parameters.arcs) {
    if (!arc.is_construction) {
      solid_edges.push_back(profile_edge_from_arc(arc));
    }
  }
  for (const auto& circle : parameters.circles) {
    if (!circle.is_construction) {
      solid_circles.push_back(&circle);
    }
  }

  const bool has_solid_arcs = std::any_of(
      solid_edges.begin(), solid_edges.end(), [](const ProfileEdge& edge) {
        return edge.kind == ProfileEdge::Kind::Arc;
      });
  const bool has_solid_lines = std::any_of(
      solid_edges.begin(), solid_edges.end(), [](const ProfileEdge& edge) {
        return edge.kind == ProfileEdge::Kind::Line;
      });
  auto line_segments = solid_line_segments(parameters);
  const bool use_circle_arrangement_profiles =
      !has_solid_arcs && has_solid_lines && !solid_circles.empty() &&
      any_line_intersects_circle_boundary(line_segments, solid_circles);

  if (!has_solid_arcs) {
    std::vector<ArrangementFace> arrangement_faces;
    if (use_circle_arrangement_profiles) {
      auto segments = line_segments;
      for (const auto* circle : solid_circles) {
        auto circle_segments = circle_boundary_segments(*circle);
        segments.insert(segments.end(), circle_segments.begin(),
                        circle_segments.end());
      }
      arrangement_faces = detect_arrangement_faces(segments);
    } else {
      arrangement_faces = detect_line_arrangement_faces(parameters);
    }

    auto unsplit_source_circle_id =
        [&](const ArrangementFace& face) -> std::optional<std::string> {
      if (face.line_ids.size() != 1) {
        return std::nullopt;
      }
      const std::string& only_id = face.line_ids.front();
      const auto circle_it = std::find_if(
          solid_circles.begin(), solid_circles.end(),
          [&](const SketchCircle* circle) { return circle->id == only_id; });
      if (circle_it == solid_circles.end()) {
        return std::nullopt;
      }
      return only_id;
    };

    for (const auto& face : arrangement_faces) {
      profiles.push_back(SketchProfileRegion{
          .id = make_polygon_profile_id(face.line_ids) + "-" +
                make_polygon_profile_id(face.point_ids),
          .kind = "polygon",
          .point_ids = face.point_ids,
          .line_ids = face.line_ids,
          .points = face.points,
          .inner_loops = {},
          .source_circle_id = unsplit_source_circle_id(face),
          .center_x = 0.0,
          .center_y = 0.0,
          .radius = 0.0,
      });
    }
  } else {
    for (const auto& component : split_edge_components(solid_edges)) {
      if (const auto loop = detect_edge_loop(component); loop.has_value()) {
        profiles.push_back(SketchProfileRegion{
            .id = make_polygon_profile_id(loop->edge_ids),
            .kind = "polygon",
            .point_ids = loop->point_ids,
            // `line_ids` historically held line ids; with arcs in the
            // mix this field carries mixed line + arc edge ids. The
            // downstream UI uses it to compute "which entities does
            // this profile depend on" for cascade-delete, which works
            // as long as the ids resolve to entities.
            .line_ids = loop->edge_ids,
            .points = loop->points,
            .inner_loops = {},
            .source_circle_id = std::nullopt,
            .center_x = 0.0,
            .center_y = 0.0,
            .radius = 0.0,
        });
      }
    }
  }

  apply_nested_polygon_holes(profiles);

  if (use_circle_arrangement_profiles) {
    return profiles;
  }

  std::map<std::string, std::vector<std::vector<SketchProfilePoint>>>
      circle_inner_loops;
  std::map<std::string, std::vector<std::string>> circle_hole_ids;

  for (const auto* circle : solid_circles) {
    const SketchProfilePoint center{.x = circle->center_x, .y = circle->center_y};
    SketchProfileRegion* containing_profile = nullptr;
    double containing_area = std::numeric_limits<double>::max();
    for (auto& profile : profiles) {
      if (profile.kind != "polygon" || !point_in_polygon(center, profile.points)) {
        continue;
      }
      const double area = std::abs(polygon_signed_area(profile.points));
      if (area < containing_area) {
        containing_profile = &profile;
        containing_area = area;
      }
    }

    const SketchCircle* containing_circle = nullptr;
    double containing_circle_area = std::numeric_limits<double>::max();
    for (const auto* candidate : solid_circles) {
      if (!circle_contains_circle(*candidate, *circle)) {
        continue;
      }
      const double area = circle_area(*candidate);
      if (area < containing_circle_area) {
        containing_circle = candidate;
        containing_circle_area = area;
      }
    }

    if (containing_circle != nullptr &&
        containing_circle_area < containing_area) {
      circle_inner_loops[containing_circle->id].push_back(
          sample_circle_loop(*circle));
      circle_hole_ids[containing_circle->id].push_back(circle->id);
    } else if (containing_profile != nullptr) {
      containing_profile->inner_loops.push_back(sample_circle_loop(*circle));
      containing_profile->id += "-hole-" + circle->id;
    }
  }

  for (const auto* circle : solid_circles) {
    const auto hole_it = circle_inner_loops.find(circle->id);
    if (hole_it != circle_inner_loops.end()) {
      std::string profile_id = "profile-circle-" + circle->id;
      const auto hole_id_it = circle_hole_ids.find(circle->id);
      if (hole_id_it != circle_hole_ids.end()) {
        for (const auto& hole_id : hole_id_it->second) {
          profile_id += "-hole-" + hole_id;
        }
      }
      profiles.push_back(SketchProfileRegion{
          .id = profile_id,
          .kind = "polygon",
          .point_ids = {"point-circle-" + circle->id + "-center"},
          .line_ids = {},
          .points = sample_circle_loop(*circle),
          .inner_loops = hole_it->second,
          .source_circle_id = circle->id,
          .center_x = 0.0,
          .center_y = 0.0,
          .radius = 0.0,
      });
      continue;
    }
    profiles.push_back(SketchProfileRegion{
        .id = "profile-circle-" + circle->id,
        .kind = "circle",
        .point_ids = {"point-circle-" + circle->id + "-center"},
        .line_ids = {},
        .points = {},
        .inner_loops = {},
        .source_circle_id = circle->id,
        .center_x = circle->center_x,
        .center_y = circle->center_y,
        .radius = circle->radius,
    });
  }

  return profiles;
}

DetectedSketchProfiles detect_sketch_profiles(const FeatureEntry& feature) {
  DetectedSketchProfiles profiles;

  if (feature.kind != "sketch" || !feature.sketch_parameters.has_value()) {
    return profiles;
  }

  const auto& sketch = feature.sketch_parameters.value();

  for (const auto& profile : sketch.profiles) {
    if (profile.kind == "polygon") {
      profiles.polygons.push_back(PolygonSketchProfile{
          .id = profile.id,
          .plane_id = sketch.plane_id,
          .plane_frame = sketch.plane_frame,
          .points = profile.points,
          .inner_loops = profile.inner_loops,
      });
      continue;
    }

    if (profile.kind == "circle") {
      profiles.circles.push_back(CircleSketchProfile{
          .id = profile.id,
          .plane_id = sketch.plane_id,
          .plane_frame = sketch.plane_frame,
          .center_x = profile.center_x,
          .center_y = profile.center_y,
          .radius = profile.radius,
      });
    }
  }

  return profiles;
}

}  // namespace polysmith::core
