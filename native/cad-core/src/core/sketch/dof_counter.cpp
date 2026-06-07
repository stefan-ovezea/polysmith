#include "core/dof_counter.h"

#include <string>
#include <unordered_map>
#include <vector>

namespace polysmith::core {
namespace {

int intrinsic_dof(const std::string& kind) {
  if (kind == "line")    return 4;
  if (kind == "circle")  return 3;
  if (kind == "polygon") return 3;
  if (kind == "arc")     return 6;
  if (kind == "point")   return 2;
  return 0;
}

int constraint_cost(const std::string& kind) {
  if (kind == "coincident")   return 2;
  if (kind == "concentric")   return 2;
  if (kind == "fixed")        return 2;
  if (kind == "horizontal")   return 1;
  if (kind == "vertical")     return 1;
  if (kind == "parallel")     return 1;
  if (kind == "perpendicular") return 1;
  if (kind == "equal_length") return 1;
  if (kind == "tangent")      return 1;
  if (kind == "symmetry")     return 2;
  return 1;
}

struct E {
  int total;
  int consumed = 0;
  DofStatus status() const {
    int r = total - consumed;
    if (r > 0) return DofStatus::UnderConstrained;
    if (r == 0) return DofStatus::FullyConstrained;
    return DofStatus::OverConstrained;
  }
};

} // namespace

std::vector<EntityDofResult> count_sketch_dof(
    const SketchFeatureParameters& params) {
  std::unordered_map<std::string, E> map;

  for (const auto& l : params.lines)   map[l.id] = {intrinsic_dof("line"), 0};
  for (const auto& c : params.circles) map[c.id] = {intrinsic_dof("circle"), 0};
  for (const auto& p : params.polygons) map[p.id] = {intrinsic_dof("polygon"), 0};
  for (const auto& a : params.arcs)    map[a.id] = {intrinsic_dof("arc"), 0};
  for (const auto& p : params.points)  map[p.id] = {intrinsic_dof("point"), 0};

  // Inline constraints on lines.
  for (const auto& l : params.lines) {
    if (l.constraint.has_value()) {
      map[l.id].consumed += constraint_cost(l.constraint.value());
    }
  }

  // Line relations.
  for (const auto& r : params.line_relations) {
    int c = constraint_cost(r.kind);
    if (map.count(r.first_line_id))  map[r.first_line_id].consumed += c;
    if (map.count(r.second_line_id)) map[r.second_line_id].consumed += c;
  }

  // Implicit coincident constraints from shared endpoints.
  // Each shared corner costs 1 DOF per connected line (not 2,
  // since the constraint reduces system-level DOF).
  for (size_t i = 0; i < params.lines.size(); ++i) {
    for (size_t j = i + 1; j < params.lines.size(); ++j) {
      const auto& a = params.lines[i];
      const auto& b = params.lines[j];
      int shared = 0;
      if (a.start_point_id == b.start_point_id || a.start_point_id == b.end_point_id) ++shared;
      if (a.end_point_id == b.start_point_id || a.end_point_id == b.end_point_id) ++shared;
      if (shared > 0 && map.count(a.id) && map.count(b.id)) {
        map[a.id].consumed += shared; // 1 per shared point per line
        map[b.id].consumed += shared;
      }
    }
  }

  // General constraints.
  for (const auto& c : params.constraints) {
    int cost = constraint_cost(c.kind);
    for (const auto& tid : c.target_ids) {
      if (map.count(tid)) map[tid].consumed += cost;
    }
  }

  // Driving dimensions.
  for (const auto& d : params.dimensions) {
    if (d.driven) continue;
    if (map.count(d.entity_id)) map[d.entity_id].consumed += 1;
  }

  // Fixed points.
  for (const auto& p : params.points) {
    if (p.is_fixed && map.count(p.id)) {
      map[p.id].consumed += constraint_cost("fixed");
    }
  }

  // Midpoint anchors.
  for (const auto& a : params.midpoint_anchors) {
    if (map.count(a.point_id)) map[a.point_id].consumed += 2;
  }

  std::vector<EntityDofResult> results;
  for (const auto& l : params.lines) {
    auto& e = map[l.id]; results.push_back({l.id, "line", e.total, e.consumed, e.status()});
  }
  for (const auto& c : params.circles) {
    auto& e = map[c.id]; results.push_back({c.id, "circle", e.total, e.consumed, e.status()});
  }
  for (const auto& p : params.polygons) {
    auto& e = map[p.id]; results.push_back({p.id, "polygon", e.total, e.consumed, e.status()});
  }
  for (const auto& a : params.arcs) {
    auto& e = map[a.id]; results.push_back({a.id, "arc", e.total, e.consumed, e.status()});
  }
  for (const auto& p : params.points) {
    auto& e = map[p.id]; results.push_back({p.id, "point", e.total, e.consumed, e.status()});
  }
  return results;
}

DofStatus get_entity_dof_status(
    const SketchFeatureParameters& params,
    const std::string& entity_id) {
  auto results = count_sketch_dof(params);
  for (const auto& r : results) {
    if (r.entity_id == entity_id) return r.status;
  }
  return DofStatus::UnderConstrained;
}

} // namespace polysmith::core
