    // Fillet / chamfer: re-resolve each stored edge by its captured
    // fingerprint (then index) against the pre-modifier body, so a topology
    // change re-binds to the same physical edge instead of silently grabbing
    // a different one. A near-tie surfaces `dependency_ambiguous`; a missing
    // edge surfaces `dependency_broken`.
    if (feature.kind == "fillet" && feature.fillet_parameters.has_value()) {
      auto& params = feature.fillet_parameters.value();
      const EdgeRefreshSummary summary = refresh_modifier_edges(
          document, i, params.target_body_id, params.edge_ids,
          params.edge_fingerprints);
      if (summary.any_broken) {
        feature.dependency_broken = true;
        feature.dependency_warning =
            "Fillet references an edge that no longer exists on the target "
            "body. Edit upstream features to restore it.";
      } else {
        feature.dependency_broken = false;
        feature.dependency_warning.clear();
      }
      if (summary.any_ambiguous) {
        feature.dependency_ambiguous = true;
        feature.dependency_ambiguous_warning =
            summary.ambiguous_count == 1
                ? "Fillet edge re-bound to a near-identical edge — verify the "
                  "selection."
                : std::to_string(summary.ambiguous_count) +
                      " fillet edges re-bound to near-identical edges — "
                      "verify the selection.";
      }
    }

    if (feature.kind == "chamfer" && feature.chamfer_parameters.has_value()) {
      auto& params = feature.chamfer_parameters.value();
      const EdgeRefreshSummary summary = refresh_modifier_edges(
          document, i, params.target_body_id, params.edge_ids,
          params.edge_fingerprints);
      if (summary.any_broken) {
        feature.dependency_broken = true;
        feature.dependency_warning =
            "Chamfer references an edge that no longer exists on the target "
            "body. Edit upstream features to restore it.";
      } else {
        feature.dependency_broken = false;
        feature.dependency_warning.clear();
      }
      if (summary.any_ambiguous) {
        feature.dependency_ambiguous = true;
        feature.dependency_ambiguous_warning =
            summary.ambiguous_count == 1
                ? "Chamfer edge re-bound to a near-identical edge — verify "
                  "the selection."
                : std::to_string(summary.ambiguous_count) +
                      " chamfer edges re-bound to near-identical edges — "
                      "verify the selection.";
      }
    }
