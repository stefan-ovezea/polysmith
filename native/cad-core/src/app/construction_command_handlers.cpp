#include "app/construction_command_handlers.h"

#include "app/command_context.h"
#include "core/document/document.h"

namespace polysmith {

bool handle_construction_command(
    const polysmith::protocol::CommandMessage& command,
    polysmith::core::DocumentManager& manager) {
  // ── Transform (move / body copy) ─────────────────────────────────
  if (command.type == "create_move") {
    const auto document = manager.create_move(
        read_string(command.payload, "target_body_id"),
        read_move_parameters(command.payload));
    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return true;
  }

  if (command.type == "update_move_parameters") {
    const std::string feature_id = read_string(command.payload, "feature_id");
    const auto document = manager.update_move_parameters(
        feature_id, read_move_parameters(command.payload));
    if (emit_feature_changed(command.id, feature_id, document)) {
      return true;
    }
    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return true;
  }

  if (command.type == "confirm_move") {
    const auto document = manager.confirm_move(
        read_string(command.payload, "feature_id"));
    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return true;
  }

  if (command.type == "create_body_copy") {
    const auto document = manager.create_body_copy(
        read_string(command.payload, "source_body_id"),
        read_optional_string(command.payload, "copy_mode").value_or("linked"));
    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return true;
  }

  if (command.type == "unlink_body_copy") {
    const auto document = manager.unlink_body_copy(
        read_string(command.payload, "feature_id"));
    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return true;
  }

  // ── Construction planes / axis / point ───────────────────────────
  if (command.type == "create_offset_plane") {
    const auto document = manager.create_offset_plane(
        read_string(command.payload, "source_plane_id"),
        read_dimension(command.payload, "offset"));
    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return true;
  }

  if (command.type == "create_midplane") {
    std::vector<std::string> source_plane_ids;
    if (command.payload.contains("source_plane_ids") &&
        command.payload.at("source_plane_ids").is_array()) {
      for (const auto& id : command.payload.at("source_plane_ids")) {
        if (id.is_string()) {
          source_plane_ids.push_back(id.get<std::string>());
        }
      }
    }
    if (source_plane_ids.size() != 2) {
      throw std::runtime_error("create_midplane requires two source_plane_ids");
    }
    const auto document =
        manager.create_midplane(source_plane_ids[0], source_plane_ids[1]);
    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return true;
  }

  if (command.type == "create_tangent_plane") {
    const auto document = manager.create_tangent_plane(
        read_string(command.payload, "source_face_id"));
    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return true;
  }

  if (command.type == "create_angle_plane") {
    const auto document = manager.create_angle_plane(
        read_string(command.payload, "source_plane_id"),
        read_string(command.payload, "source_axis_id"),
        read_dimension(command.payload, "angle_degrees"));
    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return true;
  }

  if (command.type == "create_construction_axis") {
    const auto document = manager.create_construction_axis(
        read_string(command.payload, "source_id"));
    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return true;
  }

  if (command.type == "create_construction_point") {
    const auto document = manager.create_construction_point(
        read_string(command.payload, "source_id"));
    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return true;
  }

  if (command.type == "update_offset_plane") {
    const std::string feature_id = read_string(command.payload, "feature_id");
    const auto document = manager.update_offset_plane(
        feature_id, read_dimension(command.payload, "offset"));
    if (emit_feature_changed(command.id, feature_id, document)) {
      return true;
    }
    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return true;
  }

  if (command.type == "update_angle_plane") {
    const std::string feature_id = read_string(command.payload, "feature_id");
    const auto document = manager.update_angle_plane(
        feature_id, read_dimension(command.payload, "angle_degrees"));
    if (emit_feature_changed(command.id, feature_id, document)) {
      return true;
    }
    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return true;
  }

  return false;
}

}  // namespace polysmith
