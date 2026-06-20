json to_payload(const polysmith::core::ViewportSketchLinePrimitive& primitive) {
  return {
      {"line_id", primitive.line_id},
      {"start_point_id", primitive.start_point_id},
      {"end_point_id", primitive.end_point_id},
      {"plane_id", primitive.plane_id},
      {"start",
       {
           {"x", primitive.start_x},
           {"y", primitive.start_y},
           {"z", primitive.start_z},
       }},
      {"end",
       {
           {"x", primitive.end_x},
           {"y", primitive.end_y},
           {"z", primitive.end_z},
       }},
      {"is_selected", primitive.is_selected},
      {"constraint",
       primitive.constraint.has_value()
           ? json(primitive.constraint.value())
           : json(nullptr)},
      {"is_construction", primitive.is_construction},
      {"is_preview", primitive.is_preview},
  };
}

json to_payload(const polysmith::core::ViewportSketchPolygonPrimitive& primitive) {
  return {
      {"polygon_id", primitive.polygon_id},
      {"plane_id", primitive.plane_id},
      {"plane_frame", primitive.plane_frame.has_value() ? viewport_sketch_plane_frame_to_payload(primitive.plane_frame.value()) : json(nullptr)},
      {"corner_x", primitive.corner_x},
      {"corner_y", primitive.corner_y},
      {"corner_z", primitive.corner_z},
      {"sides", primitive.sides},
      {"mode", primitive.mode},
      {"center", {{"x", primitive.center_x}, {"y", primitive.center_y}, {"z", primitive.center_z}}},
      {"radius", primitive.radius},
      {"is_selected", primitive.is_selected},
      {"is_construction", primitive.is_construction},
      {"is_preview", primitive.is_preview},
  };
}

json to_payload(const polysmith::core::ViewportSketchCirclePrimitive& primitive) {
  return {
      {"circle_id", primitive.circle_id},
      {"plane_id", primitive.plane_id},
      {"plane_frame",
       primitive.plane_frame.has_value()
           ? viewport_sketch_plane_frame_to_payload(primitive.plane_frame.value())
           : json(nullptr)},
      {"center",
       {
           {"x", primitive.center_x},
           {"y", primitive.center_y},
           {"z", primitive.center_z},
       }},
      {"radius", primitive.radius},
      {"is_selected", primitive.is_selected},
      {"is_construction", primitive.is_construction},
      {"is_preview", primitive.is_preview},
  };
}

json to_payload(const polysmith::core::ViewportSketchArcPrimitive& primitive) {
  return {
      {"arc_id", primitive.arc_id},
      {"start_point_id", primitive.start_point_id},
      {"end_point_id", primitive.end_point_id},
      {"plane_id", primitive.plane_id},
      {"plane_frame",
       primitive.plane_frame.has_value()
           ? viewport_sketch_plane_frame_to_payload(primitive.plane_frame.value())
           : json(nullptr)},
      {"center",
       {
           {"x", primitive.center_x},
           {"y", primitive.center_y},
           {"z", primitive.center_z},
       }},
      {"radius", primitive.radius},
      {"start",
       {
           {"x", primitive.start_x},
           {"y", primitive.start_y},
           {"z", primitive.start_z},
       }},
      {"end",
       {
           {"x", primitive.end_x},
           {"y", primitive.end_y},
           {"z", primitive.end_z},
       }},
      {"ccw", primitive.ccw},
      {"is_selected", primitive.is_selected},
      {"is_construction", primitive.is_construction},
      {"is_preview", primitive.is_preview},
  };
}

json to_payload(const polysmith::core::ViewportSketchPointPrimitive& primitive) {
  return {
      {"point_id", primitive.point_id},
      {"plane_id", primitive.plane_id},
      {"kind", primitive.kind},
      {"position",
       {
           {"x", primitive.position_x},
           {"y", primitive.position_y},
           {"z", primitive.position_z},
       }},
      {"is_fixed", primitive.is_fixed},
      {"is_selected", primitive.is_selected},
  };
}

