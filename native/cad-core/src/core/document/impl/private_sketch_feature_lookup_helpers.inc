std::vector<FeatureEntry>::iterator find_sketch_feature_owning_profile(
    std::vector<FeatureEntry>& features, const std::string& profile_id) {
  return std::find_if(
      features.begin(), features.end(), [&](const FeatureEntry& feature) {
        if (feature.kind != "sketch" || !feature.sketch_parameters.has_value()) {
          return false;
        }
        const auto& profiles = feature.sketch_parameters->profiles;
        return std::any_of(
            profiles.begin(), profiles.end(),
            [&](const SketchProfileRegion& profile) {
              return profile.id == profile_id;
            });
      });
}

std::vector<FeatureEntry>::iterator find_sketch_feature_owning_line(
    std::vector<FeatureEntry>& features, const std::string& line_id) {
  return std::find_if(
      features.begin(), features.end(), [&](const FeatureEntry& feature) {
        if (feature.kind != "sketch" || !feature.sketch_parameters.has_value()) {
          return false;
        }
        const auto& lines = feature.sketch_parameters->lines;
        return std::any_of(
            lines.begin(), lines.end(),
            [&](const SketchLine& line) { return line.id == line_id; });
      });
}

SketchLine* find_line_by_id(SketchFeatureParameters& sketch,
                            const std::string& line_id) {
  const auto line_it = std::find_if(
      sketch.lines.begin(),
      sketch.lines.end(),
      [&](const SketchLine& line) { return line.id == line_id; });
  return line_it == sketch.lines.end() ? nullptr : &(*line_it);
}

const SketchLine* find_line_by_id(const SketchFeatureParameters& sketch,
                                  const std::string& line_id) {
  const auto line_it = std::find_if(
      sketch.lines.begin(),
      sketch.lines.end(),
      [&](const SketchLine& line) { return line.id == line_id; });
  return line_it == sketch.lines.end() ? nullptr : &(*line_it);
}

std::vector<FeatureEntry>::iterator find_sketch_feature_owning_path_entity(
    std::vector<FeatureEntry>& features, const std::string& entity_id) {
  return std::find_if(
      features.begin(), features.end(), [&](const FeatureEntry& feature) {
        if (feature.kind != "sketch" || !feature.sketch_parameters.has_value()) {
          return false;
        }
        const auto& sketch = feature.sketch_parameters.value();
        const bool owns_line = std::any_of(
            sketch.lines.begin(), sketch.lines.end(),
            [&](const SketchLine& line) { return line.id == entity_id; });
        const bool owns_arc = std::any_of(
            sketch.arcs.begin(), sketch.arcs.end(),
            [&](const SketchArc& arc) { return arc.id == entity_id; });
        return owns_line || owns_arc;
      });
}
