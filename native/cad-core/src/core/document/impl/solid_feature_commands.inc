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
  document_->selected_sketch_point_id = std::nullopt;
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

DocumentState DocumentManager::extrude_profile(
    const std::string& profile_id,
    double depth,
    const std::string& mode,
    const std::optional<std::string>& target_body_id,
    const std::optional<ExtrudeFeatureParameters>& parameters) {
  return extrude_profiles({profile_id}, depth, mode, target_body_id, parameters);
}

DocumentState DocumentManager::extrude_profiles(
    const std::vector<std::string>& profile_ids,
    double depth,
    const std::string& mode,
    const std::optional<std::string>& target_body_id,
    const std::optional<ExtrudeFeatureParameters>& parameters) {
  require_document();
  if (profile_ids.empty()) {
    throw std::runtime_error("No sketch profiles selected");
  }

  // Extrusion runs on any sketch profile in the document, even if its parent
  // sketch is finished (i.e. not the active sketch).
  const auto feature_it = find_sketch_feature_owning_profile(
      document_->feature_history, profile_ids.front());

  if (feature_it == document_->feature_history.end()) {
    throw std::runtime_error("Sketch profile not found: " + profile_ids.front());
  }

  const auto& sketch = feature_it->sketch_parameters.value();
  std::vector<SketchProfileRegion> selected_profiles;
  for (const auto& profile_id : profile_ids) {
    const auto owner_it = find_sketch_feature_owning_profile(
        document_->feature_history, profile_id);
    if (owner_it == document_->feature_history.end() ||
        owner_it->id != feature_it->id) {
      throw std::runtime_error(
          "Selected profiles must belong to the same sketch plane");
    }
    const auto profile_it = std::find_if(
        sketch.profiles.begin(), sketch.profiles.end(),
        [&](const SketchProfileRegion& profile) { return profile.id == profile_id; });
    if (profile_it == sketch.profiles.end()) {
      throw std::runtime_error("Sketch profile not found: " + profile_id);
    }
    selected_profiles.push_back(*profile_it);
  }

  const bool automatic_mode = mode.empty();
  std::vector<std::vector<SketchProfileRegion>> profile_groups;
  if (selected_profiles.size() == 1 || mode == "cut" || mode == "intersect" ||
      (mode == "join" && target_body_id.has_value())) {
    profile_groups.push_back(selected_profiles);
  } else if (automatic_mode || mode == "join") {
    profile_groups = group_touching_profiles(selected_profiles);
  } else {
    for (const auto& profile : selected_profiles) {
      profile_groups.push_back({profile});
    }
  }

  std::vector<ExtrudeFeatureParameters> extrude_parameter_groups;
  extrude_parameter_groups.reserve(profile_groups.size());
  for (const auto& group : profile_groups) {
    std::optional<ExtrudeFeatureParameters> extrude_parameters =
        make_extrude_parameters_for_profiles(*feature_it, group, depth);
    if (!extrude_parameters.has_value()) {
      throw std::runtime_error("Sketch profile not found");
    }
    if (parameters.has_value()) {
      const auto source = *extrude_parameters;
      *extrude_parameters = parameters.value();
      preserve_extrude_source_geometry(*extrude_parameters, source);
    }
    extrude_parameters->depth = depth;
    extrude_parameters->target_body_id = target_body_id;
    if (automatic_mode) {
      apply_automatic_extrude_mode(*document_,
                                   extrude_parameters.value(),
                                   group.size() > 1);
    } else {
      const bool split_join_without_target =
          mode == "join" && !target_body_id.has_value();
      extrude_parameters->mode = split_join_without_target ? "new_body" : mode;
      if (extrude_parameters->operation == "new_body" &&
          mode != "new_body") {
        extrude_parameters->operation = mode;
      }
      normalize_extrude_operation_mode(extrude_parameters.value());
    }
    normalize_extrude_parameters(*document_, extrude_parameters.value());
    extrude_parameter_groups.push_back(extrude_parameters.value());
  }

  push_undo_state();
  clear_redo_stack();
  for (const auto& extrude_parameters : extrude_parameter_groups) {
    document_->feature_history.push_back(
        create_extrude_feature(next_feature_id_++, extrude_parameters));
  }
  document_->selected_feature_id = document_->feature_history.back().id;
  document_->selected_reference_id = std::nullopt;
  document_->active_sketch_plane_id = std::nullopt;
  document_->active_sketch_feature_id = std::nullopt;
  document_->active_sketch_tool = std::nullopt;
  document_->selected_sketch_point_id = std::nullopt;
  document_->selected_sketch_entity_id = std::nullopt;
  document_->selected_sketch_dimension_id = std::nullopt;
  bump_geometry_revision();
  return document_.value();
}

