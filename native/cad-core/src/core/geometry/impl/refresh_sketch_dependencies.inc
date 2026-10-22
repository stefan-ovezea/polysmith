    // Sketch on a body face / construction plane: re-resolve the
    // plane frame against upstream geometry. We compile the prefix
    // [0, i) — every earlier feature has already been refreshed in
    // place by previous iterations, so the upstream geometry
    // reflects the latest edits.
    if (feature.kind == "sketch" && feature.sketch_parameters.has_value()) {
      feature.dependency_ambiguous = false;
      feature.dependency_ambiguous_warning.clear();
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

        // Face provenance: re-resolve the face by its captured plane
        // fingerprint before trusting the index, so a shifted face index
        // re-binds to the same plane instead of silently grabbing a
        // different face. A gone face is handled here (broken) and the
        // index-based resolve below is skipped, because the stale index
        // could still point at an unrelated live face.
        bool handled_by_fingerprint = false;
        if (face_target.has_value() &&
            feature.sketch_parameters->face_fingerprint.has_value() &&
            feature.sketch_parameters->face_fingerprint->valid) {
          const auto& fingerprint = *feature.sketch_parameters->face_fingerprint;
          const CompiledBodies compiled = compile_bodies(prefix, false);
          const TopoDS_Shape* target_shape = nullptr;
          for (const auto& body : compiled.bodies) {
            if (body.id == face_target->body_id && !body.shape.IsNull()) {
              target_shape = &body.shape;
              break;
            }
          }
          if (target_shape != nullptr) {
            const FaceProvenanceResolution resolved =
                resolve_face_fingerprint(fingerprint, *target_shape);
            if (resolved.missing) {
              feature.dependency_broken = true;
              feature.dependency_warning =
                  std::string{"Sketch plane references a face that no "
                              "longer exists on body '"} +
                  face_target->body_id +
                  "'. Edit upstream features to restore it.";
              handled_by_fingerprint = true;
            } else {
              if (resolved.index != face_target->face_index) {
                feature.sketch_parameters->plane_id =
                    face_target->body_id + ":face:" +
                    std::to_string(resolved.index);
              }
              if (resolved.ambiguous) {
                feature.dependency_ambiguous = true;
                feature.dependency_ambiguous_warning =
                    "Sketch plane re-bound to a near-identical face — "
                    "verify the selection.";
              }
            }
          }
        }

        if (!handled_by_fingerprint) {
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
