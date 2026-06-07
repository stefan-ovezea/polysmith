#pragma once

#include <string>
#include <vector>

#include "core/feature.h"

namespace polysmith::core {

// A single intersection between a target entity and another entity.
struct TrimIntersection {
  double x;                    // sketch-local coordinate
  double y;
  double param_on_target;      // parameter along the target entity
  double param_on_other;       // parameter along the other entity
  std::string other_entity_id;
};

// A candidate segment of the target entity after splitting.
struct TrimSegment {
  enum Kind { LINE_SEGMENT, ARC_SEGMENT };
  Kind kind;
  double param_start;          // parameter range along the original entity
  double param_end;
  // Cached endpoint coordinates (sketch-local)
  double start_x;
  double start_y;
  double end_x;
  double end_y;
  // For arcs: center, radius, ccw from the source entity.
  double center_x = 0.0;
  double center_y = 0.0;
  double radius = 0.0;
  bool ccw = false;
};

// Intersection detection — lines

std::vector<TrimIntersection> find_all_intersections(
    const SketchLine& target,
    const SketchFeatureParameters& params);

std::optional<TrimIntersection> intersect_line_line(
    const SketchLine& target,
    const SketchLine& other);

std::vector<TrimIntersection> intersect_line_circle(
    const SketchLine& target,
    const SketchCircle& other);

// Intersection detection — circles

std::vector<TrimIntersection> find_all_intersections(
    const SketchCircle& target,
    const SketchFeatureParameters& params);

std::vector<TrimIntersection> intersect_circle_line(
    const SketchCircle& target,
    const SketchLine& other);

std::vector<TrimIntersection> intersect_circle_circle(
    const SketchCircle& target,
    const SketchCircle& other);

// Intersection detection — arcs

std::vector<TrimIntersection> find_all_intersections(
    const SketchArc& target,
    const SketchFeatureParameters& params);

std::vector<TrimIntersection> intersect_arc_line(
    const SketchArc& target,
    const SketchLine& other);

std::vector<TrimIntersection> intersect_arc_circle(
    const SketchArc& target,
    const SketchCircle& other);

std::vector<TrimIntersection> intersect_arc_arc(
    const SketchArc& target,
    const SketchArc& other);

// Splitting

std::vector<TrimSegment> split_line_at_intersections(
    const SketchLine& line,
    const std::vector<TrimIntersection>& intersections);

std::vector<TrimSegment> split_circle_at_intersections(
    const SketchCircle& circle,
    const std::vector<TrimIntersection>& intersections);

// Segment selection

int select_clicked_segment(
    const std::vector<TrimSegment>& segments,
    const SketchLine& original_line,
    double click_x,
    double click_y);

int select_clicked_segment(
    const std::vector<TrimSegment>& segments,
    const SketchCircle& original_circle,
    double click_x,
    double click_y);

std::vector<TrimSegment> split_arc_at_intersections(
    const SketchArc& arc,
    const std::vector<TrimIntersection>& intersections);

int select_clicked_segment(
    const std::vector<TrimSegment>& segments,
    const SketchArc& original_arc,
    double click_x,
    double click_y);

constexpr double kTrimCoincidentTolerance = 0.01;  // mm

}  // namespace polysmith::core
