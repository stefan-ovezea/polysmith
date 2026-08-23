// Attaches circle-slave tangent relations (kind "tangent_circle_line")
// from a circle created by the circle tool's tangent modes to each of
// its defining lines. `enforce_tangent_circle_line_relations` keeps
// the circle tangent by re-deriving its radius on every refresh.
void attach_circle_tangent_relations(
    FeatureEntry& feature,
    const std::string& circle_id,
    const std::vector<std::string>& line_ids) {
  if (feature.kind != "sketch" || !feature.sketch_parameters.has_value()) {
    throw std::runtime_error("Only sketch features accept circle tangents");
  }
  auto& parameters = *feature.sketch_parameters;
  for (const auto& line_id : line_ids) {
    const auto line_it = std::find_if(
        parameters.lines.begin(), parameters.lines.end(),
        [&](const SketchLine& line) { return line.id == line_id; });
    if (line_it == parameters.lines.end()) {
      throw std::runtime_error(
          "Circle tangent: defining line not found: " + line_id);
    }
    parameters.line_relations.push_back(SketchLineRelation{
        .id = "rel-circle-tangent-" + circle_id + "-" + line_id,
        .kind = "tangent_circle_line",
        .first_line_id = circle_id,
        .second_line_id = line_id,
    });
  }
}
