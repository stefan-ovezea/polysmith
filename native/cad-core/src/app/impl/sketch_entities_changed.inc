// Builds the phase-D1/D2 reply for a command that changed sketch entities.
//
// `before` is the document as it was before the command ran (the manager hands
// out copies), `after` what it returned.  Vertices and lines store their own
// coordinates, so the diff needs no serialisation.  Returns true when the event
// was written -- the caller then stops; false means "send the full state".
inline bool emit_sketch_entities_changed(
    const std::string& command_id,
    const std::optional<polysmith::core::DocumentState>& before,
    const polysmith::core::DocumentState& after) {
  const std::string active_id = after.active_sketch_feature_id.value_or("");
  if (!before.has_value() || active_id.empty()) {
    return false;
  }
  const polysmith::core::FeatureEntry* before_feature = nullptr;
  const polysmith::core::FeatureEntry* after_feature = nullptr;
  for (const auto& feature : before->feature_history) {
    if (feature.id == active_id) {
      before_feature = &feature;
    }
  }
  for (const auto& feature : after.feature_history) {
    if (feature.id == active_id) {
      after_feature = &feature;
    }
  }
  if (before_feature == nullptr || after_feature == nullptr ||
      !before_feature->sketch_parameters.has_value() ||
      !after_feature->sketch_parameters.has_value()) {
    return false;
  }
  const auto& before_params = before_feature->sketch_parameters.value();
  const auto& after_params = after_feature->sketch_parameters.value();

  polysmith::protocol::json changed_vertices = polysmith::protocol::json::array();
  polysmith::protocol::json changed_lines = polysmith::protocol::json::array();
  polysmith::protocol::json removed_ids = polysmith::protocol::json::array();

  for (const auto& vertex : after_params.vertices) {
    const polysmith::core::SketchVertex* previous = nullptr;
    for (const auto& candidate : before_params.vertices) {
      if (candidate.vertex_id == vertex.vertex_id) {
        previous = &candidate;
        break;
      }
    }
    if (previous == nullptr || previous->x != vertex.x ||
        previous->y != vertex.y) {
      changed_vertices.push_back({
          {"vertex_id", vertex.vertex_id},
          {"x", vertex.x},
          {"y", vertex.y},
      });
    }
  }
  for (const auto& vertex : before_params.vertices) {
    bool still_there = false;
    for (const auto& candidate : after_params.vertices) {
      if (candidate.vertex_id == vertex.vertex_id) {
        still_there = true;
        break;
      }
    }
    if (!still_there) {
      removed_ids.push_back(vertex.vertex_id);
    }
  }
  for (const auto& line : after_params.lines) {
    const polysmith::core::SketchLine* previous = nullptr;
    for (const auto& candidate : before_params.lines) {
      if (candidate.id == line.id) {
        previous = &candidate;
        break;
      }
    }
    if (previous == nullptr || previous->start_x != line.start_x ||
        previous->start_y != line.start_y || previous->end_x != line.end_x ||
        previous->end_y != line.end_y) {
      changed_lines.push_back({
          {"line_id", line.id},
          {"start_x", line.start_x},
          {"start_y", line.start_y},
          {"end_x", line.end_x},
          {"end_y", line.end_y},
      });
    }
  }
  for (const auto& line : before_params.lines) {
    bool still_there = false;
    for (const auto& candidate : after_params.lines) {
      if (candidate.id == line.id) {
        still_there = true;
        break;
      }
    }
    if (!still_there) {
      removed_ids.push_back(line.id);
    }
  }

  if (changed_vertices.empty() && changed_lines.empty() && removed_ids.empty()) {
    return false;
  }
  polysmith::protocol::write_message(
      polysmith::protocol::make_sketch_entities_changed_event(
          command_id,
          polysmith::protocol::json{
              {"revision", after.revision},
              {"feature_id", active_id},
              {"lines", changed_lines},
              {"vertices", changed_vertices},
              {"removed", removed_ids},
              {"derived",
               polysmith::protocol::json{
                   {"profile_count", after_params.profiles.size()},
                   {"vertex_count", after_params.vertices.size()},
               }},
          }));
  return true;
}
