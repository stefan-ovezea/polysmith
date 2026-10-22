DocumentState DocumentManager::select_sketch_profiles(
    const std::vector<std::string>& profile_ids, bool additive) {
  require_document();

  // Marquee over SURFACES: the UI decides which faces the rubber band covers
  // (picking is interaction state), the core records the result in one revision
  // so 50 selected faces are not 50 round trips.  Works without an active
  // sketch, because selecting surfaces for an extrusion happens in the model
  // view.
  std::vector<std::string> valid_ids;
  std::vector<std::string> unknown_ids;
  std::optional<std::string> owning_feature_id;
  for (const std::string& profile_id : profile_ids) {
    const auto feature_it = find_sketch_feature_owning_profile(
        document_->feature_history, profile_id);
    if (feature_it == document_->feature_history.end()) {
      unknown_ids.push_back(profile_id);
      continue;
    }
    if (!owning_feature_id.has_value()) {
      owning_feature_id = feature_it->id;
    }
    valid_ids.push_back(profile_id);
  }
  if (!unknown_ids.empty()) {
    log_warn("sketch_selection",
             "select_sketch_profiles: " +
                 std::to_string(unknown_ids.size()) +
                 " unknown profile id(s) skipped");
  }
  if (valid_ids.empty()) {
    return document_.value();
  }

  document_->selected_feature_id = owning_feature_id.value();
  document_->selected_sketch_vertex_id = std::nullopt;
  document_->selected_sketch_vertex_ids.clear();
  document_->selected_sketch_entity_id = std::nullopt;
  document_->selected_sketch_entity_ids.clear();
  document_->selected_sketch_dimension_id = std::nullopt;
  if (additive) {
    auto& selected = document_->selected_sketch_profile_ids;
    for (const std::string& profile_id : valid_ids) {
      const auto existing =
          std::find(selected.begin(), selected.end(), profile_id);
      if (existing == selected.end()) {
        selected.push_back(profile_id);
      } else {
        selected.erase(existing);
      }
    }
  } else {
    document_->selected_sketch_profile_ids = valid_ids;
  }
  document_->selected_sketch_profile_id =
      document_->selected_sketch_profile_ids.empty()
          ? std::optional<std::string>{}
          : std::optional<std::string>{
                document_->selected_sketch_profile_ids.back()};
  return document_.value();
}

DocumentState DocumentManager::select_sketch_profiles_rect(
    const std::string& feature_id, double x1, double y1, double x2, double y2,
    bool window_mode, bool additive) {
  require_document();

  const std::string target_id =
      feature_id.empty()
          ? (document_->active_sketch_feature_id.has_value()
                 ? document_->active_sketch_feature_id.value()
                 : std::string{})
          : feature_id;
  if (target_id.empty()) {
    throw std::runtime_error("No sketch to resolve the marquee against");
  }
  const auto feature_it = std::find_if(
      document_->feature_history.begin(), document_->feature_history.end(),
      [&](const FeatureEntry& feature) { return feature.id == target_id; });
  if (feature_it == document_->feature_history.end() ||
      !feature_it->sketch_parameters.has_value()) {
    throw std::runtime_error("Sketch feature not found: " + target_id);
  }

  const double rx1 = std::min(x1, x2);
  const double ry1 = std::min(y1, y2);
  const double rx2 = std::max(x1, x2);
  const double ry2 = std::max(y1, y2);

  std::vector<std::string> hit_ids;
  for (const auto& profile : feature_it->sketch_parameters->profiles) {
    double bx1 = 0.0, by1 = 0.0, bx2 = 0.0, by2 = 0.0;
    bool have_bounds = false;
    const auto expand = [&](double px, double py) {
      if (!have_bounds) {
        bx1 = bx2 = px;
        by1 = by2 = py;
        have_bounds = true;
      } else {
        bx1 = std::min(bx1, px);
        by1 = std::min(by1, py);
        bx2 = std::max(bx2, px);
        by2 = std::max(by2, py);
      }
    };
    for (const auto& point : profile.points) {
      expand(point.x, point.y);
    }
    if (!have_bounds) {
      for (const auto& edge : profile.boundary_edges) {
        expand(edge.start_x, edge.start_y);
        expand(edge.end_x, edge.end_y);
      }
    }
    if (!have_bounds && profile.radius > 0.0) {
      expand(profile.center_x - profile.radius, profile.center_y - profile.radius);
      expand(profile.center_x + profile.radius, profile.center_y + profile.radius);
    }
    if (!have_bounds) {
      continue;
    }
    const bool hit = window_mode
                         ? (bx1 >= rx1 && bx2 <= rx2 && by1 >= ry1 && by2 <= ry2)
                         : !(bx2 < rx1 || bx1 > rx2 || by2 < ry1 || by1 > ry2);
    if (hit) {
      hit_ids.push_back(profile.id);
    }
  }

  // A non-additive marquee over empty space clears the profile
  // selection (a drag is an explicit re-selection, not a click).
  if (hit_ids.empty() && !additive) {
    document_->selected_sketch_profile_ids.clear();
    document_->selected_sketch_profile_id = std::nullopt;
    document_->selected_sketch_vertex_ids.clear();
    document_->selected_sketch_entity_ids.clear();
    return document_.value();
  }
  return select_sketch_profiles(hit_ids, additive);
}

