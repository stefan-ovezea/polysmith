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
