#include "app/solid_feature_command_handlers.h"

#include "app/command_context.h"
#include "core/document/document.h"

namespace polysmith {

bool handle_solid_feature_command(
    const polysmith::protocol::CommandMessage& command,
    polysmith::core::DocumentManager& manager) {
  // ── Loft ─────────────────────────────────────────────────────────
  if (command.type == "loft_profiles") {
    std::vector<std::string> profile_ids;
    if (command.payload.contains("profile_ids") &&
        command.payload.at("profile_ids").is_array()) {
      for (const auto& id : command.payload.at("profile_ids")) {
        if (id.is_string()) {
          profile_ids.push_back(id.get<std::string>());
        }
      }
    }
    bool ruled = false;
    if (command.payload.contains("ruled") &&
        command.payload.at("ruled").is_boolean()) {
      ruled = command.payload.at("ruled").get<bool>();
    }
    const auto document = manager.loft_profiles(profile_ids, ruled);
    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return true;
  }

  if (command.type == "update_loft_profiles") {
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
    const auto document = manager.update_loft_profiles(feature_id, profile_ids);
    if (emit_feature_changed(command.id, feature_id, document)) {
      return true;
    }
    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return true;
  }

  if (command.type == "update_loft_ruled") {
    if (!command.payload.contains("ruled") ||
        !command.payload.at("ruled").is_boolean()) {
      throw std::runtime_error("Command payload is missing bool field 'ruled'");
    }
    const std::string feature_id = read_string(command.payload, "feature_id");
    const auto document = manager.update_loft_ruled(
        feature_id, command.payload.at("ruled").get<bool>());
    if (emit_feature_changed(command.id, feature_id, document)) {
      return true;
    }
    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return true;
  }

  // ── Revolve ─────────────────────────────────────────────────────
  if (command.type == "revolve_profile") {
    double angle_degrees = 360.0;
    if (command.payload.contains("angle_degrees") &&
        command.payload.at("angle_degrees").is_number()) {
      angle_degrees = command.payload.at("angle_degrees").get<double>();
    }
    const auto document = manager.revolve_profile(
        read_string(command.payload, "profile_id"),
        read_string(command.payload, "axis_entity_id"), angle_degrees);
    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return true;
  }

  if (command.type == "update_revolve_profile") {
    const std::string feature_id = read_string(command.payload, "feature_id");
    const auto document = manager.update_revolve_profile(
        feature_id, read_string(command.payload, "profile_id"));
    if (emit_feature_changed(command.id, feature_id, document)) {
      return true;
    }
    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return true;
  }

  if (command.type == "update_revolve_axis") {
    const std::string feature_id = read_string(command.payload, "feature_id");
    const auto document = manager.update_revolve_axis(
        feature_id, read_string(command.payload, "axis_entity_id"));
    if (emit_feature_changed(command.id, feature_id, document)) {
      return true;
    }
    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return true;
  }

  if (command.type == "update_revolve_angle") {
    const std::string feature_id = read_string(command.payload, "feature_id");
    const auto document = manager.update_revolve_angle(
        feature_id, read_dimension(command.payload, "angle_degrees"));
    if (emit_feature_changed(command.id, feature_id, document)) {
      return true;
    }
    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return true;
  }

  // ── Sweep ───────────────────────────────────────────────────────
  if (command.type == "sweep_profile") {
    const auto document = manager.sweep_profile(
        read_string(command.payload, "profile_id"),
        read_string(command.payload, "path_entity_id"));
    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return true;
  }

  if (command.type == "update_sweep_profile") {
    const std::string feature_id = read_string(command.payload, "feature_id");
    const auto document = manager.update_sweep_profile(
        feature_id, read_string(command.payload, "profile_id"));
    if (emit_feature_changed(command.id, feature_id, document)) {
      return true;
    }
    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return true;
  }

  if (command.type == "update_sweep_path") {
    const std::string feature_id = read_string(command.payload, "feature_id");
    const auto document = manager.update_sweep_path(
        feature_id, read_string(command.payload, "path_entity_id"));
    if (emit_feature_changed(command.id, feature_id, document)) {
      return true;
    }
    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return true;
  }

  // ── Profile selection ───────────────────────────────────────────
  if (command.type == "select_sketch_profiles") {
    std::vector<std::string> profile_ids;
    if (command.payload.contains("profile_ids") &&
        command.payload.at("profile_ids").is_array()) {
      for (const auto& entry : command.payload.at("profile_ids")) {
        if (entry.is_string()) {
          profile_ids.push_back(entry.get<std::string>());
        }
      }
    }
    bool additive = false;
    if (command.payload.contains("additive") &&
        command.payload.at("additive").is_boolean()) {
      additive = command.payload.at("additive").get<bool>();
    }
    const auto document =
        manager.select_sketch_profiles(profile_ids, additive);
    polysmith::protocol::write_message(
        polysmith::protocol::make_selection_changed_event(
            command.id, polysmith::protocol::to_selection_payload(document)));
    return true;
  }

  if (command.type == "select_sketch_profile") {
    bool additive = false;
    if (command.payload.contains("additive") &&
        command.payload.at("additive").is_boolean()) {
      additive = command.payload.at("additive").get<bool>();
    }
    const bool by_entity = command.payload.contains("entity_id") &&
                           !command.payload.at("entity_id").is_null();
    bool smallest_only = false;
    if (command.payload.contains("smallest_only") &&
        command.payload.at("smallest_only").is_boolean()) {
      smallest_only = command.payload.at("smallest_only").get<bool>();
    }
    const auto document =
        by_entity
            ? manager.select_sketch_profile_by_entity(
                  read_string(command.payload, "entity_id"), additive,
                  smallest_only)
            : manager.select_sketch_profile(
                  read_string(command.payload, "profile_id"), additive);
    polysmith::protocol::write_message(
        polysmith::protocol::make_selection_changed_event(
            command.id, polysmith::protocol::to_selection_payload(document)));
    return true;
  }

  return false;
}

}  // namespace polysmith