DocumentState DocumentManager::extrude_open_entities(
    const std::vector<std::string>& entity_ids,
    double depth,
    const std::string& mode,
    const std::optional<std::string>& target_body_id,
    const std::optional<ExtrudeFeatureParameters>& parameters) {
  require_document();
  if (entity_ids.empty()) {
    throw std::runtime_error("No sketch entities selected");
  }

  FeatureEntry* sketch_feature = nullptr;
  std::vector<SketchProfilePoint> chain;
  for (const auto& entity_id : entity_ids) {
    FeatureEntry* owner = nullptr;
    const SketchLine* line = nullptr;
    const SketchArc* arc = nullptr;
    for (auto& feature : document_->feature_history) {
      if (feature.kind != "sketch" || !feature.sketch_parameters.has_value()) {
        continue;
      }
      const auto& sketch = feature.sketch_parameters.value();
      const auto line_it = std::find_if(
          sketch.lines.begin(),
          sketch.lines.end(),
          [&](const SketchLine& candidate) { return candidate.id == entity_id; });
      if (line_it != sketch.lines.end()) {
        owner = &feature;
        line = &(*line_it);
        break;
      }
      const auto arc_it = std::find_if(
          sketch.arcs.begin(),
          sketch.arcs.end(),
          [&](const SketchArc& candidate) { return candidate.id == entity_id; });
      if (arc_it != sketch.arcs.end()) {
        owner = &feature;
        arc = &(*arc_it);
        break;
      }
    }
    if (owner == nullptr) {
      throw std::runtime_error("Sketch entity not found: " + entity_id);
    }
    if (sketch_feature != nullptr && sketch_feature->id != owner->id) {
      throw std::runtime_error(
          "Open thin extrude entities must belong to the same sketch");
    }
    sketch_feature = owner;

    auto append_point = [&](double x, double y) {
      if (!chain.empty()) {
        const auto& last = chain.back();
        const double dx = last.x - x;
        const double dy = last.y - y;
        if (std::sqrt(dx * dx + dy * dy) <= 1.0e-6) {
          return;
        }
      }
      chain.push_back({.x = x, .y = y});
    };

    if (line != nullptr) {
      append_point(line->start_x, line->start_y);
      append_point(line->end_x, line->end_y);
    } else if (arc != nullptr) {
      const double start_angle =
          std::atan2(arc->start_y - arc->center_y, arc->start_x - arc->center_x);
      double end_angle =
          std::atan2(arc->end_y - arc->center_y, arc->end_x - arc->center_x);
      if (arc->ccw && end_angle < start_angle) {
        end_angle += 2.0 * 3.14159265358979323846;
      } else if (!arc->ccw && end_angle > start_angle) {
        end_angle -= 2.0 * 3.14159265358979323846;
      }
      constexpr int kArcSegments = 16;
      for (int index = 0; index <= kArcSegments; ++index) {
        const double t = static_cast<double>(index) / kArcSegments;
        const double angle = start_angle + (end_angle - start_angle) * t;
        append_point(arc->center_x + std::cos(angle) * arc->radius,
                     arc->center_y + std::sin(angle) * arc->radius);
      }
    }
  }

  if (sketch_feature == nullptr || !sketch_feature->sketch_parameters.has_value()) {
    throw std::runtime_error("Open thin extrude source sketch not found");
  }
  if (chain.size() < 2) {
    throw std::runtime_error("Open thin extrude requires a connected chain");
  }

  const auto& sketch = sketch_feature->sketch_parameters.value();
  const std::string plane_id =
      sketch.plane_frame.has_value() ? plane_id_from_frame(sketch.plane_frame.value())
                                     : sketch.plane_id;
  const std::optional<PlaneFrame> plane_frame =
      sketch.plane_frame.has_value()
          ? std::optional<PlaneFrame>(make_plane_frame(sketch.plane_frame.value()))
          : std::nullopt;

  ExtrudeFeatureParameters extrude_parameters =
      parameters.value_or(ExtrudeFeatureParameters{});
  extrude_parameters.sketch_feature_id = sketch_feature->id;
  extrude_parameters.profile_id = "open-chain";
  extrude_parameters.profile_ids = {};
  extrude_parameters.open_entity_ids = entity_ids;
  extrude_parameters.plane_id = plane_id;
  extrude_parameters.plane_frame = plane_frame;
  extrude_parameters.profile_kind = "open_chain";
  extrude_parameters.start_x = 0.0;
  extrude_parameters.start_y = 0.0;
  extrude_parameters.width = 0.0;
  extrude_parameters.height = 0.0;
  extrude_parameters.radius = 0.0;
  extrude_parameters.profile_points = chain;
  extrude_parameters.inner_loops.clear();
  extrude_parameters.additional_profile_points.clear();
  extrude_parameters.additional_inner_loops.clear();
  extrude_parameters.depth = depth;
  extrude_parameters.mode = mode;
  if (mode.empty()) {
    extrude_parameters.mode = "new_body";
    extrude_parameters.operation = "new_body";
  } else if (extrude_parameters.operation == "new_body" && mode != "new_body") {
    extrude_parameters.operation = mode;
  }
  extrude_parameters.target_body_id = target_body_id;
  extrude_parameters.thin.enabled = true;
  normalize_extrude_parameters(*document_, extrude_parameters);

  push_undo_state();
  clear_redo_stack();
  document_->feature_history.push_back(
      create_extrude_feature(next_feature_id_++, extrude_parameters));
  document_->selected_feature_id = document_->feature_history.back().id;
  document_->selected_reference_id = std::nullopt;
  document_->selected_sketch_entity_ids.clear();
  document_->selected_sketch_entity_id = std::nullopt;
  bump_geometry_revision();
  return document_.value();
}

