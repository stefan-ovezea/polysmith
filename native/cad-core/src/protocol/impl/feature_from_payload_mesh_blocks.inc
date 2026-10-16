  if (payload.contains("mesh_import_parameters") &&
      !payload.at("mesh_import_parameters").is_null()) {
    const json& mp = payload.at("mesh_import_parameters");
    polysmith::core::MeshImportFeatureParameters params{};
    params.file_path = read_string(mp, "file_path");
    // Default 1.0 when absent so older saves (written before scale
    // existed) load at their original size.
    if (mp.contains("scale") && mp.at("scale").is_number()) {
      params.scale = mp.at("scale").get<double>();
    }
    feature.mesh_import_parameters = params;
  }

  if (payload.contains("mesh_to_body_parameters") &&
      !payload.at("mesh_to_body_parameters").is_null()) {
    const json& mp = payload.at("mesh_to_body_parameters");
    polysmith::core::MeshToBodyFeatureParameters params{};
    params.source_body_id = read_string(mp, "source_body_id");
    if (mp.contains("serialized_shape") && mp.at("serialized_shape").is_string()) {
      params.serialized_shape = mp.at("serialized_shape").get<std::string>();
    }
    feature.mesh_to_body_parameters = params;
  }

  if (payload.contains("step_import_parameters") &&
      !payload.at("step_import_parameters").is_null()) {
    const json& mp = payload.at("step_import_parameters");
    polysmith::core::StepImportFeatureParameters params{};
    params.file_path = read_string(mp, "file_path");
    params.source_units = read_string(mp, "source_units");
    // The live `imported_shape` handle stays null after load — the
    // body compiler deserializes `serialized_shape` on its first
    // compile.
    if (mp.contains("serialized_shape") && mp.at("serialized_shape").is_string()) {
      params.serialized_shape = mp.at("serialized_shape").get<std::string>();
    }
    if (mp.contains("solid_count") && mp.at("solid_count").is_number()) {
      params.solid_count = mp.at("solid_count").get<int>();
    }
    if (mp.contains("face_count") && mp.at("face_count").is_number()) {
      params.face_count = mp.at("face_count").get<int>();
    }
    feature.step_import_parameters = params;
  }

  if (payload.contains("iges_import_parameters") &&
      !payload.at("iges_import_parameters").is_null()) {
    const json& mp = payload.at("iges_import_parameters");
    polysmith::core::IgesImportFeatureParameters params{};
    params.file_path = read_string(mp, "file_path");
    params.source_units = read_string(mp, "source_units");
    // Same lazy-deserialize model as step_import — the live handle
    // stays null after load and the compiler falls back to the
    // snapshot.
    if (mp.contains("serialized_shape") && mp.at("serialized_shape").is_string()) {
      params.serialized_shape = mp.at("serialized_shape").get<std::string>();
    }
    if (mp.contains("solid_count") && mp.at("solid_count").is_number()) {
      params.solid_count = mp.at("solid_count").get<int>();
    }
    if (mp.contains("face_count") && mp.at("face_count").is_number()) {
      params.face_count = mp.at("face_count").get<int>();
    }
    feature.iges_import_parameters = params;
  }

  if (payload.contains("pcb_layer_parameters") &&
      !payload.at("pcb_layer_parameters").is_null()) {
    const json& mp = payload.at("pcb_layer_parameters");
    polysmith::core::PcbLayerParameters params{};
    params.file_path = read_string(mp, "file_path");
    params.format = read_string(mp, "format") == "excellon"
                        ? polysmith::core::PcbLayerFormat::Excellon
                        : polysmith::core::PcbLayerFormat::Gerber;
    params.role = polysmith::core::parse_pcb_role(read_string(mp, "role"));
    params.file_polarity = read_string(mp, "file_polarity");
    params.file_function = read_string(mp, "file_function");
    if (mp.contains("role_from_attributes") && mp.at("role_from_attributes").is_boolean()) {
      params.role_from_attributes = mp.at("role_from_attributes").get<bool>();
    }
    if (mp.contains("mirror_x") && mp.at("mirror_x").is_boolean()) {
      params.mirror_x = mp.at("mirror_x").get<bool>();
    }
    // Summary counts are informational: a missing or malformed number must not
    // fail the load, because the geometry is re-derived from the file anyway.
    const auto read_int = [&mp](const char* key) {
      if (mp.contains(key) && mp.at(key).is_number()) {
        return mp.at(key).get<int>();
      }
      return 0;
    };
    params.ring_count = read_int("ring_count");
    params.hit_count = read_int("hit_count");
    params.tool_count = read_int("tool_count");
    if (mp.contains("bounds_valid") && mp.at("bounds_valid").is_boolean()) {
      params.bounds_valid = mp.at("bounds_valid").get<bool>();
    }
    const auto read_double = [&mp](const char* key) {
      if (mp.contains(key) && mp.at(key).is_number()) {
        return mp.at(key).get<double>();
      }
      return 0.0;
    };
    params.min_x = read_double("min_x");
    params.min_y = read_double("min_y");
    params.max_x = read_double("max_x");
    params.max_y = read_double("max_y");
    if (mp.contains("warnings") && mp.at("warnings").is_array()) {
      for (const auto& entry : mp.at("warnings")) {
        if (entry.is_string()) {
          params.warnings.push_back(entry.get<std::string>());
        }
      }
    }
    feature.pcb_layer_parameters = params;
  }
