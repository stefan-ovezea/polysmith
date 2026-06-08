  if (payload.contains("box_parameters") &&
      !payload.at("box_parameters").is_null()) {
    const json& box_payload = payload.at("box_parameters");
    polysmith::core::BoxFeatureParameters box{};
    box.width = read_number(box_payload, "width");
    box.height = read_number(box_payload, "height");
    box.depth = read_number(box_payload, "depth");
    feature.box_parameters = box;
  }

  if (payload.contains("cylinder_parameters") &&
      !payload.at("cylinder_parameters").is_null()) {
    const json& cyl_payload = payload.at("cylinder_parameters");
    polysmith::core::CylinderFeatureParameters cylinder{};
    cylinder.radius = read_number(cyl_payload, "radius");
    cylinder.height = read_number(cyl_payload, "height");
    feature.cylinder_parameters = cylinder;
  }
