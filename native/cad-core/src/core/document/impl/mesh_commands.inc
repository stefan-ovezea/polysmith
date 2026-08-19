DocumentState DocumentManager::import_stl(const std::string& file_path,
                                          double scale) {
  require_document();

  if (file_path.empty()) {
    throw std::runtime_error("Import path cannot be empty");
  }
  if (scale <= 0.0) {
    throw std::runtime_error("Import scale must be greater than zero");
  }
  if (!std::filesystem::exists(file_path)) {
    throw std::runtime_error("STL file not found: " + file_path);
  }

  push_undo_state();
  clear_redo_stack();

  MeshImportFeatureParameters params{};
  params.file_path = file_path;
  params.scale = scale;

  // The document stores only the path — the mesh is re-read from disk
  // on every compile, so the .polysmith file never embeds triangle data.
  const std::string file_name = std::filesystem::path(file_path).filename().string();
  std::ostringstream summary;
  summary << file_name;
  if (std::abs(scale - 1.0) > 1e-12) {
    summary << " · scale " << scale;
  }

  FeatureEntry feature{
      .id = "feature-" + std::to_string(next_feature_id_++),
      .kind = "mesh_import",
      .name = "Mesh Import",
      .status = "healthy",
      .parameters_summary = summary.str(),
      .mesh_import_parameters = params,
  };
  document_->feature_history.push_back(std::move(feature));
  document_->selected_feature_id = document_->feature_history.back().id;
  document_->selected_face_id = std::nullopt;
  document_->selected_edge_ids.clear();
  document_->selected_vertex_ids.clear();
  bump_geometry_revision();
  return document_.value();
}

DocumentState DocumentManager::convert_mesh_to_body(
    const std::string& source_body_id) {
  require_document();

  if (source_body_id.empty()) {
    throw std::runtime_error("Body id cannot be empty");
  }

  const auto source_it = std::find_if(
      document_->feature_history.begin(), document_->feature_history.end(),
      [&](const FeatureEntry& feature) { return feature.id == source_body_id; });
  if (source_it == document_->feature_history.end()) {
    throw std::runtime_error("Body not found: " + source_body_id);
  }
  if (source_it->kind != "mesh_import" ||
      !source_it->mesh_import_parameters.has_value()) {
    throw std::runtime_error(
        "convert_mesh_to_body requires a mesh body: " + source_body_id);
  }

  // Validate the conversion up front so the user gets a clear error
  // instead of a silently missing body.
  const TopoDS_Shape mesh_shape = build_feature_shape(*source_it);
  if (mesh_shape.IsNull()) {
    throw std::runtime_error(
        "Mesh source file is missing or unreadable: " +
        source_it->mesh_import_parameters->file_path);
  }
  const TopoDS_Shape converted = convert_mesh_to_solid(mesh_shape);
  if (converted.IsNull()) {
    throw std::runtime_error(
        "Mesh is not watertight — cannot convert to a solid body");
  }

  push_undo_state();
  clear_redo_stack();

  MeshToBodyFeatureParameters params{};
  params.source_body_id = source_body_id;
  // Snapshot the converted solid at creation (same pattern as a
  // standalone body copy): the result is independent of the mesh body,
  // which the user can delete afterwards without losing the solid.
  params.serialized_shape = serialize_shape_snapshot(converted);

  FeatureEntry feature{
      .id = "feature-" + std::to_string(next_feature_id_++),
      .kind = "mesh_to_body",
      .name = "Convert Mesh",
      .status = "healthy",
      .parameters_summary = "From " + source_it->name,
      .mesh_to_body_parameters = params,
  };
  document_->feature_history.push_back(std::move(feature));
  document_->selected_feature_id = document_->feature_history.back().id;
  bump_geometry_revision();
  return document_.value();
}

