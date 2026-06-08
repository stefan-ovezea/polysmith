    if (feature.kind == "helix" && feature.helix_parameters.has_value()) {
      const auto& params = feature.helix_parameters.value();
      DocumentState prefix = document;
      prefix.feature_history.resize(i);
      const auto axis =
          resolve_construction_axis_source(prefix, params.axis_source_id);
      if (axis.has_value()) {
        HelixFeatureParameters next = params;
        next.axis_start_x = axis->start_x;
        next.axis_start_y = axis->start_y;
        next.axis_start_z = axis->start_z;
        next.axis_end_x = axis->end_x;
        next.axis_end_y = axis->end_y;
        next.axis_end_z = axis->end_z;
        next.turns = next.pitch > 0.0 ? next.height / next.pitch : 0.0;
        next.points = make_refreshed_helix_points(axis.value(), next);
        feature.helix_parameters = next;
        feature.dependency_broken = next.points.empty();
        feature.dependency_warning =
            next.points.empty() ? "Helix parameters are invalid." : "";
      } else {
        feature.dependency_broken = true;
        feature.dependency_warning =
            "Helix references '" + params.axis_source_id +
            "', which is no longer available or is no longer linear.";
      }
    }

    if (feature.kind == "thread" && feature.thread_parameters.has_value()) {
      const auto& params = feature.thread_parameters.value();
      DocumentState prefix = document;
      prefix.feature_history.resize(i);
      const CompiledBodies compiled = compile_bodies(prefix);
      const bool target_body_exists = std::any_of(
          compiled.bodies.begin(), compiled.bodies.end(),
          [&](const CompiledBody& body) { return body.id == params.target_body_id; });
      const auto axis =
          resolve_construction_axis_source(prefix, params.axis_source_id);
      if (target_body_exists && axis.has_value()) {
        feature.dependency_broken = false;
        feature.dependency_warning.clear();
      } else if (!target_body_exists) {
        feature.dependency_broken = true;
        feature.dependency_warning =
            "Thread target body '" + params.target_body_id +
            "' is no longer available.";
      } else {
        feature.dependency_broken = true;
        feature.dependency_warning =
            "Thread axis source '" + params.axis_source_id +
            "' is no longer available or is no longer linear.";
      }
    }

    if (feature.kind == "move" && feature.move_parameters.has_value()) {
      const auto& params = feature.move_parameters.value();
      DocumentState prefix = document;
      prefix.feature_history.resize(i);
      const CompiledBodies compiled = compile_bodies(prefix);
      const bool target_body_exists = std::any_of(
          compiled.bodies.begin(), compiled.bodies.end(),
          [&](const CompiledBody& body) {
            return body.id == params.target_body_id;
          });
      if (target_body_exists) {
        feature.dependency_broken = false;
        feature.dependency_warning.clear();
      } else {
        feature.dependency_broken = true;
        feature.dependency_warning =
            "Move target body '" + params.target_body_id +
            "' is no longer available.";
      }
    }

    if (feature.kind == "body_copy" && feature.body_copy_parameters.has_value()) {
      const auto& params = feature.body_copy_parameters.value();
      if (params.copy_mode == "standalone") {
        if (!params.serialized_shape.empty()) {
          feature.dependency_broken = false;
          feature.dependency_warning.clear();
        } else {
          feature.dependency_broken = true;
          feature.dependency_warning = "Independent copy shape is unavailable.";
        }
        continue;
      }
      DocumentState prefix = document;
      prefix.feature_history.resize(i);
      const CompiledBodies compiled = compile_bodies(prefix);
      const bool source_body_exists = std::any_of(
          compiled.bodies.begin(), compiled.bodies.end(),
          [&](const CompiledBody& body) {
            return body.id == params.source_body_id;
          });
      if (source_body_exists) {
        feature.dependency_broken = false;
        feature.dependency_warning.clear();
      } else {
        feature.dependency_broken = true;
        feature.dependency_warning =
            "Copy source body '" + params.source_body_id +
            "' is no longer available.";
      }
    }
