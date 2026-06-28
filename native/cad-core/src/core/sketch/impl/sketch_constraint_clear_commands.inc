void clear_sketch_line_constraints(FeatureEntry& feature,
                                   const std::string& line_id) {
  if (feature.kind != "sketch" || !feature.sketch_parameters.has_value()) {
    throw std::runtime_error("Only sketch features can clear line constraints");
  }

  auto& parameters = feature.sketch_parameters.value();
  auto& line = require_line(parameters, line_id);

  // Clear the inline H/V constraint.
  line.constraint = std::nullopt;
  line.constraint_driven = false;

  // Clear every line‑line relation that involves this line.
  remove_line_relations_for_line(parameters, "equal_length", line_id);
  remove_line_relations_for_line(parameters, "perpendicular", line_id);
  remove_line_relations_for_line(parameters, "parallel", line_id);
  remove_line_relations_for_line(parameters, "tangent_line_circle", line_id);

  // Clear midpoint and point‑line anchors that reference this line.
  parameters.midpoint_anchors.erase(
      std::remove_if(parameters.midpoint_anchors.begin(),
                     parameters.midpoint_anchors.end(),
                     [&](const SketchMidpointAnchor& a) {
                       return a.line_id == line_id;
                     }),
      parameters.midpoint_anchors.end());
  parameters.point_line_anchors.erase(
      std::remove_if(parameters.point_line_anchors.begin(),
                     parameters.point_line_anchors.end(),
                     [&](const SketchPointLineAnchor& a) {
                       return a.line_id == line_id;
                     }),
      parameters.point_line_anchors.end());

  sync_all_line_dimensions(parameters);
  refresh_sketch_derived_state(feature);
}
