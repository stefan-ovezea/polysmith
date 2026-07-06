polysmith::core::DocumentState document_from_payload(const json& payload) {
  polysmith::core::DocumentState document{};
  document.id = read_string(payload, "document_id");
  document.name = read_string(payload, "name");
  document.units = read_string(payload, "units");
  document.revision = read_int(payload, "revision");
  document.selected_feature_id =
      read_optional_string(payload, "selected_feature_id");
  document.selected_reference_id =
      read_optional_string(payload, "selected_reference_id");
  document.selected_face_id = read_optional_string(payload, "selected_face_id");
  // Backward compat: pre-multi-select saves used a single
  // `selected_edge_id` (string|null). New saves use an array
  // `selected_edge_ids`. Read whichever is present so older `.polysmith`
  // files keep loading.
  if (payload.contains("selected_edge_ids") &&
      payload.at("selected_edge_ids").is_array()) {
    for (const auto& entry : payload.at("selected_edge_ids")) {
      if (entry.is_string()) {
        document.selected_edge_ids.push_back(entry.get<std::string>());
      }
    }
  } else {
    const auto legacy = read_optional_string(payload, "selected_edge_id");
    if (legacy.has_value()) {
      document.selected_edge_ids.push_back(*legacy);
    }
  }
  // Same back-compat shape as selected_edge_ids: read the array form
  // when present, fall back to the legacy single `selected_vertex_id`
  // for old `.polysmith` saves.
  if (payload.contains("selected_vertex_ids") &&
      payload.at("selected_vertex_ids").is_array()) {
    for (const auto& entry : payload.at("selected_vertex_ids")) {
      if (entry.is_string()) {
        document.selected_vertex_ids.push_back(entry.get<std::string>());
      }
    }
  } else {
    const auto legacy = read_optional_string(payload, "selected_vertex_id");
    if (legacy.has_value()) {
      document.selected_vertex_ids.push_back(*legacy);
    }
  }
  document.active_sketch_plane_id =
      read_optional_string(payload, "active_sketch_plane_id");
  document.active_sketch_face_id =
      read_optional_string(payload, "active_sketch_face_id");
  document.active_sketch_feature_id =
      read_optional_string(payload, "active_sketch_feature_id");
  document.active_sketch_tool =
      read_optional_string(payload, "active_sketch_tool");
  document.selected_sketch_point_id =
      read_optional_string(payload, "selected_sketch_point_id");
  document.selected_sketch_entity_id =
      read_optional_string(payload, "selected_sketch_entity_id");
  if (payload.contains("selected_sketch_point_ids") &&
      payload.at("selected_sketch_point_ids").is_array()) {
    for (const auto& id : payload.at("selected_sketch_point_ids")) {
      if (id.is_string()) {
        document.selected_sketch_point_ids.push_back(id.get<std::string>());
      }
    }
  }
  if (payload.contains("selected_sketch_entity_ids") &&
      payload.at("selected_sketch_entity_ids").is_array()) {
    for (const auto& id : payload.at("selected_sketch_entity_ids")) {
      if (id.is_string()) {
        document.selected_sketch_entity_ids.push_back(id.get<std::string>());
      }
    }
  }
  document.selected_sketch_dimension_id =
      read_optional_string(payload, "selected_sketch_dimension_id");
  document.selected_sketch_profile_id =
      read_optional_string(payload, "selected_sketch_profile_id");
  if (payload.contains("selected_sketch_profile_ids") &&
      payload.at("selected_sketch_profile_ids").is_array()) {
    for (const auto& entry : payload.at("selected_sketch_profile_ids")) {
      if (entry.is_string()) {
        document.selected_sketch_profile_ids.push_back(entry.get<std::string>());
      }
    }
  } else if (document.selected_sketch_profile_id.has_value()) {
    document.selected_sketch_profile_ids.push_back(
        document.selected_sketch_profile_id.value());
  }
  if (payload.contains("timeline_cursor") &&
      payload.at("timeline_cursor").is_number_integer()) {
    document.timeline_cursor = payload.at("timeline_cursor").get<int>();
  } else {
    document.timeline_cursor = std::nullopt;
  }

  if (payload.contains("feature_history") &&
      payload.at("feature_history").is_array()) {
    for (const auto& feature_payload : payload.at("feature_history")) {
      document.feature_history.push_back(
          feature_entry_from_payload(feature_payload));
    }
  }

  // Parameters — absent in older saves, default to empty
  if (payload.contains("parameters") &&
      payload.at("parameters").is_array()) {
    for (const auto& param_payload : payload.at("parameters")) {
      polysmith::core::ParameterEntry param;
      param.name = read_string(param_payload, "name");
      param.expression = read_string(param_payload, "expression");
      param.resolved_value = read_number(param_payload, "resolved_value");
      // kind default is "length" for backward compat with old saves
      if (param_payload.contains("kind") &&
          param_payload.at("kind").is_string()) {
        param.kind = param_payload.at("kind").get<std::string>();
      }
      param.has_error = read_bool(param_payload, "has_error");
      if (param_payload.contains("error_message") &&
          param_payload.at("error_message").is_string()) {
        param.error_message =
            param_payload.at("error_message").get<std::string>();
      }
      document.parameters.push_back(param);
    }
  }

  if (payload.contains("appearance") &&
      payload.at("appearance").is_object()) {
    const auto& appearance = payload.at("appearance");
    if (appearance.contains("body_colors") &&
        appearance.at("body_colors").is_array()) {
      for (const auto& entry_payload : appearance.at("body_colors")) {
        if (!entry_payload.is_object()) {
          continue;
        }
        polysmith::core::BodyAppearanceOverride entry;
        entry.body_id = read_string(entry_payload, "body_id");
        entry.color = read_string(entry_payload, "color");
        if (polysmith::core::is_valid_appearance_color(entry.color)) {
          document.appearance.body_colors.push_back(entry);
        }
      }
    }
    if (appearance.contains("face_colors") &&
        appearance.at("face_colors").is_array()) {
      for (const auto& entry_payload : appearance.at("face_colors")) {
        if (!entry_payload.is_object()) {
          continue;
        }
        polysmith::core::FaceAppearanceOverride entry;
        entry.face_id = read_string(entry_payload, "face_id");
        entry.owner_body_id = read_string(entry_payload, "owner_body_id");
        entry.signature = read_string(entry_payload, "signature");
        entry.color = read_string(entry_payload, "color");
        if (polysmith::core::is_valid_appearance_color(entry.color)) {
          document.appearance.face_colors.push_back(entry);
        }
      }
    }
  }

  // Read selection filter (v1 default if absent in old payloads)
  if (payload.contains("selection_filter") &&
      payload.at("selection_filter").is_object()) {
    const auto& sf = payload.at("selection_filter");
    auto& filter = document.selection_filter;
    if (sf.contains("select_curves") && sf.at("select_curves").is_boolean())
      filter.select_curves = sf.at("select_curves").get<bool>();
    if (sf.contains("select_points") && sf.at("select_points").is_boolean())
      filter.select_points = sf.at("select_points").get<bool>();
    if (sf.contains("select_construction") && sf.at("select_construction").is_boolean())
      filter.select_construction = sf.at("select_construction").get<bool>();
    if (sf.contains("select_constraints") && sf.at("select_constraints").is_boolean())
      filter.select_constraints = sf.at("select_constraints").get<bool>();
    if (sf.contains("snap_endpoint") && sf.at("snap_endpoint").is_boolean())
      filter.snap_endpoint = sf.at("snap_endpoint").get<bool>();
    if (sf.contains("snap_midpoint") && sf.at("snap_midpoint").is_boolean())
      filter.snap_midpoint = sf.at("snap_midpoint").get<bool>();
    if (sf.contains("snap_center") && sf.at("snap_center").is_boolean())
      filter.snap_center = sf.at("snap_center").get<bool>();
    if (sf.contains("snap_intersection") && sf.at("snap_intersection").is_boolean())
      filter.snap_intersection = sf.at("snap_intersection").get<bool>();
    if (sf.contains("snap_nearest") && sf.at("snap_nearest").is_boolean())
      filter.snap_nearest = sf.at("snap_nearest").get<bool>();
    if (sf.contains("snap_quadrant") && sf.at("snap_quadrant").is_boolean())
      filter.snap_quadrant = sf.at("snap_quadrant").get<bool>();
    if (sf.contains("snap_perpendicular") && sf.at("snap_perpendicular").is_boolean())
      filter.snap_perpendicular = sf.at("snap_perpendicular").get<bool>();
    if (sf.contains("snap_parallel") && sf.at("snap_parallel").is_boolean())
      filter.snap_parallel = sf.at("snap_parallel").get<bool>();
    if (sf.contains("snap_tangent") && sf.at("snap_tangent").is_boolean())
      filter.snap_tangent = sf.at("snap_tangent").get<bool>();
    if (sf.contains("snap_grid") && sf.at("snap_grid").is_boolean())
      filter.snap_grid = sf.at("snap_grid").get<bool>();
    if (sf.contains("snap_grid_line") && sf.at("snap_grid_line").is_boolean())
      filter.snap_grid_line = sf.at("snap_grid_line").get<bool>();
    if (sf.contains("snap_polar") && sf.at("snap_polar").is_boolean())
      filter.snap_polar = sf.at("snap_polar").get<bool>();
    if (sf.contains("polar_angle_degrees") && sf.at("polar_angle_degrees").is_number())
      filter.polar_angle_degrees = sf.at("polar_angle_degrees").get<int>();
    if (sf.contains("magnetic_pull") && sf.at("magnetic_pull").is_boolean())
      filter.magnetic_pull = sf.at("magnetic_pull").get<bool>();
    if (sf.contains("tolerance_px") && sf.at("tolerance_px").is_number())
      filter.tolerance_px = sf.at("tolerance_px").get<int>();
  }

  // CAM data — loaded from the "cam" key in target-schema documents.
  // Older documents used "cam_setup" / "tool_library" / "cam_operations"
  // keys; those are no longer supported.  CAM deserialization will be
  // implemented when the CAM workspace is re-built on the new types.
  // For now, documents start with an empty default CamDocumentData.
  (void)payload;

  return document;
}

