json to_payload(const polysmith::core::ViewportReferencePlane& plane) {
  return {
      {"reference_id", plane.id},
      {"label", plane.label},
      {"orientation", plane.orientation},
      {"center",
       {
           {"x", plane.center_x},
           {"y", plane.center_y},
           {"z", plane.center_z},
       }},
      {"size",
       {
           {"width", plane.width},
           {"height", plane.height},
       }},
      {"is_selected", plane.is_selected},
      {"is_active_sketch_plane", plane.is_active_sketch_plane},
      // Construction planes ship a real frame; origin planes leave
      // it null and the renderer falls back to its hardcoded
      // orientation rotation.
      {"plane_frame",
       plane.plane_frame.has_value()
           ? json{
                 {"origin",
                  {
                      {"x", plane.plane_frame->origin_x},
                      {"y", plane.plane_frame->origin_y},
                      {"z", plane.plane_frame->origin_z},
                  }},
                 {"x_axis",
                  {
                      {"x", plane.plane_frame->x_axis_x},
                      {"y", plane.plane_frame->x_axis_y},
                      {"z", plane.plane_frame->x_axis_z},
                  }},
                 {"y_axis",
                  {
                      {"x", plane.plane_frame->y_axis_x},
                      {"y", plane.plane_frame->y_axis_y},
                      {"z", plane.plane_frame->y_axis_z},
                  }},
                 {"normal",
                  {
                      {"x", plane.plane_frame->normal_x},
                      {"y", plane.plane_frame->normal_y},
                      {"z", plane.plane_frame->normal_z},
                  }},
             }
           : json(nullptr)},
  };
}

json to_payload(const polysmith::core::ViewportReferenceAxis& axis) {
  return {
      {"reference_id", axis.id},
      {"label", axis.label},
      {"axis", axis.axis},
      {"start",
       {
           {"x", axis.start_x},
           {"y", axis.start_y},
           {"z", axis.start_z},
       }},
      {"end",
       {
           {"x", axis.end_x},
           {"y", axis.end_y},
           {"z", axis.end_z},
       }},
  };
}

json to_payload(const polysmith::core::ViewportReferencePoint& point) {
  return {
      {"reference_id", point.id},
      {"label", point.label},
      {"position", {{"x", point.x}, {"y", point.y}, {"z", point.z}}},
      {"is_selected", point.is_selected},
  };
}

json to_payload(const polysmith::core::ViewportHelixPrimitive& helix) {
  return {
      {"helix_id", helix.id},
      {"label", helix.label},
      {"points", helix.points},
      {"is_selected", helix.is_selected},
  };
}
