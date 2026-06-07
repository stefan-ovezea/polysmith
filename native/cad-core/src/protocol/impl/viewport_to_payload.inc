json to_payload(const polysmith::core::ViewportBoxPrimitive& primitive) {
  return {
      {"primitive_id", primitive.id},
      {"label", primitive.label},
      {"width", primitive.width},
      {"height", primitive.height},
      {"depth", primitive.depth},
      {"x_offset", primitive.x_offset},
      {"center",
       {
           {"x", primitive.center_x},
           {"y", primitive.center_y},
           {"z", primitive.center_z},
      }},
      {"is_selected", primitive.is_selected},
      {"appearance_color",
       primitive.appearance_color.has_value()
           ? json(primitive.appearance_color.value())
           : json(nullptr)},
  };
}

json to_payload(const polysmith::core::ViewportCylinderPrimitive& primitive) {
  return {
      {"primitive_id", primitive.id},
      {"label", primitive.label},
      {"radius", primitive.radius},
      {"height", primitive.height},
      {"x_offset", primitive.x_offset},
      {"center",
       {
           {"x", primitive.center_x},
           {"y", primitive.center_y},
           {"z", primitive.center_z},
      }},
      {"is_selected", primitive.is_selected},
      {"appearance_color",
       primitive.appearance_color.has_value()
           ? json(primitive.appearance_color.value())
           : json(nullptr)},
  };
}

json to_payload(const polysmith::core::ViewportPolygonExtrudePrimitive& primitive) {
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
      {"primitive_id", primitive.id},
      {"label", primitive.label},
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
      {"profile_points", profile_points},
      {"depth", primitive.depth},
      {"is_selected", primitive.is_selected},
      {"appearance_color",
       primitive.appearance_color.has_value()
           ? json(primitive.appearance_color.value())
           : json(nullptr)},
  };
}

json to_payload(const polysmith::core::ViewportSolidFace& face) {
  return {
      {"face_id", face.face_id},
      {"owner_id", face.owner_id},
      {"owner_kind", face.owner_kind},
      {"label", face.label},
      {"sketchability", face.sketchability},
      {"center",
       {
           {"x", face.center_x},
           {"y", face.center_y},
           {"z", face.center_z},
       }},
      {"normal",
       {
           {"x", face.normal_x},
           {"y", face.normal_y},
           {"z", face.normal_z},
       }},
      {"plane_frame",
       {
           {"origin",
            {
                {"x", face.plane_frame.origin_x},
                {"y", face.plane_frame.origin_y},
                {"z", face.plane_frame.origin_z},
            }},
           {"x_axis",
            {
                {"x", face.plane_frame.x_axis_x},
                {"y", face.plane_frame.x_axis_y},
                {"z", face.plane_frame.x_axis_z},
            }},
           {"y_axis",
            {
                {"x", face.plane_frame.y_axis_x},
                {"y", face.plane_frame.y_axis_y},
                {"z", face.plane_frame.y_axis_z},
            }},
           {"normal",
            {
                {"x", face.plane_frame.normal_x},
                {"y", face.plane_frame.normal_y},
                {"z", face.plane_frame.normal_z},
            }},
       }},
      {"size",
       {
           {"width", face.width},
           {"height", face.height},
           {"radius", face.radius},
       }},
      {"triangle_positions", face.triangle_positions},
      {"triangle_indices", face.triangle_indices},
      {"is_selected", face.is_selected},
      {"appearance_color",
       face.appearance_color.has_value()
           ? json(face.appearance_color.value())
           : json(nullptr)},
  };
}

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

json to_payload(const polysmith::core::ViewportToolpathPrimitive& primitive) {
  json points = json::array();
  for (const auto& p : primitive.points) {
    points.push_back({
        {"x", p.x},
        {"y", p.y},
        {"z", p.z},
        {"is_rapid", p.is_rapid},
    });
  }
  return {
      {"id", primitive.id},
      {"label", primitive.label},
      {"points", points},
  };
}

