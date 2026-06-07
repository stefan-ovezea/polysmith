  for (size_t i = 0; i < document.feature_history.size(); ++i) {
    FeatureEntry& feature = document.feature_history[i];

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

    // Sketch on a body face / construction plane: re-resolve the
    // plane frame against upstream geometry. We compile the prefix
    // [0, i) — every earlier feature has already been refreshed in
    // place by previous iterations, so the upstream geometry
    // reflects the latest edits.
    if (feature.kind == "sketch" && feature.sketch_parameters.has_value()) {
      const std::string& plane_id = feature.sketch_parameters->plane_id;
      const auto face_target = parse_face_target(plane_id);
      const FeatureEntry* upstream_plane =
          face_target.has_value() ? nullptr : find_feature(document, plane_id);
      const bool is_construction_source =
          upstream_plane != nullptr &&
          upstream_plane->kind == "construction_plane";

      if (face_target.has_value() || is_construction_source) {
        DocumentState prefix = document;
        prefix.feature_history.resize(i);

        const std::optional<PlaneFrame> frame =
            resolve_plane_source_frame(prefix, plane_id);
        if (frame.has_value()) {
          feature.sketch_parameters->plane_frame =
              to_sketch_plane_frame(frame.value());
          feature.dependency_broken = false;
          feature.dependency_warning.clear();
        } else {
          // Leave the sketch's stored plane_frame at its last known
          // value so the UI can still render the sketch entities; the
          // warning is surfaced via `dependency_broken`.
          feature.dependency_broken = true;
          feature.dependency_warning =
              face_target.has_value()
                  ? std::string{
                        "Sketch plane references a face that no longer "
                        "exists on body '"} +
                        face_target->body_id +
                        "'. Edit upstream features to restore it."
                  : std::string{"Sketch plane references construction "
                                "plane '"} +
                        plane_id +
                        "', which is no longer available.";
        }
      } else {
        // Origin reference plane sketch (legacy plane_id like
        // "ref-plane-xy"): its frame is invariant under upstream
        // edits. Clearing any stale broken-state from a previous run
        // keeps the timeline honest.
        feature.dependency_broken = false;
        feature.dependency_warning.clear();
      }

      // Live projection refresh. Every projection on this sketch
      // re-resolves its body source against the prefix [0, i) — which
      // already reflects every upstream edit by the time we get here
      // — and patches the cached coords on the matching generated
      // sketch entities. Mirrors mainstream CAD's "edit the original,
      // the projection follows" behaviour. Skipped when the sketch
      // has no projections, or when its plane frame is missing
      // (broken sketches stay frozen on their last-known coords).
      if (!feature.sketch_parameters->projections.empty() &&
          feature.sketch_parameters->plane_frame.has_value()) {
        DocumentState prefix = document;
        prefix.feature_history.resize(i);
        const bool any_broken = refresh_sketch_projections(prefix, feature);
        if (any_broken && !feature.dependency_broken) {
          // Don't overwrite a more specific upstream-plane warning;
          // only surface the projection-broken message when the
          // feature is otherwise healthy.
          feature.dependency_broken = true;
          feature.dependency_warning =
              "One or more projected entities reference a body source "
              "that no longer exists or changed shape. Re-project to "
              "restore the live link.";
        }
        // After patching, re-run the sketch's derived state so
        // points / profiles pick up the updated coords.
        refresh_sketch_derived_state(feature);
      }
    }

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
  }
