DocumentState DocumentManager::split_sketch_entity(
    const std::string& entity_id,
    double click_x,
    double click_y,
    double split2_x,
    double split2_y) {
  require_document();

  if (!document_->active_sketch_feature_id.has_value()) {
    throw std::runtime_error("No active sketch");
  }

  const auto feature_it = std::find_if(
      document_->feature_history.begin(),
      document_->feature_history.end(),
      [&](const FeatureEntry& feature) {
        return feature.id == document_->active_sketch_feature_id.value();
      });

  if (feature_it == document_->feature_history.end()) {
    throw std::runtime_error("Active sketch feature not found");
  }

  push_undo_state();
  clear_redo_stack();
  // The engine collects the fresh split point ids; the freeze below
  // always needs them.
  std::vector<std::string> minted;
  polysmith::core::split_sketch_entity(
      *feature_it, entity_id, click_x, click_y, split2_x, split2_y,
      &minted);

  // Freeze the vertices this split minted BEFORE the refresh — the
  // same contract as trim: a fresh split point must never be a solver
  // unknown, or the planegcs pass can move it and re-open the loop
  // the split just created.
  if (!minted.empty()) {
    const std::unordered_set<std::string> minted_set(
        minted.begin(), minted.end());
    auto pin = [&](const std::string& point_id, double x, double y) {
      if (!minted_set.count(point_id)) return;
      const auto it = std::find_if(
          feature_it->sketch_parameters->vertices.begin(),
          feature_it->sketch_parameters->vertices.end(),
          [&](const SketchVertex& v) { return v.id == point_id; });
      if (it == feature_it->sketch_parameters->vertices.end()) {
        feature_it->sketch_parameters->vertices.push_back(SketchVertex{
            .id = point_id,
            .kind = "endpoint",
            .x = x,
            .y = y,
            .is_fixed = true,
        });
      } else {
        it->x = x;
        it->y = y;
        it->is_fixed = true;
      }
    };
    const auto& p = *feature_it->sketch_parameters;
    for (const auto& line : p.lines) {
      pin(line.start_vertex_id, line.start_x, line.start_y);
      pin(line.end_vertex_id, line.end_x, line.end_y);
    }
    for (const auto& arc : p.arcs) {
      pin(arc.start_vertex_id, arc.start_x, arc.start_y);
      pin(arc.end_vertex_id, arc.end_x, arc.end_y);
    }
    for (const auto& ellipse : p.ellipses) {
      if (!ellipse.has_sweep) continue;
      const double cu = std::cos(ellipse.rotation);
      const double su = std::sin(ellipse.rotation);
      auto pt_at = [&](double ang) -> std::pair<double, double> {
        const double ct = std::cos(ang), st = std::sin(ang);
        return {ellipse.center_x + ellipse.a * ct * cu -
                    ellipse.b * st * su,
                ellipse.center_y + ellipse.a * ct * su +
                    ellipse.b * st * cu};
      };
      if (!ellipse.start_vertex_id.empty()) {
        const auto [sx, sy] = pt_at(ellipse.sweep_start_angle);
        pin(ellipse.start_vertex_id, sx, sy);
      }
      if (!ellipse.end_vertex_id.empty()) {
        const auto [ex, ey] = pt_at(ellipse.sweep_end_angle);
        pin(ellipse.end_vertex_id, ex, ey);
      }
    }
    for (const auto& spline : p.splines) {
      for (size_t i = 0;
           i < spline.pole_vertex_ids.size() &&
           i < spline.pole_xs.size() && i < spline.pole_ys.size(); ++i) {
        pin(spline.pole_vertex_ids[i], spline.pole_xs[i], spline.pole_ys[i]);
      }
    }
  }

  // Refresh derived state: rebuild points, recompute profiles, reify
  // dimensions, and re-sync projection/dependency state.
  polysmith::core::refresh_sketch_derived_state(*feature_it);

  refresh_linked_extrudes(*document_, *feature_it);
  document_->selected_feature_id = feature_it->id;
  document_->selected_sketch_vertex_id = std::nullopt;
  document_->selected_sketch_entity_id = std::nullopt;
  document_->selected_sketch_dimension_id = std::nullopt;
  document_->selected_sketch_profile_id = std::nullopt;
  document_->selected_sketch_profile_ids.clear();
  document_->selected_sketch_vertex_ids.clear();
  document_->selected_sketch_entity_ids.clear();
  bump_geometry_revision();
  return document_.value();
}
