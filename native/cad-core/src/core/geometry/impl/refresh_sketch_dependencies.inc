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
