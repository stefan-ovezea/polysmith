#include "app/extrude_command_handlers.h"

#include "app/command_context.h"
#include "core/document/document.h"

namespace polysmith {

bool handle_extrude_command(
    const polysmith::protocol::CommandMessage& command,
    polysmith::core::DocumentManager& manager) {
  if (command.type == "extrude_profile") {
    std::string mode;
    if (command.payload.contains("mode") &&
        command.payload.at("mode").is_string()) {
      mode = command.payload.at("mode").get<std::string>();
    }
    std::optional<std::string> target_body_id;
    if (command.payload.contains("target_body_id") &&
        command.payload.at("target_body_id").is_string()) {
      target_body_id =
          command.payload.at("target_body_id").get<std::string>();
    }
    std::vector<std::string> profile_ids;
    if (command.payload.contains("profile_ids") &&
        command.payload.at("profile_ids").is_array()) {
      for (const auto& id : command.payload.at("profile_ids")) {
        if (id.is_string()) {
          profile_ids.push_back(id.get<std::string>());
        }
      }
    }
    const bool has_open_entity_ids =
        command.payload.contains("open_entity_ids") &&
        command.payload.at("open_entity_ids").is_array();
    if (profile_ids.empty() && !has_open_entity_ids) {
      profile_ids.push_back(read_string(command.payload, "profile_id"));
    }
    const double depth = read_dimension(command.payload, "depth");
    const auto parameters = read_optional_extrude_parameters(
        command.payload, depth, mode, target_body_id);
    if (profile_ids.empty() && has_open_entity_ids) {
      std::vector<std::string> entity_ids;
      for (const auto& id : command.payload.at("open_entity_ids")) {
        if (id.is_string()) {
          entity_ids.push_back(id.get<std::string>());
        }
      }
      const auto document = manager.extrude_open_entities(
          entity_ids, depth, mode, target_body_id, parameters);
      polysmith::protocol::write_message(
          polysmith::protocol::make_document_state_event(
              command.id, polysmith::protocol::to_payload(document)));
      return true;
    }
    const auto document = manager.extrude_profiles(
        profile_ids, depth, mode, target_body_id, parameters);
    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return true;
  }

  if (command.type == "extrude_face") {
    std::string mode;
    if (command.payload.contains("mode") &&
        command.payload.at("mode").is_string()) {
      mode = command.payload.at("mode").get<std::string>();
    }
    std::optional<std::string> target_body_id;
    if (command.payload.contains("target_body_id") &&
        command.payload.at("target_body_id").is_string()) {
      target_body_id =
          command.payload.at("target_body_id").get<std::string>();
    }
    const double depth = read_dimension(command.payload, "depth");
    const auto parameters = read_optional_extrude_parameters(
        command.payload, depth, mode, target_body_id);
    const auto document = manager.extrude_face(
        read_string(command.payload, "face_id"), depth, mode, target_body_id,
        parameters);
    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return true;
  }

  if (command.type == "update_extrude_mode") {
    const std::string feature_id = read_string(command.payload, "feature_id");
    const auto document = manager.update_extrude_mode(
        feature_id, read_string(command.payload, "mode"));
    if (emit_feature_changed(command.id, feature_id, document)) {
      return true;
    }
    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return true;
  }

  if (command.type == "update_extrude_target_body") {
    std::optional<std::string> target_body_id;
    if (command.payload.contains("target_body_id") &&
        command.payload.at("target_body_id").is_string()) {
      target_body_id =
          command.payload.at("target_body_id").get<std::string>();
    }
    const std::string feature_id = read_string(command.payload, "feature_id");
    const auto document = manager.update_extrude_target_body(
        feature_id, target_body_id);
    if (emit_feature_changed(command.id, feature_id, document)) {
      return true;
    }
    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return true;
  }

  if (command.type == "update_extrude_parameters") {
    const auto parameters =
        command.payload.contains("parameters") &&
                command.payload.at("parameters").is_object()
            ? polysmith::protocol::extrude_parameters_from_payload(
                  command.payload.at("parameters"))
            : polysmith::protocol::extrude_parameters_from_payload(
                  command.payload);
    const std::string feature_id = read_string(command.payload, "feature_id");
    const auto document = manager.update_extrude_parameters(
        feature_id, parameters);
    if (emit_feature_changed(command.id, feature_id, document)) {
      return true;
    }
    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return true;
  }

  if (command.type == "update_extrude_profiles") {
    std::vector<std::string> profile_ids;
    if (command.payload.contains("profile_ids") &&
        command.payload.at("profile_ids").is_array()) {
      for (const auto& id : command.payload.at("profile_ids")) {
        if (id.is_string()) {
          profile_ids.push_back(id.get<std::string>());
        }
      }
    }
    const std::string feature_id = read_string(command.payload, "feature_id");
    const auto document = manager.update_extrude_profiles(
        feature_id, profile_ids);
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
