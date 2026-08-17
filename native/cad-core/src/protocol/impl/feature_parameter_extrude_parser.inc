polysmith::core::ExtrudeFeatureParameters
parse_extrude_parameters_from_payload(const json& payload) {
  polysmith::core::ExtrudeFeatureParameters params{};
  params.sketch_feature_id = read_string(payload, "sketch_feature_id");
  params.profile_id = read_string(payload, "profile_id");
  if (payload.contains("profile_ids") && payload.at("profile_ids").is_array()) {
    for (const auto& id_value : payload.at("profile_ids")) {
      params.profile_ids.push_back(id_value.get<std::string>());
    }
  } else if (!params.profile_id.empty()) {
    params.profile_ids.push_back(params.profile_id);
  }
  if (payload.contains("open_entity_ids") &&
      payload.at("open_entity_ids").is_array()) {
    for (const auto& id_value : payload.at("open_entity_ids")) {
      params.open_entity_ids.push_back(id_value.get<std::string>());
    }
  }
  params.plane_id = read_string(payload, "plane_id");
  if (payload.contains("plane_frame") && !payload.at("plane_frame").is_null()) {
    params.plane_frame = plane_frame_from_payload(payload.at("plane_frame"));
  }
  if (payload.contains("sketch_edge_ids") && payload.at("sketch_edge_ids").is_array()) {
    for (const auto& id_value : payload.at("sketch_edge_ids")) {
      params.sketch_edge_ids.push_back(id_value.get<std::string>());
    }
  }
  if (payload.contains("sketch_lines") && payload.at("sketch_lines").is_array()) {
    for (const auto& line_payload : payload.at("sketch_lines")) {
      polysmith::core::SketchLine line{};
      line.id = read_string(line_payload, "line_id");
      line.start_x = read_number(line_payload, "start_x");
      line.start_y = read_number(line_payload, "start_y");
      line.end_x = read_number(line_payload, "end_x");
      line.end_y = read_number(line_payload, "end_y");
      line.start_vertex_id = read_string(line_payload, "start_vertex_id");
      line.end_vertex_id = read_string(line_payload, "end_vertex_id");
      line.is_construction = read_optional_bool_value(line_payload, "is_construction", false);
      line.constraint = read_optional_string_value(line_payload, "constraint", "");
      params.sketch_lines.push_back(std::move(line));
    }
  }
  if (payload.contains("sketch_arcs") && payload.at("sketch_arcs").is_array()) {
    for (const auto& arc_payload : payload.at("sketch_arcs")) {
      polysmith::core::SketchArc arc{};
      arc.id = read_string(arc_payload, "arc_id");
      arc.center_x = read_number(arc_payload, "center_x");
      arc.center_y = read_number(arc_payload, "center_y");
      arc.start_x = read_number(arc_payload, "start_x");
      arc.start_y = read_number(arc_payload, "start_y");
      arc.end_x = read_number(arc_payload, "end_x");
      arc.end_y = read_number(arc_payload, "end_y");
      arc.radius = read_number(arc_payload, "radius");
      arc.ccw = read_optional_bool_value(arc_payload, "ccw", true);
      arc.is_construction = read_optional_bool_value(arc_payload, "is_construction", false);
      params.sketch_arcs.push_back(std::move(arc));
    }
  }
  if (payload.contains("sketch_circles") && payload.at("sketch_circles").is_array()) {
    for (const auto& circ_payload : payload.at("sketch_circles")) {
      polysmith::core::SketchCircle circ{};
      circ.id = read_string(circ_payload, "circle_id");
      circ.center_x = read_number(circ_payload, "center_x");
      circ.center_y = read_number(circ_payload, "center_y");
      circ.radius = read_number(circ_payload, "radius");
      circ.is_construction = read_optional_bool_value(circ_payload, "is_construction", false);
      params.sketch_circles.push_back(std::move(circ));
    }
  }
  params.profile_kind = read_string(payload, "profile_kind");
  params.start_x = read_number(payload, "start_x");
  params.start_y = read_number(payload, "start_y");
  params.width = read_number(payload, "width");
  params.height = read_number(payload, "height");
  params.radius = read_number(payload, "radius");
  if (payload.contains("profile_points") && payload.at("profile_points").is_array()) {
    for (const auto& point_payload : payload.at("profile_points")) {
      params.profile_points.push_back(polysmith::core::SketchProfilePoint{
          .x = point_payload.at("x").get<double>(),
          .y = point_payload.at("y").get<double>(),
      });
    }
  }
  if (payload.contains("inner_loops") && payload.at("inner_loops").is_array()) {
    for (const auto& loop_payload : payload.at("inner_loops")) {
      std::vector<polysmith::core::SketchProfilePoint> loop;
      for (const auto& point_payload : loop_payload) {
        loop.push_back(polysmith::core::SketchProfilePoint{
            .x = point_payload.at("x").get<double>(),
            .y = point_payload.at("y").get<double>(),
        });
      }
      params.inner_loops.push_back(std::move(loop));
    }
  }
  if (payload.contains("additional_profile_points") &&
      payload.at("additional_profile_points").is_array()) {
    for (const auto& profile_payload : payload.at("additional_profile_points")) {
      std::vector<polysmith::core::SketchProfilePoint> profile;
      for (const auto& point_payload : profile_payload) {
        profile.push_back(polysmith::core::SketchProfilePoint{
            .x = point_payload.at("x").get<double>(),
            .y = point_payload.at("y").get<double>(),
        });
      }
      params.additional_profile_points.push_back(std::move(profile));
    }
  }
  if (payload.contains("additional_inner_loops") &&
      payload.at("additional_inner_loops").is_array()) {
    for (const auto& profile_payload : payload.at("additional_inner_loops")) {
      std::vector<std::vector<polysmith::core::SketchProfilePoint>> loops;
      for (const auto& loop_payload : profile_payload) {
        std::vector<polysmith::core::SketchProfilePoint> loop;
        for (const auto& point_payload : loop_payload) {
          loop.push_back(polysmith::core::SketchProfilePoint{
              .x = point_payload.at("x").get<double>(),
              .y = point_payload.at("y").get<double>(),
          });
        }
        loops.push_back(std::move(loop));
      }
      params.additional_inner_loops.push_back(std::move(loops));
    }
  }
  if (payload.contains("additional_sketch_edge_ids") &&
      payload.at("additional_sketch_edge_ids").is_array()) {
    for (const auto& profile_payload :
         payload.at("additional_sketch_edge_ids")) {
      std::vector<std::string> ids;
      for (const auto& id_value : profile_payload) {
        ids.push_back(id_value.get<std::string>());
      }
      params.additional_sketch_edge_ids.push_back(std::move(ids));
    }
  }
  if (payload.contains("additional_arc_descriptors") &&
      payload.at("additional_arc_descriptors").is_array()) {
    for (const auto& profile_payload :
         payload.at("additional_arc_descriptors")) {
      std::vector<polysmith::core::SketchArcDescriptor> descs;
      for (const auto& desc_payload : profile_payload) {
        descs.push_back(polysmith::core::SketchArcDescriptor{
            .circle_id = desc_payload.at("circle_id").get<std::string>(),
            .center_x = desc_payload.at("center_x").get<double>(),
            .center_y = desc_payload.at("center_y").get<double>(),
            .radius = desc_payload.at("radius").get<double>(),
            .start_angle_rad =
                desc_payload.at("start_angle_rad").get<double>(),
            .end_angle_rad =
                desc_payload.at("end_angle_rad").get<double>(),
            .ccw = desc_payload.at("ccw").get<bool>(),
        });
      }
      params.additional_arc_descriptors.push_back(std::move(descs));
    }
  }

  auto parse_boundary_edges =
      [](const json& edges_payload)
      -> std::vector<polysmith::core::ProfileBoundaryEdge> {
    std::vector<polysmith::core::ProfileBoundaryEdge> edges;
    for (const auto& edge_payload : edges_payload) {
      edges.push_back(polysmith::core::ProfileBoundaryEdge{
          .entity_id = edge_payload.at("entity_id").get<std::string>(),
          .entity_kind = edge_payload.at("entity_kind").get<std::string>(),
          .param_start = edge_payload.at("param_start").get<double>(),
          .param_end = edge_payload.at("param_end").get<double>(),
          .start_x = edge_payload.at("start_x").get<double>(),
          .start_y = edge_payload.at("start_y").get<double>(),
          .end_x = edge_payload.at("end_x").get<double>(),
          .end_y = edge_payload.at("end_y").get<double>(),
          .center_x = edge_payload.at("center_x").get<double>(),
          .center_y = edge_payload.at("center_y").get<double>(),
          .radius = edge_payload.at("radius").get<double>(),
          .ccw = edge_payload.at("ccw").get<bool>(),
      });
    }
    return edges;
  };

  if (payload.contains("additional_boundary_edges") &&
      payload.at("additional_boundary_edges").is_array()) {
    for (const auto& group_payload : payload.at("additional_boundary_edges")) {
      params.additional_boundary_edges.push_back(
          parse_boundary_edges(group_payload));
    }
  }
  if (payload.contains("boundary_edges") &&
      payload.at("boundary_edges").is_array()) {
    params.boundary_edges = parse_boundary_edges(payload.at("boundary_edges"));
  }
  params.depth = read_number(payload, "depth");
  params.extent_mode =
      read_optional_string_value(payload, "extent_mode", params.extent_mode);
  params.side1.distance = std::abs(params.depth);
  if (payload.contains("side1") && payload.at("side1").is_object()) {
    params.side1 = extrude_side_from_payload(payload.at("side1"), params.side1);
  }
  if (payload.contains("side2") && payload.at("side2").is_object()) {
    params.side2 =
        extrude_side_from_payload(payload.at("side2"), params.side1);
  }
  if (payload.contains("thin") && payload.at("thin").is_object()) {
    const auto& thin = payload.at("thin");
    if (thin.contains("enabled") && thin.at("enabled").is_boolean()) {
      params.thin.enabled = thin.at("enabled").get<bool>();
    }
    params.thin.thickness =
        read_optional_number(thin, "thickness", params.thin.thickness);
    params.thin.placement =
        read_optional_string_value(thin, "placement", params.thin.placement);
  }
  if (payload.contains("mode") && payload.at("mode").is_string()) {
    params.mode = payload.at("mode").get<std::string>();
  }
  params.operation =
      read_optional_string_value(payload, "operation", params.mode);
  params.intersect_result =
      read_optional_string_value(payload,
                                 "intersect_result",
                                 params.intersect_result);
  if (payload.contains("target_body_id") &&
      payload.at("target_body_id").is_string()) {
    params.target_body_id = payload.at("target_body_id").get<std::string>();
  }
  return params;
}
