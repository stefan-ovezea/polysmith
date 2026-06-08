void add_sketch_rectangle(FeatureEntry &feature, int &next_line_index,
                          double start_x, double start_y, double end_x,
                          double end_y, bool is_construction) {
  // Stash the indices used for each side so we can build the
  // line-id pair for the equal-length relations after the loop.
  // `add_sketch_line` itself reads the line id from `line-{index}`,
  // so the only way to know the resulting ids is to capture the
  // counter values we passed in.
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
  // sides are exactly axis-aligned by construction. What's missing
  // is the equal-length pairing: top↔bottom and left↔right. Adding
  // these two relations means editing one side's length dimension
  // also updates its mirror, matching common CAD workflow's behavior.
  if (!feature.sketch_parameters.has_value()) {
    return;
  }
  auto &parameters = *feature.sketch_parameters;
  const std::string top_id = "line-" + std::to_string(top_index);
  const std::string right_id = "line-" + std::to_string(right_index);
  const std::string bottom_id = "line-" + std::to_string(bottom_index);
  const std::string left_id = "line-" + std::to_string(left_index);

  parameters.dimensions.erase(
      std::remove_if(parameters.dimensions.begin(), parameters.dimensions.end(),
                     [&](const SketchDimension &dimension) {
                       return dimension.kind == "line_angle" &&
                              (dimension.entity_id == top_id ||
                               dimension.entity_id == right_id ||
                               dimension.entity_id == bottom_id ||
                               dimension.entity_id == left_id);
                     }),
      parameters.dimensions.end());

  parameters.line_relations.push_back(SketchLineRelation{
      .id = "rel-equal-length-" + top_id,
      .kind = "equal_length",
      .first_line_id = top_id,
      .second_line_id = bottom_id,
  });
  parameters.line_relations.push_back(SketchLineRelation{
      .id = "rel-equal-length-" + left_id,
      .kind = "equal_length",
      .first_line_id = left_id,
      .second_line_id = right_id,
  });
  refresh_sketch_derived_state(feature);
}
