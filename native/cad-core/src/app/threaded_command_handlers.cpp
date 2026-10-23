#include "app/threaded_command_handlers.h"

#include "app/command_context.h"
#include "core/document/document.h"

namespace polysmith {

bool handle_threaded_command(
    const polysmith::protocol::CommandMessage& command,
    polysmith::core::DocumentManager& manager) {
  if (command.type == "create_helix") {
    const auto document = manager.create_helix(
        read_string(command.payload, "axis_source_id"),
        read_helix_parameters(command.payload));
    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return true;
  }

  if (command.type == "update_helix_parameters") {
    const std::string feature_id = read_string(command.payload, "feature_id");
    const auto document = manager.update_helix_parameters(
        feature_id, read_helix_parameters(command.payload));
    if (emit_feature_changed(command.id, feature_id, document)) {
      return true;
    }
    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return true;
  }

  if (command.type == "create_thread") {
    const auto document = manager.create_thread(
        read_thread_parameters(command.payload));
    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return true;
  }

  if (command.type == "update_thread_parameters") {
    const std::string feature_id = read_string(command.payload, "feature_id");
    const auto document = manager.update_thread_parameters(
        feature_id, read_thread_parameters(command.payload));
    if (emit_feature_changed(command.id, feature_id, document)) {
      return true;
    }
    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return true;
  }

  if (command.type == "confirm_thread") {
    const auto document = manager.confirm_thread(
        read_string(command.payload, "feature_id"));
    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return true;
  }

  if (command.type == "create_fastener") {
    const auto document = manager.create_fastener(
        read_fastener_parameters(command.payload));
    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return true;
  }

  if (command.type == "update_fastener_parameters") {
    const std::string feature_id = read_string(command.payload, "feature_id");
    const auto document = manager.update_fastener_parameters(
        feature_id, read_fastener_parameters(command.payload));
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
