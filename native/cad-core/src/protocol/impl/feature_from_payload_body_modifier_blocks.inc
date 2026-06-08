  if (payload.contains("fillet_parameters") &&
      !payload.at("fillet_parameters").is_null()) {
    const json& fp = payload.at("fillet_parameters");
    polysmith::core::FilletFeatureParameters params{};
    params.target_body_id = read_string(fp, "target_body_id");
    if (fp.contains("edge_ids") && fp.at("edge_ids").is_array()) {
      for (const auto& edge : fp.at("edge_ids")) {
        params.edge_ids.push_back(edge.get<std::string>());
      }
    }
    params.radius = read_number(fp, "radius");
    // Default false when absent so older saved documents (created
    // before is_pending existed) load with the same semantics they
    // had at save time — i.e. "already confirmed".
    if (fp.contains("is_pending") && fp.at("is_pending").is_boolean()) {
      params.is_pending = fp.at("is_pending").get<bool>();
    }
    feature.fillet_parameters = params;
  }

  if (payload.contains("chamfer_parameters") &&
      !payload.at("chamfer_parameters").is_null()) {
    const json& cp = payload.at("chamfer_parameters");
    polysmith::core::ChamferFeatureParameters params{};
    params.target_body_id = read_string(cp, "target_body_id");
    if (cp.contains("edge_ids") && cp.at("edge_ids").is_array()) {
      for (const auto& edge : cp.at("edge_ids")) {
        params.edge_ids.push_back(edge.get<std::string>());
      }
    }
    params.distance = read_number(cp, "distance");
    if (cp.contains("is_pending") && cp.at("is_pending").is_boolean()) {
      params.is_pending = cp.at("is_pending").get<bool>();
    }
    feature.chamfer_parameters = params;
  }

  if (payload.contains("shell_parameters") &&
      !payload.at("shell_parameters").is_null()) {
    const json& sp = payload.at("shell_parameters");
    polysmith::core::ShellFeatureParameters params{};
    params.target_body_id = read_string(sp, "target_body_id");
    if (sp.contains("removed_face_ids") &&
        sp.at("removed_face_ids").is_array()) {
      for (const auto& face : sp.at("removed_face_ids")) {
        params.removed_face_ids.push_back(face.get<std::string>());
      }
    }
    params.thickness = read_number(sp, "thickness");
    if (sp.contains("is_pending") && sp.at("is_pending").is_boolean()) {
      params.is_pending = sp.at("is_pending").get<bool>();
    }
    feature.shell_parameters = params;
  }
