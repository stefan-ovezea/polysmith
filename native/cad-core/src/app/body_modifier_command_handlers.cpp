#include "app/body_modifier_command_handlers.h"

#include "app/command_context.h"
#include "core/document/document.h"

namespace polysmith {

bool handle_body_modifier_command(
    const polysmith::protocol::CommandMessage& command,
    polysmith::core::DocumentManager& manager) {
  // ── Chamfer ──────────────────────────────────────────────────────
  if (command.type == "create_chamfer") {
    const auto edge_ids = read_edge_ids(command.payload, /*require_one=*/true);
    const auto document = manager.create_chamfer(
        edge_ids, read_dimension(command.payload, "distance"));
    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return true;
  }

  if (command.type == "update_chamfer_edges") {
    const auto edge_ids = read_edge_ids(command.payload, /*require_one=*/false);
    const std::string feature_id = read_string(command.payload, "feature_id");
    const auto document = manager.update_chamfer_edges(feature_id, edge_ids);
    if (emit_feature_changed(command.id, feature_id, document)) {
      return true;
    }
    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return true;
  }

  if (command.type == "update_chamfer_distance") {
    const std::string feature_id = read_string(command.payload, "feature_id");
    const auto document = manager.update_chamfer_distance(
        feature_id, read_dimension(command.payload, "distance"));
    if (emit_feature_changed(command.id, feature_id, document)) {
      return true;
    }
    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return true;
  }

  if (command.type == "confirm_chamfer") {
    const auto document = manager.confirm_chamfer(
        read_string(command.payload, "feature_id"));
    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return true;
  }

  // ── Shell ────────────────────────────────────────────────────────
  if (command.type == "create_shell") {
    const auto document = manager.create_shell(
        read_string(command.payload, "face_id"),
        read_dimension(command.payload, "thickness"));
    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return true;
  }

  if (command.type == "update_shell_thickness") {
    const std::string feature_id = read_string(command.payload, "feature_id");
    const auto document = manager.update_shell_thickness(
        feature_id, read_dimension(command.payload, "thickness"));
    if (emit_feature_changed(command.id, feature_id, document)) {
      return true;
    }
    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return true;
  }

  if (command.type == "confirm_shell") {
    const auto document = manager.confirm_shell(
        read_string(command.payload, "feature_id"));
    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return true;
  }

  // ── Hole ─────────────────────────────────────────────────────────
  if (command.type == "create_hole") {
    const auto document = manager.create_hole(
        read_string(command.payload, "face_id"),
        read_dimension(command.payload, "center_x"),
        read_dimension(command.payload, "center_y"),
        read_dimension(command.payload, "center_z"),
        read_hole_parameters(command.payload));
    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return true;
  }

  if (command.type == "update_hole_parameters") {
    const std::string feature_id = read_string(command.payload, "feature_id");
    const auto document = manager.update_hole_parameters(
        feature_id, read_hole_parameters(command.payload));
    if (emit_feature_changed(command.id, feature_id, document)) {
      return true;
    }
    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return true;
  }

  if (command.type == "confirm_hole") {
    const auto document = manager.confirm_hole(
        read_string(command.payload, "feature_id"));
    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return true;
  }

  return false;
}

}  // namespace polysmith
