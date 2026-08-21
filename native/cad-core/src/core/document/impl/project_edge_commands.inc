DocumentState DocumentManager::project_edge_into_sketch(
    const std::string& edge_id) {
  require_document();
  const auto sketch_it = require_projection_target(*document_);

  // Reject body edges that belong to an extrude built from this same
  // sketch — same rationale as the face guard above.
  const auto separator = edge_id.find(":edge:");
  if (separator == std::string::npos || separator == 0) {
    throw std::runtime_error("Malformed edge id: " + edge_id);
  }
  const std::string owner_id = edge_id.substr(0, separator);
  for (const auto& feature : document_->feature_history) {
    if (feature.kind != "extrude" || !feature.extrude_parameters.has_value()) {
      continue;
    }
    if (feature.extrude_parameters->sketch_feature_id == sketch_it->id &&
        feature.id == owner_id) {
      throw std::runtime_error(
          "Cannot project an edge from an extrude back onto its own source "
          "sketch");
    }
  }

  // Idempotency: same dedup index that face projection uses.
  for (const auto& projection :
       sketch_it->sketch_parameters->projections) {
    if (projection.source_id == edge_id) {
      return document_.value();
    }
  }

  const auto edge_geometry = compute_edge_geometry(*document_, edge_id);
  if (!edge_geometry.has_value()) {
    throw std::runtime_error(
        "Edge could not be resolved against the current document: " +
        edge_id);
  }

  const auto& frame = sketch_it->sketch_parameters->plane_frame.value();

  push_undo_state();
  clear_redo_stack();

  // See `project_face_into_sketch` for the rationale: we capture the
  // ids of every emitted entity so `refresh_sketch_projections` can
  // re-derive them from the upstream edge on every recompute.
  SketchProjection record;
  record.id =
      "projection-" + std::to_string(next_sketch_projection_id_++);
  record.source_id = edge_id;
  record.source_kind = "edge";

  if (edge_geometry->kind == "line") {
    const auto a = world_to_sketch_local(frame,
                                         edge_geometry->start.x,
                                         edge_geometry->start.y,
                                         edge_geometry->start.z);
    const auto b = world_to_sketch_local(frame,
                                         edge_geometry->end.x,
                                         edge_geometry->end.y,
                                         edge_geometry->end.z);
    // Reject zero-length projections — happens when the edge runs
    // perpendicular to the sketch plane and collapses to a point.
    // v1 leaves these alone rather than generating a degenerate
    // line. Vertex projection covers the "I want a point at the
    // collapsed location" intent.
    const double dx = b.first - a.first;
    const double dy = b.second - a.second;
    if (dx * dx + dy * dy < 1e-12) {
      throw std::runtime_error(
          "Edge projects to a zero-length segment (perpendicular to "
          "sketch plane). Project the endpoint as a vertex instead.");
    }

    const size_t lines_before = sketch_it->sketch_parameters->lines.size();
    polysmith::core::add_sketch_line(*sketch_it,
                                     next_sketch_line_id_++,
                                     a.first,
                                     a.second,
                                     b.first,
                                     b.second,
                                     /*is_construction=*/false,
                                     /*snap_start=*/true,
                                     /*auto_dimensions=*/false,
                                     /*refresh_immediately=*/false);
    // Bulk insertion — see project_face_commands.inc for the rationale:
    // refresh once so the vertex records exist, then fix without
    // per-call refreshes.
    for (size_t i = lines_before;
         i < sketch_it->sketch_parameters->lines.size(); ++i) {
      record.generated_line_ids.push_back(
          sketch_it->sketch_parameters->lines[i].id);
    }
    polysmith::core::refresh_sketch_with_append_focus(
        *sketch_it, record.generated_line_ids);
    for (const auto& line_id : record.generated_line_ids) {
      const auto& line = *std::find_if(
          sketch_it->sketch_parameters->lines.begin(),
          sketch_it->sketch_parameters->lines.end(),
          [&](const auto& candidate) { return candidate.id == line_id; });
      polysmith::core::set_sketch_vertex_fixed(
          *sketch_it, line.start_vertex_id, true,
          /*refresh_immediately=*/false);
      polysmith::core::set_sketch_vertex_fixed(
          *sketch_it, line.end_vertex_id, true,
          /*refresh_immediately=*/false);
    }
  } else if (edge_geometry->kind == "circle" || edge_geometry->kind == "arc") {
    if (!circle_axis_parallel_to_sketch(frame, edge_geometry->axis)) {
      throw std::runtime_error(
          "Curved edge projects to an ellipse (its plane isn't parallel "
          "to the sketch). Not supported in v1.");
    }

    const auto center =
        world_to_sketch_local(frame,
                              edge_geometry->center.x,
                              edge_geometry->center.y,
                              edge_geometry->center.z);

    if (edge_geometry->kind == "circle") {
      const size_t circles_before =
          sketch_it->sketch_parameters->circles.size();
      polysmith::core::add_sketch_circle(*sketch_it,
                                         next_sketch_circle_id_++,
                                         center.first,
                                         center.second,
                                         edge_geometry->radius,
                                         /*is_construction=*/false,
                                         /*auto_dimensions=*/false,
                                         /*refresh_immediately=*/false);
      for (size_t i = circles_before;
           i < sketch_it->sketch_parameters->circles.size(); ++i) {
        record.generated_circle_ids.push_back(
            sketch_it->sketch_parameters->circles[i].id);
      }
    } else {
      const auto start = world_to_sketch_local(frame,
                                               edge_geometry->start.x,
                                               edge_geometry->start.y,
                                               edge_geometry->start.z);
      const auto end = world_to_sketch_local(frame,
                                             edge_geometry->end.x,
                                             edge_geometry->end.y,
                                             edge_geometry->end.z);
      // Determine winding by checking whether the body axis points
      // in the same direction as the sketch normal. When parallel,
      // the body's natural CCW direction matches the sketch's CCW
      // direction; when anti-parallel, it's flipped.
      const double dot = edge_geometry->axis.x * frame.normal_x +
                         edge_geometry->axis.y * frame.normal_y +
                         edge_geometry->axis.z * frame.normal_z;
      const bool ccw = dot > 0.0;
      const int start_point_index = next_sketch_line_id_++;
      const int end_point_index = next_sketch_line_id_++;
      const size_t arcs_before = sketch_it->sketch_parameters->arcs.size();
      polysmith::core::add_sketch_arc(*sketch_it,
                                      next_sketch_arc_id_++,
                                      start_point_index,
                                      end_point_index,
                                      start.first,
                                      start.second,
                                      end.first,
                                      end.second,
                                      center.first,
                                      center.second,
                                      edge_geometry->radius,
                                      ccw,
                                      /*is_construction=*/false,
                                      /*auto_dimensions=*/false,
                                      /*refresh_immediately=*/false);
      for (size_t i = arcs_before;
           i < sketch_it->sketch_parameters->arcs.size(); ++i) {
        record.generated_arc_ids.push_back(
            sketch_it->sketch_parameters->arcs[i].id);
      }
    }
  } else {
    // "unsupported" — propagate as a controlled error. Caller (the
    // UI) surfaces it as a transient toast.
    throw std::runtime_error(
        "Edge type is not supported by the Project tool yet: " + edge_id);
  }

  // Bulk insertion — see project_face_commands.inc for the rationale.
  // Collect the focus ids BEFORE moving the record.
  std::vector<std::string> focus_ids = record.generated_line_ids;
  focus_ids.insert(focus_ids.end(), record.generated_circle_ids.begin(),
                   record.generated_circle_ids.end());
  focus_ids.insert(focus_ids.end(), record.generated_arc_ids.begin(),
                   record.generated_arc_ids.end());
  polysmith::core::refresh_sketch_with_append_focus(*sketch_it, focus_ids);

  // Projected arcs are derived geometry — lock their endpoints AND
  // center (previously left free, so a projected fillet edge could be
  // dragged off its cached arc). Vertex records exist only after the
  // refresh above. MUST run before the record moves into the
  // projections list — a moved-from record has empty id vectors.
  for (const auto& arc_id : record.generated_arc_ids) {
    const auto arc_it = std::find_if(
        sketch_it->sketch_parameters->arcs.begin(),
        sketch_it->sketch_parameters->arcs.end(),
        [&](const auto& candidate) { return candidate.id == arc_id; });
    if (arc_it == sketch_it->sketch_parameters->arcs.end()) {
      continue;
    }
    polysmith::core::set_sketch_vertex_fixed(*sketch_it,
                                             arc_it->start_vertex_id,
                                             true,
                                             /*refresh_immediately=*/false);
    polysmith::core::set_sketch_vertex_fixed(*sketch_it,
                                             arc_it->end_vertex_id,
                                             true,
                                             /*refresh_immediately=*/false);
    polysmith::core::set_sketch_vertex_fixed(*sketch_it,
                                             arc_it->center_vertex_id,
                                             true,
                                             /*refresh_immediately=*/false);
  }

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
