#include "core/sketch/dof_counter.h"

#include <algorithm>
#include <set>
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
  if (kind == "symmetric")    return 2;
  if (kind == "midpoint")     return 2;
  if (kind == "collinear")    return 1;
  if (kind == "tangent_line_line") return 1;
  if (kind == "tangent_line_circle") return 1;
  if (kind == "tangent_circle_circle") return 1;
  if (kind == "tangent_arc_arc") return 1;
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
  for (const auto& p : params.vertices)  map[p.id] = {intrinsic_dof("point"), 0};

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
      if (a.start_vertex_id == b.start_vertex_id || a.start_vertex_id == b.end_vertex_id) ++shared;
      if (a.end_vertex_id == b.start_vertex_id || a.end_vertex_id == b.end_vertex_id) ++shared;
      if (shared > 0 && map.count(a.id) && map.count(b.id)) {
        map[a.id].consumed += shared; // 1 per shared point per line
        map[b.id].consumed += shared;
      }
    }
  }

  // General constraints.
  for (const auto& c : params.constraints) {
    // Fixed constraints are counted by the is_fixed pass below — the
    // record only drives the viewport badge, so counting it here
    // would consume the DOF twice.
    if (c.kind == "fixed") continue;
    int cost = constraint_cost(c.kind);
    for (const auto& tid : c.target_ids) {
      if (map.count(tid)) map[tid].consumed += cost;
    }
  }

  // Driving dimensions.  Auto-dimensions without a user-set expression
  // are display-only (is_auto=true, expression="") — they do NOT consume
  // DOF because the solver ignores them.
  for (const auto& d : params.dimensions) {
    if (d.driven) continue;
    if (d.is_auto && d.expression.empty()) continue;
    if (map.count(d.entity_id)) map[d.entity_id].consumed += 1;
  }

  // Fixed points.
  for (const auto& p : params.vertices) {
    if (p.is_fixed && map.count(p.id)) {
      map[p.id].consumed += constraint_cost("fixed");
    }
  }

  // Generated (text glyph) lines are derived geometry with fixed
  // endpoints — report them as fully constrained so the viewport
  // renders glyphs in the "fixed" color instead of under-constrained.
  for (const auto& l : params.lines) {
    if (l.generated_by.has_value()) {
      map[l.id].consumed = intrinsic_dof("line");
    }
  }

  // Midpoint anchors.
  for (const auto& a : params.midpoint_anchors) {
    if (map.count(a.vertex_id)) map[a.vertex_id].consumed += 2;
  }

  // ── S3: ONE DOF SOURCE ────────────────────────────────────────────
  // How many degrees of freedom REMAIN is the solver's answer to give
  // (`params.solver_dofs`). The arithmetic above is a per-entity estimate that
  // charges a constraint to every entity it touches, so it cannot agree with the
  // solver by construction — a rectangle with 4 DOF came out at 9 remaining — and
  // a badge that contradicts the sketch status makes both numbers untrustworthy.
  // The estimate is still worth keeping for ORDERING (which entity carries more
  // rules), but the STATUS now comes from the solve, and the counts are clamped so
  // they can never claim completeness the solver has not granted.
  std::set<std::string> fixed_vertex_ids;
  for (const auto& vertex : params.vertices) {
    if (vertex.is_fixed) fixed_vertex_ids.insert(vertex.id);
  }
  const bool over_constrained = params.solver_conflicting_count > 0;
  const bool fully_constrained = !over_constrained && params.solver_dofs == 0;
  const auto status_for = [&](const std::vector<std::string>& vertex_ids) {
    if (over_constrained) return DofStatus::OverConstrained;
    if (fully_constrained) return DofStatus::FullyConstrained;
    // An entity pinned entirely by its own fixed points is fully constrained even
    // while other geometry in the sketch is still free.
    if (vertex_ids.empty()) return DofStatus::UnderConstrained;
    for (const auto& id : vertex_ids) {
      if (fixed_vertex_ids.count(id) == 0) return DofStatus::UnderConstrained;
    }
    return DofStatus::FullyConstrained;
  };

  std::vector<EntityDofResult> results;
  const auto push = [&](const std::string& id, const char* kind, int total,
                        int consumed, const std::vector<std::string>& vertices) {
    const DofStatus status = status_for(vertices);
    const int clamped = status == DofStatus::UnderConstrained
                            ? std::min(consumed, std::max(total - 1, 0))
                            : total;
    results.push_back({id, kind, total, clamped, status});
  };
  for (const auto& l : params.lines) {
    push(l.id, "line", map[l.id].total, map[l.id].consumed,
         {l.start_vertex_id, l.end_vertex_id});
  }
  for (const auto& c : params.circles) {
    push(c.id, "circle", map[c.id].total, map[c.id].consumed,
         {c.center_vertex_id});
  }
  for (const auto& p : params.polygons) {
    push(p.id, "polygon", map[p.id].total, map[p.id].consumed, {});
  }
  for (const auto& a : params.arcs) {
    push(a.id, "arc", map[a.id].total, map[a.id].consumed,
         {a.start_vertex_id, a.end_vertex_id, a.center_vertex_id});
  }
  for (const auto& p : params.vertices) {
    push(p.id, "point", map[p.id].total, map[p.id].consumed, {p.id});
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
