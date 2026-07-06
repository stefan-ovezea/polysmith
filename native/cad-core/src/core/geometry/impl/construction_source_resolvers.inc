std::optional<PlaneFrame> resolve_plane_source_frame(
    const DocumentState& document,
    const std::string& source_id) {
  // Origin reference plane — static frame.
  if (auto frame = origin_plane_frame(source_id); frame.has_value()) {
    return frame;
  }

  // Body face — "<body_id>:face:<index>".
  if (const auto target = parse_face_target(source_id); target.has_value()) {
    return resolve_face_frame(document, target->body_id, target->face_index);
  }

  // Sketch profile — use the owning sketch plane, centered on the
  // profile region so the offset plane appears where the user picked.
  if (auto frame = resolve_sketch_profile_frame(document, source_id);
      frame.has_value()) {
    return frame;
  }

  // Construction-plane feature id — read its cached frame. The
  // topological walk in `refresh_history_dependencies` runs each
  // construction plane in feature_history order, so by the time a
  // downstream feature asks for an upstream construction plane's
  // frame, the upstream feature's cache has already been refreshed.
  if (const FeatureEntry* feature = find_feature(document, source_id);
      feature != nullptr &&
      feature->kind == "construction_plane" &&
      feature->construction_plane_parameters.has_value()) {
    return feature->construction_plane_parameters->plane_frame;
  }

  return std::nullopt;
}

std::optional<PlaneFrame> resolve_tangent_plane_source_frame(
    const DocumentState& document,
    const std::string& source_id) {
  const auto target = parse_face_target(source_id);
  if (!target.has_value()) {
    return std::nullopt;
  }
  return resolve_body_face_frame(document,
                                 target->body_id,
                                 target->face_index,
                                 false);
}

std::optional<ConstructionAxisFrame> resolve_angle_plane_axis(
    const DocumentState& document,
    const std::string& source_id) {
  const auto axis = resolve_construction_axis_source(document, source_id);
  if (!axis.has_value()) {
    return std::nullopt;
  }
  return ConstructionAxisFrame{
      .start_x = axis->start_x,
      .start_y = axis->start_y,
      .start_z = axis->start_z,
      .end_x = axis->end_x,
      .end_y = axis->end_y,
      .end_z = axis->end_z,
  };
}

std::optional<ConstructionAxisFeatureParameters> resolve_construction_axis_source(
    const DocumentState& document,
    const std::string& source_id) {
  for (const auto& feature : document.feature_history) {
    if (feature.id == source_id && feature.kind == "construction_axis" &&
        feature.construction_axis_parameters.has_value()) {
      ConstructionAxisFeatureParameters params =
          feature.construction_axis_parameters.value();
      params.source_id = source_id;
      params.source_kind = "construction_axis";
      return params;
    }
  }

  if (source_id.find(":edge:") != std::string::npos) {
    const auto edge = compute_edge_geometry(document, source_id);
    if (!edge.has_value() || edge->kind != "line") {
      return std::nullopt;
    }
    return ConstructionAxisFeatureParameters{
        .source_id = source_id,
        .source_kind = "edge",
        .start_x = edge->start.x,
        .start_y = edge->start.y,
        .start_z = edge->start.z,
        .end_x = edge->end.x,
        .end_y = edge->end.y,
        .end_z = edge->end.z,
    };
  }

  for (const auto& feature : document.feature_history) {
    if (feature.kind != "sketch" || !feature.sketch_parameters.has_value()) {
      continue;
    }
    const auto& sketch = feature.sketch_parameters.value();
    const auto line_it = std::find_if(
        sketch.lines.begin(), sketch.lines.end(),
        [&](const SketchLine& line) { return line.id == source_id; });
    if (line_it == sketch.lines.end()) {
      continue;
    }
    const std::optional<PlaneFrame> frame =
        sketch.plane_frame.has_value()
            ? std::optional<PlaneFrame>(
                  from_sketch_plane_frame(sketch.plane_frame.value()))
            : origin_plane_frame(sketch.plane_id);
    if (!frame.has_value()) {
      return std::nullopt;
    }

    const auto start =
        sketch_local_to_world(frame.value(), line_it->start_x, line_it->start_y);
    const auto end =
        sketch_local_to_world(frame.value(), line_it->end_x, line_it->end_y);
    return ConstructionAxisFeatureParameters{
        .source_id = source_id,
        .source_kind = "sketch_line",
        .start_x = start[0],
        .start_y = start[1],
        .start_z = start[2],
        .end_x = end[0],
        .end_y = end[1],
        .end_z = end[2],
    };
  }

  return std::nullopt;
}

std::optional<ConstructionPointFeatureParameters> resolve_construction_point_source(
    const DocumentState& document,
    const std::string& source_id) {
  if (source_id.find(":vertex:") != std::string::npos) {
    const auto point = compute_vertex_position(document, source_id);
    if (!point.has_value()) {
      return std::nullopt;
    }
    return ConstructionPointFeatureParameters{
        .source_id = source_id,
        .source_kind = "vertex",
        .x = point->x,
        .y = point->y,
        .z = point->z,
    };
  }

  for (const auto& feature : document.feature_history) {
    if (feature.kind != "sketch" || !feature.sketch_parameters.has_value()) {
      continue;
    }
    const auto& sketch = feature.sketch_parameters.value();
    const auto point_it = std::find_if(
        sketch.points.begin(), sketch.points.end(),
        [&](const SketchVertex& point) { return point.id == source_id; });
    if (point_it == sketch.points.end()) {
      continue;
    }
    const std::optional<PlaneFrame> frame =
        sketch.plane_frame.has_value()
            ? std::optional<PlaneFrame>(
                  from_sketch_plane_frame(sketch.plane_frame.value()))
            : origin_plane_frame(sketch.plane_id);
    if (!frame.has_value()) {
      return std::nullopt;
    }

    const auto world =
        sketch_local_to_world(frame.value(), point_it->x, point_it->y);
    return ConstructionPointFeatureParameters{
        .source_id = source_id,
        .source_kind = "sketch_point",
        .x = world[0],
        .y = world[1],
        .z = world[2],
    };
  }

  return std::nullopt;
}
