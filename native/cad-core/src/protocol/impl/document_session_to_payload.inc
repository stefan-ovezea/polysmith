json to_payload(const polysmith::core::DocumentState& document,
                bool include_opaque) {
  json feature_history = json::array();
  for (const auto& feature : document.feature_history) {
    feature_history.push_back(to_payload(feature, include_opaque));
  }

  return {
      {"document_id", document.id},
      {"name", document.name},
      {"units", document.units},
      {"revision", document.revision},
      {"selected_feature_id",
       document.selected_feature_id.has_value()
           ? json(document.selected_feature_id.value())
           : json(nullptr)},
      {"selected_reference_id",
       document.selected_reference_id.has_value()
           ? json(document.selected_reference_id.value())
           : json(nullptr)},
      {"selected_face_id",
       document.selected_face_id.has_value()
           ? json(document.selected_face_id.value())
           : json(nullptr)},
      {"selected_edge_ids", document.selected_edge_ids},
      {"selected_vertex_ids", document.selected_vertex_ids},
      {"active_sketch_plane_id",
       document.active_sketch_plane_id.has_value()
           ? json(document.active_sketch_plane_id.value())
           : json(nullptr)},
      {"active_sketch_face_id",
       document.active_sketch_face_id.has_value()
           ? json(document.active_sketch_face_id.value())
           : json(nullptr)},
      {"active_sketch_feature_id",
       document.active_sketch_feature_id.has_value()
           ? json(document.active_sketch_feature_id.value())
           : json(nullptr)},
      {"active_sketch_tool",
       document.active_sketch_tool.has_value()
           ? json(document.active_sketch_tool.value())
           : json(nullptr)},
      {"selected_sketch_vertex_id",
       document.selected_sketch_vertex_id.has_value()
           ? json(document.selected_sketch_vertex_id.value())
           : json(nullptr)},
      {"selected_sketch_entity_id",
       document.selected_sketch_entity_id.has_value()
           ? json(document.selected_sketch_entity_id.value())
           : json(nullptr)},
      {"selected_sketch_vertex_ids", document.selected_sketch_vertex_ids},
      {"selected_sketch_entity_ids", document.selected_sketch_entity_ids},
      {"selected_sketch_dimension_id",
       document.selected_sketch_dimension_id.has_value()
           ? json(document.selected_sketch_dimension_id.value())
           : json(nullptr)},
      {"selected_sketch_profile_id",
       document.selected_sketch_profile_id.has_value()
           ? json(document.selected_sketch_profile_id.value())
           : json(nullptr)},
      {"selected_sketch_profile_ids",
       [&document]() {
         json ids = json::array();
         for (const auto& id : document.selected_sketch_profile_ids) {
           ids.push_back(id);
         }
         return ids;
       }()},
      {"timeline_cursor",
       document.timeline_cursor.has_value()
           ? json(document.timeline_cursor.value())
           : json(nullptr)},
      {"feature_history", feature_history},
      {"parameters",
       [&document]() {
         json params = json::array();
         for (const auto& p : document.parameters) {
            params.push_back({
                {"name", p.name},
                {"expression", p.expression},
                {"resolved_value", p.resolved_value},
                {"kind", p.kind},
                {"has_error", p.has_error},
                {"error_message", p.error_message},
            });
         }
        return params;
        }()},
       {"appearance",
        [&document]() {
          json body_colors = json::array();
          for (const auto& entry : document.appearance.body_colors) {
            body_colors.push_back({
                {"body_id", entry.body_id},
                {"color", entry.color},
            });
          }
          json face_colors = json::array();
          for (const auto& entry : document.appearance.face_colors) {
            face_colors.push_back({
                {"face_id", entry.face_id},
                {"owner_body_id", entry.owner_body_id},
                {"signature", entry.signature},
                {"color", entry.color},
            });
          }
          return json{
              {"body_colors", body_colors},
              {"face_colors", face_colors},
          };
        }()},
       {"selection_filter",
        [&document]() {
          const auto& sf = document.selection_filter;
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
       {"cam", to_payload(document.cam)},
   };
}

json to_payload(const polysmith::core::SessionState& session) {
  json payload = {
      {"document_count", session.document_count},
      {"has_active_document", session.active_document_id.has_value()},
      {"can_undo", session.can_undo},
      {"can_redo", session.can_redo},
  };

  if (session.active_document_id.has_value()) {
    payload["active_document_id"] = session.active_document_id.value();
  } else {
    payload["active_document_id"] = nullptr;
  }

  return payload;
}
