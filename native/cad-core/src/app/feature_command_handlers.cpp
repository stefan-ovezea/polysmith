#include "app/feature_command_handlers.h"

#include "app/command_context.h"
#include "core/document/document.h"

namespace polysmith {

bool handle_feature_command(const polysmith::protocol::CommandMessage& command,
                            polysmith::core::DocumentManager& manager) {
  if (command.type == "add_box_feature") {
    const auto document = manager.add_box_feature(
        polysmith::core::BoxFeatureParameters{
            .width = read_dimension(command.payload, "width"),
            .height = read_dimension(command.payload, "height"),
            .depth = read_dimension(command.payload, "depth"),
        });
    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return true;
  }

  if (command.type == "add_cylinder_feature") {
    const auto document = manager.add_cylinder_feature(
        polysmith::core::CylinderFeatureParameters{
            .radius = read_dimension(command.payload, "radius"),
            .height = read_dimension(command.payload, "height"),
        });
    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return true;
  }

  if (command.type == "update_box_feature") {
    const std::string feature_id = read_string(command.payload, "feature_id");
    const auto document = manager.update_box_feature(
        feature_id,
        polysmith::core::BoxFeatureParameters{
            .width = read_dimension(command.payload, "width"),
            .height = read_dimension(command.payload, "height"),
            .depth = read_dimension(command.payload, "depth"),
        });
    if (emit_feature_changed(command.id, feature_id, document)) {
      return true;
    }
    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return true;
  }

  if (command.type == "update_cylinder_feature") {
    const std::string feature_id = read_string(command.payload, "feature_id");
    const auto document = manager.update_cylinder_feature(
        feature_id,
        polysmith::core::CylinderFeatureParameters{
            .radius = read_dimension(command.payload, "radius"),
            .height = read_dimension(command.payload, "height"),
        });
    if (emit_feature_changed(command.id, feature_id, document)) {
      return true;
    }
    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return true;
  }

  if (command.type == "create_plugin_feature") {
    const auto document = manager.create_plugin_feature(
        read_plugin_feature_parameters(command.payload));
    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return true;
  }

  if (command.type == "update_plugin_feature") {
    const auto document = manager.update_plugin_feature(
        read_string(command.payload, "feature_id"),
        read_plugin_feature_parameters(command.payload));
    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return true;
  }

  if (command.type == "confirm_plugin_feature") {
    const auto document = manager.confirm_plugin_feature(
        read_string(command.payload, "feature_id"));
    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return true;
  }

  if (command.type == "update_extrude_depth") {
    const std::string feature_id = read_string(command.payload, "feature_id");
    const auto document = manager.update_extrude_depth(
        feature_id, read_dimension(command.payload, "depth"));
    if (emit_feature_changed(command.id, feature_id, document)) {
      return true;
    }
    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return true;
  }

  if (command.type == "rename_feature") {
    const auto document = manager.rename_feature(
        read_string(command.payload, "feature_id"),
        read_string(command.payload, "name"));
    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return true;
  }

  if (command.type == "set_feature_suppressed") {
    const auto document = manager.set_feature_suppressed(
        read_string(command.payload, "feature_id"),
        command.payload.at("suppressed").get<bool>());
    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return true;
  }

  if (command.type == "delete_feature") {
    const auto document = manager.delete_feature(
        read_string(command.payload, "feature_id"));
    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return true;
  }

  if (command.type == "undo") {
    if (!manager.get_session_state().can_undo) {
      polysmith::protocol::write_message(polysmith::protocol::make_error_event(
          command.id, "EMPTY_UNDO_STACK", "Nothing to undo"));
      return true;
    }
    const auto document = manager.undo();
    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return true;
  }

  if (command.type == "redo") {
    if (!manager.get_session_state().can_redo) {
      polysmith::protocol::write_message(polysmith::protocol::make_error_event(
          command.id, "EMPTY_REDO_STACK", "Nothing to redo"));
      return true;
    }
    const auto document = manager.redo();
    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return true;
  }

  if (command.type == "undo_many") {
    const auto document = manager.undo_many(read_int(command.payload, "count"));
    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return true;
  }

  if (command.type == "undo_begin_group") {
    manager.begin_undo_group(read_string(command.payload, "name"));
    const auto document = manager.get_document();
    if (document.has_value()) {
      polysmith::protocol::write_message(
          polysmith::protocol::make_document_state_event(
              command.id, polysmith::protocol::to_payload(document.value())));
    }
    return true;
  }

  if (command.type == "undo_end_group") {
    manager.end_undo_group();
    const auto document = manager.get_document();
    if (document.has_value()) {
      polysmith::protocol::write_message(
          polysmith::protocol::make_document_state_event(
              command.id, polysmith::protocol::to_payload(document.value())));
    }
    return true;
  }

  if (command.type == "undo_abort_group") {
    manager.abort_undo_group();
    const auto document = manager.get_document();
    if (document.has_value()) {
      polysmith::protocol::write_message(
          polysmith::protocol::make_document_state_event(
              command.id, polysmith::protocol::to_payload(document.value())));
    }
    return true;
  }

  if (command.type == "undo_abort_all_groups") {
    manager.abort_all_undo_groups();
    const auto document = manager.get_document();
    if (document.has_value()) {
      polysmith::protocol::write_message(
          polysmith::protocol::make_document_state_event(
              command.id, polysmith::protocol::to_payload(document.value())));
    }
    return true;
  }

  if (command.type == "set_undo_limit") {
    manager.set_undo_limit(read_int(command.payload, "limit"));
    const auto document = manager.get_document();
    if (document.has_value()) {
      polysmith::protocol::write_message(
          polysmith::protocol::make_document_state_event(
              command.id, polysmith::protocol::to_payload(document.value())));
    }
    return true;
  }

  if (command.type == "set_timeline_cursor") {
    const auto document = manager.set_timeline_cursor(
        read_int(command.payload, "included_action_count"));
    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return true;
  }

  if (command.type == "select_feature") {
    const auto document = manager.select_feature(
        read_string(command.payload, "feature_id"));
    polysmith::protocol::write_message(
        polysmith::protocol::make_selection_changed_event(
            command.id, polysmith::protocol::to_selection_payload(document)));
    return true;
  }

  if (command.type == "select_reference") {
    const auto document = manager.select_reference(
        read_string(command.payload, "reference_id"));
    polysmith::protocol::write_message(
        polysmith::protocol::make_selection_changed_event(
            command.id, polysmith::protocol::to_selection_payload(document)));
    return true;
  }

  if (command.type == "select_face") {
    const auto document =
        manager.select_face(read_string(command.payload, "face_id"));
    polysmith::protocol::write_message(
        polysmith::protocol::make_selection_changed_event(
            command.id, polysmith::protocol::to_selection_payload(document)));
    return true;
  }

  if (command.type == "select_edge") {
    bool additive = false;
    if (command.payload.contains("additive") &&
        command.payload.at("additive").is_boolean()) {
      additive = command.payload.at("additive").get<bool>();
    }
    const auto document = manager.select_edge(
        read_string(command.payload, "edge_id"), additive);
    polysmith::protocol::write_message(
        polysmith::protocol::make_selection_changed_event(
            command.id, polysmith::protocol::to_selection_payload(document)));
    return true;
  }

  if (command.type == "select_vertex") {
    bool additive = false;
    if (command.payload.contains("additive") &&
        command.payload.at("additive").is_boolean()) {
      additive = command.payload.at("additive").get<bool>();
    }
    const auto document = manager.select_vertex(
        read_string(command.payload, "vertex_id"), additive);
    polysmith::protocol::write_message(
        polysmith::protocol::make_selection_changed_event(
            command.id, polysmith::protocol::to_selection_payload(document)));
    return true;
  }

  if (command.type == "set_body_color") {
    const auto document = manager.set_body_color(
        read_string(command.payload, "body_id"),
        read_string(command.payload, "color"));
    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return true;
  }

  if (command.type == "set_face_color") {
    const auto document = manager.set_face_color(
        read_string(command.payload, "face_id"),
        read_string(command.payload, "color"));
    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return true;
  }

  if (command.type == "clear_body_color") {
    const auto document = manager.clear_body_color(
        read_string(command.payload, "body_id"));
    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return true;
  }

  if (command.type == "clear_face_color") {
    const auto document = manager.clear_face_color(
        read_string(command.payload, "face_id"));
    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return true;
  }

  if (command.type == "clear_appearance_overrides") {
    const auto document = manager.clear_appearance_overrides();
    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return true;
  }

  return false;
}

}  // namespace polysmith
