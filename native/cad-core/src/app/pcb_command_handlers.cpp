#include "app/pcb_command_handlers.h"

#include "app/command_context.h"
#include "core/cam/cam_resolution.h"
#include "core/document/document.h"
#include "protocol/serialization.h"

namespace polysmith {

bool handle_pcb_command(const polysmith::protocol::CommandMessage& command,
                        polysmith::core::DocumentManager& manager) {
  if (command.type == "pcb_capture_layer_reference") {
    // The UI asks the core to mint the reference an operation will store.
    const std::string feature_id = command.payload.value("feature_id", "");
    const auto document = manager.get_document();
    if (!document.has_value()) {
      polysmith::protocol::write_message(polysmith::protocol::make_error_event(
          command.id, "NO_ACTIVE_DOCUMENT", "No active document"));
      return true;
    }
    const polysmith::core::FeatureEntry* layer = nullptr;
    for (const auto& feature : document->feature_history) {
      if (feature.id == feature_id) {
        layer = &feature;
        break;
      }
    }
    if (layer == nullptr || !layer->pcb_layer_parameters.has_value()) {
      polysmith::protocol::write_message(polysmith::protocol::make_error_event(
          command.id, "PCB_LAYER_NOT_FOUND",
          "No PCB layer matches " + feature_id + "."));
      return true;
    }
    const auto attestation =
        polysmith::core::pcb_layer_attestation_from_feature(document.value(),
                                                            feature_id);
    if (!attestation.has_value()) {
      polysmith::protocol::write_message(polysmith::protocol::make_error_event(
          command.id, "PCB_LAYER_NOT_FOUND",
          "No PCB layer matches " + feature_id + "."));
      return true;
    }
    polysmith::protocol::write_message(
        polysmith::protocol::make_pcb_layer_attestation_event(
            command.id,
            {{"persistent_id", layer->id},
             {"attestation", polysmith::protocol::to_payload(attestation.value())}}));
    return true;
  }

  if (command.type == "pcb_import_layer") {
    const std::optional<std::string> role =
        command.payload.contains("role") && command.payload.at("role").is_string()
            ? std::optional<std::string>(command.payload.at("role").get<std::string>())
            : std::nullopt;
    const auto document = manager.import_pcb_layer(
        read_string(command.payload, "file_path"), role);
    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return true;
  }

  if (command.type == "pcb_update_layer") {
    const std::optional<std::string> role =
        command.payload.contains("role") && command.payload.at("role").is_string()
            ? std::optional<std::string>(command.payload.at("role").get<std::string>())
            : std::nullopt;
    const std::optional<bool> mirror_x =
        command.payload.contains("mirror_x") &&
                command.payload.at("mirror_x").is_boolean()
            ? std::optional<bool>(command.payload.at("mirror_x").get<bool>())
            : std::nullopt;
    const auto document = manager.update_pcb_layer(
        read_string(command.payload, "feature_id"), role, mirror_x);
    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return true;
  }

  if (command.type == "pcb_remove_layer") {
    const auto document = manager.remove_pcb_layer(
        read_string(command.payload, "feature_id"));
    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return true;
  }

  return false;
}

}  // namespace polysmith
