// Trim breaks all existing constraints and dimensions on the entity.
// Delete them using the pre-captured point IDs (line_it may be
// invalid after push_back in the middle-segment case).
params.constraints.erase(std::remove_if(params.constraints.begin(),
                                        params.constraints.end(),
                                        [&](const SketchConstraint &c) {
                                          for (const auto &tid : c.target_ids) {
                                            if (tid == entity_id ||
                                                tid == old_sp_id ||
                                                tid == old_ep_id)
                                              return true;
                                          }
                                          return false;
                                        }),
                         params.constraints.end());
params.dimensions.erase(std::remove_if(params.dimensions.begin(),
                                       params.dimensions.end(),
                                       [&](const SketchDimension &d) {
                                         return d.entity_id == entity_id ||
                                                d.secondary_entity_id ==
                                                    entity_id;
                                       }),
                        params.dimensions.end());

// Delete midpoint and point-line anchors that reference the
// trimmed line — they become invalid when the line changes.
params.midpoint_anchors.erase(
    std::remove_if(params.midpoint_anchors.begin(),
                   params.midpoint_anchors.end(),
                   [&](const SketchMidpointAnchor &a) {
                     return a.line_id == entity_id;
                   }),
    params.midpoint_anchors.end());
params.point_line_anchors.erase(
    std::remove_if(params.point_line_anchors.begin(),
                   params.point_line_anchors.end(),
                   [&](const SketchVertexLineAnchor &a) {
                     return a.line_id == entity_id;
                   }),
    params.point_line_anchors.end());

// Delete line relations (parallel, perpendicular, equal_length,
// tangent_line_circle) that involve the trimmed line, and clear
// the H/V constraint on every line whose relation was deleted.
// Otherwise the badge that was suppressed by the relation pops
// back and looks like a "morphed" constraint.
{
  std::unordered_set<std::string> affected_line_ids;
  for (const auto &r : params.line_relations) {
    if (r.first_line_id == entity_id || r.second_line_id == entity_id) {
      if (r.first_line_id != entity_id)
        affected_line_ids.insert(r.first_line_id);
      if (r.second_line_id != entity_id)
        affected_line_ids.insert(r.second_line_id);
    }
  }
  params.line_relations.erase(
      std::remove_if(params.line_relations.begin(), params.line_relations.end(),
                     [&](const SketchLineRelation &r) {
                       return r.first_line_id == entity_id ||
                              r.second_line_id == entity_id;
                     }),
      params.line_relations.end());
  for (auto &line : params.lines) {
    if (affected_line_ids.count(line.id)) {
      line.constraint = std::nullopt;
    }
  }
}

// Delete any fillet that involves this line — trimmed geometry
// can't keep a valid fillet and orphaned fillet points cause
// ghost dots in the viewport.
params.fillets.erase(std::remove_if(params.fillets.begin(),
                                    params.fillets.end(),
                                    [&](const SketchFillet &f) {
                                      return f.line_a_id == entity_id ||
                                             f.line_b_id == entity_id ||
                                             f.arc_id == entity_id ||
                                             f.corner_point_id == old_sp_id ||
                                             f.corner_point_id == old_ep_id ||
                                             f.trim_a_point_id == old_sp_id ||
                                             f.trim_a_point_id == old_ep_id ||
                                             f.trim_b_point_id == old_sp_id ||
                                             f.trim_b_point_id == old_ep_id;
                                    }),
                     params.fillets.end());
