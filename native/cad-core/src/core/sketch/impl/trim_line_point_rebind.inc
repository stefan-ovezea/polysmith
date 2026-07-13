// Share point IDs at the new endpoints with coincident geometry
// so profile loops stay connected after trim.  Mirrors the
// circle→arc path (find_coincident_endpoint + shared/fallback IDs).
const int fresh_idx = static_cast<int>(params.lines.size()) * 10 +
                      static_cast<int>(params.arcs.size()) * 10 + 1000;

const auto shared_start = find_coincident_endpoint(
    params, entity_id, line_it->start_x, line_it->start_y);
const auto shared_end =
    find_coincident_endpoint(params, entity_id, line_it->end_x, line_it->end_y);

const std::string new_sp =
    shared_start.has_value()
        ? std::get<0>(shared_start.value())
        : "vertex-" + std::to_string(params.next_vertex_index++);
const std::string new_ep =
    shared_end.has_value() ? std::get<0>(shared_end.value())
                           : "vertex-" + std::to_string(params.next_vertex_index++);

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
// endpoint, they continue to share.  Otherwise orphan the old ref.
int next_fresh = fresh_idx + 1;
auto update_shared_pt =
    [&](std::string &pt_id, double pt_x, double pt_y, const std::string &old_id,
        const std::string &new_id, double new_x, double new_y) {
      if (pt_id != old_id)
        return;
      if (points_match(pt_x, pt_y, new_x, new_y)) {
        pt_id = new_id;
      } else {
        pt_id = "vertex-" + std::to_string(params.next_vertex_index++);
      }
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