json to_payload(const polysmith::core::ViewportState& viewport) {
  json boxes = json::array();
  for (const auto& box : viewport.boxes) {
    boxes.push_back(to_payload(box));
  }
  json solid_faces = json::array();
  for (const auto& face : viewport.solid_faces) {
    solid_faces.push_back(to_payload(face));
  }

  json cylinders = json::array();
  for (const auto& cylinder : viewport.cylinders) {
    cylinders.push_back(to_payload(cylinder));
  }

  json polygon_extrudes = json::array();
  for (const auto& polygon_extrude : viewport.polygon_extrudes) {
    polygon_extrudes.push_back(to_payload(polygon_extrude));
  }

  json reference_planes = json::array();
  for (const auto& plane : viewport.reference_planes) {
    reference_planes.push_back(to_payload(plane));
  }

  json reference_axes = json::array();
  for (const auto& axis : viewport.reference_axes) {
    reference_axes.push_back(to_payload(axis));
  }

  json reference_points = json::array();
  for (const auto& point : viewport.reference_points) {
    reference_points.push_back(to_payload(point));
  }

  json helices = json::array();
  for (const auto& helix : viewport.helices) {
    helices.push_back(to_payload(helix));
  }

  json sketch_lines = json::array();
  for (const auto& line : viewport.sketch_lines) {
    sketch_lines.push_back(to_payload(line));
  }

  json sketch_circles = json::array();
  for (const auto& circle : viewport.sketch_circles) {
    sketch_circles.push_back(to_payload(circle));
  }

  json sketch_polygons = json::array();
  for (const auto& polygon : viewport.sketch_polygons) {
    sketch_polygons.push_back(to_payload(polygon));
  }

  json sketch_arcs = json::array();
  for (const auto& arc : viewport.sketch_arcs) {
    sketch_arcs.push_back(to_payload(arc));
  }

  json sketch_points = json::array();
  for (const auto& point : viewport.sketch_points) {
    sketch_points.push_back(to_payload(point));
  }

  json sketch_dimensions = json::array();
  for (const auto& dimension : viewport.sketch_dimensions) {
    sketch_dimensions.push_back(to_payload(dimension));
  }

  json sketch_constraints = json::array();
  for (const auto& constraint : viewport.sketch_constraints) {
    sketch_constraints.push_back(to_payload(constraint));
  }

  json sketch_profiles = json::array();
  for (const auto& profile : viewport.sketch_profiles) {
    sketch_profiles.push_back(to_payload(profile));
  }

  json meshes = json::array();
  for (const auto& mesh : viewport.meshes) {
    meshes.push_back({
        {"primitive_id", mesh.id},
        {"positions", mesh.positions},
        {"normals", mesh.normals},
        {"indices", mesh.indices},
        {"is_selected", mesh.is_selected},
        {"appearance_color",
         mesh.appearance_color.has_value()
             ? json(mesh.appearance_color.value())
             : json(nullptr)},
    });
  }

  return {
      {"has_active_document", viewport.has_active_document},
      {"boxes", boxes},
      {"cylinders", cylinders},
      {"polygon_extrudes", polygon_extrudes},
      {"solid_faces", solid_faces},
      {"reference_planes", reference_planes},
      {"reference_axes", reference_axes},
      {"reference_points", reference_points},
      {"helices", helices},
      {"sketch_lines", sketch_lines},
      {"sketch_circles", sketch_circles},
      {"sketch_polygons", sketch_polygons},
      {"sketch_arcs", sketch_arcs},
      {"sketch_points", sketch_points},
      {"sketch_dimensions", sketch_dimensions},
      {"sketch_constraints", sketch_constraints},
      {"sketch_profiles", sketch_profiles},
      {"meshes", meshes},
      {"toolpaths", [&viewport]() {
         json toolpaths_json = json::array();
         for (const auto& tp : viewport.toolpaths) {
           toolpaths_json.push_back(to_payload(tp));
         }
         return toolpaths_json;
       }()},
      {"cut_previews", [&viewport]() {
         json previews = json::array();
         for (const auto& preview : viewport.cut_previews) {
           previews.push_back({
               {"id", preview.id},
               {"positions", preview.positions},
               {"normals", preview.normals},
               {"indices", preview.indices},
           });
         }
         return previews;
       }()},
      {"bodies", [&viewport]() {
         json bodies_json = json::array();
         for (const auto& body : viewport.bodies) {
           bodies_json.push_back({
               {"id", body.id},
               {"label", body.label},
               {"center",
                {{"x", body.center_x},
                 {"y", body.center_y},
                 {"z", body.center_z}}},
               {"size",
                {{"x", body.width},
                 {"y", body.height},
                 {"z", body.depth}}},
               {"local_frame",
                {
                    {"x_axis",
                     {{"x", body.local_frame.x_axis_x},
                      {"y", body.local_frame.x_axis_y},
                      {"z", body.local_frame.x_axis_z}}},
                    {"y_axis",
                     {{"x", body.local_frame.y_axis_x},
                      {"y", body.local_frame.y_axis_y},
                      {"z", body.local_frame.y_axis_z}}},
                    {"z_axis",
                     {{"x", body.local_frame.z_axis_x},
                      {"y", body.local_frame.z_axis_y},
                      {"z", body.local_frame.z_axis_z}}},
                }},
           });
         }
         return bodies_json;
       }()},
      {"edges", [&viewport]() {
         json edges_json = json::array();
         for (const auto& edge : viewport.edges) {
           edges_json.push_back({
               {"id", edge.id},
               {"owner_body_id", edge.owner_body_id},
               {"kind", edge.kind},
               {"points", edge.points},
               {"length", edge.length},
               {"is_selected", edge.is_selected},
           });
         }
         return edges_json;
       }()},
      {"vertices", [&viewport]() {
         json vertices_json = json::array();
         for (const auto& vertex : viewport.vertices) {
           vertices_json.push_back({
               {"id", vertex.id},
               {"owner_body_id", vertex.owner_body_id},
               {"position",
                {
                    {"x", vertex.x},
                    {"y", vertex.y},
                    {"z", vertex.z},
                }},
               {"is_selected", vertex.is_selected},
           });
         }
         return vertices_json;
       }()},
      {"scene_width", viewport.scene_width},
      {"scene_height", viewport.scene_height},
      {"scene_depth", viewport.scene_depth},
      {"scene_bounds",
       {
           {"center",
            {
                {"x", viewport.scene_bounds.center_x},
                {"y", viewport.scene_bounds.center_y},
                {"z", viewport.scene_bounds.center_z},
            }},
           {"size",
            {
                {"x", viewport.scene_bounds.width},
                {"y", viewport.scene_bounds.height},
                {"z", viewport.scene_bounds.depth},
            }},
            {"max_dimension", viewport.scene_bounds.max_dimension},
        }},
       {"selection_filter",
        [&viewport]() {
          const auto& sf = viewport.selection_filter;
          return json{
              {"select_curves", sf.select_curves},
              {"select_points", sf.select_points},
              {"select_construction", sf.select_construction},
              {"select_constraints", sf.select_constraints},
              {"snap_endpoint", sf.snap_endpoint},
              {"snap_midpoint", sf.snap_midpoint},
              {"snap_center", sf.snap_center},
              {"snap_intersection", sf.snap_intersection},
              {"snap_nearest", sf.snap_nearest},
              {"snap_quadrant", sf.snap_quadrant},
              {"snap_perpendicular", sf.snap_perpendicular},
              {"snap_parallel", sf.snap_parallel},
              {"snap_tangent", sf.snap_tangent},
              {"snap_grid", sf.snap_grid},
              {"snap_grid_line", sf.snap_grid_line},
              {"snap_polar", sf.snap_polar},
              {"polar_angle_degrees", sf.polar_angle_degrees},
              {"magnetic_pull", sf.magnetic_pull},
              {"tolerance_px", sf.tolerance_px},
          };
        }()},
       {"snap_candidates",
        [&viewport]() {
          json arr = json::array();
          for (const auto& c : viewport.snap_candidates) {
            arr.push_back({
                {"kind", c.kind},
                {"entity_id", c.entity_id},
                {"point_id", c.point_id},
                {"local_x", c.local_x},
                {"local_y", c.local_y},
                {"label", c.label},
            });
          }
          return arr;
        }()},
       {"dof_statuses",
        [&viewport]() {
          json statuses = json::array();
          for (const auto& e : viewport.dof_statuses) {
            std::string status_str = "under";
            if (e.status == polysmith::core::DofStatus::FullyConstrained)
              status_str = "full";
            else if (e.status == polysmith::core::DofStatus::OverConstrained)
              status_str = "over";
            statuses.push_back({
                {"entity_id", e.entity_id},
                {"entity_kind", e.entity_kind},
                {"total_dof", e.total_dof},
                {"consumed_dof", e.consumed_dof},
                {"status", status_str},
            });
          }
          return statuses;
        }()},
  };
}
