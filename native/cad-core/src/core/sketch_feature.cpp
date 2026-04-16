#include "core/sketch_feature.h"

#include <cmath>
#include <sstream>
#include <stdexcept>

namespace polysmith::core {
namespace {

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

void validate_tool(const std::string& tool) {
  if (tool != "select" && tool != "line" && tool != "rectangle" &&
      tool != "circle") {
    throw std::runtime_error("Unsupported sketch tool: " + tool);
  }
}

}  // namespace

FeatureEntry create_sketch_feature(int feature_index, const std::string& plane_id) {
  SketchFeatureParameters parameters{
      .plane_id = plane_id,
      .active_tool = "select",
      .lines = {},
      .circles = {},
  };

  return FeatureEntry{
      .id = "feature-" + std::to_string(feature_index),
      .kind = "sketch",
      .name = "Sketch",
      .status = "editing",
      .parameters_summary = make_parameters_summary(parameters),
      .box_parameters = std::nullopt,
      .cylinder_parameters = std::nullopt,
      .sketch_parameters = parameters,
  };
}

void set_sketch_tool(FeatureEntry& feature, const std::string& tool) {
  if (feature.kind != "sketch" || !feature.sketch_parameters.has_value()) {
    throw std::runtime_error("Only sketch features can change sketch tools");
  }

  validate_tool(tool);
  feature.sketch_parameters->active_tool = tool;
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

  feature.sketch_parameters->lines.push_back(SketchLine{
      .id = "line-" + std::to_string(line_index),
      .start_x = start_x,
      .start_y = start_y,
      .end_x = end_x,
      .end_y = end_y,
      .constraint_hint = infer_constraint_hint(start_x, start_y, end_x, end_y),
  });
  feature.parameters_summary = make_parameters_summary(feature.sketch_parameters.value());
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

  if (radius <= 0.001) {
    throw std::runtime_error("Sketch circles must have non-zero radius");
  }

  feature.sketch_parameters->circles.push_back(SketchCircle{
      .id = "circle-" + std::to_string(circle_index),
      .center_x = center_x,
      .center_y = center_y,
      .radius = radius,
  });
  feature.parameters_summary = make_parameters_summary(feature.sketch_parameters.value());
}

}  // namespace polysmith::core
