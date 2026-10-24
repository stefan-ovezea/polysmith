// S11 first slice: auto-constraint candidate detection (see the header).
// Included inside namespace polysmith::core, so no outer namespace here.
#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

namespace {

constexpr double kPi = 3.14159265358979323846;

double line_length(const SketchLine& line) {
  return std::hypot(line.end_x - line.start_x, line.end_y - line.start_y);
}

// Direction of a line in degrees, normalised to [0, 180) so opposite directions
// compare equal (a line carries no arrow).
double line_angle_degrees(const SketchLine& line) {
  const double dx = line.end_x - line.start_x;
  const double dy = line.end_y - line.start_y;
  double degrees = std::atan2(dy, dx) * 180.0 / kPi;
  while (degrees < 0.0) degrees += 180.0;
  while (degrees >= 180.0) degrees -= 180.0;
  return degrees;
}

// Smallest deviation from an axis (0 or 90 degrees), in degrees.
double axis_deviation(double angle_degrees, double* axis) {
  const double to_horizontal = std::min(angle_degrees, 180.0 - angle_degrees);
  const double to_vertical = std::abs(angle_degrees - 90.0);
  if (to_horizontal <= to_vertical) {
    *axis = 0.0;
    return to_horizontal;
  }
  *axis = 90.0;
  return to_vertical;
}

// Smallest deviation from parallel (0 deg) or perpendicular (90 deg) between two
// line directions.
double relation_deviation(double first, double second, const std::string& kind) {
  double delta = std::fabs(first - second);
  while (delta >= 180.0) delta -= 180.0;
  const double to_parallel = std::min(delta, 180.0 - delta);
  if (kind == "parallel") {
    return to_parallel;
  }
  return std::fabs(90.0 - to_parallel);
}

bool has_line_relation(const SketchFeatureParameters& parameters,
                       const std::string& kind, const std::string& first_line_id,
                       const std::string& second_line_id) {
  for (const auto& relation : parameters.line_relations) {
    if (relation.kind != kind) continue;
    const bool forward = relation.first_line_id == first_line_id &&
                         relation.second_line_id == second_line_id;
    const bool reversed = relation.first_line_id == second_line_id &&
                          relation.second_line_id == first_line_id;
    if (forward || reversed) return true;
  }
  return false;
}

// Axis constraints (horizontal / vertical) live on the LINE itself -
// set_sketch_line_constraint stores them in SketchLine::constraint - and the
// generic constraint list is checked too so a constraint added through another
// path is still recognised. D-009's investigation is what pinned this down.
bool has_line_axis_constraint(const SketchFeatureParameters& parameters,
                              const std::string& line_id,
                              const std::string& kind) {
  for (const auto& line : parameters.lines) {
    if (line.id != line_id) continue;
    if (line.constraint.has_value() && line.constraint.value() == kind) {
      return true;
    }
  }
  for (const auto& constraint : parameters.constraints) {
    if (constraint.kind != kind) continue;
    if (std::find(constraint.target_ids.begin(), constraint.target_ids.end(),
                  line_id) != constraint.target_ids.end()) {
      return true;
    }
  }
  for (const auto& relation : parameters.line_relations) {
    if (relation.kind != kind) continue;
    if (relation.first_line_id == line_id ||
        relation.second_line_id == line_id) {
      return true;
    }
  }
  return false;
}

bool has_constraint_kind_for(const SketchFeatureParameters& parameters,
                             const std::string& kind,
                             const std::string& first_id,
                             const std::string& second_id) {
  for (const auto& constraint : parameters.constraints) {
    if (constraint.kind != kind) continue;
    if (constraint.target_ids.size() != 2) continue;
    const bool forward = constraint.target_ids[0] == first_id &&
                         constraint.target_ids[1] == second_id;
    const bool reversed = constraint.target_ids[0] == second_id &&
                          constraint.target_ids[1] == first_id;
    if (forward || reversed) return true;
  }
  return false;
}

double center_distance(double ax, double ay, double bx, double by) {
  return std::hypot(ax - bx, ay - by);
}

}  // namespace

