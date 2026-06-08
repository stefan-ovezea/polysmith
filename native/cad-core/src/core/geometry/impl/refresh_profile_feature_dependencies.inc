    // Extrude: derive the live plane frame from its owning sketch.
    // The sketch was just refreshed in an earlier iteration (sketches
    // always come before their extrude in feature_history), so we get
    // the up-to-date frame here.
    if (feature.kind == "extrude" && feature.extrude_parameters.has_value()) {
      if (feature.status == "warning" && feature.dependency_broken) {
        // Source-profile breakage is set by the sketch refresh path.
        // Do not clear it just because the sketch plane still resolves.
        continue;
      }
      const FeatureEntry* sketch = find_feature(
          document, feature.extrude_parameters->sketch_feature_id);
      if (sketch == nullptr || !sketch->sketch_parameters.has_value()) {
        continue;
      }
      if (sketch->dependency_broken) {
        // Cascade the broken state: the extrude's plane_frame copy is
        // also stale. We keep the existing `extrude_parameters.plane_frame`
        // so the body still renders somewhere reasonable rather than
        // collapsing to the world origin.
        feature.dependency_broken = true;
        feature.dependency_warning =
            "Extrude depends on sketch '" + sketch->id +
            "', whose plane reference is broken.";
        continue;
      }
      if (sketch->sketch_parameters->plane_frame.has_value()) {
        feature.extrude_parameters->plane_frame =
            from_sketch_plane_frame(
                sketch->sketch_parameters->plane_frame.value());
        feature.dependency_broken = false;
        feature.dependency_warning.clear();
      }
    }

    if (feature.kind == "loft" && feature.loft_parameters.has_value()) {
      if (feature.status == "warning" && feature.dependency_broken) {
        continue;
      }

      bool broken = false;
      std::string warning;
      for (auto& section : feature.loft_parameters->sections) {
        const FeatureEntry* sketch = find_feature(document, section.sketch_feature_id);
        if (sketch == nullptr || !sketch->sketch_parameters.has_value()) {
          broken = true;
          warning = "Loft depends on a sketch that no longer exists.";
          break;
        }
        if (sketch->dependency_broken) {
          broken = true;
          warning = "Loft depends on sketch '" + sketch->id +
                    "', whose plane reference is broken.";
          break;
        }
        if (sketch->sketch_parameters->plane_frame.has_value()) {
          section.plane_frame =
              from_sketch_plane_frame(
                  sketch->sketch_parameters->plane_frame.value());
        }
      }

      if (broken) {
        feature.dependency_broken = true;
        feature.dependency_warning = warning;
      } else {
        feature.dependency_broken = false;
        feature.dependency_warning.clear();
      }
    }

    if (feature.kind == "revolve" && feature.revolve_parameters.has_value()) {
      if (feature.status == "warning" && feature.dependency_broken) {
        continue;
      }

      auto& parameters = feature.revolve_parameters.value();
      const FeatureEntry* profile_sketch =
          find_feature(document, parameters.sketch_feature_id);
      const FeatureEntry* axis_sketch =
          find_feature(document, parameters.axis_sketch_feature_id);

      if (profile_sketch == nullptr ||
          !profile_sketch->sketch_parameters.has_value()) {
        feature.dependency_broken = true;
        feature.dependency_warning =
            "Revolve depends on a profile sketch that no longer exists.";
        continue;
      }
      if (axis_sketch == nullptr ||
          !axis_sketch->sketch_parameters.has_value()) {
        feature.dependency_broken = true;
        feature.dependency_warning =
            "Revolve depends on an axis sketch that no longer exists.";
        continue;
      }
      if (profile_sketch->dependency_broken) {
        feature.dependency_broken = true;
        feature.dependency_warning =
            "Revolve depends on sketch '" + profile_sketch->id +
            "', whose plane reference is broken.";
        continue;
      }
      if (axis_sketch->dependency_broken) {
        feature.dependency_broken = true;
        feature.dependency_warning =
            "Revolve depends on sketch '" + axis_sketch->id +
            "', whose plane reference is broken.";
        continue;
      }
      if (profile_sketch->sketch_parameters->plane_frame.has_value()) {
        parameters.plane_frame = from_sketch_plane_frame(
            profile_sketch->sketch_parameters->plane_frame.value());
      }
      const SketchLine* axis_line = nullptr;
      for (const auto& line : axis_sketch->sketch_parameters->lines) {
        if (line.id == parameters.axis_entity_id) {
          axis_line = &line;
          break;
        }
      }
      if (axis_line == nullptr) {
        feature.dependency_broken = true;
        feature.dependency_warning =
            "Revolve depends on an axis line that no longer exists.";
        continue;
      }
      std::optional<PlaneFrame> axis_frame;
      if (axis_sketch->sketch_parameters->plane_frame.has_value()) {
        axis_frame = from_sketch_plane_frame(
            axis_sketch->sketch_parameters->plane_frame.value());
      } else {
        axis_frame = origin_plane_frame(axis_sketch->sketch_parameters->plane_id);
      }
      if (!axis_frame.has_value()) {
        feature.dependency_broken = true;
        feature.dependency_warning =
            "Revolve could not resolve the axis sketch plane.";
        continue;
      }
      const auto axis_start = sketch_local_to_world(
          axis_frame.value(), axis_line->start_x, axis_line->start_y);
      const auto axis_end = sketch_local_to_world(
          axis_frame.value(), axis_line->end_x, axis_line->end_y);
      parameters.axis_start_x = axis_start[0];
      parameters.axis_start_y = axis_start[1];
      parameters.axis_start_z = axis_start[2];
      parameters.axis_end_x = axis_end[0];
      parameters.axis_end_y = axis_end[1];
      parameters.axis_end_z = axis_end[2];

      feature.dependency_broken = false;
      feature.dependency_warning.clear();
    }

    if (feature.kind == "sweep" && feature.sweep_parameters.has_value()) {
      if (feature.status == "warning" && feature.dependency_broken) {
        continue;
      }

      auto& parameters = feature.sweep_parameters.value();
      const FeatureEntry* profile_sketch =
          find_feature(document, parameters.sketch_feature_id);
      const FeatureEntry* path_sketch =
          find_feature(document, parameters.path_sketch_feature_id);

      if (profile_sketch == nullptr ||
          !profile_sketch->sketch_parameters.has_value()) {
        feature.dependency_broken = true;
        feature.dependency_warning =
            "Sweep depends on a profile sketch that no longer exists.";
        continue;
      }
      if (path_sketch == nullptr || !path_sketch->sketch_parameters.has_value()) {
        feature.dependency_broken = true;
        feature.dependency_warning =
            "Sweep depends on a path sketch that no longer exists.";
        continue;
      }
      if (profile_sketch->dependency_broken) {
        feature.dependency_broken = true;
        feature.dependency_warning =
            "Sweep depends on sketch '" + profile_sketch->id +
            "', whose plane reference is broken.";
        continue;
      }
      if (path_sketch->dependency_broken) {
        feature.dependency_broken = true;
        feature.dependency_warning =
            "Sweep depends on sketch '" + path_sketch->id +
            "', whose plane reference is broken.";
        continue;
      }
      if (profile_sketch->sketch_parameters->plane_frame.has_value()) {
        parameters.plane_frame = from_sketch_plane_frame(
            profile_sketch->sketch_parameters->plane_frame.value());
      }
      std::optional<PlaneFrame> path_frame;
      if (path_sketch->sketch_parameters->plane_frame.has_value()) {
        path_frame = from_sketch_plane_frame(
            path_sketch->sketch_parameters->plane_frame.value());
      } else {
        path_frame = origin_plane_frame(path_sketch->sketch_parameters->plane_id);
      }
      if (!path_frame.has_value()) {
        feature.dependency_broken = true;
        feature.dependency_warning =
            "Sweep could not resolve the path sketch plane.";
        continue;
      }
      const auto path_segments = make_sweep_path_segments(
          path_sketch->sketch_parameters.value(), path_frame.value(),
          parameters.path_entity_id);
      if (path_segments.empty()) {
        feature.dependency_broken = true;
        feature.dependency_warning =
            "Sweep depends on a path entity that no longer exists or is branched.";
        continue;
      }
      parameters.path_segments = path_segments;
      parameters.path_start_x = path_segments.front().start_x;
      parameters.path_start_y = path_segments.front().start_y;
      parameters.path_start_z = path_segments.front().start_z;
      parameters.path_end_x = path_segments.back().end_x;
      parameters.path_end_y = path_segments.back().end_y;
      parameters.path_end_z = path_segments.back().end_z;

      feature.dependency_broken = false;
      feature.dependency_warning.clear();
    }