DocumentState DocumentManager::extrude_face(
    const std::string& face_id,
    double depth,
    const std::string& mode,
    const std::optional<std::string>& target_body_id,
    const std::optional<ExtrudeFeatureParameters>& parameters) {
  require_document();

  const auto profile = compute_planar_face_profile(*document_, face_id);
  if (!profile.has_value()) {
    throw std::runtime_error("Selected face is not a supported planar face");
  }

  ExtrudeFeatureParameters extrude_parameters{
      .sketch_feature_id = "",
      .profile_id = "face:" + face_id,
      .profile_ids = {"face:" + face_id},
      .plane_id = plane_id_from_frame(profile->plane_frame),
      .plane_frame = profile->plane_frame,
      .profile_kind = "polygon",
      .start_x = 0.0,
      .start_y = 0.0,
      .width = 0.0,
      .height = 0.0,
      .radius = 0.0,
      .profile_points = profile->outer_points,
      .inner_loops = profile->inner_loops,
      .depth = depth,
      .mode = mode,
      .target_body_id = target_body_id,
  };

  if (parameters.has_value()) {
    const auto source = extrude_parameters;
    extrude_parameters = parameters.value();
    extrude_parameters.sketch_feature_id = source.sketch_feature_id;
    extrude_parameters.profile_id = source.profile_id;
    extrude_parameters.profile_ids = source.profile_ids;
    extrude_parameters.plane_id = source.plane_id;
    extrude_parameters.plane_frame = source.plane_frame;
    extrude_parameters.profile_kind = source.profile_kind;
    extrude_parameters.start_x = source.start_x;
    extrude_parameters.start_y = source.start_y;
    extrude_parameters.width = source.width;
    extrude_parameters.height = source.height;
    extrude_parameters.radius = source.radius;
    extrude_parameters.profile_points = source.profile_points;
    extrude_parameters.inner_loops = source.inner_loops;
    extrude_parameters.additional_profile_points =
        source.additional_profile_points;
    extrude_parameters.additional_inner_loops = source.additional_inner_loops;
  }
  extrude_parameters.depth = depth;
  extrude_parameters.mode = mode;
  extrude_parameters.target_body_id = target_body_id;
  if (mode.empty()) {
    apply_automatic_extrude_mode(*document_, extrude_parameters, false);
  } else {
    if (extrude_parameters.operation == "new_body" && mode != "new_body") {
      extrude_parameters.operation = mode;
    }
    normalize_extrude_operation_mode(extrude_parameters);
  }
  normalize_extrude_parameters(*document_, extrude_parameters);

  push_undo_state();
  clear_redo_stack();
  document_->feature_history.push_back(
      create_extrude_feature(next_feature_id_++, extrude_parameters));
  document_->selected_feature_id = document_->feature_history.back().id;
  document_->selected_reference_id = std::nullopt;
  document_->selected_face_id = std::nullopt;
  document_->active_sketch_plane_id = std::nullopt;
  document_->active_sketch_feature_id = std::nullopt;
  document_->active_sketch_tool = std::nullopt;
  document_->selected_sketch_point_id = std::nullopt;
  document_->selected_sketch_entity_id = std::nullopt;
  document_->selected_sketch_dimension_id = std::nullopt;
  bump_geometry_revision();
  return document_.value();
}

