#include "core/sketch_feature.h"

#include <algorithm>
#include <cmath>
#include <deque>
#include <sstream>
#include <stdexcept>
#include <tuple>

#include "core/sketch_profile.h"

namespace polysmith::core {
namespace {

constexpr double kMinimumSketchDimensionValue = 0.001;
constexpr double kCoincidentTolerance = 0.01;

std::string make_parameters_summary(const SketchFeatureParameters& parameters) {
  std::ostringstream stream;
  stream << parameters.plane_id << " · " << parameters.lines.size() << " line";
  if (parameters.lines.size() != 1) {
    stream << "s";
  }
  if (!parameters.circles.empty()) {
    stream << " · " << parameters.circles.size() << " circle";
    if (parameters.circles.size() != 1) {
      stream << "s";
    }
  }
  return stream.str();
}

void validate_line(double start_x,
                   double start_y,
                   double end_x,
                   double end_y) {
  const double dx = end_x - start_x;
  const double dy = end_y - start_y;
  if (std::sqrt(dx * dx + dy * dy) <= 0.001) {
    throw std::runtime_error("Sketch lines must have non-zero length");
  }
}

double measure_line_length(const SketchLine& line) {
  const double dx = line.end_x - line.start_x;
  const double dy = line.end_y - line.start_y;
  return std::sqrt(dx * dx + dy * dy);
}

bool nearly_equal(double left, double right) {
  return std::abs(left - right) <= kCoincidentTolerance;
}

bool points_match(double left_x,
                  double left_y,
                  double right_x,
                  double right_y) {
  return nearly_equal(left_x, right_x) && nearly_equal(left_y, right_y);
}

SketchPoint* find_sketch_point(SketchFeatureParameters& parameters,
                               const std::string& point_id) {
  const auto point_it = std::find_if(
      parameters.points.begin(),
      parameters.points.end(),
      [&](const SketchPoint& point) { return point.id == point_id; });
  return point_it == parameters.points.end() ? nullptr : &(*point_it);
}

const SketchPoint* find_sketch_point(const SketchFeatureParameters& parameters,
                                     const std::string& point_id) {
  const auto point_it = std::find_if(
      parameters.points.begin(),
      parameters.points.end(),
      [&](const SketchPoint& point) { return point.id == point_id; });
  return point_it == parameters.points.end() ? nullptr : &(*point_it);
}

bool point_is_fixed(const SketchFeatureParameters& parameters,
                    const std::string& point_id) {
  const SketchPoint* point = find_sketch_point(parameters, point_id);
  return point != nullptr && point->is_fixed;
}

std::optional<std::string> infer_constraint_hint(double start_x,
                                                 double start_y,
                                                 double end_x,
                                                 double end_y) {
  const double dx = std::abs(end_x - start_x);
  const double dy = std::abs(end_y - start_y);
  constexpr double kConstraintTolerance = 0.01;

  if (dx <= kConstraintTolerance) {
    return std::string("vertical");
  }

  if (dy <= kConstraintTolerance) {
    return std::string("horizontal");
  }

  return std::nullopt;
}

void apply_line_constraint(SketchLine& line) {
  if (!line.constraint.has_value()) {
    return;
  }

  if (line.constraint.value() == "horizontal") {
    line.end_y = line.start_y;
    return;
  }

  if (line.constraint.value() == "vertical") {
    line.end_x = line.start_x;
  }
}

void apply_line_constraint_respecting_fixed_points(
    const SketchFeatureParameters& parameters,
    SketchLine& line) {
  if (!line.constraint.has_value()) {
    return;
  }

  const bool start_fixed = point_is_fixed(parameters, line.start_point_id);
  const bool end_fixed = point_is_fixed(parameters, line.end_point_id);

  if (line.constraint.value() == "horizontal") {
    if (start_fixed && end_fixed && !nearly_equal(line.start_y, line.end_y)) {
      throw std::runtime_error(
          "Cannot make a line horizontal when both endpoints are fixed");
    }
    if (end_fixed) {
      line.start_y = line.end_y;
    } else {
      line.end_y = line.start_y;
    }
    return;
  }

  if (line.constraint.value() == "vertical") {
    if (start_fixed && end_fixed && !nearly_equal(line.start_x, line.end_x)) {
      throw std::runtime_error(
          "Cannot make a line vertical when both endpoints are fixed");
    }
    if (end_fixed) {
      line.start_x = line.end_x;
    } else {
      line.end_x = line.start_x;
    }
  }
}

void validate_constraint(const std::optional<std::string>& constraint) {
  if (!constraint.has_value()) {
    return;
  }

  if (constraint.value() != "horizontal" && constraint.value() != "vertical") {
    throw std::runtime_error("Unsupported sketch constraint: " + constraint.value());
  }
}

void validate_tool(const std::string& tool) {
  if (tool != "select" && tool != "line" && tool != "rectangle" &&
      tool != "circle") {
    throw std::runtime_error("Unsupported sketch tool: " + tool);
  }
}

SketchDimension& require_dimension(SketchFeatureParameters& parameters,
                                   const std::string& dimension_id) {
  const auto dimension_it = std::find_if(
      parameters.dimensions.begin(),
      parameters.dimensions.end(),
      [&](const SketchDimension& dimension) { return dimension.id == dimension_id; });

  if (dimension_it == parameters.dimensions.end()) {
    throw std::runtime_error("Sketch dimension not found: " + dimension_id);
  }

  return *dimension_it;
}

SketchLine& require_line(SketchFeatureParameters& parameters,
                         const std::string& line_id) {
  const auto line_it = std::find_if(
      parameters.lines.begin(),
      parameters.lines.end(),
      [&](const SketchLine& line) { return line.id == line_id; });

  if (line_it == parameters.lines.end()) {
    throw std::runtime_error("Sketch line not found: " + line_id);
  }

  return *line_it;
}

void sync_line_dimension(SketchFeatureParameters& parameters,
                         const SketchLine& line) {
  const auto dimension_it = std::find_if(
      parameters.dimensions.begin(),
      parameters.dimensions.end(),
      [&](const SketchDimension& dimension) {
        return dimension.kind == "line_length" && dimension.entity_id == line.id;
      });

  if (dimension_it != parameters.dimensions.end()) {
    dimension_it->value = measure_line_length(line);
  }
}

void sync_circle_dimension(SketchFeatureParameters& parameters,
                           const SketchCircle& circle) {
  const auto dimension_it = std::find_if(
      parameters.dimensions.begin(),
      parameters.dimensions.end(),
      [&](const SketchDimension& dimension) {
        return dimension.kind == "circle_radius" && dimension.entity_id == circle.id;
      });

  if (dimension_it != parameters.dimensions.end()) {
    dimension_it->value = circle.radius;
  }
}

void drive_line_length(SketchLine& line, double value) {
  const double dx = line.end_x - line.start_x;
  const double dy = line.end_y - line.start_y;
  const double current_length = std::sqrt(dx * dx + dy * dy);

  if (current_length <= kMinimumSketchDimensionValue) {
    throw std::runtime_error("Cannot drive a zero-length sketch line");
  }

  double direction_x = dx / current_length;
  double direction_y = dy / current_length;

  if (line.constraint.has_value()) {
    if (line.constraint.value() == "horizontal") {
      direction_x = dx >= 0.0 ? 1.0 : -1.0;
      direction_y = 0.0;
    } else if (line.constraint.value() == "vertical") {
      direction_x = 0.0;
      direction_y = dy >= 0.0 ? 1.0 : -1.0;
    }
  }

  line.end_x = line.start_x + direction_x * value;
  line.end_y = line.start_y + direction_y * value;
  apply_line_constraint(line);
  validate_line(line.start_x, line.start_y, line.end_x, line.end_y);
}

void drive_line_length_from_fixed_end(SketchLine& line, double value) {
  const double dx = line.start_x - line.end_x;
  const double dy = line.start_y - line.end_y;
  const double current_length = std::sqrt(dx * dx + dy * dy);

  if (current_length <= kMinimumSketchDimensionValue) {
    throw std::runtime_error("Cannot drive a zero-length sketch line");
  }

  double direction_x = dx / current_length;
  double direction_y = dy / current_length;

  if (line.constraint.has_value()) {
    if (line.constraint.value() == "horizontal") {
      direction_x = dx >= 0.0 ? 1.0 : -1.0;
      direction_y = 0.0;
    } else if (line.constraint.value() == "vertical") {
      direction_x = 0.0;
      direction_y = dy >= 0.0 ? 1.0 : -1.0;
    }
  }

  line.start_x = line.end_x + direction_x * value;
  line.start_y = line.end_y + direction_y * value;
  if (line.constraint == "horizontal") {
    line.start_y = line.end_y;
  } else if (line.constraint == "vertical") {
    line.start_x = line.end_x;
  }
  validate_line(line.start_x, line.start_y, line.end_x, line.end_y);
}

void drive_line_length_respecting_fixed_points(
    SketchFeatureParameters& parameters,
    SketchLine& line,
    double value) {
  const bool start_fixed = point_is_fixed(parameters, line.start_point_id);
  const bool end_fixed = point_is_fixed(parameters, line.end_point_id);

  if (start_fixed && end_fixed) {
    throw std::runtime_error(
        "Cannot drive a line length when both endpoints are fixed");
  }

  if (end_fixed) {
    drive_line_length_from_fixed_end(line, value);
    return;
  }

  drive_line_length(line, value);
}

struct LineEndpointRef {
  size_t line_index;
  bool is_start;
};

std::string endpoint_point_id(const SketchLine& line, bool is_start) {
  return is_start ? line.start_point_id : line.end_point_id;
}

void set_endpoint_point_id(SketchLine& line,
                           bool is_start,
                           const std::string& point_id) {
  if (is_start) {
    line.start_point_id = point_id;
    return;
  }

  line.end_point_id = point_id;
}

double endpoint_x(const SketchLine& line, bool is_start) {
  return is_start ? line.start_x : line.end_x;
}

double endpoint_y(const SketchLine& line, bool is_start) {
  return is_start ? line.start_y : line.end_y;
}

std::optional<std::tuple<std::string, double, double>> find_coincident_endpoint(
    const SketchFeatureParameters& parameters,
    const std::string& ignored_line_id,
    double x,
    double y) {
  for (const auto& candidate : parameters.lines) {
    if (candidate.id == ignored_line_id) {
      continue;
    }

    if (points_match(candidate.start_x, candidate.start_y, x, y)) {
      return std::tuple<std::string, double, double>{
          candidate.start_point_id,
          candidate.start_x,
          candidate.start_y,
      };
    }

    if (points_match(candidate.end_x, candidate.end_y, x, y)) {
      return std::tuple<std::string, double, double>{
          candidate.end_point_id,
          candidate.end_x,
          candidate.end_y,
      };
    }
  }

  return std::nullopt;
}

std::optional<std::tuple<double, double>> find_point_position(
    const SketchFeatureParameters& parameters,
    const std::string& point_id) {
  const auto point_it = std::find_if(
      parameters.points.begin(),
      parameters.points.end(),
      [&](const SketchPoint& point) { return point.id == point_id; });
  if (point_it != parameters.points.end()) {
    return std::tuple<double, double>{point_it->x, point_it->y};
  }

  for (const auto& line : parameters.lines) {
    if (line.start_point_id == point_id) {
      return std::tuple<double, double>{line.start_x, line.start_y};
    }

    if (line.end_point_id == point_id) {
      return std::tuple<double, double>{line.end_x, line.end_y};
    }
  }

  return std::nullopt;
}

void rebuild_sketch_points(SketchFeatureParameters& parameters) {
  parameters.points.clear();

  const auto append_point = [&](const std::string& point_id,
                                const std::string& kind,
                                double x,
                                double y) {
    const auto existing_it = std::find_if(
        parameters.points.begin(),
        parameters.points.end(),
        [&](const SketchPoint& point) { return point.id == point_id; });
    if (existing_it != parameters.points.end()) {
      return;
    }

    parameters.points.push_back(SketchPoint{
        .id = point_id,
        .kind = kind,
        .x = x,
        .y = y,
        .is_fixed = false,
    });
  };

  for (const auto& line : parameters.lines) {
    append_point(line.start_point_id, "endpoint", line.start_x, line.start_y);
    append_point(line.end_point_id, "endpoint", line.end_x, line.end_y);
  }

  for (const auto& circle : parameters.circles) {
    append_point(
        "point-circle-" + circle.id + "-center", "center", circle.center_x, circle.center_y);
  }
}

void sync_fixed_point_flags(SketchFeatureParameters& parameters,
                            const std::vector<SketchPoint>& previous_points) {
  for (auto& point : parameters.points) {
    const auto previous_it = std::find_if(
        previous_points.begin(),
        previous_points.end(),
        [&](const SketchPoint& previous) { return previous.id == point.id; });
    if (previous_it != previous_points.end()) {
      point.is_fixed = previous_it->is_fixed;
    }
  }
}

void rebuild_sketch_profiles(SketchFeatureParameters& parameters) {
  parameters.profiles = build_sketch_profile_regions(parameters);
}

void refresh_sketch_derived_state(FeatureEntry& feature) {
  if (!feature.sketch_parameters.has_value()) {
    return;
  }

  const std::vector<SketchPoint> previous_points = feature.sketch_parameters->points;
  rebuild_sketch_points(*feature.sketch_parameters);
  sync_fixed_point_flags(*feature.sketch_parameters, previous_points);
  rebuild_sketch_profiles(*feature.sketch_parameters);
  feature.parameters_summary = make_parameters_summary(feature.sketch_parameters.value());
}

std::vector<std::string> collect_line_ids_for_point(
    const SketchFeatureParameters& parameters,
    const std::string& point_id) {
  std::vector<std::string> line_ids;

  for (const auto& line : parameters.lines) {
    if (line.start_point_id == point_id || line.end_point_id == point_id) {
      line_ids.push_back(line.id);
    }
  }

  return line_ids;
}

void replace_point_id_references(SketchFeatureParameters& parameters,
                                 const std::string& from_point_id,
                                 const std::string& to_point_id) {
  if (from_point_id == to_point_id) {
    return;
  }

  const bool merged_fixed =
      point_is_fixed(parameters, from_point_id) || point_is_fixed(parameters, to_point_id);

  for (auto& line : parameters.lines) {
    if (line.start_point_id == from_point_id) {
      line.start_point_id = to_point_id;
    }
    if (line.end_point_id == from_point_id) {
      line.end_point_id = to_point_id;
    }
  }

  if (SketchPoint* target_point = find_sketch_point(parameters, to_point_id)) {
    target_point->is_fixed = merged_fixed;
  }
}

void set_endpoint(SketchLine& line, bool is_start, double x, double y) {
  if (is_start) {
    line.start_x = x;
    line.start_y = y;
    return;
  }

  line.end_x = x;
  line.end_y = y;
}

void set_endpoint_with_constraint(SketchLine& line,
                                  bool is_start,
                                  double x,
                                  double y) {
  set_endpoint(line, is_start, x, y);

  if (line.constraint == "horizontal") {
    if (is_start) {
      line.end_y = y;
    } else {
      line.start_y = y;
    }
    return;
  }

  if (line.constraint == "vertical") {
    if (is_start) {
      line.end_x = x;
    } else {
      line.start_x = x;
    }
  }
}

void restore_fixed_line_endpoints(SketchFeatureParameters& parameters,
                                  SketchLine& line,
                                  double previous_start_x,
                                  double previous_start_y,
                                  double previous_end_x,
                                  double previous_end_y) {
  if (point_is_fixed(parameters, line.start_point_id)) {
    line.start_x = previous_start_x;
    line.start_y = previous_start_y;
  }

  if (point_is_fixed(parameters, line.end_point_id)) {
    line.end_x = previous_end_x;
    line.end_y = previous_end_y;
  }
}

void snap_line_endpoints_to_coincident_geometry(
    SketchFeatureParameters& parameters,
    SketchLine& line) {
  const auto snapped_start =
      find_coincident_endpoint(parameters, line.id, line.start_x, line.start_y);
  if (snapped_start.has_value() &&
      !point_is_fixed(parameters, line.start_point_id)) {
    const bool can_snap_start =
        !((line.constraint == "horizontal" &&
           point_is_fixed(parameters, line.end_point_id) &&
           !nearly_equal(std::get<2>(snapped_start.value()), line.end_y)) ||
          (line.constraint == "vertical" &&
           point_is_fixed(parameters, line.end_point_id) &&
           !nearly_equal(std::get<1>(snapped_start.value()), line.end_x)));
    if (can_snap_start) {
      const std::string& snapped_point_id = std::get<0>(snapped_start.value());
      const std::string current_point_id = line.start_point_id;
      if (!current_point_id.empty() && current_point_id != snapped_point_id) {
        replace_point_id_references(parameters, current_point_id, snapped_point_id);
      }
      line.start_point_id = snapped_point_id;
      set_endpoint_with_constraint(line,
                                   true,
                                   std::get<1>(snapped_start.value()),
                                   std::get<2>(snapped_start.value()));
    }
  }

  const auto snapped_end =
      find_coincident_endpoint(parameters, line.id, line.end_x, line.end_y);
  if (snapped_end.has_value() &&
      !point_is_fixed(parameters, line.end_point_id)) {
    const bool can_snap_end =
        !((line.constraint == "horizontal" &&
           point_is_fixed(parameters, line.start_point_id) &&
           !nearly_equal(std::get<2>(snapped_end.value()), line.start_y)) ||
          (line.constraint == "vertical" &&
           point_is_fixed(parameters, line.start_point_id) &&
           !nearly_equal(std::get<1>(snapped_end.value()), line.start_x)));
    if (can_snap_end) {
      const std::string& snapped_point_id = std::get<0>(snapped_end.value());
      const std::string current_point_id = line.end_point_id;
      if (!current_point_id.empty() && current_point_id != snapped_point_id) {
        replace_point_id_references(parameters, current_point_id, snapped_point_id);
      }
      line.end_point_id = snapped_point_id;
      set_endpoint_with_constraint(line,
                                   false,
                                   std::get<1>(snapped_end.value()),
                                   std::get<2>(snapped_end.value()));
    }
  }
}

void propagate_connected_point_move(SketchFeatureParameters& parameters,
                                    const std::string& point_id,
                                    double target_x,
                                    double target_y) {
  struct PendingMove {
    std::string point_id;
    double to_x;
    double to_y;
  };

  std::deque<PendingMove> frontier = {{
      .point_id = point_id,
      .to_x = target_x,
      .to_y = target_y,
  }};
  std::vector<std::string> visited_points;

  while (!frontier.empty()) {
    const auto move = frontier.front();
    frontier.pop_front();

    const auto current_point_position = find_point_position(parameters, move.point_id);
    if (point_is_fixed(parameters, move.point_id) &&
        current_point_position.has_value() &&
        !points_match(std::get<0>(current_point_position.value()),
                      std::get<1>(current_point_position.value()),
                      move.to_x,
                      move.to_y)) {
      continue;
    }

    const bool already_visited = std::any_of(
        visited_points.begin(),
        visited_points.end(),
        [&](const auto& visited) { return visited == move.point_id; });
    if (already_visited) {
      continue;
    }
    visited_points.push_back(move.point_id);

    std::vector<LineEndpointRef> connected_endpoints;
    for (size_t line_index = 0; line_index < parameters.lines.size(); ++line_index) {
      const auto& line = parameters.lines[line_index];
      if (line.start_point_id == move.point_id) {
        connected_endpoints.push_back({
            .line_index = line_index,
            .is_start = true,
        });
      }
      if (line.end_point_id == move.point_id) {
        connected_endpoints.push_back({
            .line_index = line_index,
            .is_start = false,
        });
      }
    }

    for (const auto& endpoint_ref : connected_endpoints) {
      auto& line = parameters.lines[endpoint_ref.line_index];
      const bool moved_start = endpoint_ref.is_start;
      const bool moved_end = !endpoint_ref.is_start;
      const double previous_other_x =
          moved_start ? line.end_x : line.start_x;
      const double previous_other_y =
          moved_start ? line.end_y : line.start_y;
      const double previous_length = measure_line_length(line);
      const double direction_sign =
          line.constraint == "horizontal"
              ? ((line.end_x - line.start_x) >= 0.0 ? 1.0 : -1.0)
              : line.constraint == "vertical"
                    ? ((line.end_y - line.start_y) >= 0.0 ? 1.0 : -1.0)
                    : 0.0;

      set_endpoint(line, endpoint_ref.is_start, move.to_x, move.to_y);

      if (line.constraint == "horizontal") {
        if (moved_start) {
          line.end_y = move.to_y;
          line.end_x = move.to_x + direction_sign * previous_length;
        } else if (moved_end) {
          line.start_y = move.to_y;
          line.start_x = move.to_x - direction_sign * previous_length;
        }
      } else if (line.constraint == "vertical") {
        if (moved_start) {
          line.end_x = move.to_x;
          line.end_y = move.to_y + direction_sign * previous_length;
        } else if (moved_end) {
          line.start_x = move.to_x;
          line.start_y = move.to_y - direction_sign * previous_length;
        }
      }

      validate_line(line.start_x, line.start_y, line.end_x, line.end_y);

      const double next_other_x = moved_start ? line.end_x : line.start_x;
      const double next_other_y = moved_start ? line.end_y : line.start_y;
      if (!points_match(previous_other_x,
                        previous_other_y,
                        next_other_x,
                        next_other_y)) {
        frontier.push_back({
            .point_id = endpoint_point_id(line, !endpoint_ref.is_start),
            .to_x = next_other_x,
            .to_y = next_other_y,
        });
      }
    }
  }
}

void sync_all_line_dimensions(SketchFeatureParameters& parameters) {
  for (const auto& line : parameters.lines) {
    sync_line_dimension(parameters, line);
  }
}

void remove_line_relations_for_line(SketchFeatureParameters& parameters,
                                    const std::string& kind,
                                    const std::string& line_id) {
  parameters.line_relations.erase(
      std::remove_if(parameters.line_relations.begin(),
                     parameters.line_relations.end(),
                     [&](const SketchLineRelation& relation) {
                       return relation.kind == kind &&
                              (relation.first_line_id == line_id ||
                               relation.second_line_id == line_id);
                     }),
      parameters.line_relations.end());
}

void enforce_equal_length_relations(SketchFeatureParameters& parameters,
                                    const std::string& seed_line_id) {
  std::deque<std::string> frontier = {seed_line_id};
  std::vector<std::string> visited_line_ids;

  while (!frontier.empty()) {
    const std::string current_line_id = frontier.front();
    frontier.pop_front();

    if (std::find(visited_line_ids.begin(),
                  visited_line_ids.end(),
                  current_line_id) != visited_line_ids.end()) {
      continue;
    }
    visited_line_ids.push_back(current_line_id);

    auto& reference_line = require_line(parameters, current_line_id);
    const double target_length = measure_line_length(reference_line);

    for (const auto& relation : parameters.line_relations) {
      if (relation.kind != "equal_length") {
        continue;
      }

      const bool matches_first = relation.first_line_id == current_line_id;
      const bool matches_second = relation.second_line_id == current_line_id;
      if (!matches_first && !matches_second) {
        continue;
      }

      const std::string other_line_id =
          matches_first ? relation.second_line_id : relation.first_line_id;
      auto& driven_line = require_line(parameters, other_line_id);
      const double previous_start_x = driven_line.start_x;
      const double previous_start_y = driven_line.start_y;
      const double previous_end_x = driven_line.end_x;
      const double previous_end_y = driven_line.end_y;

      drive_line_length_respecting_fixed_points(
          parameters, driven_line, target_length);
      snap_line_endpoints_to_coincident_geometry(parameters, driven_line);
      validate_line(driven_line.start_x,
                    driven_line.start_y,
                    driven_line.end_x,
                    driven_line.end_y);

      if (!points_match(previous_start_x,
                        previous_start_y,
                        driven_line.start_x,
                        driven_line.start_y)) {
        propagate_connected_point_move(parameters,
                                       driven_line.start_point_id,
                                       driven_line.start_x,
                                       driven_line.start_y);
      }

      if (!points_match(previous_end_x,
                        previous_end_y,
                        driven_line.end_x,
                        driven_line.end_y)) {
        propagate_connected_point_move(parameters,
                                       driven_line.end_point_id,
                                       driven_line.end_x,
                                       driven_line.end_y);
      }

      sync_line_dimension(parameters, driven_line);
      frontier.push_back(other_line_id);
    }
  }
}

void drive_line_perpendicular_to_reference(SketchLine& driven_line,
                                           const SketchLine& reference_line,
                                           const SketchFeatureParameters& parameters) {
  if (driven_line.constraint.has_value() || reference_line.constraint.has_value()) {
    throw std::runtime_error(
        "Perpendicular relations do not support horizontal or vertical constrained lines");
  }

  const double reference_dx = reference_line.end_x - reference_line.start_x;
  const double reference_dy = reference_line.end_y - reference_line.start_y;
  const double reference_length =
      std::sqrt(reference_dx * reference_dx + reference_dy * reference_dy);
  const double driven_length = measure_line_length(driven_line);

  if (reference_length <= kMinimumSketchDimensionValue ||
      driven_length <= kMinimumSketchDimensionValue) {
    throw std::runtime_error("Perpendicular relations require non-zero length lines");
  }

  const double current_dx = driven_line.end_x - driven_line.start_x;
  const double current_dy = driven_line.end_y - driven_line.start_y;
  const double candidate_a_x = -reference_dy / reference_length;
  const double candidate_a_y = reference_dx / reference_length;
  const double candidate_b_x = -candidate_a_x;
  const double candidate_b_y = -candidate_a_y;
  const double dot_a = current_dx * candidate_a_x + current_dy * candidate_a_y;
  const double dot_b = current_dx * candidate_b_x + current_dy * candidate_b_y;
  const double direction_x = dot_a >= dot_b ? candidate_a_x : candidate_b_x;
  const double direction_y = dot_a >= dot_b ? candidate_a_y : candidate_b_y;

  const bool start_fixed = point_is_fixed(parameters, driven_line.start_point_id);
  const bool end_fixed = point_is_fixed(parameters, driven_line.end_point_id);
  if (start_fixed && end_fixed) {
    throw std::runtime_error(
        "Cannot drive a perpendicular relation when both endpoints are fixed");
  }

  if (end_fixed) {
    driven_line.start_x = driven_line.end_x - direction_x * driven_length;
    driven_line.start_y = driven_line.end_y - direction_y * driven_length;
  } else {
    driven_line.end_x = driven_line.start_x + direction_x * driven_length;
    driven_line.end_y = driven_line.start_y + direction_y * driven_length;
  }
  validate_line(
      driven_line.start_x, driven_line.start_y, driven_line.end_x, driven_line.end_y);
}

void drive_line_parallel_to_reference(SketchLine& driven_line,
                                      const SketchLine& reference_line,
                                      const SketchFeatureParameters& parameters) {
  if (driven_line.constraint.has_value() || reference_line.constraint.has_value()) {
    throw std::runtime_error(
        "Parallel relations do not support horizontal or vertical constrained lines");
  }

  const double reference_dx = reference_line.end_x - reference_line.start_x;
  const double reference_dy = reference_line.end_y - reference_line.start_y;
  const double reference_length =
      std::sqrt(reference_dx * reference_dx + reference_dy * reference_dy);
  const double driven_length = measure_line_length(driven_line);

  if (reference_length <= kMinimumSketchDimensionValue ||
      driven_length <= kMinimumSketchDimensionValue) {
    throw std::runtime_error("Parallel relations require non-zero length lines");
  }

  const double current_dx = driven_line.end_x - driven_line.start_x;
  const double current_dy = driven_line.end_y - driven_line.start_y;
  const double candidate_a_x = reference_dx / reference_length;
  const double candidate_a_y = reference_dy / reference_length;
  const double candidate_b_x = -candidate_a_x;
  const double candidate_b_y = -candidate_a_y;
  const double dot_a = current_dx * candidate_a_x + current_dy * candidate_a_y;
  const double dot_b = current_dx * candidate_b_x + current_dy * candidate_b_y;
  const double direction_x = dot_a >= dot_b ? candidate_a_x : candidate_b_x;
  const double direction_y = dot_a >= dot_b ? candidate_a_y : candidate_b_y;

  const bool start_fixed = point_is_fixed(parameters, driven_line.start_point_id);
  const bool end_fixed = point_is_fixed(parameters, driven_line.end_point_id);
  if (start_fixed && end_fixed) {
    throw std::runtime_error(
        "Cannot drive a parallel relation when both endpoints are fixed");
  }

  if (end_fixed) {
    driven_line.start_x = driven_line.end_x - direction_x * driven_length;
    driven_line.start_y = driven_line.end_y - direction_y * driven_length;
  } else {
    driven_line.end_x = driven_line.start_x + direction_x * driven_length;
    driven_line.end_y = driven_line.start_y + direction_y * driven_length;
  }
  validate_line(
      driven_line.start_x, driven_line.start_y, driven_line.end_x, driven_line.end_y);
}

void enforce_perpendicular_relations(SketchFeatureParameters& parameters,
                                     const std::string& seed_line_id) {
  std::deque<std::string> frontier = {seed_line_id};
  std::vector<std::string> visited_line_ids;

  while (!frontier.empty()) {
    const std::string current_line_id = frontier.front();
    frontier.pop_front();

    if (std::find(visited_line_ids.begin(),
                  visited_line_ids.end(),
                  current_line_id) != visited_line_ids.end()) {
      continue;
    }
    visited_line_ids.push_back(current_line_id);

    auto& reference_line = require_line(parameters, current_line_id);

    for (const auto& relation : parameters.line_relations) {
      if (relation.kind != "perpendicular") {
        continue;
      }

      const bool matches_first = relation.first_line_id == current_line_id;
      const bool matches_second = relation.second_line_id == current_line_id;
      if (!matches_first && !matches_second) {
        continue;
      }

      const std::string other_line_id =
          matches_first ? relation.second_line_id : relation.first_line_id;
      auto& driven_line = require_line(parameters, other_line_id);
      const double previous_start_x = driven_line.start_x;
      const double previous_start_y = driven_line.start_y;
      const double previous_end_x = driven_line.end_x;
      const double previous_end_y = driven_line.end_y;

      drive_line_perpendicular_to_reference(
          driven_line, reference_line, parameters);
      snap_line_endpoints_to_coincident_geometry(parameters, driven_line);
      validate_line(driven_line.start_x,
                    driven_line.start_y,
                    driven_line.end_x,
                    driven_line.end_y);

      if (!points_match(previous_start_x,
                        previous_start_y,
                        driven_line.start_x,
                        driven_line.start_y)) {
        propagate_connected_point_move(parameters,
                                       driven_line.start_point_id,
                                       driven_line.start_x,
                                       driven_line.start_y);
      }

      if (!points_match(previous_end_x,
                        previous_end_y,
                        driven_line.end_x,
                        driven_line.end_y)) {
        propagate_connected_point_move(parameters,
                                       driven_line.end_point_id,
                                       driven_line.end_x,
                                       driven_line.end_y);
      }

      sync_line_dimension(parameters, driven_line);
      frontier.push_back(other_line_id);
    }
  }
}

void enforce_parallel_relations(SketchFeatureParameters& parameters,
                                const std::string& seed_line_id) {
  std::deque<std::string> frontier = {seed_line_id};
  std::vector<std::string> visited_line_ids;

  while (!frontier.empty()) {
    const std::string current_line_id = frontier.front();
    frontier.pop_front();

    if (std::find(visited_line_ids.begin(),
                  visited_line_ids.end(),
                  current_line_id) != visited_line_ids.end()) {
      continue;
    }
    visited_line_ids.push_back(current_line_id);

    auto& reference_line = require_line(parameters, current_line_id);

    for (const auto& relation : parameters.line_relations) {
      if (relation.kind != "parallel") {
        continue;
      }

      const bool matches_first = relation.first_line_id == current_line_id;
      const bool matches_second = relation.second_line_id == current_line_id;
      if (!matches_first && !matches_second) {
        continue;
      }

      const std::string other_line_id =
          matches_first ? relation.second_line_id : relation.first_line_id;
      auto& driven_line = require_line(parameters, other_line_id);
      const double previous_start_x = driven_line.start_x;
      const double previous_start_y = driven_line.start_y;
      const double previous_end_x = driven_line.end_x;
      const double previous_end_y = driven_line.end_y;

      drive_line_parallel_to_reference(driven_line, reference_line, parameters);
      snap_line_endpoints_to_coincident_geometry(parameters, driven_line);
      validate_line(driven_line.start_x,
                    driven_line.start_y,
                    driven_line.end_x,
                    driven_line.end_y);

      if (!points_match(previous_start_x,
                        previous_start_y,
                        driven_line.start_x,
                        driven_line.start_y)) {
        propagate_connected_point_move(parameters,
                                       driven_line.start_point_id,
                                       driven_line.start_x,
                                       driven_line.start_y);
      }

      if (!points_match(previous_end_x,
                        previous_end_y,
                        driven_line.end_x,
                        driven_line.end_y)) {
        propagate_connected_point_move(parameters,
                                       driven_line.end_point_id,
                                       driven_line.end_x,
                                       driven_line.end_y);
      }

      sync_line_dimension(parameters, driven_line);
      frontier.push_back(other_line_id);
    }
  }
}

}  // namespace

FeatureEntry create_sketch_feature(
    int feature_index,
    const std::string& plane_id,
    std::optional<SketchFeatureParameters::SketchPlaneFrame> plane_frame) {
  SketchFeatureParameters parameters{
      .plane_id = plane_id,
      .plane_frame = plane_frame,
      .active_tool = "select",
      .lines = {},
      .circles = {},
      .points = {},
      .dimensions = {},
      .line_relations = {},
      .profiles = {},
  };

  FeatureEntry feature{
      .id = "feature-" + std::to_string(feature_index),
      .kind = "sketch",
      .name = "Sketch",
      .status = "editing",
      .parameters_summary = make_parameters_summary(parameters),
      .box_parameters = std::nullopt,
      .cylinder_parameters = std::nullopt,
      .extrude_parameters = std::nullopt,
      .sketch_parameters = parameters,
  };

  refresh_sketch_derived_state(feature);
  return feature;
}

void set_sketch_tool(FeatureEntry& feature, const std::string& tool) {
  if (feature.kind != "sketch" || !feature.sketch_parameters.has_value()) {
    throw std::runtime_error("Only sketch features can change sketch tools");
  }

  validate_tool(tool);
  feature.sketch_parameters->active_tool = tool;
}

void update_sketch_line(FeatureEntry& feature,
                        const std::string& line_id,
                        double start_x,
                        double start_y,
                        double end_x,
                        double end_y) {
  if (feature.kind != "sketch" || !feature.sketch_parameters.has_value()) {
    throw std::runtime_error("Only sketch features can update sketch lines");
  }

  const auto line_it = std::find_if(
      feature.sketch_parameters->lines.begin(),
      feature.sketch_parameters->lines.end(),
      [&](const SketchLine& line) { return line.id == line_id; });

  if (line_it == feature.sketch_parameters->lines.end()) {
    throw std::runtime_error("Sketch line not found: " + line_id);
  }

  const double previous_start_x = line_it->start_x;
  const double previous_start_y = line_it->start_y;
  const double previous_end_x = line_it->end_x;
  const double previous_end_y = line_it->end_y;

  line_it->start_x = start_x;
  line_it->start_y = start_y;
  line_it->end_x = end_x;
  line_it->end_y = end_y;
  restore_fixed_line_endpoints(*feature.sketch_parameters,
                               *line_it,
                               previous_start_x,
                               previous_start_y,
                               previous_end_x,
                               previous_end_y);
  apply_line_constraint_respecting_fixed_points(
      *feature.sketch_parameters, *line_it);
  restore_fixed_line_endpoints(*feature.sketch_parameters,
                               *line_it,
                               previous_start_x,
                               previous_start_y,
                               previous_end_x,
                               previous_end_y);
  snap_line_endpoints_to_coincident_geometry(*feature.sketch_parameters, *line_it);
  apply_line_constraint_respecting_fixed_points(
      *feature.sketch_parameters, *line_it);
  validate_line(line_it->start_x, line_it->start_y, line_it->end_x, line_it->end_y);

  if (!points_match(previous_start_x,
                    previous_start_y,
                    line_it->start_x,
                    line_it->start_y)) {
    propagate_connected_point_move(*feature.sketch_parameters,
                                   line_it->start_point_id,
                                   line_it->start_x,
                                   line_it->start_y);
  }
  if (!points_match(previous_end_x,
                    previous_end_y,
                    line_it->end_x,
                    line_it->end_y)) {
    propagate_connected_point_move(*feature.sketch_parameters,
                                   line_it->end_point_id,
                                   line_it->end_x,
                                   line_it->end_y);
  }
  sync_all_line_dimensions(*feature.sketch_parameters);
  enforce_equal_length_relations(*feature.sketch_parameters, line_it->id);
  enforce_perpendicular_relations(*feature.sketch_parameters, line_it->id);
  enforce_parallel_relations(*feature.sketch_parameters, line_it->id);
  sync_all_line_dimensions(*feature.sketch_parameters);
  refresh_sketch_derived_state(feature);
}

void update_sketch_point(FeatureEntry& feature,
                         const std::string& point_id,
                         double x,
                         double y) {
  if (feature.kind != "sketch" || !feature.sketch_parameters.has_value()) {
    throw std::runtime_error("Only sketch features can update sketch points");
  }

  auto& parameters = feature.sketch_parameters.value();
  const SketchPoint* point = find_sketch_point(parameters, point_id);
  if (point == nullptr) {
    throw std::runtime_error("Sketch point not found: " + point_id);
  }

  if (point->is_fixed && !points_match(point->x, point->y, x, y)) {
    throw std::runtime_error("Cannot move a fixed sketch point");
  }

  const std::vector<std::string> affected_line_ids =
      collect_line_ids_for_point(parameters, point_id);

  if (point->kind == "center") {
    const std::string prefix = "point-circle-";
    const std::string suffix = "-center";
    if (point_id.rfind(prefix, 0) != 0 ||
        point_id.size() <= prefix.size() + suffix.size() ||
        point_id.substr(point_id.size() - suffix.size()) != suffix) {
      throw std::runtime_error("Unsupported sketch center point: " + point_id);
    }

    const std::string circle_id =
        point_id.substr(prefix.size(),
                        point_id.size() - prefix.size() - suffix.size());
    const auto circle_it = std::find_if(
        parameters.circles.begin(),
        parameters.circles.end(),
        [&](const SketchCircle& circle) { return circle.id == circle_id; });
    if (circle_it == parameters.circles.end()) {
      throw std::runtime_error("Sketch circle not found for point: " + point_id);
    }

    circle_it->center_x = x;
    circle_it->center_y = y;
    refresh_sketch_derived_state(feature);
    return;
  }

  propagate_connected_point_move(parameters, point_id, x, y);
  sync_all_line_dimensions(parameters);

  for (const auto& line_id : affected_line_ids) {
    enforce_equal_length_relations(parameters, line_id);
    enforce_perpendicular_relations(parameters, line_id);
    enforce_parallel_relations(parameters, line_id);
  }

  sync_all_line_dimensions(parameters);
  refresh_sketch_derived_state(feature);
}

void set_sketch_line_constraint(FeatureEntry& feature,
                                const std::string& line_id,
                                const std::optional<std::string>& constraint) {
  if (feature.kind != "sketch" || !feature.sketch_parameters.has_value()) {
    throw std::runtime_error("Only sketch features can set line constraints");
  }

  validate_constraint(constraint);

  const auto line_it = std::find_if(
      feature.sketch_parameters->lines.begin(),
      feature.sketch_parameters->lines.end(),
      [&](const SketchLine& line) { return line.id == line_id; });

  if (line_it == feature.sketch_parameters->lines.end()) {
    throw std::runtime_error("Sketch line not found: " + line_id);
  }

  const double previous_start_x = line_it->start_x;
  const double previous_start_y = line_it->start_y;
  const double previous_end_x = line_it->end_x;
  const double previous_end_y = line_it->end_y;
  line_it->constraint = constraint;
  apply_line_constraint_respecting_fixed_points(
      *feature.sketch_parameters, *line_it);
  snap_line_endpoints_to_coincident_geometry(*feature.sketch_parameters, *line_it);
  apply_line_constraint_respecting_fixed_points(
      *feature.sketch_parameters, *line_it);
  validate_line(line_it->start_x, line_it->start_y, line_it->end_x, line_it->end_y);

  if (!points_match(previous_start_x,
                    previous_start_y,
                    line_it->start_x,
                    line_it->start_y)) {
    propagate_connected_point_move(*feature.sketch_parameters,
                                   line_it->start_point_id,
                                   line_it->start_x,
                                   line_it->start_y);
  }
  if (!points_match(previous_end_x,
                    previous_end_y,
                    line_it->end_x,
                    line_it->end_y)) {
    propagate_connected_point_move(*feature.sketch_parameters,
                                   line_it->end_point_id,
                                   line_it->end_x,
                                   line_it->end_y);
  }
  sync_all_line_dimensions(*feature.sketch_parameters);
  enforce_equal_length_relations(*feature.sketch_parameters, line_it->id);
  enforce_perpendicular_relations(*feature.sketch_parameters, line_it->id);
  enforce_parallel_relations(*feature.sketch_parameters, line_it->id);
  sync_all_line_dimensions(*feature.sketch_parameters);
  refresh_sketch_derived_state(feature);
}

void set_sketch_equal_length_constraint(
    FeatureEntry& feature,
    const std::string& line_id,
    const std::optional<std::string>& other_line_id) {
  if (feature.kind != "sketch" || !feature.sketch_parameters.has_value()) {
    throw std::runtime_error("Only sketch features can set equal-length constraints");
  }

  auto& parameters = feature.sketch_parameters.value();
  auto& line = require_line(parameters, line_id);

  remove_line_relations_for_line(parameters, "equal_length", line_id);

  if (!other_line_id.has_value()) {
    refresh_sketch_derived_state(feature);
    return;
  }

  if (other_line_id.value() == line_id) {
    throw std::runtime_error("A sketch line cannot be equal-length to itself");
  }

  auto& other_line = require_line(parameters, other_line_id.value());
  parameters.line_relations.push_back(SketchLineRelation{
      .id = "rel-equal-length-" + line_id,
      .kind = "equal_length",
      .first_line_id = line_id,
      .second_line_id = other_line_id.value(),
  });

  const double target_length = measure_line_length(other_line);
  const double previous_end_x = line.end_x;
  const double previous_end_y = line.end_y;
  drive_line_length_respecting_fixed_points(parameters, line, target_length);
  snap_line_endpoints_to_coincident_geometry(parameters, line);
  validate_line(line.start_x, line.start_y, line.end_x, line.end_y);

  if (!points_match(previous_end_x, previous_end_y, line.end_x, line.end_y)) {
    propagate_connected_point_move(
        parameters, line.end_point_id, line.end_x, line.end_y);
  }

  sync_all_line_dimensions(parameters);
  enforce_equal_length_relations(parameters, line_id);
  enforce_perpendicular_relations(parameters, line_id);
  enforce_parallel_relations(parameters, line_id);
  sync_all_line_dimensions(parameters);
  refresh_sketch_derived_state(feature);
}

void set_sketch_perpendicular_constraint(
    FeatureEntry& feature,
    const std::string& line_id,
    const std::optional<std::string>& other_line_id) {
  if (feature.kind != "sketch" || !feature.sketch_parameters.has_value()) {
    throw std::runtime_error("Only sketch features can set perpendicular constraints");
  }

  auto& parameters = feature.sketch_parameters.value();
  auto& line = require_line(parameters, line_id);

  remove_line_relations_for_line(parameters, "perpendicular", line_id);

  if (!other_line_id.has_value()) {
    refresh_sketch_derived_state(feature);
    return;
  }

  if (other_line_id.value() == line_id) {
    throw std::runtime_error("A sketch line cannot be perpendicular to itself");
  }

  auto& other_line = require_line(parameters, other_line_id.value());
  if (line.constraint.has_value() || other_line.constraint.has_value()) {
    throw std::runtime_error(
        "Perpendicular relations require both lines to have no direct axis constraint");
  }

  parameters.line_relations.push_back(SketchLineRelation{
      .id = "rel-perpendicular-" + line_id,
      .kind = "perpendicular",
      .first_line_id = line_id,
      .second_line_id = other_line_id.value(),
  });

  const double previous_start_x = line.start_x;
  const double previous_start_y = line.start_y;
  const double previous_end_x = line.end_x;
  const double previous_end_y = line.end_y;
  drive_line_perpendicular_to_reference(line, other_line, parameters);
  snap_line_endpoints_to_coincident_geometry(parameters, line);
  validate_line(line.start_x, line.start_y, line.end_x, line.end_y);

  if (!points_match(previous_start_x, previous_start_y, line.start_x, line.start_y)) {
    propagate_connected_point_move(
        parameters, line.start_point_id, line.start_x, line.start_y);
  }

  if (!points_match(previous_end_x, previous_end_y, line.end_x, line.end_y)) {
    propagate_connected_point_move(
        parameters, line.end_point_id, line.end_x, line.end_y);
  }

  sync_all_line_dimensions(parameters);
  enforce_equal_length_relations(parameters, line_id);
  enforce_perpendicular_relations(parameters, line_id);
  enforce_parallel_relations(parameters, line_id);
  sync_all_line_dimensions(parameters);
  refresh_sketch_derived_state(feature);
}

void set_sketch_parallel_constraint(
    FeatureEntry& feature,
    const std::string& line_id,
    const std::optional<std::string>& other_line_id) {
  if (feature.kind != "sketch" || !feature.sketch_parameters.has_value()) {
    throw std::runtime_error("Only sketch features can set parallel constraints");
  }

  auto& parameters = feature.sketch_parameters.value();
  auto& line = require_line(parameters, line_id);

  remove_line_relations_for_line(parameters, "parallel", line_id);

  if (!other_line_id.has_value()) {
    refresh_sketch_derived_state(feature);
    return;
  }

  if (other_line_id.value() == line_id) {
    throw std::runtime_error("A sketch line cannot be parallel to itself");
  }

  auto& other_line = require_line(parameters, other_line_id.value());
  if (line.constraint.has_value() || other_line.constraint.has_value()) {
    throw std::runtime_error(
        "Parallel relations require both lines to have no direct axis constraint");
  }

  parameters.line_relations.push_back(SketchLineRelation{
      .id = "rel-parallel-" + line_id,
      .kind = "parallel",
      .first_line_id = line_id,
      .second_line_id = other_line_id.value(),
  });

  const double previous_start_x = line.start_x;
  const double previous_start_y = line.start_y;
  const double previous_end_x = line.end_x;
  const double previous_end_y = line.end_y;
  drive_line_parallel_to_reference(line, other_line, parameters);
  snap_line_endpoints_to_coincident_geometry(parameters, line);
  validate_line(line.start_x, line.start_y, line.end_x, line.end_y);

  if (!points_match(previous_start_x, previous_start_y, line.start_x, line.start_y)) {
    propagate_connected_point_move(
        parameters, line.start_point_id, line.start_x, line.start_y);
  }

  if (!points_match(previous_end_x, previous_end_y, line.end_x, line.end_y)) {
    propagate_connected_point_move(
        parameters, line.end_point_id, line.end_x, line.end_y);
  }

  sync_all_line_dimensions(parameters);
  enforce_equal_length_relations(parameters, line_id);
  enforce_perpendicular_relations(parameters, line_id);
  enforce_parallel_relations(parameters, line_id);
  sync_all_line_dimensions(parameters);
  refresh_sketch_derived_state(feature);
}

void set_sketch_coincident_constraint(FeatureEntry& feature,
                                      const std::string& point_id,
                                      const std::string& other_point_id) {
  if (feature.kind != "sketch" || !feature.sketch_parameters.has_value()) {
    throw std::runtime_error("Only sketch features can set coincident constraints");
  }

  auto& parameters = feature.sketch_parameters.value();

  if (point_id == other_point_id) {
    return;
  }

  if (!find_point_position(parameters, point_id).has_value()) {
    throw std::runtime_error("Sketch point not found: " + point_id);
  }

  const auto other_point_position = find_point_position(parameters, other_point_id);
  if (!other_point_position.has_value()) {
    throw std::runtime_error("Sketch point not found: " + other_point_id);
  }

  for (const auto& line : parameters.lines) {
    const bool spans_points =
        (line.start_point_id == point_id && line.end_point_id == other_point_id) ||
        (line.start_point_id == other_point_id && line.end_point_id == point_id);
    if (spans_points) {
      throw std::runtime_error(
          "Cannot make both endpoints of a sketch line coincident");
    }
  }

  auto affected_line_ids = collect_line_ids_for_point(parameters, point_id);
  const auto other_line_ids =
      collect_line_ids_for_point(parameters, other_point_id);
  for (const auto& line_id : other_line_ids) {
    if (std::find(affected_line_ids.begin(), affected_line_ids.end(), line_id) ==
        affected_line_ids.end()) {
      affected_line_ids.push_back(line_id);
    }
  }

  propagate_connected_point_move(parameters,
                                 point_id,
                                 std::get<0>(other_point_position.value()),
                                 std::get<1>(other_point_position.value()));
  replace_point_id_references(parameters, point_id, other_point_id);
  sync_all_line_dimensions(parameters);

  for (const auto& line_id : affected_line_ids) {
    enforce_equal_length_relations(parameters, line_id);
    enforce_perpendicular_relations(parameters, line_id);
    enforce_parallel_relations(parameters, line_id);
  }

  sync_all_line_dimensions(parameters);
  refresh_sketch_derived_state(feature);
}

void set_sketch_point_fixed(FeatureEntry& feature,
                            const std::string& point_id,
                            bool is_fixed) {
  if (feature.kind != "sketch" || !feature.sketch_parameters.has_value()) {
    throw std::runtime_error("Only sketch features can set fixed point state");
  }

  auto& parameters = feature.sketch_parameters.value();
  SketchPoint* point = find_sketch_point(parameters, point_id);
  if (point == nullptr) {
    throw std::runtime_error("Sketch point not found: " + point_id);
  }

  point->is_fixed = is_fixed;
  refresh_sketch_derived_state(feature);
}

void update_sketch_circle(FeatureEntry& feature,
                          const std::string& circle_id,
                          double center_x,
                          double center_y,
                          double radius) {
  if (feature.kind != "sketch" || !feature.sketch_parameters.has_value()) {
    throw std::runtime_error("Only sketch features can update sketch circles");
  }

  if (radius <= kMinimumSketchDimensionValue) {
    throw std::runtime_error("Sketch circles must have non-zero radius");
  }

  const auto circle_it = std::find_if(
      feature.sketch_parameters->circles.begin(),
      feature.sketch_parameters->circles.end(),
      [&](const SketchCircle& circle) { return circle.id == circle_id; });

  if (circle_it == feature.sketch_parameters->circles.end()) {
    throw std::runtime_error("Sketch circle not found: " + circle_id);
  }

  const std::string center_point_id = "point-circle-" + circle_it->id + "-center";
  if (!point_is_fixed(*feature.sketch_parameters, center_point_id)) {
    circle_it->center_x = center_x;
    circle_it->center_y = center_y;
  }
  circle_it->radius = radius;
  sync_circle_dimension(*feature.sketch_parameters, *circle_it);
  refresh_sketch_derived_state(feature);
}

void update_sketch_dimension(FeatureEntry& feature,
                             const std::string& dimension_id,
                             double value) {
  if (feature.kind != "sketch" || !feature.sketch_parameters.has_value()) {
    throw std::runtime_error("Only sketch features can update sketch dimensions");
  }

  if (value <= kMinimumSketchDimensionValue) {
    throw std::runtime_error("Sketch dimensions must be greater than zero");
  }

  auto& parameters = feature.sketch_parameters.value();
  auto& dimension = require_dimension(parameters, dimension_id);

  if (dimension.kind == "line_length") {
    const auto line_it = std::find_if(
        parameters.lines.begin(),
        parameters.lines.end(),
        [&](const SketchLine& line) { return line.id == dimension.entity_id; });

    if (line_it == parameters.lines.end()) {
      throw std::runtime_error("Sketch line not found for dimension: " + dimension_id);
    }

    const double previous_start_x = line_it->start_x;
    const double previous_start_y = line_it->start_y;
    const double previous_end_x = line_it->end_x;
    const double previous_end_y = line_it->end_y;

    drive_line_length_respecting_fixed_points(parameters, *line_it, value);
    snap_line_endpoints_to_coincident_geometry(parameters, *line_it);
    if (!points_match(previous_start_x,
                      previous_start_y,
                      line_it->start_x,
                      line_it->start_y)) {
      propagate_connected_point_move(parameters,
                                     line_it->start_point_id,
                                     line_it->start_x,
                                     line_it->start_y);
    }
    if (!points_match(previous_end_x,
                      previous_end_y,
                      line_it->end_x,
                      line_it->end_y)) {
      propagate_connected_point_move(parameters,
                                     line_it->end_point_id,
                                     line_it->end_x,
                                     line_it->end_y);
    }
    sync_all_line_dimensions(parameters);
    enforce_equal_length_relations(parameters, line_it->id);
    enforce_perpendicular_relations(parameters, line_it->id);
    enforce_parallel_relations(parameters, line_it->id);
    sync_all_line_dimensions(parameters);
    refresh_sketch_derived_state(feature);
    return;
  }

  if (dimension.kind == "circle_radius") {
    const auto circle_it = std::find_if(
        parameters.circles.begin(),
        parameters.circles.end(),
        [&](const SketchCircle& circle) {
          return circle.id == dimension.entity_id;
        });

    if (circle_it == parameters.circles.end()) {
      throw std::runtime_error("Sketch circle not found for dimension: " + dimension_id);
    }

    circle_it->radius = value;
    sync_circle_dimension(parameters, *circle_it);
    refresh_sketch_derived_state(feature);
    return;
  }

  throw std::runtime_error("Unsupported sketch dimension kind: " + dimension.kind);
}

void add_sketch_line(FeatureEntry& feature,
                     int line_index,
                     double start_x,
                     double start_y,
                     double end_x,
                     double end_y) {
  if (feature.kind != "sketch" || !feature.sketch_parameters.has_value()) {
    throw std::runtime_error("Only sketch features can accept sketch lines");
  }

  validate_line(start_x, start_y, end_x, end_y);

  const auto shared_start_point =
      find_coincident_endpoint(*feature.sketch_parameters, "", start_x, start_y);
  const auto shared_end_point =
      find_coincident_endpoint(*feature.sketch_parameters, "", end_x, end_y);

  feature.sketch_parameters->lines.push_back(SketchLine{
      .id = "line-" + std::to_string(line_index),
      .start_point_id = shared_start_point.has_value()
                            ? std::get<0>(shared_start_point.value())
                            : "point-line-" + std::to_string(line_index) + "-start",
      .end_point_id = shared_end_point.has_value()
                          ? std::get<0>(shared_end_point.value())
                          : "point-line-" + std::to_string(line_index) + "-end",
      .start_x = start_x,
      .start_y = start_y,
      .end_x = end_x,
      .end_y = end_y,
      .constraint = infer_constraint_hint(start_x, start_y, end_x, end_y),
  });
  auto& line = feature.sketch_parameters->lines.back();
  apply_line_constraint(line);
  snap_line_endpoints_to_coincident_geometry(*feature.sketch_parameters, line);
  validate_line(line.start_x, line.start_y, line.end_x, line.end_y);
  feature.sketch_parameters->dimensions.push_back(SketchDimension{
      .id = "dim-line-" + line.id,
      .kind = "line_length",
      .entity_id = line.id,
      .value = measure_line_length(line),
  });
  refresh_sketch_derived_state(feature);
}

void add_sketch_rectangle(FeatureEntry& feature,
                          int& next_line_index,
                          double start_x,
                          double start_y,
                          double end_x,
                          double end_y) {
  add_sketch_line(feature, next_line_index++, start_x, start_y, end_x, start_y);
  add_sketch_line(feature, next_line_index++, end_x, start_y, end_x, end_y);
  add_sketch_line(feature, next_line_index++, end_x, end_y, start_x, end_y);
  add_sketch_line(feature, next_line_index++, start_x, end_y, start_x, start_y);
}

void add_sketch_circle(FeatureEntry& feature,
                       int circle_index,
                       double center_x,
                       double center_y,
                       double radius) {
  if (feature.kind != "sketch" || !feature.sketch_parameters.has_value()) {
    throw std::runtime_error("Only sketch features can accept sketch circles");
  }

  if (radius <= kMinimumSketchDimensionValue) {
    throw std::runtime_error("Sketch circles must have non-zero radius");
  }

  feature.sketch_parameters->circles.push_back(SketchCircle{
      .id = "circle-" + std::to_string(circle_index),
      .center_x = center_x,
      .center_y = center_y,
      .radius = radius,
  });
  const auto& circle = feature.sketch_parameters->circles.back();
  feature.sketch_parameters->dimensions.push_back(SketchDimension{
      .id = "dim-circle-" + circle.id,
      .kind = "circle_radius",
      .entity_id = circle.id,
      .value = circle.radius,
  });
  refresh_sketch_derived_state(feature);
}

}  // namespace polysmith::core
