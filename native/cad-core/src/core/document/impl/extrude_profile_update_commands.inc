DocumentState DocumentManager::update_extrude_profiles(
    const std::string& feature_id,
    const std::vector<std::string>& profile_ids) {
  require_document();

  const auto extrude_it = std::find_if(
      document_->feature_history.begin(),
      document_->feature_history.end(),
      [&](const FeatureEntry& feature) { return feature.id == feature_id; });
  if (extrude_it == document_->feature_history.end()) {
    throw std::runtime_error("Feature not found: " + feature_id);
  }
  if (extrude_it->kind != "extrude" ||
      !extrude_it->extrude_parameters.has_value()) {
    throw std::runtime_error(
        "update_extrude_profiles requires an extrude feature: " + feature_id);
  }

  if (profile_ids.empty()) {
    // Deselecting every surface empties the extrude.  Clear the profile
    // geometry so the body compiler emits no shape, but keep the feature in
    // the timeline so the panel can show "0 selected" and a later re-select
    // rebuilds it through the normal parameter builder.
    auto& params = extrude_it->extrude_parameters.value();
    params.profile_id.clear();
    params.profile_ids.clear();
    params.profile_points.clear();
    params.inner_loops.clear();
    params.additional_profile_points.clear();
    params.additional_inner_loops.clear();
    params.sketch_lines.clear();
    params.sketch_arcs.clear();
    params.sketch_circles.clear();
    params.sketch_edge_ids.clear();
    params.boundary_edges.clear();
    params.arc_descriptors.clear();
    params.inner_loop_edges.clear();
    params.additional_sketch_edge_ids.clear();
    params.additional_arc_descriptors.clear();
    params.additional_boundary_edges.clear();
    bump_geometry_revision();
    return document_.value();
  }

  const auto sketch_it = find_sketch_feature_owning_profile(
      document_->feature_history, profile_ids.front());
  if (sketch_it == document_->feature_history.end()) {
    throw std::runtime_error("Sketch profile not found: " + profile_ids.front());
  }

  const auto& sketch = sketch_it->sketch_parameters.value();
  std::vector<SketchProfileRegion> selected_profiles;
  for (const auto& profile_id : profile_ids) {
    const auto owner_it = find_sketch_feature_owning_profile(
        document_->feature_history, profile_id);
    if (owner_it == document_->feature_history.end() ||
        owner_it->id != sketch_it->id) {
      throw std::runtime_error(
          "Selected profiles must belong to the same sketch plane");
    }
    // A stored id is a reference: the panel sends live ids, but an edit between
    // the click and the update can rewrite one, and the strict matcher is what
    // used to kill an otherwise valid re-bind.
    const SketchProfileRegion* profile =
        resolve_stored_profile(sketch.profiles, profile_id);
    if (profile == nullptr) {
      throw std::runtime_error("Sketch profile not found: " + profile_id);
    }
    selected_profiles.push_back(*profile);
  }

  const std::string target_id = extrude_it->id;
  const std::string operation_id = extrude_it->operation_id.empty()
                                       ? target_id
                                       : extrude_it->operation_id;
  const double depth = extrude_it->extrude_parameters->depth;
  const std::string mode = extrude_it->extrude_parameters->mode;
  const std::optional<std::string> target_body_id =
      extrude_it->extrude_parameters->target_body_id;

  // The SAME grouping rule as creation. Selecting surfaces before arming Extrude
  // and selecting them after it must produce the same model; this path used to
  // put every region into one feature, i.e. one body holding two disconnected
  // cylinders, while creation split them into one body each.
  const std::vector<std::vector<SketchProfileRegion>> profile_groups =
      extrude_profile_groups(selected_profiles, mode, target_body_id);

  std::vector<ExtrudeFeatureParameters> group_parameters;
  group_parameters.reserve(profile_groups.size());
  for (const auto& group : profile_groups) {
    // Delegate to the shared parameter builder — it handles multi-profile
    // groups, sets up additional profiles and inner loops, and deliberately
    // leaves sketch curve data empty so the shape builder routes through
    // the polygon-prism path (which handles every profile in the group).
    const std::optional<ExtrudeFeatureParameters> built_parameters =
        make_extrude_parameters_for_profiles(*sketch_it, group, depth);
    if (!built_parameters.has_value()) {
      throw std::runtime_error("Sketch profile not found");
    }
    // Copy the existing parameters and overwrite only what the profile
    // determines. Re-binding used to force `side1.distance` back to `depth` and
    // drop everything else the user had set (two-sided/symmetric extents, thin
    // walls, targets, taper).
    ExtrudeFeatureParameters next_parameters = *extrude_it->extrude_parameters;
    preserve_extrude_source_geometry(next_parameters, *built_parameters);
    next_parameters.depth = depth;
    group_parameters.push_back(std::move(next_parameters));
  }

  const auto apply_group = [&](FeatureEntry& feature, size_t group_index) {
    feature.extrude_parameters = group_parameters[group_index];
    feature.operation_id = operation_id;
    // This is the repair path for a feature that lost a source region: a
    // successful re-bind must clear the stale failure state, or the body stays
    // suppressed and confirming the panel looks like it did nothing.
    feature.status = "healthy";
    feature.dependency_broken = false;
    feature.dependency_warning.clear();
    feature.parameters_summary =
        feature.extrude_parameters->profile_id + " · " +
        std::to_string(feature.extrude_parameters->depth) + " mm";
  };

  // Reuse the operation's existing features as slots (the target first), so a
  // shrinking or growing selection keeps body ids stable instead of churning
  // them and invalidating every downstream reference.
  const auto index_of = [&](const std::string& id) -> std::optional<size_t> {
    for (size_t index = 0; index < document_->feature_history.size(); ++index) {
      if (document_->feature_history[index].id == id) {
        return index;
      }
    }
    return std::nullopt;
  };
  std::vector<std::string> members{target_id};
  for (const auto& feature : document_->feature_history) {
    if (feature.id != target_id && feature.kind == "extrude" &&
        feature.extrude_parameters.has_value() &&
        feature.operation_id == operation_id) {
      members.push_back(feature.id);
    }
  }

  for (size_t group_index = 0; group_index < group_parameters.size();
       ++group_index) {
    if (group_index < members.size()) {
      const auto slot_index = index_of(members[group_index]);
      if (slot_index.has_value()) {
        apply_group(document_->feature_history[slot_index.value()], group_index);
      }
      continue;
    }
    FeatureEntry created =
        create_extrude_feature(next_feature_id_++, group_parameters[group_index]);
    created.operation_id = operation_id;
    document_->feature_history.push_back(std::move(created));
  }

  // Fewer groups than before: drop the members that no longer carry a group.
  for (size_t group_index = group_parameters.size(); group_index < members.size();
       ++group_index) {
    document_->feature_history.erase(
        std::remove_if(document_->feature_history.begin(),
                       document_->feature_history.end(),
                       [&](const FeatureEntry& feature) {
                         return feature.id == members[group_index];
                       }),
        document_->feature_history.end());
  }

  // Keep the operation's features ADJACENT in history, right where the target
  // was: the timeline folds consecutive members into one row, and history order
  // must match what selecting the same surfaces BEFORE creating produces (the
  // new body would otherwise be appended at the end of the history and read as
  // a second operation).
  const auto target_position = index_of(target_id);
  if (target_position.has_value()) {
    std::vector<FeatureEntry> owned;
    for (auto it = document_->feature_history.begin();
         it != document_->feature_history.end();) {
      const bool is_member =
          it->id == target_id ||
          (it->kind == "extrude" && it->operation_id == operation_id);
      if (is_member) {
        owned.push_back(std::move(*it));
        it = document_->feature_history.erase(it);
      } else {
        ++it;
      }
    }
    const size_t insert_at =
        std::min(target_position.value(), document_->feature_history.size());
    document_->feature_history.insert(
        document_->feature_history.begin() +
            static_cast<std::vector<FeatureEntry>::difference_type>(insert_at),
        std::make_move_iterator(owned.begin()),
        std::make_move_iterator(owned.end()));
  }

  bump_geometry_revision();
  return document_.value();
}
