void refresh_linked_extrudes(DocumentState& document,
                             const FeatureEntry& sketch_feature) {
  if (!sketch_feature.sketch_parameters.has_value()) {
    return;
  }
  const auto mark_source_profile_warning = [](FeatureEntry& feature) {
    feature.status = "warning";
    feature.parameters_summary = "Source profile unavailable";
    feature.dependency_broken = true;
    feature.dependency_warning =
        "Extrude source profile no longer exists. Edit the source sketch "
        "or recreate the extrude.";
  };

  for (auto& feature : document.feature_history) {
    if (feature.kind != "extrude" || !feature.extrude_parameters.has_value() ||
        feature.extrude_parameters->sketch_feature_id != sketch_feature.id) {
      continue;
    }

    const auto& sketch = sketch_feature.sketch_parameters.value();
    const std::vector<std::string> source_profile_ids =
        feature.extrude_parameters->profile_ids.empty()
            ? std::vector<std::string>{feature.extrude_parameters->profile_id}
            : feature.extrude_parameters->profile_ids;
    std::vector<SketchProfileRegion> source_profiles;
    for (const auto& profile_id : source_profile_ids) {
      const SketchProfileRegion* profile =
          find_equivalent_profile(sketch.profiles, profile_id);
      if (profile == nullptr) {
        mark_source_profile_warning(feature);
        source_profiles.clear();
        break;
      }
      source_profiles.push_back(*profile);
    }
    if (source_profiles.empty()) {
      continue;
    }

    const double depth = feature.extrude_parameters->depth;
    const std::string mode = feature.extrude_parameters->mode;
    const auto target_body_id = feature.extrude_parameters->target_body_id;
    std::optional<ExtrudeFeatureParameters> next_parameters;
    if (source_profiles.size() == 1 && source_profiles.front().kind == "circle") {
      next_parameters =
          make_extrude_parameters_for_profile(sketch_feature,
                                              source_profiles.front(),
                                              depth);
    } else {
      const auto& first = source_profiles.front();
      const std::string plane_id =
          sketch.plane_frame.has_value() ? plane_id_from_frame(sketch.plane_frame.value())
                                         : sketch.plane_id;
      const std::optional<PlaneFrame> plane_frame =
          sketch.plane_frame.has_value()
              ? std::optional<PlaneFrame>(make_plane_frame(sketch.plane_frame.value()))
              : std::nullopt;
      std::vector<std::string> ids;
      for (const auto& profile : source_profiles) {
        ids.push_back(profile.id);
      }
      next_parameters = ExtrudeFeatureParameters{
          .sketch_feature_id = sketch_feature.id,
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
      for (size_t index = 1; index < source_profiles.size(); ++index) {
        const auto& profile = source_profiles[index];
        next_parameters->additional_profile_points.push_back(
            profile.kind == "circle" ? sample_circle_profile_points(profile)
                                     : profile.points);
        next_parameters->additional_inner_loops.push_back(profile.inner_loops);
      }
    }
    if (!next_parameters.has_value()) {
      feature.status = "warning";
      feature.parameters_summary = "Source profile unsupported";
      feature.dependency_broken = true;
      feature.dependency_warning =
          "Extrude source profile is no longer supported after the sketch edit.";
      continue;
    }

    feature.extrude_parameters = next_parameters.value();
    feature.extrude_parameters->mode = mode;
    feature.extrude_parameters->target_body_id = target_body_id;
    feature.status = "healthy";
    feature.dependency_broken = false;
    feature.dependency_warning.clear();
    feature.parameters_summary =
        feature.extrude_parameters->profile_id + " · " +
        std::to_string(feature.extrude_parameters->depth) + " mm";
  }

  const auto mark_loft_profile_warning = [](FeatureEntry& feature,
                                            const std::string& message) {
    feature.status = "warning";
    feature.parameters_summary = "Source profile unavailable";
    feature.dependency_broken = true;
    feature.dependency_warning = message;
  };

  for (auto& feature : document.feature_history) {
    if (feature.kind != "loft" || !feature.loft_parameters.has_value()) {
      continue;
    }

    bool references_sketch = false;
    for (const auto& section : feature.loft_parameters->sections) {
      if (section.sketch_feature_id == sketch_feature.id) {
        references_sketch = true;
        break;
      }
    }
    if (!references_sketch) {
      continue;
    }

    const auto& sketch = sketch_feature.sketch_parameters.value();
    LoftFeatureParameters next_parameters = feature.loft_parameters.value();
    bool failed = false;
    for (auto& section : next_parameters.sections) {
      if (section.sketch_feature_id != sketch_feature.id) {
        continue;
      }
      const SketchProfileRegion* profile =
          find_equivalent_profile(sketch.profiles, section.profile_id);
      if (profile == nullptr) {
        mark_loft_profile_warning(
            feature,
            "Loft source profile no longer exists. Edit the source sketch "
            "or recreate the loft.");
        failed = true;
        break;
      }
      const auto next_section =
          make_loft_section_for_profile(sketch_feature, *profile);
      if (!next_section.has_value()) {
        mark_loft_profile_warning(
            feature,
            "Loft source profile now contains unsupported holes. Edit the "
            "source sketch or recreate the loft.");
        failed = true;
        break;
      }
      section = next_section.value();
    }
    if (failed) {
      continue;
    }

    try {
      const TopoDS_Shape next_shape = build_loft_shape(next_parameters);
      if (next_shape.IsNull()) {
        throw std::runtime_error("Loft rebuild produced an empty shape");
      }
    } catch (const std::exception&) {
      mark_loft_profile_warning(
          feature,
          "Loft could not rebuild from the updated profile shape. Edit the "
          "source sketch or recreate the loft.");
      continue;
    }

    feature.loft_parameters = next_parameters;
    feature.status = "healthy";
    feature.dependency_broken = false;
    feature.dependency_warning.clear();
    feature.parameters_summary =
        std::to_string(feature.loft_parameters->sections.size()) +
        " sections" + (feature.loft_parameters->ruled ? " · ruled" : "");
  }

  const auto mark_revolve_warning = [](FeatureEntry& feature,
                                       const std::string& message) {
    feature.status = "warning";
    feature.parameters_summary = "Source profile or axis unavailable";
    feature.dependency_broken = true;
    feature.dependency_warning = message;
  };

  for (auto& feature : document.feature_history) {
    if (feature.kind != "revolve" ||
        !feature.revolve_parameters.has_value()) {
      continue;
    }
    const bool references_profile_sketch =
        feature.revolve_parameters->sketch_feature_id == sketch_feature.id;
    const bool references_axis_sketch =
        feature.revolve_parameters->axis_sketch_feature_id == sketch_feature.id;
    if (!references_profile_sketch && !references_axis_sketch) {
      continue;
    }

    RevolveFeatureParameters next_parameters =
        feature.revolve_parameters.value();
    const FeatureEntry* profile_sketch_feature = nullptr;
    const FeatureEntry* axis_sketch_feature = nullptr;
    const SketchProfileRegion* profile = nullptr;
    const SketchLine* axis_line = nullptr;

    for (const auto& candidate : document.feature_history) {
      if (candidate.id == next_parameters.sketch_feature_id) {
        profile_sketch_feature = &candidate;
      }
      if (candidate.id == next_parameters.axis_sketch_feature_id) {
        axis_sketch_feature = &candidate;
      }
    }
    if (profile_sketch_feature == nullptr ||
        !profile_sketch_feature->sketch_parameters.has_value()) {
      mark_revolve_warning(
          feature,
          "Revolve source profile sketch no longer exists. Edit the source "
          "sketch or recreate the revolve.");
      continue;
    }
    if (axis_sketch_feature == nullptr ||
        !axis_sketch_feature->sketch_parameters.has_value()) {
      mark_revolve_warning(
          feature,
          "Revolve axis sketch no longer exists. Edit the source sketch "
          "or recreate the revolve.");
      continue;
    }

    profile = find_equivalent_profile(
        profile_sketch_feature->sketch_parameters->profiles,
        next_parameters.profile_id);
    axis_line = find_line_by_id(axis_sketch_feature->sketch_parameters.value(),
                                next_parameters.axis_entity_id);
    if (profile == nullptr) {
      mark_revolve_warning(
          feature,
          "Revolve source profile no longer exists. Edit the source sketch "
          "or recreate the revolve.");
      continue;
    }
    if (axis_line == nullptr) {
      mark_revolve_warning(
          feature,
          "Revolve axis line no longer exists. Edit the source sketch "
          "or recreate the revolve.");
      continue;
    }

    const auto rebuilt =
        make_revolve_parameters_for_profile(*profile_sketch_feature,
                                            *profile,
                                            *axis_sketch_feature,
                                            *axis_line,
                                            next_parameters.angle_degrees);
    if (!rebuilt.has_value()) {
      mark_revolve_warning(
          feature,
          "Revolve source profile or axis is no longer supported after "
          "the sketch edit.");
      continue;
    }

    try {
      const TopoDS_Shape next_shape = build_revolve_shape(rebuilt.value());
      if (next_shape.IsNull()) {
        throw std::runtime_error("Revolve rebuild produced an empty shape");
      }
    } catch (const std::exception&) {
      mark_revolve_warning(
          feature,
          "Revolve could not rebuild from the updated profile or axis. "
          "Edit the source sketch or recreate the revolve.");
      continue;
    }

    feature.revolve_parameters = rebuilt.value();
    feature.status = "healthy";
    feature.dependency_broken = false;
    feature.dependency_warning.clear();
    feature.parameters_summary =
        feature.revolve_parameters->profile_id + " · " +
        std::to_string(feature.revolve_parameters->angle_degrees) + " deg";
  }

  const auto mark_sweep_warning = [](FeatureEntry& feature,
                                     const std::string& message) {
    feature.status = "warning";
    feature.parameters_summary = "Source profile or path unavailable";
    feature.dependency_broken = true;
    feature.dependency_warning = message;
  };

  for (auto& feature : document.feature_history) {
    if (feature.kind != "sweep" || !feature.sweep_parameters.has_value()) {
      continue;
    }
    const bool references_profile_sketch =
        feature.sweep_parameters->sketch_feature_id == sketch_feature.id;
    const bool references_path_sketch =
        feature.sweep_parameters->path_sketch_feature_id == sketch_feature.id;
    if (!references_profile_sketch && !references_path_sketch) {
      continue;
    }

    try {
      const SweepFeatureParameters rebuilt =
          make_sweep_parameters(document.feature_history,
                                feature.sweep_parameters->profile_id,
                                feature.sweep_parameters->path_entity_id);
      const TopoDS_Shape next_shape = build_sweep_shape(rebuilt);
      if (next_shape.IsNull()) {
        throw std::runtime_error("Sweep rebuild produced an empty shape");
      }
      feature.sweep_parameters = rebuilt;
      feature.status = "healthy";
      feature.dependency_broken = false;
      feature.dependency_warning.clear();
      feature.parameters_summary = "Profile · path";
    } catch (const std::exception&) {
      mark_sweep_warning(
          feature,
          "Sweep could not rebuild from the updated profile or path. Edit the "
          "source sketch or recreate the sweep.");
    }
  }
}
