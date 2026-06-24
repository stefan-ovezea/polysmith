void add_sketch_distance_dimension(FeatureEntry& feature,
                                   const std::string& first_entity_id,
                                   const std::string& second_entity_id) {
  if (feature.kind != "sketch" || !feature.sketch_parameters.has_value()) {
    throw std::runtime_error(
        "Only sketch features can hold distance dimensions");
  }
  if (first_entity_id == second_entity_id) {
    throw std::runtime_error("Distance dimension requires two distinct entities");
  }

  auto& parameters = *feature.sketch_parameters;
  const bool first_is_circle = first_entity_id.rfind("circle-", 0) == 0;
  const bool second_is_circle = second_entity_id.rfind("circle-", 0) == 0;
  const bool first_is_line = first_entity_id.rfind("line-", 0) == 0;
  const bool second_is_line = second_entity_id.rfind("line-", 0) == 0;

  std::string kind;
  std::string driven_entity_id;
  std::string reference_entity_id;
  double value = 0.0;
  if (first_is_circle && second_is_circle) {
    const auto& first_circle = require_circle(parameters, first_entity_id);
    const auto& second_circle = require_circle(parameters, second_entity_id);
    kind = "circle_center_distance";
    driven_entity_id = second_entity_id;
    reference_entity_id = first_entity_id;
    value = distance_between_circles(first_circle, second_circle);
  } else if ((first_is_circle && second_is_line) ||
             (first_is_line && second_is_circle)) {
    const std::string circle_id = first_is_circle ? first_entity_id : second_entity_id;
    const std::string line_id = first_is_line ? first_entity_id : second_entity_id;
    const auto& circle = require_circle(parameters, circle_id);
    const auto& line = require_line(parameters, line_id);
    kind = "circle_line_distance";
    driven_entity_id = circle_id;
    reference_entity_id = line_id;
    value = std::abs(signed_circle_line_distance(circle, line));
  } else if (first_is_line && second_is_line) {
    const auto& first_line = require_line(parameters, first_entity_id);
    const auto& second_line = require_line(parameters, second_entity_id);
    kind = "line_line_distance";
    driven_entity_id = second_entity_id;
    reference_entity_id = first_entity_id;
    value = std::abs(signed_line_line_distance(second_line, first_line));
  } else {
    throw std::runtime_error(
        "Distance dimensions currently support line-line, circle-circle, or circle-line picks");
  }

  if (value <= kMinimumSketchDimensionValue) {
    throw std::runtime_error("Distance dimension must be greater than zero");
  }

  const auto duplicate_it = std::find_if(
      parameters.dimensions.begin(),
      parameters.dimensions.end(),
      [&](const SketchDimension& dim) {
        return dim.kind == kind && dim.entity_id == driven_entity_id &&
               dim.secondary_entity_id == reference_entity_id;
      });
  if (duplicate_it != parameters.dimensions.end()) {
    return;
  }

  const std::string dim_id =
      "dim-" + kind + "-" + driven_entity_id + "-" + reference_entity_id;
  parameters.dimensions.push_back(SketchDimension{
      .id = dim_id,
      .kind = kind,
      .entity_id = driven_entity_id,
      .secondary_entity_id = reference_entity_id,
      .value = value,
  });
  finalize_new_dimension(feature, parameters, dim_id);
}
