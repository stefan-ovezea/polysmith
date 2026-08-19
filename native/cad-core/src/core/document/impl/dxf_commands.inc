DocumentState DocumentManager::import_dxf(
    const std::string& file_path,
    const std::optional<std::string>& plane_id) {
  require_document();

  if (file_path.empty()) {
    throw std::runtime_error("DXF file path is empty");
  }
  if (!std::filesystem::exists(file_path)) {
    throw std::runtime_error("DXF file not found: " + file_path);
  }

  // Parse FIRST, before any document mutation — a parse failure leaves
  // the document untouched and the error event is the only side effect.
  const DxfImportResult parsed = polysmith::core::import_dxf_geometry(file_path);

  // Reuse the sketch-creation path verbatim: validates the plane,
  // pushes ONE undo state (covering the whole import), creates and
  // activates the sketch feature.
  const std::string plane = plane_id.value_or("ref-plane-xy");
  start_sketch_on_plane(plane);

  FeatureEntry& sketch = document_->feature_history.back();

  // Bulk insertion — one derived-state refresh for the whole batch
  // instead of one per entity (per-entity refreshes are O(n²) and
  // freeze the core for large files). Mirrors the projection path.
  const size_t lines_before = sketch.sketch_parameters->lines.size();
  const size_t circles_before = sketch.sketch_parameters->circles.size();
  const size_t arcs_before = sketch.sketch_parameters->arcs.size();

  for (const auto& line : parsed.lines) {
    // Imported geometry stays EXACT: no coincident-line-body snapping
    // (dense tessellations would stick to nearby segments) and no
    // H/V constraint inference (near-axis segments would be snapped
    // axis-aligned, deforming curves). Shared exact endpoints still
    // coalesce through find_coincident_endpoint inside add_sketch_line.
    polysmith::core::add_sketch_line(sketch,
                                     next_sketch_line_id_++,
                                     line.x1,
                                     line.y1,
                                     line.x2,
                                     line.y2,
                                     /*is_construction=*/false,
                                     /*snap_start=*/false,
                                     /*auto_dimensions=*/false,
                                     /*refresh_immediately=*/false,
                                     /*infer_constraints=*/false);
  }
  for (const auto& circle : parsed.circles) {
    polysmith::core::add_sketch_circle(sketch,
                                       next_sketch_circle_id_++,
                                       circle.cx,
                                       circle.cy,
                                       circle.r,
                                       /*is_construction=*/false,
                                       /*auto_dimensions=*/false,
                                       /*refresh_immediately=*/false);
  }
  for (const auto& arc : parsed.arcs) {
    // Arc endpoint point ids are minted from the shared line counter
    // space so ids never collide (same convention as projection).
    const int start_point_index = next_sketch_line_id_++;
    const int end_point_index = next_sketch_line_id_++;
    polysmith::core::add_sketch_arc(sketch,
                                    next_sketch_arc_id_++,
                                    start_point_index,
                                    end_point_index,
                                    arc.sx,
                                    arc.sy,
                                    arc.ex,
                                    arc.ey,
                                    arc.cx,
                                    arc.cy,
                                    arc.r,
                                    arc.ccw,
                                    /*is_construction=*/false,
                                    /*auto_dimensions=*/false,
                                    /*refresh_immediately=*/false);
  }
  int point_index = 0;
  for (const auto& point : parsed.points) {
    // Standalone points live in projected_points so the vertex rebuild
    // preserves them (they are not derivable from any entity).
    SketchProjectedPoint entry;
    entry.id = "projected-point-" +
               std::to_string(next_sketch_projected_point_id_++);
    entry.source_id = "dxf:point:" + std::to_string(point_index++);
    entry.x = point.x;
    entry.y = point.y;
    sketch.sketch_parameters->projected_points.push_back(entry);
  }

  // Refresh once to build the vertex records, then refresh again with
  // the final focus set. Unlike projection, imported geometry is NOT
  // locked: it is ordinary editable sketch geometry, and fixed flags
  // would both clutter the canvas with fix badges and block later
  // constraint edits ("Cannot make a line horizontal when both
  // endpoints are fixed").
  std::vector<std::string> focus_ids;
  focus_ids.reserve(parsed.lines.size() + parsed.circles.size() +
                    parsed.arcs.size());
  for (size_t i = lines_before; i < sketch.sketch_parameters->lines.size();
       ++i) {
    focus_ids.push_back(sketch.sketch_parameters->lines[i].id);
  }
  for (size_t i = circles_before;
       i < sketch.sketch_parameters->circles.size(); ++i) {
    focus_ids.push_back(sketch.sketch_parameters->circles[i].id);
  }
  for (size_t i = arcs_before; i < sketch.sketch_parameters->arcs.size();
       ++i) {
    focus_ids.push_back(sketch.sketch_parameters->arcs[i].id);
  }
  polysmith::core::refresh_sketch_with_append_focus(sketch, focus_ids);

  // Imported points live in projected_points (the only standalone-point
  // storage that survives the vertex rebuild), but that mechanism
  // auto-locks them as "derived" on every refresh. Unfix them so they
  // behave like regular imported geometry; sync_fixed_point_flags
  // preserves the unfix across later refreshes.
  const size_t points_base =
      sketch.sketch_parameters->projected_points.size() - parsed.points.size();
  for (size_t i = 0; i < parsed.points.size(); ++i) {
    polysmith::core::set_sketch_vertex_fixed(
        sketch, sketch.sketch_parameters->projected_points[points_base + i].id,
        false,
        /*refresh_immediately=*/false);
  }

  polysmith::core::refresh_sketch_with_append_focus(sketch, focus_ids);

  // Set AFTER the final refresh — refresh_sketch_derived_state
  // regenerates parameters_summary and would clobber this.
  sketch.name = "DXF Import";
  std::string summary = std::to_string(parsed.entity_count()) + " entities";
  if (parsed.skipped_count > 0) {
    summary += " · skipped " + std::to_string(parsed.skipped_count);
  }
  sketch.parameters_summary = summary;

  polysmith::core::log_info(
      "dxf_import",
      "Imported " + std::to_string(parsed.entity_count()) +
          " entities from " + file_path +
          (parsed.units_scaled ? " (inches scaled to mm)" : ""));
  for (const auto& warning : parsed.warnings) {
    polysmith::core::log_info("dxf_import", warning);
  }
  if (parsed.skipped_count > 0) {
    std::string breakdown;
    for (const auto& [kind, count] : parsed.skipped_by_kind) {
      if (!breakdown.empty()) {
        breakdown += ", ";
      }
      breakdown += kind + ": " + std::to_string(count);
    }
    polysmith::core::log_warn(
        "dxf_import",
        "Skipped " + std::to_string(parsed.skipped_count) +
            " unsupported or degenerate entities (" + breakdown + ")");
  }

  bump_geometry_revision();
  return document_.value();
}

ExportResult DocumentManager::export_document_as_dxf(
    const std::string& file_path) const {
  require_document();

  if (file_path.empty()) {
    throw std::runtime_error("DXF file path is empty");
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
      sketch_it->kind != "sketch" || !sketch_it->sketch_parameters.has_value()) {
    throw std::runtime_error("No active sketch");
  }

  return polysmith::core::export_sketch_as_dxf(
      sketch_it->sketch_parameters.value(), document_->units, file_path);
}