DocumentState DocumentManager::select_sketch_profile(const std::string& profile_id,
                                                     bool additive) {
  require_document();

  // Selection of profiles is allowed both inside and outside an active sketch.
  // The owning sketch feature is located by scanning the feature history.
  const auto feature_it = find_sketch_feature_owning_profile(
      document_->feature_history, profile_id);

  if (feature_it == document_->feature_history.end()) {
    throw std::runtime_error("Sketch profile not found: " + profile_id);
  }

  const auto profile_it = std::find_if(
      feature_it->sketch_parameters->profiles.begin(),
      feature_it->sketch_parameters->profiles.end(),
      [&](const SketchProfileRegion& profile) { return profile.id == profile_id; });

  if (profile_it == feature_it->sketch_parameters->profiles.end()) {
    throw std::runtime_error("Sketch profile not found: " + profile_id);
  }

  document_->selected_feature_id = feature_it->id;
  document_->selected_sketch_vertex_id = std::nullopt;
  document_->selected_sketch_entity_id = std::nullopt;
  document_->selected_sketch_dimension_id = std::nullopt;
  if (additive) {
    auto& selected = document_->selected_sketch_profile_ids;
    const auto existing = std::find(selected.begin(), selected.end(), profile_id);
    if (existing == selected.end()) {
      selected.push_back(profile_id);
    } else {
      selected.erase(existing);
    }
  } else {
    document_->selected_sketch_profile_ids = {profile_id};
    // A non-additive profile click REPLACES the previous selection:
    // entity and vertex ids survive here otherwise, so a Delete after
    // clicking a surface would remove BOTH the previously-selected
    // entities and the surface's boundary — deleting more than the
    // user can see selected.
    document_->selected_sketch_entity_ids.clear();
    document_->selected_sketch_vertex_ids.clear();
  }
  document_->selected_sketch_profile_id =
      document_->selected_sketch_profile_ids.empty()
          ? std::optional<std::string>{}
          : std::optional<std::string>{document_->selected_sketch_profile_ids.back()};
  return document_.value();
}

namespace {

// A profile owns an entity when the entity appears on its boundary in
// ANY of the region's id lists: the exact walk order (ordered_edge_ids
// / boundary_edges), the legacy line list, or as the source circle of
// a circle-kind region.
bool profile_owns_entity(const SketchProfileRegion& profile,
                         const std::string& entity_id) {
  if (profile.source_circle_id.has_value() &&
      profile.source_circle_id.value() == entity_id) {
    return true;
  }
  const auto id_in = [&](const std::vector<std::string>& ids) {
    return std::find(ids.begin(), ids.end(), entity_id) != ids.end();
  };
  if (id_in(profile.ordered_edge_ids) || id_in(profile.line_ids)) {
    return true;
  }
  return std::any_of(
      profile.boundary_edges.begin(), profile.boundary_edges.end(),
      [&](const ProfileBoundaryEdge& edge) {
        return edge.entity_id == entity_id;
      });
}

}  // namespace

