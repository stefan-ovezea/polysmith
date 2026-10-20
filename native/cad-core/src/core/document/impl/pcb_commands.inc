DocumentState DocumentManager::import_pcb_layer(
    const std::string& file_path, const std::optional<std::string>& role) {
  require_document();

  if (file_path.empty()) {
    throw std::runtime_error("PCB layer path is empty");
  }
  std::error_code exists_error;
  if (!std::filesystem::exists(file_path, exists_error)) {
    throw std::runtime_error("PCB layer file not found: " + file_path);
  }

  const PcbRole role_override =
      role.has_value() ? parse_pcb_role(*role) : PcbRole::Unknown;

  // Parse BEFORE mutating: an unreadable file still gets a feature (so the
  // user sees *what* failed in the timeline) but the document's geometry is
  // untouched and the failure is reported, not thrown.
  const polysmith::core::pcb::PcbLayerLoad loaded =
      polysmith::core::pcb::load_pcb_layer(file_path, role_override);

  push_undo_state();
  clear_redo_stack();

  PcbLayerParameters params{};
  params.file_path = file_path;
  params.format = loaded.format;
  params.role = loaded.role;
  params.file_polarity = loaded.file_polarity;
  params.file_function = loaded.file_function;
  params.role_from_attributes = loaded.role_from_attributes;
  params.mirror_x = pcb_role_is_bottom(loaded.role);
  params.ring_count = loaded.ring_count;
  params.hit_count = loaded.hit_count;
  params.tool_count = loaded.tool_count;
  params.bounds_valid = loaded.bounds.valid;
  params.min_x = loaded.bounds.min_x;
  params.min_y = loaded.bounds.min_y;
  params.max_x = loaded.bounds.max_x;
  params.max_y = loaded.bounds.max_y;
  params.warnings = loaded.warnings;
  if (!loaded.ok && !loaded.error.empty()) {
    params.warnings.insert(params.warnings.begin(), loaded.error);
  }

  const std::string file_name = std::filesystem::path(file_path).filename().string();
  std::ostringstream summary;
  summary << file_name;
  if (loaded.format == PcbLayerFormat::Excellon) {
    summary << " · " << loaded.tool_count << " tool(s) · " << loaded.hit_count << " hole(s)";
  } else {
    summary << " · " << loaded.ring_count << " ring(s)";
  }
  if (loaded.bounds.valid) {
    summary << " · " << std::fixed << std::setprecision(1)
            << (loaded.bounds.max_x - loaded.bounds.min_x) << "×"
            << (loaded.bounds.max_y - loaded.bounds.min_y) << " mm";
  }
  if (!loaded.ok) {
    summary << " · " << loaded.error;
  }

  FeatureEntry feature{
      .id = "feature-" + std::to_string(next_feature_id_++),
      .kind = "pcb_layer",
      .name = pcb_role_label(params.role),
      .status = loaded.ok ? "healthy" : "error",
      .parameters_summary = summary.str(),
      .dependency_broken = !loaded.ok,
      .dependency_warning = loaded.ok ? std::string() : loaded.error,
      .pcb_layer_parameters = params,
  };
  document_->feature_history.push_back(std::move(feature));
  document_->selected_feature_id = document_->feature_history.back().id;
  document_->selected_face_id = std::nullopt;
  document_->selected_edge_ids.clear();
  document_->selected_vertex_ids.clear();
  bump_geometry_revision();

  if (loaded.ok) {
    // Polarity only applies to Gerber layers (drill plating is part of the
    // role), so it is omitted rather than printed as an empty pair.
    std::string polarity;
    if (!params.file_polarity.empty()) {
      polarity = " (" + params.file_polarity + ")";
    }
    log_info("pcb_import",
             "Imported '" + file_name + "' as " + pcb_role_label(params.role) + polarity +
                 "; role from " + loaded.role_note);
    for (const std::string& warning : params.warnings) {
      log_warn("pcb_import", file_name + ": " + warning);
    }
  } else {
    log_error("pcb_import", "Could not import '" + file_name + "': " + loaded.error);
  }
  return document_.value();
}

