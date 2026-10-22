#include "app/document_session_command_handlers.h"

#include "app/command_context.h"
#include "core/diagnostics/logger.h"
#include "core/document/document.h"
#include "core/viewport/viewport.h"

namespace polysmith {

bool handle_document_session_command(
    const polysmith::protocol::CommandMessage& command,
    polysmith::core::DocumentManager& manager) {
  if (command.type == "ping") {
    polysmith::protocol::write_message(
        polysmith::protocol::make_pong_event(command.id));
    return true;
  }

  if (command.type == "create_document") {
    const auto document = manager.create_document();
    polysmith::protocol::write_message(
        polysmith::protocol::make_document_created_event(
            command.id, polysmith::protocol::to_payload(document)));
    return true;
  }

  if (command.type == "get_document_state") {
    const auto document = manager.get_document();
    if (!document.has_value()) {
      polysmith::core::log_error("cad_core", "No active document");
      polysmith::protocol::write_message(polysmith::protocol::make_error_event(
          command.id, "NO_ACTIVE_DOCUMENT", "No active document"));
      return true;
    }
    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document.value())));
    return true;
  }

  if (command.type == "get_session_state") {
    polysmith::protocol::write_message(
        polysmith::protocol::make_session_state_event(
            command.id,
            polysmith::protocol::to_payload(manager.get_session_state())));
    return true;
  }

  if (command.type == "get_sketch_profiles") {
    const auto document = manager.get_document();
    if (!document.has_value()) {
      polysmith::core::log_error("cad_core", "No active document");
      polysmith::protocol::write_message(polysmith::protocol::make_error_event(
          command.id, "NO_ACTIVE_DOCUMENT", "No active document"));
      return true;
    }
    const std::string feature_id = read_string(command.payload, "feature_id");
    for (const auto& feature : document->feature_history) {
      if (feature.id != feature_id) {
        continue;
      }
      if (!feature.sketch_parameters.has_value()) {
        polysmith::protocol::write_message(polysmith::protocol::make_error_event(
            command.id, "NOT_A_SKETCH",
            "Feature '" + feature_id + "' is not a sketch"));
        return true;
      }
      const auto feature_payload = polysmith::protocol::to_payload(
          feature, /*include_opaque=*/false, /*include_vertices=*/false,
          /*include_profiles=*/true);
      const auto& params = feature_payload.at("sketch_parameters");
      polysmith::protocol::write_message(
          polysmith::protocol::make_sketch_profiles_event(
              command.id,
              polysmith::protocol::json{
                  {"feature_id", feature.id},
                  {"profile_count", params.at("profile_count")},
                  {"profiles", params.at("profiles")},
              }));
      return true;
    }
    polysmith::protocol::write_message(polysmith::protocol::make_error_event(
        command.id, "FEATURE_NOT_FOUND", "No feature '" + feature_id + "'"));
    return true;
  }

  if (command.type == "get_viewport_state") {
    polysmith::protocol::write_message(
        polysmith::protocol::make_viewport_state_event(
            command.id,
            polysmith::protocol::to_payload(
                polysmith::core::build_viewport_state(manager.get_document()))));
    return true;
  }

  if (command.type == "export_document") {
    const auto result = manager.export_document_as_step(
        read_string(command.payload, "file_path"));
    polysmith::protocol::write_message(
        polysmith::protocol::make_document_exported_event(
            command.id, polysmith::protocol::json{
                            {"file_path", result.file_path},
                            {"format", result.format},
                            {"exported_feature_count",
                             result.exported_feature_count},
                        }));
    return true;
  }

  if (command.type == "export_document_stl") {
    const auto result = manager.export_document_as_stl(
        read_string(command.payload, "file_path"));
    polysmith::protocol::write_message(
        polysmith::protocol::make_document_exported_event(
            command.id, polysmith::protocol::json{
                            {"file_path", result.file_path},
                            {"format", result.format},
                            {"exported_feature_count",
                             result.exported_feature_count},
                        }));
    return true;
  }

  if (command.type == "export_body_stl") {
    const auto result = manager.export_body_as_stl(
        read_string(command.payload, "file_path"),
        read_string(command.payload, "body_id"));
    polysmith::protocol::write_message(
        polysmith::protocol::make_document_exported_event(
            command.id, polysmith::protocol::json{
                            {"file_path", result.file_path},
                            {"format", result.format},
                            {"exported_feature_count",
                             result.exported_feature_count},
                        }));
    return true;
  }

  if (command.type == "export_body_step") {
    const auto result = manager.export_body_as_step(
        read_string(command.payload, "file_path"),
        read_string(command.payload, "body_id"));
    polysmith::protocol::write_message(
        polysmith::protocol::make_document_exported_event(
            command.id, polysmith::protocol::json{
                            {"file_path", result.file_path},
                            {"format", result.format},
                            {"exported_feature_count",
                             result.exported_feature_count},
                        }));
    return true;
  }

  if (command.type == "save_document") {
    const std::string file_path = read_string(command.payload, "file_path");
    manager.save_document_to_path(file_path);
    polysmith::protocol::write_message(
        polysmith::protocol::make_document_saved_event(command.id, file_path));
    return true;
  }

  if (command.type == "load_document") {
    const std::string file_path = read_string(command.payload, "file_path");
    const auto document = manager.load_document_from_path(file_path);
    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return true;
  }

  return false;
}

}  // namespace polysmith
