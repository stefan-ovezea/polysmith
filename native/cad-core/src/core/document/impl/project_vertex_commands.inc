DocumentState DocumentManager::project_vertex_into_sketch(
    const std::string& vertex_id) {
  require_document();
  const auto sketch_it = require_projection_target(*document_);

  // Idempotency: same dedup index that face / edge projection uses.
  for (const auto& projection :
       sketch_it->sketch_parameters->projections) {
    if (projection.source_id == vertex_id) {
      return document_.value();
    }
  }

  const auto position = compute_vertex_position(*document_, vertex_id);
  if (!position.has_value()) {
    throw std::runtime_error(
        "Vertex could not be resolved against the current document: " +
        vertex_id);
  }

  const auto& frame = sketch_it->sketch_parameters->plane_frame.value();
  const auto local = world_to_sketch_local(frame,
                                           position->x,
                                           position->y,
                                           position->z);

  push_undo_state();
  clear_redo_stack();

  const std::string point_id = "projected-point-" +
                               std::to_string(next_sketch_projected_point_id_++);
  sketch_it->sketch_parameters->projected_points.push_back(
      SketchProjectedPoint{
          .id = point_id,
          .source_id = vertex_id,
          .x = local.first,
          .y = local.second,
      });

  // Live-link record. Same shape as face / edge but only points to a
  // single `SketchProjectedPoint`; `refresh_sketch_projections` uses
  // `generated_point_id` to find and patch the cached (x, y).
  SketchProjection projection_record;
  projection_record.id =
      "projection-" + std::to_string(next_sketch_projection_id_++);
  projection_record.source_id = vertex_id;
  projection_record.source_kind = "vertex";
  projection_record.generated_point_id = point_id;
  sketch_it->sketch_parameters->projections.push_back(
      std::move(projection_record));

  // Force the points list to pick up the new projected point in this
  // frame. (`refresh_linked_extrudes` won't run sketch derived state
  // for us — it only refreshes downstream features.)
  refresh_sketch_derived_state(*sketch_it);
  refresh_linked_extrudes(*document_, *sketch_it);
  document_->selected_feature_id = sketch_it->id;
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
