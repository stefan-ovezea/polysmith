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

polysmith::core::LoftFeatureParameters
loft_parameters_from_payload(const json& payload) {
  polysmith::core::LoftFeatureParameters params{};
  if (payload.contains("ruled") && payload.at("ruled").is_boolean()) {
    params.ruled = payload.at("ruled").get<bool>();
  }
  if (payload.contains("sections") && payload.at("sections").is_array()) {
    for (const auto& section_payload : payload.at("sections")) {
      polysmith::core::LoftSectionParameters section{};
      section.sketch_feature_id =
          read_string(section_payload, "sketch_feature_id");
      section.profile_id = read_string(section_payload, "profile_id");
      section.plane_id = read_string(section_payload, "plane_id");
      if (section_payload.contains("plane_frame") &&
          !section_payload.at("plane_frame").is_null()) {
        section.plane_frame =
            plane_frame_from_payload(section_payload.at("plane_frame"));
      }
      if (section_payload.contains("profile_points") &&
          section_payload.at("profile_points").is_array()) {
        for (const auto& point_payload : section_payload.at("profile_points")) {
          section.profile_points.push_back(polysmith::core::SketchProfilePoint{
              .x = point_payload.at("x").get<double>(),
              .y = point_payload.at("y").get<double>(),
          });
        }
      }
      params.sections.push_back(std::move(section));
    }
  }
  return params;
}

