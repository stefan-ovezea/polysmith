  if (payload.contains("extrude_parameters") &&
      !payload.at("extrude_parameters").is_null()) {
    feature.extrude_parameters =
        parse_extrude_parameters_from_payload(payload.at("extrude_parameters"));
  }

  if (payload.contains("loft_parameters") &&
      !payload.at("loft_parameters").is_null()) {
    feature.loft_parameters =
        loft_parameters_from_payload(payload.at("loft_parameters"));
  }

  if (payload.contains("revolve_parameters") &&
      !payload.at("revolve_parameters").is_null()) {
    feature.revolve_parameters =
        revolve_parameters_from_payload(payload.at("revolve_parameters"));
  }

  if (payload.contains("sweep_parameters") &&
      !payload.at("sweep_parameters").is_null()) {
    feature.sweep_parameters =
        sweep_parameters_from_payload(payload.at("sweep_parameters"));
  }

  if (payload.contains("sketch_parameters") &&
      !payload.at("sketch_parameters").is_null()) {
    feature.sketch_parameters =
        sketch_parameters_from_payload(payload.at("sketch_parameters"));
  }
