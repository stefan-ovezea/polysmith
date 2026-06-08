    // Construction plane: re-derive its cached frame from the
    // current source. Every earlier feature has already been
    // refreshed in place by previous iterations, so reading the
    // upstream construction plane's cache or the upstream face's
    // body works against fresh state.
    if (feature.kind == "construction_plane" &&
        feature.construction_plane_parameters.has_value()) {
      const auto& params = feature.construction_plane_parameters.value();
      DocumentState prefix = document;
      prefix.feature_history.resize(i);
      std::optional<PlaneFrame> next_frame;
      if (params.plane_type == "midplane") {
        if (params.source_plane_ids.size() >= 2) {
          const auto first_frame =
              resolve_plane_source_frame(prefix, params.source_plane_ids[0]);
          const auto second_frame =
              resolve_plane_source_frame(prefix, params.source_plane_ids[1]);
          if (first_frame.has_value() && second_frame.has_value()) {
            try {
              next_frame =
                  derive_midplane_frame(first_frame.value(), second_frame.value());
            } catch (const std::exception&) {
              next_frame = std::nullopt;
            }
          }
        }
      } else if (params.plane_type == "tangent") {
        next_frame =
            resolve_tangent_plane_source_frame(prefix, params.source_plane_id);
      } else if (params.plane_type == "angle") {
        const auto source_frame =
            resolve_plane_source_frame(prefix, params.source_plane_id);
        const auto axis =
            resolve_angle_plane_axis(prefix, params.source_axis_id);
        if (source_frame.has_value() && axis.has_value()) {
          try {
            next_frame = derive_angle_plane_frame(
                source_frame.value(), axis.value(), params.angle_degrees);
          } catch (const std::exception&) {
            next_frame = std::nullopt;
          }
        }
      } else {
        const auto source_frame =
            resolve_plane_source_frame(prefix, params.source_plane_id);
        if (source_frame.has_value()) {
          next_frame = derive_offset_frame(source_frame.value(), params.offset);
        }
      }

      if (next_frame.has_value()) {
        ConstructionPlaneFeatureParameters next = params;
        next.plane_frame = next_frame.value();
        feature.construction_plane_parameters = next;
        feature.dependency_broken = false;
        feature.dependency_warning.clear();
      } else {
        // Leave the cached frame at its last value so the UI can
        // still render the plane somewhere; the warning surfaces
        // via dependency_broken.
        feature.dependency_broken = true;
        const std::string source_summary =
            params.plane_type == "angle"
                ? params.source_plane_id + "' / '" + params.source_axis_id
                : params.source_plane_id;
        feature.dependency_warning =
            "Construction plane references '" + source_summary +
            "', which is no longer available. Edit upstream features to "
            "restore it.";
      }
    }

    if (feature.kind == "construction_axis" &&
        feature.construction_axis_parameters.has_value()) {
      const auto& params = feature.construction_axis_parameters.value();
      DocumentState prefix = document;
      prefix.feature_history.resize(i);
      const auto next_axis =
          resolve_construction_axis_source(prefix, params.source_id);
      if (next_axis.has_value()) {
        feature.construction_axis_parameters = next_axis.value();
        feature.dependency_broken = false;
        feature.dependency_warning.clear();
      } else {
        feature.dependency_broken = true;
        feature.dependency_warning =
            "Construction axis references '" + params.source_id +
            "', which is no longer available or is no longer linear.";
      }
    }

    if (feature.kind == "construction_point" &&
        feature.construction_point_parameters.has_value()) {
      const auto& params = feature.construction_point_parameters.value();
      DocumentState prefix = document;
      prefix.feature_history.resize(i);
      const auto next_point =
          resolve_construction_point_source(prefix, params.source_id);
      if (next_point.has_value()) {
        feature.construction_point_parameters = next_point.value();
        feature.dependency_broken = false;
        feature.dependency_warning.clear();
      } else {
        feature.dependency_broken = true;
        feature.dependency_warning =
            "Construction point references '" + params.source_id +
            "', which is no longer available.";
      }
    }

    if (feature.kind == "hole" && feature.hole_parameters.has_value()) {
      const auto& params = feature.hole_parameters.value();
      DocumentState prefix = document;
      prefix.feature_history.resize(i);
      const auto face_target = parse_face_target(params.source_face_id);
      std::optional<PlaneFrame> next_frame;
      if (face_target.has_value()) {
        next_frame = resolve_face_frame(
            prefix, face_target->body_id, face_target->face_index);
      }
      if (next_frame.has_value()) {
        HoleFeatureParameters next = params;
        next.target_body_id =
            face_target.has_value() ? face_target->body_id : params.target_body_id;
        next.plane_frame = next_frame.value();
        feature.hole_parameters = next;
        feature.dependency_broken = false;
        feature.dependency_warning.clear();
      } else {
        feature.dependency_broken = true;
        feature.dependency_warning =
            "Hole references '" + params.source_face_id +
            "', which is no longer available as a planar face.";
      }
    }