json to_payload(const polysmith::core::ViewportSketchDimensionPrimitive& primitive) {
  return {
      {"dimension_id", primitive.dimension_id},
      {"plane_id", primitive.plane_id},
      {"kind", primitive.kind},
      {"entity_id", primitive.entity_id},
      {"label", primitive.label},
      {"is_selected", primitive.is_selected},
      {"anchor_start",
       {
           {"x", primitive.anchor_start_x},
           {"y", primitive.anchor_start_y},
           {"z", primitive.anchor_start_z},
       }},
      {"anchor_end",
       {
           {"x", primitive.anchor_end_x},
           {"y", primitive.anchor_end_y},
           {"z", primitive.anchor_end_z},
       }},
      {"dimension_start",
       {
           {"x", primitive.dimension_start_x},
           {"y", primitive.dimension_start_y},
           {"z", primitive.dimension_start_z},
       }},
      {"dimension_end",
       {
           {"x", primitive.dimension_end_x},
           {"y", primitive.dimension_end_y},
           {"z", primitive.dimension_end_z},
       }},
      {"label_position",
       {
           {"x", primitive.label_x},
           {"y", primitive.label_y},
           {"z", primitive.label_z},
       }},
      {"arc_center",
       {
           {"x", primitive.arc_center_x},
           {"y", primitive.arc_center_y},
           {"z", primitive.arc_center_z},
       }},
      {"arc_radius", primitive.arc_radius},
      {"arc_start_angle", primitive.arc_start_angle},
      {"arc_end_angle", primitive.arc_end_angle},
      {"arc_ccw", primitive.arc_ccw},
      {"ref_line_start",
       {
           {"x", primitive.ref_line_start_x},
           {"y", primitive.ref_line_start_y},
           {"z", primitive.ref_line_start_z},
       }},
      {"ref_line_end",
        {
            {"x", primitive.ref_line_end_x},
            {"y", primitive.ref_line_end_y},
            {"z", primitive.ref_line_end_z},
        }},
      {"driven", primitive.driven},
      {"display_as", primitive.display_as},
  };
}

json to_payload(const polysmith::core::ViewportSketchConstraintPrimitive& primitive) {
  return {
      {"constraint_id", primitive.constraint_id},
      {"plane_id", primitive.plane_id},
      {"kind", primitive.kind},
      {"entity_id", primitive.entity_id},
      {"related_entity_id",
       primitive.related_entity_id.has_value()
           ? json(primitive.related_entity_id.value())
           : json(nullptr)},
      {"label", primitive.label},
      {"is_selected", primitive.is_selected},
      {"position",
       {
           {"x", primitive.position_x},
           {"y", primitive.position_y},
           {"z", primitive.position_z},
       }},
  };
}

json to_payload(const polysmith::core::ViewportSketchProfilePrimitive& primitive) {
  json profile_points = json::array();
  for (const auto& point : primitive.profile_points) {
    profile_points.push_back({
        {"x", point.x},
        {"y", point.y},
    });
  }
  json inner_loops = json::array();
  for (const auto& loop : primitive.inner_loops) {
    json loop_payload = json::array();
    for (const auto& point : loop) {
      loop_payload.push_back({
          {"x", point.x},
          {"y", point.y},
      });
    }
    inner_loops.push_back(loop_payload);
  }

  return {
      {"profile_id", primitive.profile_id},
      {"plane_id", primitive.plane_id},
      {"plane_frame",
       primitive.plane_frame.has_value()
           ? json{
                 {"origin",
                  {
                      {"x", primitive.plane_frame->origin_x},
                      {"y", primitive.plane_frame->origin_y},
                      {"z", primitive.plane_frame->origin_z},
                  }},
                 {"x_axis",
                  {
                      {"x", primitive.plane_frame->x_axis_x},
                      {"y", primitive.plane_frame->x_axis_y},
                      {"z", primitive.plane_frame->x_axis_z},
                  }},
                 {"y_axis",
                  {
                      {"x", primitive.plane_frame->y_axis_x},
                      {"y", primitive.plane_frame->y_axis_y},
                      {"z", primitive.plane_frame->y_axis_z},
                  }},
                 {"normal",
                  {
                      {"x", primitive.plane_frame->normal_x},
                      {"y", primitive.plane_frame->normal_y},
                      {"z", primitive.plane_frame->normal_z},
                  }},
             }
           : json(nullptr)},
      {"profile_kind", primitive.profile_kind},
      {"profile_points", profile_points},
      {"inner_loops", inner_loops},
      {"start_x", primitive.start_x},
      {"start_y", primitive.start_y},
      {"width", primitive.width},
      {"height", primitive.height},
      {"radius", primitive.radius},
      {"is_selected", primitive.is_selected},
  };
}
