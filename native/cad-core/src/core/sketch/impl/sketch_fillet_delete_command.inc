void delete_sketch_fillet(FeatureEntry &feature, const std::string &fillet_id) {
  if (feature.kind != "sketch" || !feature.sketch_parameters.has_value()) {
    throw std::runtime_error("Only sketch features can delete sketch fillets");
  }

  auto &parameters = *feature.sketch_parameters;
  const auto fillet_it = std::find_if(
      parameters.fillets.begin(), parameters.fillets.end(),
      [&](const SketchFillet &fillet) { return fillet.id == fillet_id; });
  if (fillet_it == parameters.fillets.end()) {
    throw std::runtime_error("Sketch fillet not found: " + fillet_id);
  }

  const SketchFillet fillet_copy = *fillet_it;

  // Restore from the fillet's cached corner coords (refreshed on
  // every recompute) rather than re-reading the points table. The
  // points table is rebuilt from the fillet on every refresh, so
  // these two values are always in sync; the cache is what makes
  // the round-trip work even when no other entity references the
  // corner anymore.
  const double corner_x = fillet_copy.corner_x;
  const double corner_y = fillet_copy.corner_y;

  // Restore line A's filleted endpoint (the trim point) back to the
  // corner point. Same for line B.
  const auto restore_line = [&](const std::string &line_id,
                                const std::string &trim_point_id) {
    const auto line_it = std::find_if(
        parameters.lines.begin(), parameters.lines.end(),
        [&](const SketchLine &line) { return line.id == line_id; });
    if (line_it == parameters.lines.end()) {
      return;
    }
    if (line_start_matches(*line_it, trim_point_id)) {
      line_it->start_vertex_id = fillet_copy.corner_vertex_id;
      line_it->start_x = corner_x;
      line_it->start_y = corner_y;
    } else if (line_it->end_vertex_id == trim_point_id) {
      line_it->end_vertex_id = fillet_copy.corner_vertex_id;
      line_it->end_x = corner_x;
      line_it->end_y = corner_y;
    }
  };
  restore_line(fillet_copy.line_a_id, fillet_copy.trim_a_vertex_id);
  restore_line(fillet_copy.line_b_id, fillet_copy.trim_b_vertex_id);

  // Remove the generated arc.
  parameters.arcs.erase(std::remove_if(parameters.arcs.begin(),
                                       parameters.arcs.end(),
                                       [&](const SketchArc &arc) {
                                         return arc.id == fillet_copy.arc_id;
                                       }),
                        parameters.arcs.end());

  // Remove the fillet record itself; trim points get cleaned up on
  // the next `rebuild_sketch_vertices` because nothing references
  // them anymore.
  parameters.fillets.erase(std::remove_if(parameters.fillets.begin(),
                                          parameters.fillets.end(),
                                          [&](const SketchFillet &fillet) {
                                            return fillet.id == fillet_id;
                                          }),
                           parameters.fillets.end());

  refresh_sketch_derived_state(feature);
}
