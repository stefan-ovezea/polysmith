void add_sketch_rectangle(FeatureEntry &feature, int &next_line_index,
                          double start_x, double start_y, double end_x,
                          double end_y, bool is_construction) {
  const int top_index = next_line_index++;
  const int right_index = next_line_index++;
  const int bottom_index = next_line_index++;
  const int left_index = next_line_index++;

  add_sketch_line(feature, top_index, start_x, start_y, end_x, start_y,
                  is_construction);
  add_sketch_line(feature, right_index, end_x, start_y, end_x, end_y,
                  is_construction);
  add_sketch_line(feature, bottom_index, end_x, end_y, start_x, end_y,
                  is_construction);
  add_sketch_line(feature, left_index, start_x, end_y, start_x, start_y,
                  is_construction);

  // H/V constraints on each side are already inferred by
  // `add_sketch_line` (via `infer_constraint_hint`) because the
  // sides are exactly axis-aligned by construction.  Coincident
  // endpoints + H/V constraints are sufficient to keep the shape
  // rectangular — no extra relations needed (matching Fusion 360
  // behaviour, which only adds H/V constraints).
  if (!feature.sketch_parameters.has_value()) {
    return;
  }
  auto &parameters = *feature.sketch_parameters;

  // Strip the auto-generated line_angle dimensions — they are
  // redundant with the H/V constraints (already marked driven).
  parameters.dimensions.erase(
      std::remove_if(parameters.dimensions.begin(), parameters.dimensions.end(),
                     [&](const SketchDimension &dimension) {
                       return dimension.kind == "line_angle" &&
                              (dimension.entity_id ==
                                   "line-" + std::to_string(top_index) ||
                               dimension.entity_id ==
                                   "line-" + std::to_string(right_index) ||
                               dimension.entity_id ==
                                   "line-" + std::to_string(bottom_index) ||
                               dimension.entity_id ==
                                   "line-" + std::to_string(left_index));
                     }),
      parameters.dimensions.end());

  refresh_sketch_derived_state(feature);
}
