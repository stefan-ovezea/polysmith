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
  // Skip silently when the driven line still carries an H/V
  // constraint — its angle is already fully determined by the axis
  // constraint, so there is nothing for the relation to drive.
  if (driven_line.constraint.has_value()) {
    return;
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
  // Skip silently when the driven line still carries an H/V
  // constraint — its angle is already fully determined by the axis
  // constraint, so there is nothing for the relation to drive.
  if (driven_line.constraint.has_value()) {
    return;
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

// Pull every midpoint-anchored point to its host line's current
// midpoint, propagating the move through connected lines via the
// existing endpoint-cascade machinery. Safe to call repeatedly:
// when the host line itself doesn't change between calls, the
// computed target equals the current point position so the inner
// propagation is a no-op.
void enforce_midpoint_anchors(SketchFeatureParameters& parameters) {
  for (const auto& anchor : parameters.midpoint_anchors) {
    const auto host_it = std::find_if(
        parameters.lines.begin(),
        parameters.lines.end(),
        [&](const SketchLine& line) { return line.id == anchor.line_id; });
    if (host_it == parameters.lines.end()) {
      continue;
    }
    const double mid_x = (host_it->start_x + host_it->end_x) / 2.0;
    const double mid_y = (host_it->start_y + host_it->end_y) / 2.0;
    propagate_connected_point_move(parameters, anchor.point_id, mid_x, mid_y);
  }
}

// Pull every line-anchored point back to its parametric position
// along the host line. Mirrors `enforce_midpoint_anchors` but uses
// the stored `t` instead of always 0.5, so the user can anchor at
// any fraction along the line and have it ride with edits.
void enforce_point_line_anchors(SketchFeatureParameters& parameters) {
  for (const auto& anchor : parameters.point_line_anchors) {
    const auto host_it = std::find_if(
        parameters.lines.begin(),
        parameters.lines.end(),
        [&](const SketchLine& line) { return line.id == anchor.line_id; });
    if (host_it == parameters.lines.end()) {
      continue;
    }
    const double tx =
        host_it->start_x + anchor.t * (host_it->end_x - host_it->start_x);
    const double ty =
        host_it->start_y + anchor.t * (host_it->end_y - host_it->start_y);
    propagate_connected_point_move(parameters, anchor.point_id, tx, ty);
  }
}

// Re-projects each tangent-bound line's end onto the closer of the
// two tangent points from its start to the host circle. The line's
// start, the circle center, and the radius can all change between
// refreshes; recomputing the tangent on every pass keeps the line
// geometrically tangent without us having to know which input
// changed. The branch selection (T₁ vs T₂) uses the current end as
// a tiebreaker so the line doesn't flip across the circle on small
// edits — flips can still happen if the user drags the start
// through the circle interior, but the user-visible result matches
// what they'd expect from a "stickier" tangent.
void enforce_tangent_line_circle_relations(
    SketchFeatureParameters& parameters) {
  for (const auto& relation : parameters.line_relations) {
    if (relation.kind != "tangent_line_circle") {
      continue;
    }
    const auto line_it = std::find_if(
        parameters.lines.begin(),
        parameters.lines.end(),
        [&](const SketchLine& line) {
          return line.id == relation.first_line_id;
        });
    if (line_it == parameters.lines.end()) {
      continue;
    }
    const auto circle_it = std::find_if(
        parameters.circles.begin(),
        parameters.circles.end(),
        [&](const SketchCircle& circle) {
          return circle.id == relation.second_line_id;
        });
    if (circle_it == parameters.circles.end()) {
      continue;
    }

    const double sx = line_it->start_x;
    const double sy = line_it->start_y;
    const double cx = circle_it->center_x;
    const double cy = circle_it->center_y;
    const double r = circle_it->radius;
    const double dx = cx - sx;
    const double dy = cy - sy;
    const double d_squared = dx * dx + dy * dy;
    // Start lies inside (or on) the circle: no real tangent line
    // through it. Skip rather than throw — the user might be
    // mid-drag and the geometry will become valid again. Validation
    // at op-add time prevents creating tangents that are
    // permanently degenerate.
    if (d_squared <= r * r + 1e-12) {
      continue;
    }
    const double d = std::sqrt(d_squared);
    const double tangent_length = std::sqrt(d_squared - r * r);
    // Unit vector from start toward center, plus its left-perp.
    const double ux = dx / d;
    const double uy = dy / d;
    const double vx = -uy;
    const double vy = ux;
    // Tangent point T = S + (L²/d) * u ± (L*r/d) * v. Derivation:
    // (T-C) ⊥ (T-S) and |T-C| = r, so T sits at angle θ off the
    // S→C ray with sin θ = r/d, cos θ = L/d, at distance L from S.
    const double along = (tangent_length * tangent_length) / d;
    const double perp = (tangent_length * r) / d;
    const double t1x = sx + along * ux + perp * vx;
    const double t1y = sy + along * uy + perp * vy;
    const double t2x = sx + along * ux - perp * vx;
    const double t2y = sy + along * uy - perp * vy;

    const double end_x = line_it->end_x;
    const double end_y = line_it->end_y;
    const double dist1_sq =
        (end_x - t1x) * (end_x - t1x) + (end_y - t1y) * (end_y - t1y);
    const double dist2_sq =
        (end_x - t2x) * (end_x - t2x) + (end_y - t2y) * (end_y - t2y);
    const double tx = dist1_sq <= dist2_sq ? t1x : t2x;
    const double ty = dist1_sq <= dist2_sq ? t1y : t2y;

    if (points_match(end_x, end_y, tx, ty)) {
      continue;
    }
    propagate_connected_point_move(
        parameters, line_it->end_point_id, tx, ty);
  }
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

// After constraint enforcement, multiple lines that share a point ID may
// have been driven to different (x, y) coordinates for that shared point.
// `rebuild_sketch_points` deduplicates by point ID and renders a single
// sphere at whichever line's coordinates it encounters first — leaving the
// other line's endpoint visually orphaned.  This pass forces every shared
// point to a single canonical position before the rebuild so the sphere
// matches all lines that reference it.
void reconcile_shared_point_positions(SketchFeatureParameters& parameters) {
  // Build a map from point_id → list of (line_index, is_start_endpoint)
  struct Reference { size_t line_index; bool is_start; };
  std::unordered_map<std::string, std::vector<Reference>> shared;

  for (size_t i = 0; i < parameters.lines.size(); ++i) {
    const auto& line = parameters.lines[i];
    shared[line.start_point_id].push_back({i, true});
    shared[line.end_point_id].push_back({i, false});
  }

  for (const auto& [point_id, refs] : shared) {
    if (refs.size() <= 1) continue;

    // Pick the canonical position.  Priority:
    //   1. A line with a fixed endpoint at this point (authoritative).
    //   2. A line with an H/V constraint (angle is determined).
    //   3. The first reference (fallback).
    size_t canonical_ref = 0;
    bool found_authoritative = false;
    for (size_t r = 0; r < refs.size(); ++r) {
      const auto& l = parameters.lines[refs[r].line_index];
      const std::string& pt =
          refs[r].is_start ? l.start_point_id : l.end_point_id;
      if (point_is_fixed(parameters, pt)) {
        canonical_ref = r;
        found_authoritative = true;
        break;
      }
    }
    if (!found_authoritative) {
      for (size_t r = 0; r < refs.size(); ++r) {
        if (parameters.lines[refs[r].line_index].constraint.has_value()) {
          canonical_ref = r;
          break;
        }
      }
    }

    const auto& canonical_line = parameters.lines[refs[canonical_ref].line_index];
    const double canonical_x = refs[canonical_ref].is_start
        ? canonical_line.start_x : canonical_line.end_x;
    const double canonical_y = refs[canonical_ref].is_start
        ? canonical_line.start_y : canonical_line.end_y;

    for (size_t r = 1; r < refs.size(); ++r) {
      auto& line = parameters.lines[refs[r].line_index];
      const double current_x = refs[r].is_start ? line.start_x : line.end_x;
      const double current_y = refs[r].is_start ? line.start_y : line.end_y;

      if (!points_match(canonical_x, canonical_y, current_x, current_y)) {
        if (refs[r].is_start) {
          line.start_x = canonical_x;
          line.start_y = canonical_y;
        } else {
          line.end_x = canonical_x;
          line.end_y = canonical_y;
        }
      }
    }
  }
}
