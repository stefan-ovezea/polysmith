DocumentState DocumentManager::delete_sketch_selection(
    const std::vector<std::string>& entity_ids,
    const std::vector<std::string>& point_ids,
    const std::vector<std::string>& profile_ids) {
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
  if (feature_it == document_->feature_history.end() ||
      !feature_it->sketch_parameters.has_value()) {
    throw std::runtime_error("Active sketch feature not found");
  }

  auto& parameters = *feature_it->sketch_parameters;
  std::unordered_set<std::string> line_ids;
  std::unordered_set<std::string> circle_ids;
  std::unordered_set<std::string> arc_ids;
  std::unordered_set<std::string> projected_point_ids;
  std::unordered_set<std::string> selected_point_ids;
  std::unordered_set<std::string> fillet_ids;

  add_ids(selected_point_ids, point_ids);

  for (const auto& profile_id : profile_ids) {
    const auto profile_it = std::find_if(
        parameters.profiles.begin(),
        parameters.profiles.end(),
        [&](const SketchProfileRegion& profile) {
          return profile.id == profile_id;
        });
    if (profile_it == parameters.profiles.end()) {
      continue;
    }
    add_ids(line_ids, profile_it->line_ids);
    if (profile_it->source_circle_id.has_value()) {
      add_id(circle_ids, profile_it->source_circle_id.value());
    }
  }

  for (const auto& entity_id : entity_ids) {
    add_id(line_ids, entity_id);
    add_id(circle_ids, entity_id);
    add_id(arc_ids, entity_id);
    add_id(fillet_ids, entity_id);
  }

  for (const auto& point_id : selected_point_ids) {
    for (const auto& line : parameters.lines) {
      if (line.start_point_id == point_id || line.end_point_id == point_id) {
        add_id(line_ids, line.id);
      }
    }
    for (const auto& arc : parameters.arcs) {
      if (arc.start_point_id == point_id || arc.end_point_id == point_id) {
        add_id(arc_ids, arc.id);
      }
    }
    for (const auto& circle : parameters.circles) {
      if ("point-circle-" + circle.id + "-center" == point_id) {
        add_id(circle_ids, circle.id);
      }
    }
    for (const auto& projected : parameters.projected_points) {
      if (projected.id == point_id) {
        add_id(projected_point_ids, projected.id);
      }
    }
  }

  for (const auto& fillet : parameters.fillets) {
    if (id_in_set(line_ids, fillet.line_a_id) ||
        id_in_set(line_ids, fillet.line_b_id) ||
        id_in_set(arc_ids, fillet.arc_id) ||
        id_in_set(selected_point_ids, fillet.corner_point_id) ||
        id_in_set(selected_point_ids, fillet.trim_a_point_id) ||
        id_in_set(selected_point_ids, fillet.trim_b_point_id)) {
      add_id(fillet_ids, fillet.id);
    }
  }

  const auto has_selected_line = [&](const SketchLine& line) {
    return id_in_set(line_ids, line.id);
  };
  const auto has_selected_circle = [&](const SketchCircle& circle) {
    return id_in_set(circle_ids, circle.id);
  };
  const auto has_selected_arc = [&](const SketchArc& arc) {
    return id_in_set(arc_ids, arc.id);
  };
  const auto has_selected_projected_point =
      [&](const SketchProjectedPoint& projected) {
        return id_in_set(projected_point_ids, projected.id);
      };

  const bool will_delete =
      std::any_of(parameters.lines.begin(), parameters.lines.end(), has_selected_line) ||
      std::any_of(parameters.circles.begin(),
                  parameters.circles.end(),
                  has_selected_circle) ||
      std::any_of(parameters.arcs.begin(), parameters.arcs.end(), has_selected_arc) ||
      std::any_of(parameters.projected_points.begin(),
                  parameters.projected_points.end(),
                  has_selected_projected_point) ||
      std::any_of(parameters.fillets.begin(),
                  parameters.fillets.end(),
                  [&](const SketchFillet& fillet) {
                    return id_in_set(fillet_ids, fillet.id);
                  });
  if (!will_delete) {
    return document_.value();
  }

  // Collect point IDs that will become orphaned (referenced only by
  // deleted entities).  MUST run BEFORE erasing lines/circles/arcs —
  // after the erase the entity lists are empty and we can't find the
  // endpoints anymore.
  std::unordered_set<std::string> deleted_point_ids;
  for (const auto& line : parameters.lines) {
    if (id_in_set(line_ids, line.id)) {
      add_id(deleted_point_ids, line.start_point_id);
      add_id(deleted_point_ids, line.end_point_id);
    }
  }
  for (const auto& circle : parameters.circles) {
    if (id_in_set(circle_ids, circle.id)) {
      add_id(deleted_point_ids, "point-circle-" + circle.id + "-center");
    }
  }
  for (const auto& arc : parameters.arcs) {
    if (id_in_set(arc_ids, arc.id)) {
      add_id(deleted_point_ids, arc.start_point_id);
      add_id(deleted_point_ids, arc.end_point_id);
    }
  }
  // Remove points that are still referenced by surviving entities.
  for (const auto& line : parameters.lines) {
    if (!id_in_set(line_ids, line.id)) {
      deleted_point_ids.erase(line.start_point_id);
      deleted_point_ids.erase(line.end_point_id);
    }
  }
  for (const auto& circle : parameters.circles) {
    if (!id_in_set(circle_ids, circle.id)) {
      deleted_point_ids.erase("point-circle-" + circle.id + "-center");
    }
  }
  for (const auto& arc : parameters.arcs) {
    if (!id_in_set(arc_ids, arc.id)) {
      deleted_point_ids.erase(arc.start_point_id);
      deleted_point_ids.erase(arc.end_point_id);
    }
  }

  push_undo_state();
  clear_redo_stack();

  std::vector<std::string> fillets_to_delete;
  for (const auto& fillet : parameters.fillets) {
    if (id_in_set(fillet_ids, fillet.id)) {
      fillets_to_delete.push_back(fillet.id);
    }
  }
  for (const auto& fillet_id : fillets_to_delete) {
    polysmith::core::delete_sketch_fillet(*feature_it, fillet_id);
  }

  parameters.lines.erase(
      std::remove_if(parameters.lines.begin(), parameters.lines.end(), has_selected_line),
      parameters.lines.end());
  parameters.circles.erase(
      std::remove_if(parameters.circles.begin(),
                     parameters.circles.end(),
                     has_selected_circle),
      parameters.circles.end());
  parameters.arcs.erase(
      std::remove_if(parameters.arcs.begin(), parameters.arcs.end(), has_selected_arc),
      parameters.arcs.end());
  parameters.projected_points.erase(
      std::remove_if(parameters.projected_points.begin(),
                     parameters.projected_points.end(),
                     has_selected_projected_point),
      parameters.projected_points.end());

  const auto deleted_entity = [&](const std::string& id) {
    return id_in_set(line_ids, id) || id_in_set(circle_ids, id) ||
           id_in_set(arc_ids, id) || id_in_set(deleted_point_ids, id);
  };
  parameters.dimensions.erase(
      std::remove_if(parameters.dimensions.begin(),
                     parameters.dimensions.end(),
                     [&](const SketchDimension& dimension) {
                       return deleted_entity(dimension.entity_id) ||
                              deleted_entity(dimension.secondary_entity_id);
                     }),
      parameters.dimensions.end());
  parameters.line_relations.erase(
      std::remove_if(parameters.line_relations.begin(),
                     parameters.line_relations.end(),
                     [&](const SketchLineRelation& relation) {
                       return deleted_entity(relation.first_line_id) ||
                              deleted_entity(relation.second_line_id);
                     }),
      parameters.line_relations.end());
  parameters.midpoint_anchors.erase(
      std::remove_if(parameters.midpoint_anchors.begin(),
                     parameters.midpoint_anchors.end(),
                     [&](const SketchMidpointAnchor& anchor) {
                       return id_in_set(selected_point_ids, anchor.point_id) ||
                              id_in_set(line_ids, anchor.line_id);
                     }),
      parameters.midpoint_anchors.end());
  parameters.point_line_anchors.erase(
      std::remove_if(parameters.point_line_anchors.begin(),
                     parameters.point_line_anchors.end(),
                     [&](const SketchPointLineAnchor& anchor) {
                       return id_in_set(selected_point_ids, anchor.point_id) ||
                              id_in_set(line_ids, anchor.line_id);
                     }),
      parameters.point_line_anchors.end());
  for (auto& projection : parameters.projections) {
    remove_ids_from_vector(projection.generated_line_ids, line_ids);
    remove_ids_from_vector(projection.generated_circle_ids, circle_ids);
    remove_ids_from_vector(projection.generated_arc_ids, arc_ids);
    if (!projection.generated_point_id.empty() &&
        id_in_set(projected_point_ids, projection.generated_point_id)) {
      projection.generated_point_id.clear();
    }
  }
  parameters.projections.erase(
      std::remove_if(parameters.projections.begin(),
                     parameters.projections.end(),
                     [](const SketchProjection& projection) {
                       return !projection_has_generated_entities(projection);
                     }),
      parameters.projections.end());

  refresh_sketch_derived_state(*feature_it);
  refresh_linked_extrudes(*document_, *feature_it);
  document_->selected_feature_id = feature_it->id;
  document_->selected_sketch_point_id = std::nullopt;
  document_->selected_sketch_entity_id = std::nullopt;
  document_->selected_sketch_dimension_id = std::nullopt;
  document_->selected_sketch_profile_id = std::nullopt;
  document_->selected_sketch_profile_ids.clear();
  document_->selected_sketch_point_ids.clear();
  document_->selected_sketch_entity_ids.clear();
  bump_geometry_revision();
  return document_.value();
}
