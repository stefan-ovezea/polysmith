std::vector<std::string> collect_line_ids_for_point(
    const SketchFeatureParameters& parameters,
    const std::string& point_id) {
  std::vector<std::string> line_ids;

  for (const auto& line : parameters.lines) {
    if (line.start_point_id == point_id || line.end_point_id == point_id) {
      line_ids.push_back(line.id);
    }
  }

  return line_ids;
}

void replace_point_id_references(SketchFeatureParameters& parameters,
                                 const std::string& from_point_id,
                                 const std::string& to_point_id) {
  if (from_point_id == to_point_id) {
    return;
  }

  const bool merged_fixed =
      point_is_fixed(parameters, from_point_id) || point_is_fixed(parameters, to_point_id);

  for (auto& line : parameters.lines) {
    if (line.start_point_id == from_point_id) {
      line.start_point_id = to_point_id;
    }
    if (line.end_point_id == from_point_id) {
      line.end_point_id = to_point_id;
    }
  }

  if (SketchVertex* target_point = find_sketch_point(parameters, to_point_id)) {
    target_point->is_fixed = merged_fixed;
  }
}