std::vector<AutoConstraintCandidate> detect_auto_constraints(
    const SketchFeatureParameters& parameters, double tolerance_mm,
    double angle_tolerance_deg) {
  std::vector<AutoConstraintCandidate> candidates;
  const double tolerance = std::max(0.0, tolerance_mm);
  const double angle_tolerance = std::max(0.0, angle_tolerance_deg);

  // Axis alignment: horizontal / vertical.
  for (const auto& line : parameters.lines) {
    if (line_length(line) <= 1e-9) continue;
    double axis = 0.0;
    const double deviation = axis_deviation(line_angle_degrees(line), &axis);
    if (deviation > angle_tolerance) continue;
    const std::string kind = axis == 0.0 ? "horizontal" : "vertical";
    if (has_line_axis_constraint(parameters, line.id, kind)) continue;
    candidates.push_back(AutoConstraintCandidate{
        .kind = kind,
        .target_ids = {line.id},
        .error = deviation,
        .reason = "line is " + std::to_string(deviation) + " deg off " + kind,
    });
  }

  // Equal length. Lines only in v1: arcs are excluded on purpose, they are the
  // least predictable case for the solver.
  for (std::size_t a = 0; a < parameters.lines.size(); ++a) {
    for (std::size_t b = a + 1; b < parameters.lines.size(); ++b) {
      const auto& first = parameters.lines[a];
      const auto& second = parameters.lines[b];
      const double first_length = line_length(first);
      const double second_length = line_length(second);
      if (first_length <= 1e-9 || second_length <= 1e-9) continue;
      const double difference = std::fabs(first_length - second_length);
      if (difference > tolerance) continue;
      if (has_line_relation(parameters, "equal_length", first.id, second.id)) {
        continue;
      }
      candidates.push_back(AutoConstraintCandidate{
          .kind = "equal_length",
          .target_ids = {first.id, second.id},
          .error = difference,
          .reason = "lengths differ by " + std::to_string(difference) + " mm",
      });
    }
  }

  // Parallel / perpendicular.
  for (std::size_t a = 0; a < parameters.lines.size(); ++a) {
    for (std::size_t b = a + 1; b < parameters.lines.size(); ++b) {
      const auto& first = parameters.lines[a];
      const auto& second = parameters.lines[b];
      if (line_length(first) <= 1e-9 || line_length(second) <= 1e-9) continue;
      const double first_angle = line_angle_degrees(first);
      const double second_angle = line_angle_degrees(second);
      for (const std::string kind : {"parallel", "perpendicular"}) {
        if (has_line_relation(parameters, kind, first.id, second.id)) continue;
        const double deviation =
            relation_deviation(first_angle, second_angle, kind);
        if (deviation > angle_tolerance) continue;
        candidates.push_back(AutoConstraintCandidate{
            .kind = kind,
            .target_ids = {first.id, second.id},
            .error = deviation,
            .reason = "directions are " + std::to_string(deviation) +
                      " deg off " + kind,
        });
      }
    }
  }

  // Coincident endpoints - the same condition S5 reports as an untied pair.
  struct Endpoint {
    std::string vertex_id;
    std::string line_id;
    double x = 0.0;
    double y = 0.0;
  };
  std::vector<Endpoint> endpoints;
  for (const auto& line : parameters.lines) {
    endpoints.push_back(Endpoint{line.start_vertex_id, line.id, line.start_x,
                                 line.start_y});
    endpoints.push_back(
        Endpoint{line.end_vertex_id, line.id, line.end_x, line.end_y});
  }
  for (std::size_t a = 0; a < endpoints.size(); ++a) {
    for (std::size_t b = a + 1; b < endpoints.size(); ++b) {
      const auto& first = endpoints[a];
      const auto& second = endpoints[b];
      if (first.vertex_id == second.vertex_id) continue;  // already one vertex
      if (first.line_id == second.line_id) continue;      // a line's own ends
      const double gap = center_distance(first.x, first.y, second.x, second.y);
      if (gap > tolerance) continue;
      if (has_constraint_kind_for(parameters, "coincident", first.vertex_id,
                                  second.vertex_id)) {
        continue;
      }
      candidates.push_back(AutoConstraintCandidate{
          .kind = "coincident",
          .target_ids = {first.vertex_id, second.vertex_id},
          .error = gap,
          .reason = "endpoints are " + std::to_string(gap) + " mm apart",
      });
    }
  }

  // Concentric circles / arcs.
  struct Curve {
    std::string id;
    double center_x = 0.0;
    double center_y = 0.0;
  };
  std::vector<Curve> curves;
  for (const auto& circle : parameters.circles) {
    curves.push_back(Curve{circle.id, circle.center_x, circle.center_y});
  }
  for (const auto& arc : parameters.arcs) {
    curves.push_back(Curve{arc.id, arc.center_x, arc.center_y});
  }
  for (std::size_t a = 0; a < curves.size(); ++a) {
    for (std::size_t b = a + 1; b < curves.size(); ++b) {
      const auto& first = curves[a];
      const auto& second = curves[b];
      const double gap = center_distance(first.center_x, first.center_y,
                                         second.center_x, second.center_y);
      if (gap > tolerance) continue;
      if (has_constraint_kind_for(parameters, "concentric", first.id,
                                  second.id)) {
        continue;
      }
      candidates.push_back(AutoConstraintCandidate{
          .kind = "concentric",
          .target_ids = {first.id, second.id},
          .error = gap,
          .reason = "centres are " + std::to_string(gap) + " mm apart",
      });
    }
  }

  // Deterministic order: kind, then the target ids.
  std::stable_sort(candidates.begin(), candidates.end(),
                   [](const AutoConstraintCandidate& a,
                      const AutoConstraintCandidate& b) {
                     if (a.kind != b.kind) return a.kind < b.kind;
                     return a.target_ids < b.target_ids;
                   });
  return candidates;
}