// One cleanup pipeline for EVERY trim path. Removes every attachment
// of `entity_id` — constraints (by entity id or any of its point
// ids), dimensions (primary AND secondary side), line relations,
// midpoint / point-line anchors, and fillet / chamfer records whose
// operands, corner point, or owned geometry reference the entity or
// its endpoints. The isolated-delete and single-survivor paths used
// to run their own smaller erasures that leaked line relations,
// anchors and fillets — orphaned fillet records draw ghost dots in
// the viewport and stale relation badges survive on partners.
void trim_remove_entity_attachments(
    SketchFeatureParameters& parameters,
    const std::string& entity_id,
    const std::vector<std::string>& point_ids) {
  const auto touches = [&](const std::string& id) {
    if (id.empty()) return false;
    if (id == entity_id) return true;
    return std::find(point_ids.begin(), point_ids.end(), id) !=
           point_ids.end();
  };

  parameters.constraints.erase(
      std::remove_if(parameters.constraints.begin(),
                     parameters.constraints.end(),
                     [&](const SketchConstraint& c) {
                       for (const auto& tid : c.target_ids) {
                         if (touches(tid)) return true;
                       }
                       return false;
                     }),
      parameters.constraints.end());

  parameters.dimensions.erase(
      std::remove_if(parameters.dimensions.begin(),
                     parameters.dimensions.end(),
                     [&](const SketchDimension& d) {
                       return touches(d.entity_id) ||
                              touches(d.secondary_entity_id);
                     }),
      parameters.dimensions.end());

  // Line relations (parallel, perpendicular, equal_length,
  // tangent_line_circle) involving the trimmed entity, plus the H/V
  // badge suppression on surviving partners: a badge that was hidden
  // by the relation must not pop back after the relation dies.
  {
    std::unordered_set<std::string> partner_line_ids;
    for (const auto& r : parameters.line_relations) {
      if (touches(r.first_line_id) || touches(r.second_line_id)) {
        if (!touches(r.first_line_id)) partner_line_ids.insert(r.first_line_id);
        if (!touches(r.second_line_id)) partner_line_ids.insert(r.second_line_id);
      }
    }
    parameters.line_relations.erase(
        std::remove_if(parameters.line_relations.begin(),
                       parameters.line_relations.end(),
                       [&](const SketchLineRelation& r) {
                         return touches(r.first_line_id) ||
                                touches(r.second_line_id);
                       }),
        parameters.line_relations.end());
    for (auto& line : parameters.lines) {
      if (partner_line_ids.count(line.id)) {
        line.constraint = std::nullopt;
      }
    }
  }

  parameters.midpoint_anchors.erase(
      std::remove_if(parameters.midpoint_anchors.begin(),
                     parameters.midpoint_anchors.end(),
                     [&](const SketchMidpointAnchor& a) {
                       return touches(a.line_id);
                     }),
      parameters.midpoint_anchors.end());
  parameters.point_line_anchors.erase(
      std::remove_if(parameters.point_line_anchors.begin(),
                     parameters.point_line_anchors.end(),
                     [&](const SketchVertexLineAnchor& a) {
                       return touches(a.line_id);
                     }),
      parameters.point_line_anchors.end());

  // Fillets and chamfers whose operand, corner, or owned geometry
  // touches the trimmed entity — the owned arc / trim points cannot
  // outlive their record.
  parameters.fillets.erase(
      std::remove_if(parameters.fillets.begin(), parameters.fillets.end(),
                     [&](const SketchFillet& f) {
                       return touches(f.line_a_id) || touches(f.line_b_id) ||
                              touches(f.arc_a_id) || touches(f.arc_b_id) ||
                              touches(f.arc_id) ||
                              touches(f.corner_vertex_id) ||
                              touches(f.trim_a_vertex_id) ||
                              touches(f.trim_b_vertex_id);
                     }),
      parameters.fillets.end());
  parameters.chamfers.erase(
      std::remove_if(parameters.chamfers.begin(), parameters.chamfers.end(),
                     [&](const SketchChamfer& c) {
                       return touches(c.line_a_id) || touches(c.line_b_id) ||
                              touches(c.corner_vertex_id) ||
                              touches(c.trim_a_vertex_id) ||
                              touches(c.trim_b_vertex_id) ||
                              touches(c.chamfer_line_id);
                     }),
      parameters.chamfers.end());

  // Projection pruning: trimming a projected entity breaks its live
  // link. Left alone, the dependency walk's patch pass would rewrite
  // the trimmed endpoints back to the source outline on the next
  // refresh (the trim visually reverts) — a dead id after a full
  // erase instead trips the count mismatch with a stale id stuck in
  // the record. Prune the entity id from every record and drop
  // emptied records; the walk then flags the sketch dependency_broken
  // (count mismatch) and the user heals via Remove/Unlink
  // projections — the same contract as delete_sketch_selection.
  {
    auto prune = [](std::vector<std::string>& ids, const std::string& id) {
      const size_t before = ids.size();
      ids.erase(std::remove(ids.begin(), ids.end(), id), ids.end());
      return ids.size() != before;
    };
    bool any_pruned = false;
    for (auto& projection : parameters.projections) {
      any_pruned |= prune(projection.generated_line_ids, entity_id);
      any_pruned |= prune(projection.generated_circle_ids, entity_id);
      any_pruned |= prune(projection.generated_arc_ids, entity_id);
    }
    if (any_pruned) {
      parameters.projections.erase(
          std::remove_if(parameters.projections.begin(),
                         parameters.projections.end(),
                         [](const SketchProjection& projection) {
                           return projection.generated_line_ids.empty() &&
                                  projection.generated_circle_ids.empty() &&
                                  projection.generated_arc_ids.empty() &&
                                  projection.generated_vertex_id.empty();
                         }),
          parameters.projections.end());
    }
  }
}

