#include "app/sketch_create_command_handlers.h"

#include "app/command_context.h"
#include "core/document/document.h"

namespace polysmith {

bool handle_sketch_create_command(
    const polysmith::protocol::CommandMessage& command,
    polysmith::core::DocumentManager& manager) {
  if (command.type == "add_sketch_point") {
    const auto before_document = manager.get_document();
    const auto document = manager.add_sketch_point(
        read_dimension(command.payload, "x"),
        read_dimension(command.payload, "y"));
    // A point only changes the vertex table (and the `points` record), both
    // of which the partial event cannot carry — the reply guard in
    // emit_sketch_entities_changed falls back to the full state for it.
    if (emit_sketch_entities_changed(command.id, before_document, document)) {
      return true;
    }
    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return true;
  }

  if (command.type == "add_sketch_line") {
    bool is_construction = false;
    if (command.payload.contains("is_construction") &&
        command.payload.at("is_construction").is_boolean()) {
      is_construction = command.payload.at("is_construction").get<bool>();
    }
    const auto before_document = manager.get_document();
    const auto document = manager.add_sketch_line(
        read_dimension(command.payload, "start_x"),
        read_dimension(command.payload, "start_y"),
        read_dimension(command.payload, "end_x"),
        read_dimension(command.payload, "end_y"), is_construction,
        read_optional_string(command.payload, "line_type").value_or(""));
    if (emit_sketch_entities_changed(command.id, before_document, document)) {
      return true;
    }
    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return true;
  }

  if (command.type == "set_sketch_midpoint_anchor") {
    std::string host_line_id;
    if (command.payload.contains("host_line_id") &&
        command.payload.at("host_line_id").is_string()) {
      host_line_id = command.payload.at("host_line_id").get<std::string>();
    }
    const auto document = manager.set_sketch_midpoint_anchor(
        read_string(command.payload, "vertex_id"), host_line_id);
    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return true;
  }

  if (command.type == "set_sketch_vertex_line_anchor") {
    std::string host_line_id;
    if (command.payload.contains("host_line_id") &&
        command.payload.at("host_line_id").is_string()) {
      host_line_id = command.payload.at("host_line_id").get<std::string>();
    }
    double t = 0.5;
    if (command.payload.contains("t") &&
        command.payload.at("t").is_number()) {
      t = command.payload.at("t").get<double>();
    }
    const auto document = manager.set_sketch_vertex_line_anchor(
        read_string(command.payload, "vertex_id"), host_line_id, t);
    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return true;
  }

  if (command.type == "set_sketch_line_construction") {
    bool is_construction = false;
    if (command.payload.contains("is_construction") &&
        command.payload.at("is_construction").is_boolean()) {
      is_construction = command.payload.at("is_construction").get<bool>();
    }
    const auto document = manager.set_sketch_line_construction(
        read_string(command.payload, "line_id"), is_construction);
    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return true;
  }

  if (command.type == "add_sketch_rectangle") {
    bool is_construction = false;
    if (command.payload.contains("is_construction") &&
        command.payload.at("is_construction").is_boolean()) {
      is_construction = command.payload.at("is_construction").get<bool>();
    }
    const auto before_document = manager.get_document();
    const auto document = manager.add_sketch_rectangle(
        read_dimension(command.payload, "start_x"),
        read_dimension(command.payload, "start_y"),
        read_dimension(command.payload, "end_x"),
        read_dimension(command.payload, "end_y"), is_construction);
    if (emit_sketch_entities_changed(command.id, before_document, document)) {
      return true;
    }
    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return true;
  }

  if (command.type == "add_sketch_rounded_rectangle") {
    bool is_construction = false;
    if (command.payload.contains("is_construction") &&
        command.payload.at("is_construction").is_boolean()) {
      is_construction = command.payload.at("is_construction").get<bool>();
    }
    const auto before_document = manager.get_document();
    const auto document = manager.add_sketch_rounded_rectangle(
        read_dimension(command.payload, "start_x"),
        read_dimension(command.payload, "start_y"),
        read_dimension(command.payload, "end_x"),
        read_dimension(command.payload, "end_y"),
        read_dimension(command.payload, "radius"), is_construction);
    if (emit_sketch_entities_changed(command.id, before_document, document)) {
      return true;
    }
    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return true;
  }

  if (command.type == "add_sketch_arc") {
    bool is_construction = false;
    if (command.payload.contains("is_construction") &&
        command.payload.at("is_construction").is_boolean()) {
      is_construction = command.payload.at("is_construction").get<bool>();
    }
    const auto before_document = manager.get_document();
    const auto document = manager.add_sketch_arc(
        read_dimension(command.payload, "start_x"),
        read_dimension(command.payload, "start_y"),
        read_dimension(command.payload, "end_x"),
        read_dimension(command.payload, "end_y"),
        read_dimension(command.payload, "anchor_x"),
        read_dimension(command.payload, "anchor_y"),
        read_string(command.payload, "mode"), is_construction);
    if (emit_sketch_entities_changed(command.id, before_document, document)) {
      return true;
    }
    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return true;
  }

  if (command.type == "add_sketch_circle") {
    bool is_construction = false;
    if (command.payload.contains("is_construction") &&
        command.payload.at("is_construction").is_boolean()) {
      is_construction = command.payload.at("is_construction").get<bool>();
    }
    const auto optional_string = [&](const char* key) {
      if (command.payload.contains(key) &&
          command.payload.at(key).is_string()) {
        return command.payload.at(key).get<std::string>();
      }
      return std::string{};
    };
    const auto optional_dimension = [&](const char* key) {
      if (command.payload.contains(key) &&
          command.payload.at(key).is_number()) {
        return command.payload.at(key).get<double>();
      }
      return 0.0;
    };
    const auto before_document = manager.get_document();
    const auto document = manager.add_sketch_circle(
        read_dimension(command.payload, "center_x"),
        read_dimension(command.payload, "center_y"),
        read_dimension(command.payload, "radius"), is_construction,
        optional_string("mode"), optional_dimension("p1_x"),
        optional_dimension("p1_y"), optional_dimension("p2_x"),
        optional_dimension("p2_y"), optional_dimension("p3_x"),
        optional_dimension("p3_y"), optional_string("line_a_id"),
        optional_string("line_b_id"), optional_string("line_c_id"),
        optional_dimension("hint_x"), optional_dimension("hint_y"));
    if (emit_sketch_entities_changed(command.id, before_document, document)) {
      return true;
    }
    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return true;
  }

  if (command.type == "add_sketch_polygon") {
    bool is_construction = false;
    if (command.payload.contains("is_construction") &&
        command.payload.at("is_construction").is_boolean()) {
      is_construction = command.payload.at("is_construction").get<bool>();
    }
    const auto before_document = manager.get_document();
    const auto document = manager.add_sketch_polygon(
        static_cast<int>(read_dimension(command.payload, "sides")),
        read_string(command.payload, "mode"),
        read_dimension(command.payload, "start_x"),
        read_dimension(command.payload, "start_y"),
        read_dimension(command.payload, "end_x"),
        read_dimension(command.payload, "end_y"), is_construction);
    if (emit_sketch_entities_changed(command.id, before_document, document)) {
      return true;
    }
    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return true;
  }

  if (command.type == "add_sketch_ellipse") {
    bool is_construction = false;
    if (command.payload.contains("is_construction") &&
        command.payload.at("is_construction").is_boolean()) {
      is_construction = command.payload.at("is_construction").get<bool>();
    }
    const auto before_document = manager.get_document();
    const auto document = manager.add_sketch_ellipse(
        read_dimension(command.payload, "center_x"),
        read_dimension(command.payload, "center_y"),
        read_dimension(command.payload, "axis_a_x"),
        read_dimension(command.payload, "axis_a_y"),
        read_dimension(command.payload, "axis_b_x"),
        read_dimension(command.payload, "axis_b_y"), is_construction);
    if (emit_sketch_entities_changed(command.id, before_document, document)) {
      return true;
    }
    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return true;
  }

  if (command.type == "add_sketch_spline") {
    bool is_construction = false;
    if (command.payload.contains("is_construction") &&
        command.payload.at("is_construction").is_boolean()) {
      is_construction = command.payload.at("is_construction").get<bool>();
    }
    std::vector<std::pair<double, double>> points;
    if (command.payload.contains("points") &&
        command.payload.at("points").is_array()) {
      for (const auto& point : command.payload.at("points")) {
        points.push_back(
            {read_dimension(point, "x"), read_dimension(point, "y")});
      }
    }
    const auto before_document = manager.get_document();
    const auto document = manager.add_sketch_spline(points, is_construction);
    if (emit_sketch_entities_changed(command.id, before_document, document)) {
      return true;
    }
    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return true;
  }

  if (command.type == "add_sketch_slot") {
    bool is_construction = false;
    if (command.payload.contains("is_construction") &&
        command.payload.at("is_construction").is_boolean()) {
      is_construction = command.payload.at("is_construction").get<bool>();
    }
    const auto before_document = manager.get_document();
    const auto document = manager.add_sketch_slot(
        read_dimension(command.payload, "center_x"),
        read_dimension(command.payload, "center_y"),
        read_dimension(command.payload, "length"),
        read_dimension(command.payload, "radius"),
        read_dimension(command.payload, "rotation"), is_construction);
    if (emit_sketch_entities_changed(command.id, before_document, document)) {
      return true;
    }
    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return true;
  }

  if (command.type == "update_sketch_slot") {
    const auto document = manager.update_sketch_slot(
        read_string(command.payload, "slot_id"),
        read_dimension(command.payload, "center_x"),
        read_dimension(command.payload, "center_y"),
        read_dimension(command.payload, "length"),
        read_dimension(command.payload, "radius"),
        read_dimension(command.payload, "rotation"));
    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return true;
  }

  // ── Fillet / chamfer ────────────────────────────────────────────
  if (command.type == "add_sketch_fillet") {
    const auto document = manager.add_sketch_fillet(
        read_string(command.payload, "corner_vertex_id"),
        read_string(command.payload, "line_a_id"),
        read_string(command.payload, "line_b_id"),
        read_dimension(command.payload, "radius"),
        read_optional_string(command.payload, "arc_a_id").value_or(""),
        read_optional_string(command.payload, "arc_b_id").value_or(""));
    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return true;
  }

  if (command.type == "update_sketch_fillet_radius") {
    const auto document = manager.update_sketch_fillet_radius(
        read_string(command.payload, "fillet_id"),
        read_dimension(command.payload, "radius"));
    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return true;
  }

  if (command.type == "delete_sketch_fillet") {
    const auto before_document = manager.get_document();
    const auto document = manager.delete_sketch_fillet(
        read_string(command.payload, "fillet_id"));
    if (emit_sketch_entities_changed(command.id, before_document, document)) {
      return true;
    }
    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return true;
  }

  if (command.type == "merge_coincident_sketch_points") {
    const auto document = manager.merge_coincident_sketch_points(
        read_string(command.payload, "feature_id"));
    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return true;
  }

  if (command.type == "add_sketch_chamfer") {
    // Optional creation mode (parity: equal / two_distance / distance_angle).
    // Absent means two_distance, the legacy behaviour.
    const std::string chamfer_mode =
        read_optional_string(command.payload, "mode").value_or("");
    const double angle_degrees =
        read_optional_dimension(command.payload, "angle_degrees", 0.0);
    double distance_b =
        read_optional_dimension(command.payload, "distance_b", -1.0);
    if (chamfer_mode == "equal" || chamfer_mode == "distance_angle") {
      // One driving value: the core derives the second distance.
      distance_b = 0.0;
    }
    const auto document = manager.add_sketch_chamfer(
        read_string(command.payload, "corner_vertex_id"),
        read_string(command.payload, "line_a_id"),
        read_string(command.payload, "line_b_id"),
        read_dimension(command.payload, "distance_a"), distance_b,
        chamfer_mode, angle_degrees);
    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return true;
  }

  if (command.type == "update_sketch_chamfer") {
    const std::string chamfer_mode =
        read_optional_string(command.payload, "mode").value_or("");
    const double angle_degrees =
        read_optional_dimension(command.payload, "angle_degrees", 0.0);
    const auto document = manager.update_sketch_chamfer(
        read_string(command.payload, "chamfer_id"),
        read_dimension(command.payload, "distance_a"),
        read_optional_dimension(command.payload, "distance_b", 0.0),
        chamfer_mode, angle_degrees);
    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return true;
  }

  if (command.type == "delete_sketch_chamfer") {
    const auto before_document = manager.get_document();
    const auto document = manager.delete_sketch_chamfer(
        read_string(command.payload, "chamfer_id"));
    if (emit_sketch_entities_changed(command.id, before_document, document)) {
      return true;
    }
    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return true;
  }

  // ── Dimension / delete / projection ─────────────────────────────
  if (command.type == "delete_sketch_dimension") {
    const auto before_document = manager.get_document();
    const auto document = manager.delete_sketch_dimension(
        read_string(command.payload, "dimension_id"));
    if (emit_sketch_entities_changed(command.id, before_document, document)) {
      return true;
    }
    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return true;
  }

  if (command.type == "update_sketch_dimension_display") {
    const auto document = manager.update_sketch_dimension_display(
        read_string(command.payload, "dimension_id"),
        read_string(command.payload, "display_as"));
    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return true;
  }

  if (command.type == "toggle_sketch_dimension_driven") {
    const auto document = manager.toggle_sketch_dimension_driven(
        read_string(command.payload, "dimension_id"));
    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return true;
  }

  if (command.type == "delete_sketch_selection") {
    std::vector<std::string> entity_ids;
    std::vector<std::string> point_ids;
    std::vector<std::string> profile_ids;
    if (command.payload.contains("entity_ids") &&
        command.payload.at("entity_ids").is_array()) {
      for (const auto& id : command.payload.at("entity_ids")) {
        if (id.is_string()) {
          entity_ids.push_back(id.get<std::string>());
        }
      }
    }
    if (command.payload.contains("vertex_ids") &&
        command.payload.at("vertex_ids").is_array()) {
      for (const auto& id : command.payload.at("vertex_ids")) {
        if (id.is_string()) {
          point_ids.push_back(id.get<std::string>());
        }
      }
    }
    if (command.payload.contains("profile_ids") &&
        command.payload.at("profile_ids").is_array()) {
      for (const auto& id : command.payload.at("profile_ids")) {
        if (id.is_string()) {
          profile_ids.push_back(id.get<std::string>());
        }
      }
    }
    const auto document =
        manager.delete_sketch_selection(entity_ids, point_ids, profile_ids);
    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return true;
  }

  if (command.type == "project_face_into_sketch") {
    const auto document = manager.project_face_into_sketch(
        read_string(command.payload, "face_id"));
    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return true;
  }

  if (command.type == "project_profile_into_sketch") {
    const auto document = manager.project_profile_into_sketch(
        read_string(command.payload, "profile_id"));
    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return true;
  }

  if (command.type == "project_edge_into_sketch") {
    const auto document = manager.project_edge_into_sketch(
        read_string(command.payload, "edge_id"));
    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return true;
  }

  if (command.type == "project_vertex_into_sketch") {
    const auto document = manager.project_vertex_into_sketch(
        read_string(command.payload, "vertex_id"));
    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return true;
  }

  if (command.type == "project_body_into_sketch") {
    const auto document = manager.project_body_into_sketch(
        read_string(command.payload, "body_id"),
        read_string(command.payload, "mode"));
    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return true;
  }

  if (command.type == "remove_sketch_projections") {
    const auto document = manager.remove_sketch_projections(
        read_string(command.payload, "feature_id"),
        read_bool(command.payload, "keep_geometry"));
    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return true;
  }

  // ── Selection session / clear ───────────────────────────────────
  if (command.type == "select_sketch_vertex") {
    bool additive = false;
    if (command.payload.contains("additive") &&
        command.payload.at("additive").is_boolean()) {
      additive = command.payload.at("additive").get<bool>();
    }
    const auto document = manager.select_sketch_vertex(
        read_string(command.payload, "vertex_id"), additive);
    polysmith::protocol::write_message(
        polysmith::protocol::make_selection_changed_event(
            command.id, polysmith::protocol::to_selection_payload(document)));
    return true;
  }

  if (command.type == "set_sketch_vertex_fixed") {
    const auto document = manager.set_sketch_vertex_fixed(
        read_string(command.payload, "vertex_id"),
        command.payload.at("is_fixed").get<bool>());
    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return true;
  }

  if (command.type == "select_sketch_entity") {
    bool additive = false;
    if (command.payload.contains("additive") &&
        command.payload.at("additive").is_boolean()) {
      additive = command.payload.at("additive").get<bool>();
    }
    const auto document = manager.select_sketch_entity(
        read_string(command.payload, "entity_id"), additive);
    polysmith::protocol::write_message(
        polysmith::protocol::make_selection_changed_event(
            command.id, polysmith::protocol::to_selection_payload(document)));
    return true;
  }

  if (command.type == "select_sketch_entities") {
    bool additive = false;
    if (command.payload.contains("additive") &&
        command.payload.at("additive").is_boolean()) {
      additive = command.payload.at("additive").get<bool>();
    }
    std::vector<std::string> entity_ids;
    if (command.payload.contains("entity_ids") &&
        command.payload.at("entity_ids").is_array()) {
      for (const auto& id : command.payload.at("entity_ids")) {
        if (id.is_string()) {
          entity_ids.push_back(id.get<std::string>());
        }
      }
    }
    const auto document =
        manager.select_sketch_entities(entity_ids, additive);
    polysmith::protocol::write_message(
        polysmith::protocol::make_selection_changed_event(
            command.id, polysmith::protocol::to_selection_payload(document)));
    return true;
  }

  if (command.type == "select_sketch_rect") {
    bool additive = false;
    if (command.payload.contains("additive") &&
        command.payload.at("additive").is_boolean()) {
      additive = command.payload.at("additive").get<bool>();
    }
    bool window_mode = true;
    if (command.payload.contains("window_mode") &&
        command.payload.at("window_mode").is_boolean()) {
      window_mode = command.payload.at("window_mode").get<bool>();
    }
    const auto document = manager.select_sketch_rect(
        read_dimension(command.payload, "x1"),
        read_dimension(command.payload, "y1"),
        read_dimension(command.payload, "x2"),
        read_dimension(command.payload, "y2"), window_mode, additive);
    polysmith::protocol::write_message(
        polysmith::protocol::make_selection_changed_event(
            command.id, polysmith::protocol::to_selection_payload(document)));
    return true;
  }

  if (command.type == "select_sketch_profiles_rect") {
    bool additive = false;
    if (command.payload.contains("additive") &&
        command.payload.at("additive").is_boolean()) {
      additive = command.payload.at("additive").get<bool>();
    }
    bool window_mode = true;
    if (command.payload.contains("window_mode") &&
        command.payload.at("window_mode").is_boolean()) {
      window_mode = command.payload.at("window_mode").get<bool>();
    }
    std::string feature_id;
    if (command.payload.contains("feature_id") &&
        command.payload.at("feature_id").is_string()) {
      feature_id = command.payload.at("feature_id").get<std::string>();
    }
    const auto document = manager.select_sketch_profiles_rect(
        feature_id, read_dimension(command.payload, "x1"),
        read_dimension(command.payload, "y1"),
        read_dimension(command.payload, "x2"),
        read_dimension(command.payload, "y2"), window_mode, additive);
    polysmith::protocol::write_message(
        polysmith::protocol::make_selection_changed_event(
            command.id, polysmith::protocol::to_selection_payload(document)));
    return true;
  }

  if (command.type == "select_sketch_dimension") {
    const auto document = manager.select_sketch_dimension(
        read_string(command.payload, "dimension_id"));
    polysmith::protocol::write_message(
        polysmith::protocol::make_selection_changed_event(
            command.id, polysmith::protocol::to_selection_payload(document)));
    return true;
  }

  if (command.type == "finish_sketch") {
    const auto document = manager.finish_sketch();
    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return true;
  }

  if (command.type == "reenter_sketch") {
    const auto document = manager.reenter_sketch(
        read_string(command.payload, "feature_id"));
    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(
                            document, /*include_opaque=*/true)));
    return true;
  }

  if (command.type == "clear_selection") {
    const auto document = manager.clear_selection();
    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return true;
  }

  return false;
}

}  // namespace polysmith