DocumentState DocumentManager::detach_body_projections(
    const std::string& body_id) {
  require_document();

  if (body_id.empty()) {
    throw std::runtime_error("Body id cannot be empty");
  }

  // Remove every live projection link sourced from this body (face /
  // edge / vertex projections carry "<body_id>:..." source ids; body
  // projections carry "body:<body_id>:<mode>"). The generated sketch
  // lines stay put — they are ordinary fixed lines; only the live
  // link goes away, so the body can be deleted without breaking the
  // sketches (and extrudes) built on top of the projection.
  const std::string face_prefix = body_id + ":";
  const std::string body_prefix = "body:" + face_prefix;
  bool would_remove = false;
  for (const auto& feature : document_->feature_history) {
    if (feature.kind != "sketch" || !feature.sketch_parameters.has_value()) {
      continue;
    }
    const auto& projections = feature.sketch_parameters->projections;
    if (std::any_of(projections.begin(), projections.end(),
                    [&](const SketchProjection& projection) {
                      return projection.source_id.rfind(face_prefix, 0) == 0 ||
                             projection.source_id.rfind(body_prefix, 0) == 0;
                    })) {
      would_remove = true;
      break;
    }
  }
  if (!would_remove) {
    return document_.value();
  }

  push_undo_state();
  clear_redo_stack();

  for (auto& feature : document_->feature_history) {
    if (feature.kind != "sketch" || !feature.sketch_parameters.has_value()) {
      continue;
    }
    auto& projections = feature.sketch_parameters->projections;
    const auto new_end = std::remove_if(
        projections.begin(), projections.end(),
        [&](const SketchProjection& projection) {
          return projection.source_id.rfind(face_prefix, 0) == 0 ||
                 projection.source_id.rfind(body_prefix, 0) == 0;
        });
    projections.erase(new_end, projections.end());
  }

  bump_geometry_revision();
  return document_.value();
}