// Every point id the sketch currently owns, with its position:
// entity endpoints, centers, axis points, poles, derived quadrant
// points, and fillet/chamfer corner bookkeeping points. Trim paths
// use the id set to drop coincident constraints that reference ids
// orphaned by a trim; the constraint transfer uses the positions to
// verify a coincident pair STILL coincides (a severed endpoint pair
// keeps its ids alive). ONE copy of the quadrant id format — the old
// code hardcoded it in three separate safety nets (circle/arc/ellipse
// phases).
// TODO(phase-6-followup): give quadrants stable vertex-N IDs so this
// deterministic legacy format can die.
std::unordered_map<std::string, std::pair<double, double>>
live_sketch_point_positions(const SketchFeatureParameters& parameters) {
  std::unordered_map<std::string, std::pair<double, double>> live;
  for (const auto& line : parameters.lines) {
    live[line.start_vertex_id] = {line.start_x, line.start_y};
    live[line.end_vertex_id] = {line.end_x, line.end_y};
  }
  for (const auto& circle : parameters.circles) {
    live[circle_center_vertex_id(circle)] = {circle.center_x, circle.center_y};
    const double r = circle.radius;
    live["vertex-" + circle.id + "-quadrant-0"] = {circle.center_x + r,
                                                   circle.center_y};
    live["vertex-" + circle.id + "-quadrant-1"] = {circle.center_x,
                                                   circle.center_y + r};
    live["vertex-" + circle.id + "-quadrant-2"] = {circle.center_x - r,
                                                   circle.center_y};
    live["vertex-" + circle.id + "-quadrant-3"] = {circle.center_x,
                                                   circle.center_y - r};
  }
  for (const auto& arc : parameters.arcs) {
    live[arc.start_vertex_id] = {arc.start_x, arc.start_y};
    live[arc.end_vertex_id] = {arc.end_x, arc.end_y};
    live[arc.center_vertex_id] = {arc.center_x, arc.center_y};
  }
  for (const auto& ellipse : parameters.ellipses) {
    live[ellipse.center_vertex_id] = {ellipse.center_x, ellipse.center_y};
    live[ellipse.axis_a_vertex_id] = {
        ellipse.center_x + ellipse.a * std::cos(ellipse.rotation),
        ellipse.center_y + ellipse.a * std::sin(ellipse.rotation)};
    live[ellipse.axis_b_vertex_id] = {
        ellipse.center_x - ellipse.b * std::sin(ellipse.rotation),
        ellipse.center_y + ellipse.b * std::cos(ellipse.rotation)};
    if (ellipse.has_sweep) {
      // The ellipse stores no endpoint coordinates — derive them from
      // the sweep angles (eccentric-anomaly parameters, same convention
      // as the trim splitter / UI sampler).
      const double cu = std::cos(ellipse.rotation);
      const double su = std::sin(ellipse.rotation);
      const auto pt_at = [&](double t) {
        const double px = ellipse.a * std::cos(t);
        const double py = ellipse.b * std::sin(t);
        return std::pair{ellipse.center_x + px * cu - py * su,
                         ellipse.center_y + px * su + py * cu};
      };
      live[ellipse.start_vertex_id] = pt_at(ellipse.sweep_start_angle);
      live[ellipse.end_vertex_id] = pt_at(ellipse.sweep_end_angle);
    }
  }
  for (const auto& spline : parameters.splines) {
    for (size_t i = 0; i < spline.pole_vertex_ids.size(); ++i) {
      live[spline.pole_vertex_ids[i]] = {spline.pole_xs[i], spline.pole_ys[i]};
    }
  }
  for (const auto& fillet : parameters.fillets) {
    live[fillet.corner_vertex_id] = {fillet.corner_x, fillet.corner_y};
  }
  for (const auto& chamfer : parameters.chamfers) {
    live[chamfer.corner_vertex_id] = {chamfer.corner_x, chamfer.corner_y};
  }
  return live;
}

std::unordered_set<std::string> live_sketch_point_ids(
    const SketchFeatureParameters& parameters) {
  std::unordered_set<std::string> live;
  for (const auto& [id, unused] : live_sketch_point_positions(parameters)) {
    (void)unused;
    live.insert(id);
  }
  return live;
}

// True when an entity id is currently live in any sketch array (used
// by the constraint transfer to re-target relational dimensions only
// onto partners that still exist).
bool entity_live(const SketchFeatureParameters& parameters,
                 const std::string& entity_id) {
  const auto in_lines = std::find_if(
      parameters.lines.begin(), parameters.lines.end(),
      [&](const SketchLine& l) { return l.id == entity_id; });
  if (in_lines != parameters.lines.end()) return true;
  const auto in_circles = std::find_if(
      parameters.circles.begin(), parameters.circles.end(),
      [&](const SketchCircle& c) { return c.id == entity_id; });
  if (in_circles != parameters.circles.end()) return true;
  const auto in_arcs = std::find_if(
      parameters.arcs.begin(), parameters.arcs.end(),
      [&](const SketchArc& a) { return a.id == entity_id; });
  if (in_arcs != parameters.arcs.end()) return true;
  const auto in_ellipses = std::find_if(
      parameters.ellipses.begin(), parameters.ellipses.end(),
      [&](const SketchEllipse& e) { return e.id == entity_id; });
  if (in_ellipses != parameters.ellipses.end()) return true;
  const auto in_splines = std::find_if(
      parameters.splines.begin(), parameters.splines.end(),
      [&](const SketchSpline& s) { return s.id == entity_id; });
  return in_splines != parameters.splines.end();
}