DocumentState DocumentManager::update_extrude_profiles(
    const std::string& feature_id,
    const std::vector<std::string>& profile_ids) {
  require_document();
  if (profile_ids.empty()) {
    throw std::runtime_error("No sketch profiles selected");
  }

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
    const auto profile_it = std::find_if(
        sketch.profiles.begin(), sketch.profiles.end(),
        [&](const SketchProfileRegion& profile) { return profile.id == profile_id; });
    if (profile_it == sketch.profiles.end()) {
      throw std::runtime_error("Sketch profile not found: " + profile_id);
    }
    selected_profiles.push_back(*profile_it);
  }

  const double depth = extrude_it->extrude_parameters->depth;
  const std::string mode = extrude_it->extrude_parameters->mode;
  const auto target_body_id = extrude_it->extrude_parameters->target_body_id;
  std::optional<ExtrudeFeatureParameters> next_parameters;
  if (selected_profiles.size() == 1 && selected_profiles.front().kind == "circle") {
    next_parameters =
        make_extrude_parameters_for_profile(*sketch_it,
                                            selected_profiles.front(),
                                            depth);
  } else {
    const auto& first = selected_profiles.front();
    const std::string plane_id =
        sketch.plane_frame.has_value() ? plane_id_from_frame(sketch.plane_frame.value())
                                       : sketch.plane_id;
    const std::optional<PlaneFrame> plane_frame =
        sketch.plane_frame.has_value()
            ? std::optional<PlaneFrame>(make_plane_frame(sketch.plane_frame.value()))
            : std::nullopt;
    std::vector<std::string> ids;
    for (const auto& profile : selected_profiles) {
      ids.push_back(profile.id);
    }
    next_parameters = ExtrudeFeatureParameters{
        .sketch_feature_id = sketch_it->id,
        .profile_id = ids.front(),
        .profile_ids = ids,
        .plane_id = plane_id,
        .plane_frame = plane_frame,
        .profile_kind = "polygon",
        .start_x = 0.0,
        .start_y = 0.0,
        .width = 0.0,
        .height = 0.0,
        .radius = 0.0,
        .profile_points = first.kind == "circle"
                              ? sample_circle_profile_points(first)
                              : first.points,
        .inner_loops = first.inner_loops,
        .depth = depth,
    };
    for (size_t index = 1; index < selected_profiles.size(); ++index) {
      const auto& profile = selected_profiles[index];
      next_parameters->additional_profile_points.push_back(
          profile.kind == "circle" ? sample_circle_profile_points(profile)
                                   : profile.points);
      next_parameters->additional_inner_loops.push_back(profile.inner_loops);
    }
  }
  if (!next_parameters.has_value()) {
    throw std::runtime_error("Sketch profile not found");
  }
  next_parameters->mode = mode;
  next_parameters->target_body_id = target_body_id;
  extrude_it->extrude_parameters = next_parameters.value();
  extrude_it->parameters_summary =
      extrude_it->extrude_parameters->profile_id + " · " +
      std::to_string(extrude_it->extrude_parameters->depth) + " mm";
  bump_geometry_revision();
  return document_.value();
}

DocumentState DocumentManager::loft_profiles(
    const std::vector<std::string>& profile_ids,
    bool ruled) {
  require_document();
  const LoftFeatureParameters parameters =
      make_loft_parameters_for_profiles(document_->feature_history,
                                        profile_ids,
                                        ruled);

  push_undo_state();
  clear_redo_stack();
  document_->feature_history.push_back(
      create_loft_feature(next_feature_id_++, parameters));
  document_->selected_feature_id = document_->feature_history.back().id;
  document_->selected_reference_id = std::nullopt;
  document_->selected_face_id = std::nullopt;
  document_->selected_edge_ids.clear();
  document_->selected_vertex_ids.clear();
  document_->active_sketch_plane_id = std::nullopt;
  document_->active_sketch_feature_id = std::nullopt;
  document_->active_sketch_tool = std::nullopt;
  document_->selected_sketch_point_id = std::nullopt;
  document_->selected_sketch_entity_id = std::nullopt;
  document_->selected_sketch_dimension_id = std::nullopt;
  bump_geometry_revision();
  return document_.value();
}