DocumentState DocumentManager::select_sketch_profile_by_entity(
    const std::string& entity_id, bool additive, bool smallest_only) {
  require_document();

  // The clicked entity belongs to one sketch feature; scan the history
  // for the owner (same doctrine as the profile lookup).
  const auto feature_it = std::find_if(
      document_->feature_history.begin(), document_->feature_history.end(),
      [&](const FeatureEntry& feature) {
        if (feature.kind != "sketch" ||
            !feature.sketch_parameters.has_value()) {
          return false;
        }
        const auto& sketch = feature.sketch_parameters.value();
        const auto owns = [&](const auto& entities) {
          return std::any_of(entities.begin(), entities.end(),
                             [&](const auto& entity) {
                               return entity.id == entity_id;
                             });
        };
        return owns(sketch.lines) || owns(sketch.circles) ||
               owns(sketch.arcs) || owns(sketch.ellipses) ||
               owns(sketch.splines);
      });

  if (feature_it == document_->feature_history.end()) {
    throw std::runtime_error("Sketch entity not found: " + entity_id);
  }

  std::vector<std::string> owning_profile_ids;
  for (const auto& profile : feature_it->sketch_parameters->profiles) {
    if (profile_owns_entity(profile, entity_id)) {
      owning_profile_ids.push_back(profile.id);
    }
  }
  if (owning_profile_ids.empty()) {
    throw std::runtime_error(
        "Sketch entity is not on a profile boundary: " + entity_id);
  }

  if (smallest_only && owning_profile_ids.size() > 1) {
    // Shared-boundary click: keep the smallest owning region so an
    // outline click behaves like an interior click.  Circle regions
    // use πr²; polygon regions use the shoelace area of their
    // sampled points (the ordering is robust — a plate region is
    // orders of magnitude larger than a spoke sharing its hole edge).
    const auto area_of = [&](const std::string& profile_id) {
      const auto it = std::find_if(
          feature_it->sketch_parameters->profiles.begin(),
          feature_it->sketch_parameters->profiles.end(),
          [&](const SketchProfileRegion& profile) {
            return profile.id == profile_id;
          });
      if (it == feature_it->sketch_parameters->profiles.end()) {
        return std::numeric_limits<double>::infinity();
      }
      if (it->kind == "circle") {
        return 3.14159265358979323846 * it->radius * it->radius;
      }
      double twice_area = 0.0;
      const auto& pts = it->points;
      for (size_t i = 0; i < pts.size(); ++i) {
        const auto& a = pts[i];
        const auto& b = pts[(i + 1) % pts.size()];
        twice_area += a.x * b.y - b.x * a.y;
      }
      return std::abs(twice_area) / 2.0;
    };
    owning_profile_ids = {*std::min_element(
        owning_profile_ids.begin(), owning_profile_ids.end(),
        [&](const std::string& lhs, const std::string& rhs) {
          return area_of(lhs) < area_of(rhs);
        })};
  }

  document_->selected_feature_id = feature_it->id;
  document_->selected_sketch_vertex_id = std::nullopt;
  document_->selected_sketch_entity_id = std::nullopt;
  document_->selected_sketch_dimension_id = std::nullopt;
  if (additive) {
    for (const auto& profile_id : owning_profile_ids) {
      auto& selected = document_->selected_sketch_profile_ids;
      const auto existing =
          std::find(selected.begin(), selected.end(), profile_id);
      if (existing == selected.end()) {
        selected.push_back(profile_id);
      } else {
        selected.erase(existing);
      }
    }
  } else {
    document_->selected_sketch_profile_ids = owning_profile_ids;
  }
  document_->selected_sketch_profile_id =
      document_->selected_sketch_profile_ids.empty()
          ? std::optional<std::string>{}
          : std::optional<std::string>{
                document_->selected_sketch_profile_ids.back()};
  return document_.value();
}
