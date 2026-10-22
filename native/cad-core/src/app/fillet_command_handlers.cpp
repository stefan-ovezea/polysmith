#include "app/fillet_command_handlers.h"

#include "app/command_context.h"
#include "core/document/document.h"

namespace polysmith {

bool handle_fillet_command(const polysmith::protocol::CommandMessage& command,
                           polysmith::core::DocumentManager& manager) {
  if (command.type == "create_fillet") {
    // Accept either a single `edge_id` (legacy) or an `edge_ids` array.
    const auto edge_ids = read_edge_ids(command.payload, /*require_one=*/true);
    const auto document = manager.create_fillet(
        edge_ids, read_dimension(command.payload, "radius"));
    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return true;
  }

  if (command.type == "update_fillet_edges") {
    const auto edge_ids = read_edge_ids(command.payload, /*require_one=*/false);
    const std::string feature_id = read_string(command.payload, "feature_id");
    const auto document = manager.update_fillet_edges(feature_id, edge_ids);
    if (emit_feature_changed(command.id, feature_id, document)) {
      return true;
    }
    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return true;
  }

  if (command.type == "update_fillet_radius") {
    const std::string feature_id = read_string(command.payload, "feature_id");
    const auto document = manager.update_fillet_radius(
        feature_id, read_dimension(command.payload, "radius"));
    if (emit_feature_changed(command.id, feature_id, document)) {
      return true;
    }
    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return true;
  }

  if (command.type == "confirm_fillet") {
    const auto document = manager.confirm_fillet(
        read_string(command.payload, "feature_id"));
    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return true;
  }

  return false;
}

}  // namespace polysmith