DocumentState DocumentManager::update_loft_profiles(
    const std::string& feature_id,
    const std::vector<std::string>& profile_ids) {
  require_document();
  const auto feature_it = std::find_if(
      document_->feature_history.begin(),
      document_->feature_history.end(),
      [&](const FeatureEntry& feature) { return feature.id == feature_id; });
  if (feature_it == document_->feature_history.end()) {
    throw std::runtime_error("Feature not found: " + feature_id);
  }
  if (feature_it->kind != "loft" || !feature_it->loft_parameters.has_value()) {
    throw std::runtime_error(
        "update_loft_profiles requires a loft feature: " + feature_id);
  }

  const bool ruled = feature_it->loft_parameters->ruled;
  const LoftFeatureParameters next_parameters =
      make_loft_parameters_for_profiles(document_->feature_history,
                                        profile_ids,
                                        ruled);
  const TopoDS_Shape next_shape = build_loft_shape(next_parameters);
  if (next_shape.IsNull()) {
    throw std::runtime_error("Loft update produced an empty shape");
  }
  feature_it->loft_parameters = next_parameters;
  feature_it->parameters_summary =
      std::to_string(feature_it->loft_parameters->sections.size()) +
      " sections" + (feature_it->loft_parameters->ruled ? " · ruled" : "");
  feature_it->status = "healthy";
  feature_it->dependency_broken = false;
  feature_it->dependency_warning.clear();
  bump_geometry_revision();
  return document_.value();
}

DocumentState DocumentManager::update_loft_ruled(const std::string& feature_id,
                                                 bool ruled) {
  require_document();
  const auto feature_it = std::find_if(
      document_->feature_history.begin(),
      document_->feature_history.end(),
      [&](const FeatureEntry& feature) { return feature.id == feature_id; });
  if (feature_it == document_->feature_history.end()) {
    throw std::runtime_error("Feature not found: " + feature_id);
  }

  polysmith::core::update_loft_ruled(*feature_it, ruled);
  bump_geometry_revision();
  return document_.value();
}

DocumentState DocumentManager::revolve_profile(
    const std::string& profile_id,
    const std::string& axis_entity_id,
    double angle_degrees) {
  require_document();
  const RevolveFeatureParameters parameters =
      make_revolve_parameters(document_->feature_history,
                              profile_id,
                              axis_entity_id,
                              angle_degrees);

  push_undo_state();
  clear_redo_stack();
  document_->feature_history.push_back(
      create_revolve_feature(next_feature_id_++, parameters));
  document_->selected_feature_id = document_->feature_history.back().id;
  document_->selected_reference_id = std::nullopt;
  document_->selected_face_id = std::nullopt;
  document_->selected_edge_ids.clear();
  document_->selected_vertex_ids.clear();
  document_->active_sketch_plane_id = std::nullopt;
  document_->active_sketch_feature_id = std::nullopt;
  document_->active_sketch_tool = std::nullopt;
  document_->selected_sketch_point_id = std::nullopt;
  document_->selected_sketch_entity_id = std::nullopt;
  document_->selected_sketch_dimension_id = std::nullopt;
  bump_geometry_revision();
  return document_.value();
}

DocumentState DocumentManager::update_revolve_profile(
    const std::string& feature_id,
    const std::string& profile_id) {
  require_document();
  const auto feature_it = std::find_if(
      document_->feature_history.begin(),
      document_->feature_history.end(),
      [&](const FeatureEntry& feature) { return feature.id == feature_id; });
  if (feature_it == document_->feature_history.end()) {
    throw std::runtime_error("Feature not found: " + feature_id);
  }
  if (feature_it->kind != "revolve" ||
      !feature_it->revolve_parameters.has_value()) {
    throw std::runtime_error(
        "update_revolve_profile requires a revolve feature: " + feature_id);
  }

  const RevolveFeatureParameters next_parameters =
      make_revolve_parameters(document_->feature_history,
                              profile_id,
                              feature_it->revolve_parameters->axis_entity_id,
                              feature_it->revolve_parameters->angle_degrees);
  const TopoDS_Shape next_shape = build_revolve_shape(next_parameters);
  if (next_shape.IsNull()) {
    throw std::runtime_error("Revolve update produced an empty shape");
  }
  feature_it->revolve_parameters = next_parameters;
  feature_it->parameters_summary =
      next_parameters.profile_id + " · " +
      std::to_string(next_parameters.angle_degrees) + " deg";
  feature_it->status = "healthy";
  feature_it->dependency_broken = false;
  feature_it->dependency_warning.clear();
  bump_geometry_revision();
  return document_.value();
}

