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

  if (!feature.sketch_parameters.has_value()) {
    return;
  }
  auto &parameters = *feature.sketch_parameters;

  const std::string top_id = "line-" + std::to_string(top_index);
  const std::string right_id = "line-" + std::to_string(right_index);
  const std::string bottom_id = "line-" + std::to_string(bottom_index);
  const std::string left_id = "line-" + std::to_string(left_index);

  // Strip the auto-generated line_angle dimensions — they are
  // redundant with the H/V constraints (already marked driven).
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

  // A rectangle is defined by its ANGLES and by opposite sides being
  // parallel — not by axis-locking all four sides. `add_sketch_line`
  // inferred H/V on every side because the sides are drawn
  // axis-aligned; keep exactly ONE of those statements (the bottom
  // horizontal, which pins the orientation but does not forbid the
  // shape from being re-angled later) and state the rest as relations:
  //
  //   horizontal(bottom)
  //   parallel(top, bottom)
  //   parallel(left, right)
  //   perpendicular(bottom, right)     ← the 90° that makes it a rectangle
  //
  // Four independent constraints over eight coordinates, so four
  // degrees of freedom remain: x, y, width, height. That is exactly
  // the set Fusion 360 puts on a rectangle, and unlike four axis
  // constraints it shows up as "these sides are parallel" and "this
  // corner is square" — an angle the user can see and reason about.
  for (auto &line : parameters.lines) {
    if (line.id == bottom_id) {
      line.constraint = std::string("horizontal");
    } else if (line.id == top_id || line.id == right_id ||
               line.id == left_id) {
      line.constraint = std::nullopt;
    }
  }

  // Relation ids follow the same scheme as the explicit constraint
  // commands (`rel-<kind>-<a>-<b>`, ids sorted) so deleting a side's
  // relation from the UI finds them by id.
  auto add_relation = [&](const char *kind, const std::string &a,
                          const std::string &b) {
    const std::string &first = a < b ? a : b;
    const std::string &second = a < b ? b : a;
    parameters.line_relations.push_back(SketchLineRelation{
        .id = "rel-" + std::string(kind) + "-" + first + "-" + second,
        .kind = kind,
        .first_line_id = a,
        .second_line_id = b,
    });
  };
  add_relation("parallel", top_id, bottom_id);
  add_relation("parallel", left_id, right_id);
  add_relation("perpendicular", bottom_id, right_id);

  refresh_sketch_derived_state(feature);
}
