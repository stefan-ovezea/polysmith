void delete_sketch_chamfer(FeatureEntry &feature, const std::string &chamfer_id) {
  if (feature.kind != "sketch" || !feature.sketch_parameters.has_value()) {
    throw std::runtime_error("Only sketch features can delete sketch chamfers");
  }

  auto &parameters = *feature.sketch_parameters;
  const auto chamfer_it = std::find_if(
      parameters.chamfers.begin(), parameters.chamfers.end(),
      [&](const SketchChamfer &chamfer) { return chamfer.id == chamfer_id; });
  if (chamfer_it == parameters.chamfers.end()) {
    throw std::runtime_error("Sketch chamfer not found: " + chamfer_id);
  }

  const SketchChamfer chamfer_copy = *chamfer_it;

  // Restore from the chamfer's cached corner coords (refreshed on
  // every recompute) rather than re-reading the points table — the
  // same rationale as the fillet delete.
  const double corner_x = chamfer_copy.corner_x;
  const double corner_y = chamfer_copy.corner_y;

  // Restore line A's trimmed endpoint back to the corner point. Same
  // for line B.
  const auto restore_line = [&](const std::string &line_id,
                                const std::string &trim_point_id) {
    const auto line_it = std::find_if(
        parameters.lines.begin(), parameters.lines.end(),
        [&](const SketchLine &line) { return line.id == line_id; });
    if (line_it == parameters.lines.end()) {
      return;
    }
    if (line_start_matches(*line_it, trim_point_id)) {
      line_it->start_vertex_id = chamfer_copy.corner_vertex_id;
      line_it->start_x = corner_x;
      line_it->start_y = corner_y;
    } else if (line_it->end_vertex_id == trim_point_id) {
      line_it->end_vertex_id = chamfer_copy.corner_vertex_id;
      line_it->end_x = corner_x;
      line_it->end_y = corner_y;
    }
  };
  restore_line(chamfer_copy.line_a_id, chamfer_copy.trim_a_vertex_id);
  restore_line(chamfer_copy.line_b_id, chamfer_copy.trim_b_vertex_id);

  // Remove the generated chamfer line.
  parameters.lines.erase(std::remove_if(parameters.lines.begin(),
                                        parameters.lines.end(),
                                        [&](const SketchLine &line) {
                                          return line.id ==
                                                 chamfer_copy.chamfer_line_id;
                                        }),
                         parameters.lines.end());

  // Remove the chamfer record itself; trim points get cleaned up on
  // the next `rebuild_sketch_vertices` because nothing references
  // them anymore.
  parameters.chamfers.erase(
      std::remove_if(parameters.chamfers.begin(), parameters.chamfers.end(),
                     [&](const SketchChamfer &chamfer) {
                       return chamfer.id == chamfer_id;
                     }),
      parameters.chamfers.end());

  refresh_sketch_derived_state(feature);
}