DocumentState DocumentManager::update_revolve_axis(
    const std::string& feature_id,
    const std::string& axis_entity_id) {
  require_document();
  const auto feature_it = std::find_if(
      document_->feature_history.begin(),
      document_->feature_history.end(),
      [&](const FeatureEntry& feature) { return feature.id == feature_id; });
  if (feature_it == document_->feature_history.end()) {
    throw std::runtime_error("Feature not found: " + feature_id);
  }
  if (feature_it->kind != "revolve" ||
      !feature_it->revolve_parameters.has_value()) {
    throw std::runtime_error(
        "update_revolve_axis requires a revolve feature: " + feature_id);
  }

  const RevolveFeatureParameters next_parameters =
      make_revolve_parameters(document_->feature_history,
                              feature_it->revolve_parameters->profile_id,
                              axis_entity_id,
                              feature_it->revolve_parameters->angle_degrees);
  const TopoDS_Shape next_shape = build_revolve_shape(next_parameters);
  if (next_shape.IsNull()) {
    throw std::runtime_error("Revolve update produced an empty shape");
  }
  feature_it->revolve_parameters = next_parameters;
  feature_it->parameters_summary =
      next_parameters.profile_id + " · " +
      std::to_string(next_parameters.angle_degrees) + " deg";
  feature_it->status = "healthy";
  feature_it->dependency_broken = false;
  feature_it->dependency_warning.clear();
  bump_geometry_revision();
  return document_.value();
}

DocumentState DocumentManager::update_revolve_angle(
    const std::string& feature_id,
    double angle_degrees) {
  require_document();
  const auto feature_it = std::find_if(
      document_->feature_history.begin(),
      document_->feature_history.end(),
      [&](const FeatureEntry& feature) { return feature.id == feature_id; });
  if (feature_it == document_->feature_history.end()) {
    throw std::runtime_error("Feature not found: " + feature_id);
  }

  polysmith::core::update_revolve_angle(*feature_it, angle_degrees);
  bump_geometry_revision();
  return document_.value();
}

DocumentState DocumentManager::sweep_profile(
    const std::string& profile_id,
    const std::string& path_entity_id) {
  require_document();
  const SweepFeatureParameters parameters =
      make_sweep_parameters(document_->feature_history, profile_id, path_entity_id);

  push_undo_state();
  clear_redo_stack();
  document_->feature_history.push_back(
      create_sweep_feature(next_feature_id_++, parameters));
  document_->selected_feature_id = document_->feature_history.back().id;
  document_->selected_reference_id = std::nullopt;
  document_->selected_face_id = std::nullopt;
  document_->selected_edge_ids.clear();
  document_->selected_vertex_ids.clear();
  document_->active_sketch_plane_id = std::nullopt;
  document_->active_sketch_feature_id = std::nullopt;
  document_->active_sketch_tool = std::nullopt;
  document_->selected_sketch_point_id = std::nullopt;
  document_->selected_sketch_entity_id = std::nullopt;
  document_->selected_sketch_dimension_id = std::nullopt;
  bump_geometry_revision();
  return document_.value();
}

DocumentState DocumentManager::update_sweep_profile(
    const std::string& feature_id,
    const std::string& profile_id) {
  require_document();
  const auto feature_it = std::find_if(
      document_->feature_history.begin(),
      document_->feature_history.end(),
      [&](const FeatureEntry& feature) { return feature.id == feature_id; });
  if (feature_it == document_->feature_history.end()) {
    throw std::runtime_error("Feature not found: " + feature_id);
  }
  if (feature_it->kind != "sweep" || !feature_it->sweep_parameters.has_value()) {
    throw std::runtime_error(
        "update_sweep_profile requires a sweep feature: " + feature_id);
  }

  const SweepFeatureParameters next_parameters =
      make_sweep_parameters(document_->feature_history,
                            profile_id,
                            feature_it->sweep_parameters->path_entity_id);
  const TopoDS_Shape next_shape = build_sweep_shape(next_parameters);
  if (next_shape.IsNull()) {
    throw std::runtime_error("Sweep update produced an empty shape");
  }
  feature_it->sweep_parameters = next_parameters;
  feature_it->parameters_summary = "Profile · path";
  feature_it->status = "healthy";
  feature_it->dependency_broken = false;
  feature_it->dependency_warning.clear();
  bump_geometry_revision();
  return document_.value();
}