DocumentState DocumentManager::update_pcb_layer(
    const std::string& feature_id, const std::optional<std::string>& role,
    const std::optional<bool>& mirror_x) {
  require_document();

  const auto found = std::find_if(
      document_->feature_history.begin(), document_->feature_history.end(),
      [&](const FeatureEntry& feature) { return feature.id == feature_id; });
  if (found == document_->feature_history.end() ||
      !found->pcb_layer_parameters.has_value()) {
    throw std::runtime_error("Unknown PCB layer: " + feature_id);
  }

  // An empty role re-resolves from the file's own attributes.
  PcbRole role_override = PcbRole::Unknown;
  const bool role_given = role.has_value() && !role->empty();
  if (role_given) {
    role_override = parse_pcb_role(*role);
    if (role_override == PcbRole::Unknown) {
      throw std::runtime_error("Unknown PCB layer role: " + *role);
    }
  }

  push_undo_state();
  clear_redo_stack();

  const auto refreshed = std::find_if(
      document_->feature_history.begin(), document_->feature_history.end(),
      [&](const FeatureEntry& feature) { return feature.id == feature_id; });
  PcbLayerParameters& params = *refreshed->pcb_layer_parameters;

  // Re-read the source: the summary must reflect what is on disk now, and a
  // file that vanished since import has to show up as a broken dependency
  // rather than a silently stale count.
  const polysmith::core::pcb::PcbLayerLoad loaded =
      polysmith::core::pcb::load_pcb_layer(params.file_path, role_override);
  const std::string file_name =
      std::filesystem::path(params.file_path).filename().string();
  if (loaded.ok) {
    params.format = loaded.format;
    params.role = loaded.role;
    params.file_polarity = loaded.file_polarity;
    params.file_function = loaded.file_function;
    params.role_from_attributes = loaded.role_from_attributes;
    params.ring_count = loaded.ring_count;
    params.hit_count = loaded.hit_count;
    params.tool_count = loaded.tool_count;
    params.bounds_valid = loaded.bounds.valid;
    params.min_x = loaded.bounds.min_x;
    params.min_y = loaded.bounds.min_y;
    params.max_x = loaded.bounds.max_x;
    params.max_y = loaded.bounds.max_y;
    params.warnings = loaded.warnings;
    refreshed->dependency_broken = false;
    refreshed->dependency_warning.clear();
    refreshed->status = "healthy";
    refreshed->name = pcb_role_label(params.role);
    std::ostringstream summary;
    summary << file_name << " · ";
    if (params.format == PcbLayerFormat::Excellon) {
      summary << params.tool_count << " tool(s) · " << params.hit_count << " hole(s)";
    } else {
      summary << params.ring_count << " ring(s)";
    }
    refreshed->parameters_summary = summary.str();
  } else {
    refreshed->dependency_broken = true;
    refreshed->dependency_warning = loaded.error;
    refreshed->status = "error";
    log_warn("pcb_import", "Layer '" + file_name + "' is no longer readable: " + loaded.error);
  }

  if (mirror_x.has_value()) {
    params.mirror_x = *mirror_x;
  } else if (role_given) {
    // Re-derive the default mirroring from the new role.
    params.mirror_x = pcb_role_is_bottom(params.role);
  }

  bump_geometry_revision();
  log_info("pcb_import", "Updated layer '" + file_name + "' → " +
                             pcb_role_label(params.role) +
                             (params.mirror_x ? " (mirrored)" : ""));
  return document_.value();
}

DocumentState DocumentManager::remove_pcb_layer(const std::string& feature_id) {
  require_document();

  const auto found = std::find_if(
      document_->feature_history.begin(), document_->feature_history.end(),
      [&](const FeatureEntry& feature) { return feature.id == feature_id; });
  if (found == document_->feature_history.end() ||
      !found->pcb_layer_parameters.has_value()) {
    throw std::runtime_error("Unknown PCB layer: " + feature_id);
  }

  push_undo_state();
  clear_redo_stack();

  const std::string file_name =
      std::filesystem::path(found->pcb_layer_parameters->file_path).filename().string();
  document_->feature_history.erase(found);
  if (document_->selected_feature_id.has_value() &&
      document_->selected_feature_id.value() == feature_id) {
    document_->selected_feature_id = std::nullopt;
  }
  // CAM operations that referenced this layer keep their parameters and any
  // captured witness data; their next recompute reports the broken dependency
  // the same way a deleted sketch would (the project's TNP rule: degrade with
  // a warning, never crash).
  bump_geometry_revision();
  log_info("pcb_import", "Removed layer '" + file_name + "'");
  return document_.value();
}
