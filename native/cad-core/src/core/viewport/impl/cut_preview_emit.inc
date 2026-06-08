  // Cut preview overlay: when the user has a cut extrude selected (i.e.
  // the floating Extrude panel is open and editing it), emit a
  // translucent red mesh of the cutter volume so they can see exactly
  // what's about to be removed. This is a UI overlay only — the
  // booleaned body itself already renders the post-cut shape via
  // `meshes`. We only emit the preview while the feature is the
  // currently-selected one to avoid clutter on saved documents.
  if (view->selected_feature_id.has_value()) {
    for (const auto& feature : view->feature_history) {
      if (feature.id != view->selected_feature_id.value()) {
        continue;
      }
      if (feature.kind != "extrude" ||
          !feature.extrude_parameters.has_value() ||
          feature.extrude_parameters->mode != "cut") {
        break;
      }
      try {
        const TopoDS_Shape cutter =
            build_extrude_shape(feature.extrude_parameters.value());
        if (cutter.IsNull()) {
          break;
        }
        ViewportCutPreview preview{};
        preview.id = feature.id;
        tessellate_shape_to_arrays(cutter,
                                   preview.positions,
                                   preview.normals,
                                   preview.indices);
        if (!preview.positions.empty() && !preview.indices.empty()) {
          cut_previews.push_back(std::move(preview));
        }
      } catch (const std::exception&) {
        // Cutter build failures shouldn't break the rest of the
        // viewport; just skip the preview for this snapshot.
      }
      break;
    }
  }