polysmith::core::RevolveFeatureParameters
revolve_parameters_from_payload(const json& payload) {
  polysmith::core::RevolveFeatureParameters params{};
  params.sketch_feature_id = read_string(payload, "sketch_feature_id");
  params.profile_id = read_string(payload, "profile_id");
  params.plane_id = read_string(payload, "plane_id");
  if (payload.contains("plane_frame") && !payload.at("plane_frame").is_null()) {
    params.plane_frame = plane_frame_from_payload(payload.at("plane_frame"));
  }
  params.profile_kind = read_string(payload, "profile_kind");
  if (payload.contains("profile_points") &&
      payload.at("profile_points").is_array()) {
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
  params.axis_sketch_feature_id =
      read_string(payload, "axis_sketch_feature_id");
  params.axis_entity_id = read_string(payload, "axis_entity_id");
  params.axis_start_x = read_number(payload, "axis_start_x");
  params.axis_start_y = read_number(payload, "axis_start_y");
  params.axis_start_z = read_number(payload, "axis_start_z");
  params.axis_end_x = read_number(payload, "axis_end_x");
  params.axis_end_y = read_number(payload, "axis_end_y");
  params.axis_end_z = read_number(payload, "axis_end_z");
  params.angle_degrees = read_number(payload, "angle_degrees");
  return params;
}

polysmith::core::SweepFeatureParameters
sweep_parameters_from_payload(const json& payload) {
  polysmith::core::SweepFeatureParameters params{};
  params.sketch_feature_id = read_string(payload, "sketch_feature_id");
  params.profile_id = read_string(payload, "profile_id");
  params.plane_id = read_string(payload, "plane_id");
  if (payload.contains("plane_frame") && !payload.at("plane_frame").is_null()) {
    params.plane_frame = plane_frame_from_payload(payload.at("plane_frame"));
  }
  params.profile_kind = read_string(payload, "profile_kind");
  if (payload.contains("profile_points") &&
      payload.at("profile_points").is_array()) {
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
  params.path_sketch_feature_id =
      read_string(payload, "path_sketch_feature_id");
  params.path_entity_id = read_string(payload, "path_entity_id");
  params.path_start_x = read_number(payload, "path_start_x");
  params.path_start_y = read_number(payload, "path_start_y");
  params.path_start_z = read_number(payload, "path_start_z");
  params.path_end_x = read_number(payload, "path_end_x");
  params.path_end_y = read_number(payload, "path_end_y");
  params.path_end_z = read_number(payload, "path_end_z");
  if (payload.contains("path_segments") &&
      payload.at("path_segments").is_array()) {
    for (const auto& segment_payload : payload.at("path_segments")) {
      polysmith::core::SweepFeatureParameters::PathSegment segment{};
      segment.entity_id = read_string(segment_payload, "entity_id");
      segment.kind = read_string(segment_payload, "kind");
      segment.start_x = read_number(segment_payload, "start_x");
      segment.start_y = read_number(segment_payload, "start_y");
      segment.start_z = read_number(segment_payload, "start_z");
      segment.end_x = read_number(segment_payload, "end_x");
      segment.end_y = read_number(segment_payload, "end_y");
      segment.end_z = read_number(segment_payload, "end_z");
      segment.center_x = read_number(segment_payload, "center_x");
      segment.center_y = read_number(segment_payload, "center_y");
      segment.center_z = read_number(segment_payload, "center_z");
      segment.mid_x = read_number(segment_payload, "mid_x");
      segment.mid_y = read_number(segment_payload, "mid_y");
      segment.mid_z = read_number(segment_payload, "mid_z");
      segment.radius = read_number(segment_payload, "radius");
      segment.ccw = segment_payload.contains("ccw") &&
                    segment_payload.at("ccw").is_boolean()
                        ? segment_payload.at("ccw").get<bool>()
                        : true;
      params.path_segments.push_back(segment);
    }
  }
  if (params.path_segments.empty()) {
    params.path_segments.push_back(
        polysmith::core::SweepFeatureParameters::PathSegment{
            .entity_id = params.path_entity_id,
            .kind = "line",
            .start_x = params.path_start_x,
            .start_y = params.path_start_y,
            .start_z = params.path_start_z,
            .end_x = params.path_end_x,
            .end_y = params.path_end_y,
            .end_z = params.path_end_z,
        });
  }
  return params;
}

polysmith::core::SketchFeatureParameters
sketch_parameters_from_payload(const json& payload) {
  polysmith::core::SketchFeatureParameters params{};
  params.plane_id = read_string(payload, "plane_id");
  if (payload.contains("plane_frame") && !payload.at("plane_frame").is_null()) {
    params.plane_frame = sketch_plane_frame_from_payload(payload.at("plane_frame"));
  }
  params.active_tool = payload.contains("active_tool") &&
                               payload.at("active_tool").is_string()
                           ? payload.at("active_tool").get<std::string>()
                           : std::string{};
  if (payload.contains("lines") && payload.at("lines").is_array()) {
    for (const auto& line_payload : payload.at("lines")) {
      polysmith::core::SketchLine line{};
      line.id = read_string(line_payload, "line_id");
      line.start_point_id = read_string(line_payload, "start_point_id");
      line.end_point_id = read_string(line_payload, "end_point_id");
      line.start_x = read_number(line_payload, "start_x");
      line.start_y = read_number(line_payload, "start_y");
      line.end_x = read_number(line_payload, "end_x");
      line.end_y = read_number(line_payload, "end_y");
      line.constraint = read_optional_string(line_payload, "constraint");
      // Older saves predate construction lines; default to solid.
      if (line_payload.contains("is_construction") &&
          line_payload.at("is_construction").is_boolean()) {
        line.is_construction = line_payload.at("is_construction").get<bool>();
      }
      params.lines.push_back(line);
    }
  }
  if (payload.contains("circles") && payload.at("circles").is_array()) {
    for (const auto& circle_payload : payload.at("circles")) {
      polysmith::core::SketchCircle circle{};
      circle.id = read_string(circle_payload, "circle_id");
      circle.center_x = read_number(circle_payload, "center_x");
      circle.center_y = read_number(circle_payload, "center_y");
      circle.radius = read_number(circle_payload, "radius");
      if (circle_payload.contains("is_construction") &&
          circle_payload.at("is_construction").is_boolean()) {
        circle.is_construction =
            circle_payload.at("is_construction").get<bool>();
      }
      params.circles.push_back(circle);
    }
  }
  // Older saves predate arcs; absence of the key just means no arcs
  // exist, mirroring how `circles` and `lines` degrade gracefully.
  if (payload.contains("arcs") && payload.at("arcs").is_array()) {
    for (const auto& arc_payload : payload.at("arcs")) {
      polysmith::core::SketchArc arc{};
      arc.id = read_string(arc_payload, "arc_id");
      arc.start_point_id = read_string(arc_payload, "start_point_id");
      arc.end_point_id = read_string(arc_payload, "end_point_id");
      arc.center_x = read_number(arc_payload, "center_x");
      arc.center_y = read_number(arc_payload, "center_y");
      arc.radius = read_number(arc_payload, "radius");
      arc.start_x = read_number(arc_payload, "start_x");
      arc.start_y = read_number(arc_payload, "start_y");
      arc.end_x = read_number(arc_payload, "end_x");
      arc.end_y = read_number(arc_payload, "end_y");
      arc.ccw = read_bool(arc_payload, "ccw");
      if (arc_payload.contains("is_construction") &&
          arc_payload.at("is_construction").is_boolean()) {
        arc.is_construction = arc_payload.at("is_construction").get<bool>();
      }
      params.arcs.push_back(arc);
    }
  }
  // Older saves predate fillets; absence of the key just means no
  // fillets exist (lines/arcs that originated as fillet outputs are
  // already round-tripped above as plain entities).
  if (payload.contains("fillets") && payload.at("fillets").is_array()) {
    for (const auto& fillet_payload : payload.at("fillets")) {
      polysmith::core::SketchFillet fillet{};
      fillet.id = read_string(fillet_payload, "fillet_id");
      fillet.corner_point_id = read_string(fillet_payload, "corner_point_id");
      fillet.corner_x = read_number(fillet_payload, "corner_x");
      fillet.corner_y = read_number(fillet_payload, "corner_y");
      fillet.line_a_id = read_string(fillet_payload, "line_a_id");
      fillet.line_b_id = read_string(fillet_payload, "line_b_id");
      fillet.trim_a_point_id = read_string(fillet_payload, "trim_a_point_id");
      fillet.trim_b_point_id = read_string(fillet_payload, "trim_b_point_id");
      fillet.arc_id = read_string(fillet_payload, "arc_id");
      fillet.radius = read_number(fillet_payload, "radius");
      params.fillets.push_back(fillet);
    }
  }
  // Older saves predate polygons; absence of the key just means none exist.
  if (payload.contains("polygons") && payload.at("polygons").is_array()) {
    for (const auto& polygon_payload : payload.at("polygons")) {
      polysmith::core::SketchPolygon polygon{};
      polygon.id = read_string(polygon_payload, "polygon_id");
      polygon.center_x = read_number(polygon_payload, "center_x");
      polygon.center_y = read_number(polygon_payload, "center_y");
      polygon.radius = read_number(polygon_payload, "radius");
      polygon.sides = static_cast<int>(read_number(polygon_payload, "sides"));
      polygon.mode = read_string(polygon_payload, "mode");
      polygon.start_x = read_number(polygon_payload, "start_x");
      polygon.start_y = read_number(polygon_payload, "start_y");
      polygon.end_x = read_number(polygon_payload, "end_x");
      polygon.end_y = read_number(polygon_payload, "end_y");
      if (polygon_payload.contains("is_construction") &&
          polygon_payload.at("is_construction").is_boolean()) {
        polygon.is_construction =
            polygon_payload.at("is_construction").get<bool>();
      }
      params.polygons.push_back(polygon);
    }
  }
  if (payload.contains("points") && payload.at("points").is_array()) {
    for (const auto& point_payload : payload.at("points")) {
      polysmith::core::SketchPoint point{};
      point.id = read_string(point_payload, "point_id");
      point.kind = read_string(point_payload, "kind");
      point.x = read_number(point_payload, "x");
      point.y = read_number(point_payload, "y");
      point.is_fixed = read_bool(point_payload, "is_fixed");
      params.points.push_back(point);
    }
  }
  if (payload.contains("dimensions") && payload.at("dimensions").is_array()) {
    for (const auto& dim_payload : payload.at("dimensions")) {
      polysmith::core::SketchDimension dimension{};
      dimension.id = read_string(dim_payload, "dimension_id");
      dimension.kind = read_string(dim_payload, "kind");
      dimension.entity_id = read_string(dim_payload, "entity_id");
      // Older saves predate angle dimensions; fall back to "" so the
      // unary dimensions round-trip without churn.
      if (dim_payload.contains("secondary_entity_id") &&
          dim_payload.at("secondary_entity_id").is_string()) {
        dimension.secondary_entity_id =
            dim_payload.at("secondary_entity_id").get<std::string>();
      }
      dimension.value = read_number(dim_payload, "value");
      // Expression field — absent in older saves, default to ""
      if (dim_payload.contains("expression") &&
          dim_payload.at("expression").is_string()) {
        dimension.expression =
            dim_payload.at("expression").get<std::string>();
      }
      // Driven field — absent in older saves, default to false (driving)
      if (dim_payload.contains("driven") && dim_payload.at("driven").is_boolean()) {
        dimension.driven = dim_payload.at("driven").get<bool>();
      }
      // is_auto field — absent in older saves, default to false.
      // A post-load fixup (below) sets it to true for dimensions whose
      // IDs match the auto-dimension naming pattern.
      if (dim_payload.contains("is_auto") && dim_payload.at("is_auto").is_boolean()) {
        dimension.is_auto = dim_payload.at("is_auto").get<bool>();
      }
      // Display_as field — absent in older saves, default to "" (diameter)
      if (dim_payload.contains("display_as") &&
          dim_payload.at("display_as").is_string()) {
        dimension.display_as =
            dim_payload.at("display_as").get<std::string>();
      }
      if (dim_payload.contains("label_x") &&
          dim_payload.at("label_x").is_number()) {
        dimension.label_x = dim_payload.at("label_x").get<double>();
      }
      if (dim_payload.contains("label_y") &&
          dim_payload.at("label_y").is_number()) {
        dimension.label_y = dim_payload.at("label_y").get<double>();
      }
      params.dimensions.push_back(dimension);
    }
    // Backward compat: dimensions from older saves lack the `is_auto`
    // field. Detect auto-dimensions by their ID prefix pattern so they
    // aren't mistakenly treated as manual driving constraints.
    for (auto& dim : params.dimensions) {
      if (!dim.is_auto) {
        bool matches_auto = false;
        if (dim.id.rfind("dim-line-", 0) == 0) matches_auto = true;
        else if (dim.id.rfind("dim-line-angle-", 0) == 0) matches_auto = true;
        else if (dim.id.rfind("dim-circle-", 0) == 0) matches_auto = true;
        else if (dim.id.rfind("dim-polygon-", 0) == 0) matches_auto = true;
        if (matches_auto) dim.is_auto = true;
      }
    }
  }
  if (payload.contains("line_relations") && payload.at("line_relations").is_array()) {
    for (const auto& relation_payload : payload.at("line_relations")) {
      polysmith::core::SketchLineRelation relation{};
      relation.id = read_string(relation_payload, "relation_id");
      relation.kind = read_string(relation_payload, "kind");
      relation.first_line_id = read_string(relation_payload, "first_line_id");
      relation.second_line_id = read_string(relation_payload, "second_line_id");
      params.line_relations.push_back(relation);
    }
  }
  // Older saves predate the general constraints array — silently default
  // to none.
  if (payload.contains("constraints") && payload.at("constraints").is_array()) {
    for (const auto& constraint_payload : payload.at("constraints")) {
      polysmith::core::SketchConstraint constraint{};
      constraint.constraint_id = read_string(constraint_payload, "constraint_id");
      constraint.kind = read_string(constraint_payload, "kind");
      if (constraint_payload.contains("target_ids") &&
          constraint_payload.at("target_ids").is_array()) {
        for (const auto& tid : constraint_payload.at("target_ids")) {
          constraint.target_ids.push_back(tid.get<std::string>());
        }
      }
      if (constraint_payload.contains("value") &&
          constraint_payload.at("value").is_number()) {
        constraint.value = constraint_payload.at("value").get<double>();
      }
      if (constraint_payload.contains("driven") &&
          constraint_payload.at("driven").is_boolean()) {
        constraint.driven = constraint_payload.at("driven").get<bool>();
      }
      params.constraints.push_back(constraint);
    }
  }
  // Older saves predate midpoint anchors — silently default to none.
  if (payload.contains("midpoint_anchors") &&
      payload.at("midpoint_anchors").is_array()) {
    for (const auto& anchor_payload : payload.at("midpoint_anchors")) {
      polysmith::core::SketchMidpointAnchor anchor{};
      anchor.id = read_string(anchor_payload, "anchor_id");
      anchor.point_id = read_string(anchor_payload, "point_id");
      anchor.line_id = read_string(anchor_payload, "line_id");
      params.midpoint_anchors.push_back(anchor);
    }
  }
  // Older saves predate the Project tool's standalone projected
  // points — absence of these keys just means no projections exist.
  if (payload.contains("projected_points") &&
      payload.at("projected_points").is_array()) {
    for (const auto& point_payload : payload.at("projected_points")) {
      polysmith::core::SketchProjectedPoint projected{};
      projected.id = read_string(point_payload, "point_id");
      projected.source_id = read_string(point_payload, "source_id");
      projected.x = read_number(point_payload, "x");
      projected.y = read_number(point_payload, "y");
      params.projected_points.push_back(projected);
    }
  }
  if (payload.contains("projected_sources") &&
      payload.at("projected_sources").is_array()) {
    for (const auto& source_payload : payload.at("projected_sources")) {
      if (!source_payload.is_string()) {
        continue;
      }
      params.projected_sources.push_back(source_payload.get<std::string>());
    }
  }
  // Live-link projection records. Saves predating the live-link work
  // (or a save that came from a build with the old face-only Project
  // tool) won't include this key — those documents lose the live
  // link until the user re-projects, which gracefully degrades to
  // the previous "frozen at projection time" behaviour.
  if (payload.contains("projections") &&
      payload.at("projections").is_array()) {
    for (const auto& projection_payload : payload.at("projections")) {
      polysmith::core::SketchProjection projection{};
      projection.id = read_string(projection_payload, "projection_id");
      projection.source_id = read_string(projection_payload, "source_id");
      projection.source_kind = read_string(projection_payload, "source_kind");
      if (projection_payload.contains("generated_line_ids") &&
          projection_payload.at("generated_line_ids").is_array()) {
        for (const auto& id : projection_payload.at("generated_line_ids")) {
          if (id.is_string()) {
            projection.generated_line_ids.push_back(id.get<std::string>());
          }
        }
      }
      if (projection_payload.contains("generated_circle_ids") &&
          projection_payload.at("generated_circle_ids").is_array()) {
        for (const auto& id : projection_payload.at("generated_circle_ids")) {
          if (id.is_string()) {
            projection.generated_circle_ids.push_back(id.get<std::string>());
          }
        }
      }
      if (projection_payload.contains("generated_arc_ids") &&
          projection_payload.at("generated_arc_ids").is_array()) {
        for (const auto& id : projection_payload.at("generated_arc_ids")) {
          if (id.is_string()) {
            projection.generated_arc_ids.push_back(id.get<std::string>());
          }
        }
      }
      if (projection_payload.contains("generated_point_id") &&
          projection_payload.at("generated_point_id").is_string()) {
        projection.generated_point_id =
            projection_payload.at("generated_point_id").get<std::string>();
      }
      if (projection_payload.contains("dependency_broken") &&
          projection_payload.at("dependency_broken").is_boolean()) {
        projection.dependency_broken =
            projection_payload.at("dependency_broken").get<bool>();
      }
      if (projection_payload.contains("dependency_warning") &&
          projection_payload.at("dependency_warning").is_string()) {
        projection.dependency_warning =
            projection_payload.at("dependency_warning").get<std::string>();
      }
      params.projections.push_back(std::move(projection));
    }
  }
  // Older saves predate point-line anchors — silently default to none.
  if (payload.contains("point_line_anchors") &&
      payload.at("point_line_anchors").is_array()) {
    for (const auto& anchor_payload : payload.at("point_line_anchors")) {
      polysmith::core::SketchPointLineAnchor anchor{};
      anchor.id = read_string(anchor_payload, "anchor_id");
      anchor.point_id = read_string(anchor_payload, "point_id");
      anchor.line_id = read_string(anchor_payload, "line_id");
      anchor.t = anchor_payload.contains("t") &&
                         anchor_payload.at("t").is_number()
                     ? anchor_payload.at("t").get<double>()
                     : 0.5;
      params.point_line_anchors.push_back(anchor);
    }
  }
  if (payload.contains("profiles") && payload.at("profiles").is_array()) {
    for (const auto& profile_payload : payload.at("profiles")) {
      polysmith::core::SketchProfileRegion profile{};
      profile.id = read_string(profile_payload, "profile_id");
      profile.kind = read_string(profile_payload, "kind");
      if (profile_payload.contains("point_ids") &&
          profile_payload.at("point_ids").is_array()) {
        for (const auto& id_value : profile_payload.at("point_ids")) {
          profile.point_ids.push_back(id_value.get<std::string>());
        }
      }
      if (profile_payload.contains("line_ids") &&
          profile_payload.at("line_ids").is_array()) {
        for (const auto& id_value : profile_payload.at("line_ids")) {
          profile.line_ids.push_back(id_value.get<std::string>());
        }
      }
      if (profile_payload.contains("points") &&
          profile_payload.at("points").is_array()) {
        for (const auto& pt_payload : profile_payload.at("points")) {
          profile.points.push_back(polysmith::core::SketchProfilePoint{
              .x = pt_payload.at("x").get<double>(),
              .y = pt_payload.at("y").get<double>(),
          });
        }
      }
      if (profile_payload.contains("inner_loops") &&
          profile_payload.at("inner_loops").is_array()) {
        for (const auto& loop_payload : profile_payload.at("inner_loops")) {
          std::vector<polysmith::core::SketchProfilePoint> loop;
          for (const auto& pt_payload : loop_payload) {
            loop.push_back(polysmith::core::SketchProfilePoint{
                .x = pt_payload.at("x").get<double>(),
                .y = pt_payload.at("y").get<double>(),
            });
          }
          profile.inner_loops.push_back(std::move(loop));
        }
      }
      profile.source_circle_id =
          read_optional_string(profile_payload, "source_circle_id");
      profile.center_x = read_number(profile_payload, "center_x");
      profile.center_y = read_number(profile_payload, "center_y");
      profile.radius = read_number(profile_payload, "radius");
      params.profiles.push_back(profile);
    }
  }
  // Pending mirror state. Older saves (and most live document
  // states outside an in-progress mirror) won't have this key —
  // that's fine, the field stays as `nullopt`.
  if (payload.contains("pending_mirror") &&
      payload.at("pending_mirror").is_object()) {
    polysmith::core::SketchFeatureParameters::PendingMirror pending{};
    const auto& pm = payload.at("pending_mirror");
    pending.axis_line_id = read_optional_string(pm, "axis_line_id");
    if (pm.contains("object_ids") && pm.at("object_ids").is_array()) {
      for (const auto& id_value : pm.at("object_ids")) {
        if (id_value.is_string()) {
          pending.object_ids.push_back(id_value.get<std::string>());
        }
      }
    }
    // Generated geometry is regenerated on every parameter
    // change, so it doesn't need to round-trip through saves —
    // but we still rebuild it from the payload for the live
    // viewport/document round-trip during a session.
    if (pm.contains("generated_lines") &&
        pm.at("generated_lines").is_array()) {
      for (const auto& line_payload : pm.at("generated_lines")) {
        polysmith::core::SketchLine line{};
        line.id = read_string(line_payload, "line_id");
        line.start_point_id = read_string(line_payload, "start_point_id");
        line.end_point_id = read_string(line_payload, "end_point_id");
        line.start_x = read_number(line_payload, "start_x");
        line.start_y = read_number(line_payload, "start_y");
        line.end_x = read_number(line_payload, "end_x");
        line.end_y = read_number(line_payload, "end_y");
        if (line_payload.contains("is_construction") &&
            line_payload.at("is_construction").is_boolean()) {
          line.is_construction = line_payload.at("is_construction").get<bool>();
        }
        pending.generated_lines.push_back(line);
      }
    }
    if (pm.contains("generated_circles") &&
        pm.at("generated_circles").is_array()) {
      for (const auto& circle_payload : pm.at("generated_circles")) {
        polysmith::core::SketchCircle circle{};
        circle.id = read_string(circle_payload, "circle_id");
        circle.center_x = read_number(circle_payload, "center_x");
        circle.center_y = read_number(circle_payload, "center_y");
        circle.radius = read_number(circle_payload, "radius");
        if (circle_payload.contains("is_construction") &&
            circle_payload.at("is_construction").is_boolean()) {
          circle.is_construction =
              circle_payload.at("is_construction").get<bool>();
        }
        pending.generated_circles.push_back(circle);
      }
    }
    params.pending_mirror = pending;
  }
  return params;
}