DocumentState DocumentManager::project_body_into_sketch(
    const std::string& body_id, const std::string& mode) {
  require_document();

  if (mode != "section" && mode != "silhouette") {
    throw std::runtime_error(
        "project_body_into_sketch mode must be 'section' or 'silhouette'");
  }
  if (!document_->active_sketch_feature_id.has_value()) {
    throw std::runtime_error("No active sketch");
  }

  const auto sketch_it = std::find_if(
      document_->feature_history.begin(), document_->feature_history.end(),
      [&](const FeatureEntry& feature) {
        return feature.id == document_->active_sketch_feature_id.value();
      });
  if (sketch_it == document_->feature_history.end() ||
      !sketch_it->sketch_parameters.has_value()) {
    throw std::runtime_error(
        "Active sketch does not have a plane frame for projection");
  }

  // Unlike the face/edge/vertex tools, body projection also works on
  // origin ref-plane sketches via the legacy frame fallback.
  const auto frame = resolve_sketch_projection_frame(
      *sketch_it->sketch_parameters);
  if (!frame.has_value()) {
    throw std::runtime_error(
        "Active sketch does not have a plane frame for projection");
  }

  const auto body_it = std::find_if(
      document_->feature_history.begin(), document_->feature_history.end(),
      [&](const FeatureEntry& feature) { return feature.id == body_id; });
  if (body_it == document_->feature_history.end()) {
    throw std::runtime_error("Body not found: " + body_id);
  }
  const bool is_imported_mesh =
      body_it->kind == "mesh_import" &&
      body_it->mesh_import_parameters.has_value();
  const bool is_converted_mesh = body_it->kind == "mesh_to_body";
  if (!is_imported_mesh && !is_converted_mesh) {
    throw std::runtime_error(
        "project_body_into_sketch requires a mesh body (mesh_import or "
        "mesh_to_body): " + body_id);
  }

  // Imported meshes re-read the STL; converted meshes resolve through
  // the compiled body (their creation-time snapshot).
  TopoDS_Shape mesh_shape;
  if (is_imported_mesh) {
    mesh_shape = build_feature_shape(*body_it);
    if (mesh_shape.IsNull()) {
      throw std::runtime_error(
          "Mesh source file is missing or unreadable: " +
          body_it->mesh_import_parameters->file_path);
    }
  } else {
    const CompiledBodies compiled =
        compile_bodies(*document_, /*include_meshes=*/false);
    const auto body_it_compiled = std::find_if(
        compiled.bodies.begin(), compiled.bodies.end(),
        [&](const CompiledBody& body) { return body.id == body_id; });
    if (body_it_compiled == compiled.bodies.end()) {
      throw std::runtime_error(
          "Converted mesh body is unavailable: " + body_id);
    }
    mesh_shape = body_it_compiled->shape;
  }

  // Dedup index is shared with the face/edge/vertex tools; the mode is
  // part of the source id so section and silhouette projections of the
  // same body can coexist.
  const std::string source_id = "body:" + body_id + ":" + mode;
  for (const auto& projection : sketch_it->sketch_parameters->projections) {
    if (projection.source_id == source_id) {
      return document_.value();
    }
  }

  const auto polylines =
      mode == "section"
          ? compute_mesh_section_polylines(mesh_shape, frame.value())
          : compute_mesh_silhouette_polylines(mesh_shape, frame.value());
  if (polylines.empty()) {
    throw std::runtime_error(
        mode == "section"
            ? "The mesh body does not intersect the active sketch plane"
            : "The mesh body has no silhouette on the active sketch plane");
  }

  // If every projected segment already exists in the sketch (e.g. the
  // imported mesh and its converted solid share the same footprint),
  // the projection is a no-op — inserting coincident lines corrupts
  // the sketch (duplicate coincident vertices crash the rebuild).
  size_t total_segments = 0;
  size_t duplicate_segments = 0;
  for (const auto& polyline : polylines) {
    for (size_t k = 0; k + 1 < polyline.size(); ++k) {
      const auto a = world_to_sketch_local(frame.value(), polyline[k]);
      const auto b = world_to_sketch_local(frame.value(), polyline[k + 1]);
      if (a == b) {
        continue;  // degenerate closing artifact — not a real segment
      }
      ++total_segments;
      const bool duplicate = std::any_of(
          sketch_it->sketch_parameters->lines.begin(),
          sketch_it->sketch_parameters->lines.end(),
          [&](const auto& line) {
            const bool same_direction =
                std::abs(line.start_x - a.first) < 1e-6 &&
                std::abs(line.start_y - a.second) < 1e-6 &&
                std::abs(line.end_x - b.first) < 1e-6 &&
                std::abs(line.end_y - b.second) < 1e-6;
            const bool opposite_direction =
                std::abs(line.start_x - b.first) < 1e-6 &&
                std::abs(line.start_y - b.second) < 1e-6 &&
                std::abs(line.end_x - a.first) < 1e-6 &&
                std::abs(line.end_y - a.second) < 1e-6;
            return same_direction || opposite_direction;
          });
      if (duplicate) {
        ++duplicate_segments;
      }
    }
  }
  if (total_segments > 0 && duplicate_segments == total_segments) {
    return document_.value();  // already projected — no-op
  }

  // Refuse projections too complex to insert (see the constant's
  // comment) instead of hanging the core.
  size_t point_count = 0;
  for (const auto& polyline : polylines) {
    point_count += polyline.size();
  }
  if (point_count > kMaxProjectionOutlineCorners) {
    throw std::runtime_error(
        "Mesh projection is too complex (" + std::to_string(point_count) +
        " points, limit " + std::to_string(kMaxProjectionOutlineCorners) +
        "). Project a simpler body or use a reference plane.");
  }

  push_undo_state();
  clear_redo_stack();

  // A polyline of N points emits N-1 lines; closed loops carry the
  // duplicated closing point, so the emission rule is uniform.
  SketchProjection record;
  record.id =
      "projection-" + std::to_string(next_sketch_projection_id_++);
  record.source_id = source_id;
  record.source_kind = "body";

  const size_t lines_before = sketch_it->sketch_parameters->lines.size();
  for (const auto& polyline : polylines) {
    for (size_t k = 0; k + 1 < polyline.size(); ++k) {
      const auto a = world_to_sketch_local(frame.value(), polyline[k]);
      const auto b = world_to_sketch_local(frame.value(), polyline[k + 1]);
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
    }
  }
  // Bulk insertion — see project_face_commands.inc for the rationale:
  // one refresh builds the vertex records, the fix loop sets flags
  // without per-call refreshes, a final refresh picks the flags up.
  std::vector<std::string> focus_ids;
  for (size_t i = lines_before; i < sketch_it->sketch_parameters->lines.size();
       ++i) {
    focus_ids.push_back(sketch_it->sketch_parameters->lines[i].id);
  }
  polysmith::core::refresh_sketch_with_append_focus(*sketch_it, focus_ids);

  // Lock every endpoint — projected geometry is derived, same as the
  // face/edge/vertex Project tool.
  for (const auto& line_id : focus_ids) {
    const auto& line = *std::find_if(
        sketch_it->sketch_parameters->lines.begin(),
        sketch_it->sketch_parameters->lines.end(),
        [&](const auto& candidate) { return candidate.id == line_id; });
    polysmith::core::set_sketch_vertex_fixed(*sketch_it,
                                             line.start_vertex_id,
                                             true,
                                             /*refresh_immediately=*/false);
    polysmith::core::set_sketch_vertex_fixed(*sketch_it,
                                             line.end_vertex_id,
                                             true,
                                             /*refresh_immediately=*/false);
    record.generated_line_ids.push_back(line.id);
  }

  sketch_it->sketch_parameters->projections.push_back(std::move(record));
  polysmith::core::refresh_sketch_with_append_focus(*sketch_it, focus_ids);

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
