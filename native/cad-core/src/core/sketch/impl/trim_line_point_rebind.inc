// Share point IDs at the new endpoints with coincident geometry
// so profile loops stay connected after trim.  Resolves through the
// nearest-wins shared-point service instead of the first-match-wins
// scan — the old scan could return a freshly pushed sibling's own
// vertex id purely by iteration order.
const std::unordered_set<std::string> excluded_entities{entity_id};

const auto shared_start = resolve_shared_point(
    params, line_it->start_x, line_it->start_y, excluded_entities);
const auto shared_end = resolve_shared_point(
    params, line_it->end_x, line_it->end_y, excluded_entities);

const std::string new_sp =
    shared_start.has_value() ? std::get<0>(shared_start.value())
                             : mint_vertex_id();
const std::string new_ep =
    shared_end.has_value() ? std::get<0>(shared_end.value())
                           : mint_vertex_id();

line_it->start_vertex_id = new_sp;
line_it->end_vertex_id = new_ep;

// Dissolve all polygon records — a trimmed polygon line breaks
// the parametric polygon. Remaining lines become independent.
params.dimensions.erase(std::remove_if(params.dimensions.begin(),
                                       params.dimensions.end(),
                                       [&](const SketchDimension &d) {
                                         return d.kind == "polygon_radius";
                                       }),
                        params.dimensions.end());
params.polygons.clear();

// Update other lines that shared the old point IDs.  If the other
// line's endpoint is still coincident with the trimmed line's new
// endpoint, they continue to share.  Otherwise re-resolve the point
// against everything else — the old code minted a fresh vertex id
// here unconditionally, silently breaking the shared point without
// any coincident constraint to replace it.
auto update_shared_pt =
    [&](std::string &pt_id, double pt_x, double pt_y, const std::string &old_id,
        const std::string &new_id, double new_x, double new_y) {
      if (pt_id != old_id)
        return;
      if (points_match(pt_x, pt_y, new_x, new_y)) {
        pt_id = new_id;
        return;
      }
      const auto resolved = resolve_shared_point(params, pt_x, pt_y,
                                                 excluded_entities);
      pt_id = resolved.has_value() ? std::get<0>(resolved.value())
                                   : mint_vertex_id();
    };

for (auto &ol : params.lines) {
  if (ol.id == entity_id)
    continue;
  update_shared_pt(ol.start_vertex_id, ol.start_x, ol.start_y, old_sp_id, new_sp,
                   line_it->start_x, line_it->start_y);
  update_shared_pt(ol.end_vertex_id, ol.end_x, ol.end_y, old_sp_id, new_sp,
                   line_it->start_x, line_it->start_y);
  update_shared_pt(ol.start_vertex_id, ol.start_x, ol.start_y, old_ep_id, new_ep,
                   line_it->end_x, line_it->end_y);
  update_shared_pt(ol.end_vertex_id, ol.end_x, ol.end_y, old_ep_id, new_ep,
                   line_it->end_x, line_it->end_y);
}
