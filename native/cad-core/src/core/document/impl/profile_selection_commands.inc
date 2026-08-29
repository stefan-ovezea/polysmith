DocumentState DocumentManager::select_sketch_profile(const std::string& profile_id,
                                                     bool additive) {
  require_document();

  // Selection of profiles is allowed both inside and outside an active sketch.
  // The owning sketch feature is located by scanning the feature history.
  const auto feature_it = find_sketch_feature_owning_profile(
      document_->feature_history, profile_id);

  if (feature_it == document_->feature_history.end()) {
    throw std::runtime_error("Sketch profile not found: " + profile_id);
  }

  const auto profile_it = std::find_if(
      feature_it->sketch_parameters->profiles.begin(),
      feature_it->sketch_parameters->profiles.end(),
      [&](const SketchProfileRegion& profile) { return profile.id == profile_id; });

  if (profile_it == feature_it->sketch_parameters->profiles.end()) {
    throw std::runtime_error("Sketch profile not found: " + profile_id);
  }

  document_->selected_feature_id = feature_it->id;
  document_->selected_sketch_vertex_id = std::nullopt;
  document_->selected_sketch_entity_id = std::nullopt;
  document_->selected_sketch_dimension_id = std::nullopt;
  if (additive) {
    auto& selected = document_->selected_sketch_profile_ids;
    const auto existing = std::find(selected.begin(), selected.end(), profile_id);
    if (existing == selected.end()) {
      selected.push_back(profile_id);
    } else {
      selected.erase(existing);
    }
  } else {
    document_->selected_sketch_profile_ids = {profile_id};
  }
  document_->selected_sketch_profile_id =
      document_->selected_sketch_profile_ids.empty()
          ? std::optional<std::string>{}
          : std::optional<std::string>{document_->selected_sketch_profile_ids.back()};
  return document_.value();
}

namespace {

// A profile owns an entity when the entity appears on its boundary in
// ANY of the region's id lists: the exact walk order (ordered_edge_ids
// / boundary_edges), the legacy line list, or as the source circle of
// a circle-kind region.
bool profile_owns_entity(const SketchProfileRegion& profile,
                         const std::string& entity_id) {
  if (profile.source_circle_id.has_value() &&
      profile.source_circle_id.value() == entity_id) {
    return true;
  }
  const auto id_in = [&](const std::vector<std::string>& ids) {
    return std::find(ids.begin(), ids.end(), entity_id) != ids.end();
  };
  if (id_in(profile.ordered_edge_ids) || id_in(profile.line_ids)) {
    return true;
  }
  return std::any_of(
      profile.boundary_edges.begin(), profile.boundary_edges.end(),
      [&](const ProfileBoundaryEdge& edge) {
        return edge.entity_id == entity_id;
      });
}

}  // namespace

DocumentState DocumentManager::select_sketch_profile_by_entity(
    const std::string& entity_id, bool additive) {
  require_document();

  // The clicked entity belongs to one sketch feature; scan the history
  // for the owner (same doctrine as the profile lookup).
  const auto feature_it = std::find_if(
      document_->feature_history.begin(), document_->feature_history.end(),
      [&](const FeatureEntry& feature) {
        if (feature.kind != "sketch" ||
            !feature.sketch_parameters.has_value()) {
          return false;
        }
        const auto& sketch = feature.sketch_parameters.value();
        const auto owns = [&](const auto& entities) {
          return std::any_of(entities.begin(), entities.end(),
                             [&](const auto& entity) {
                               return entity.id == entity_id;
                             });
        };
        return owns(sketch.lines) || owns(sketch.circles) ||
               owns(sketch.arcs) || owns(sketch.ellipses) ||
               owns(sketch.splines);
      });

  if (feature_it == document_->feature_history.end()) {
    throw std::runtime_error("Sketch entity not found: " + entity_id);
  }

  std::vector<std::string> owning_profile_ids;
  for (const auto& profile : feature_it->sketch_parameters->profiles) {
    if (profile_owns_entity(profile, entity_id)) {
      owning_profile_ids.push_back(profile.id);
    }
  }
  if (owning_profile_ids.empty()) {
    throw std::runtime_error(
        "Sketch entity is not on a profile boundary: " + entity_id);
  }

  document_->selected_feature_id = feature_it->id;
  document_->selected_sketch_vertex_id = std::nullopt;
  document_->selected_sketch_entity_id = std::nullopt;
  document_->selected_sketch_dimension_id = std::nullopt;
  if (additive) {
    for (const auto& profile_id : owning_profile_ids) {
      auto& selected = document_->selected_sketch_profile_ids;
      const auto existing =
          std::find(selected.begin(), selected.end(), profile_id);
      if (existing == selected.end()) {
        selected.push_back(profile_id);
      } else {
        selected.erase(existing);
      }
    }
  } else {
    document_->selected_sketch_profile_ids = owning_profile_ids;
  }
  document_->selected_sketch_profile_id =
      document_->selected_sketch_profile_ids.empty()
          ? std::optional<std::string>{}
          : std::optional<std::string>{
                document_->selected_sketch_profile_ids.back()};
  return document_.value();
}
