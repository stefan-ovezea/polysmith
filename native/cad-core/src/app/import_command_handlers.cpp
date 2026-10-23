#include "app/import_command_handlers.h"

#include "app/command_context.h"
#include "core/document/document.h"
#include "protocol/serialization.h"

namespace polysmith {

using polysmith::protocol::json;

bool handle_import_command(const polysmith::protocol::CommandMessage& command,
                           polysmith::core::DocumentManager& manager) {
  if (command.type == "import_step") {
    const auto document =
        manager.import_step(read_string(command.payload, "file_path"));
    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return true;
  }

  if (command.type == "import_dxf") {
    const auto document = manager.import_dxf(
        read_string(command.payload, "file_path"),
        read_optional_string(command.payload, "plane_id"));
    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return true;
  }

  if (command.type == "export_document_dxf") {
    const auto result = manager.export_document_as_dxf(
        read_string(command.payload, "file_path"));
    polysmith::protocol::write_message(
        polysmith::protocol::make_document_exported_event(
            command.id, json{{"file_path", result.file_path},
                             {"format", result.format},
                             {"exported_feature_count",
                              result.exported_feature_count}}));
    return true;
  }

  if (command.type == "import_iges") {
    const auto document =
        manager.import_iges(read_string(command.payload, "file_path"));
    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return true;
  }

  if (command.type == "export_document_iges") {
    const auto result = manager.export_document_as_iges(
        read_string(command.payload, "file_path"));
    polysmith::protocol::write_message(
        polysmith::protocol::make_document_exported_event(
            command.id, json{{"file_path", result.file_path},
                             {"format", result.format},
                             {"exported_feature_count",
                              result.exported_feature_count}}));
    return true;
  }

  if (command.type == "import_stl") {
    const auto document = manager.import_stl(
        read_string(command.payload, "file_path"),
        read_optional_dimension(command.payload, "scale", /*fallback=*/1.0));
    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return true;
  }

  if (command.type == "convert_mesh_to_body") {
    const auto document =
        manager.convert_mesh_to_body(read_string(command.payload, "body_id"));
    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return true;
  }

  if (command.type == "detach_body_projections") {
    const auto document =
        manager.detach_body_projections(read_string(command.payload, "body_id"));
    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return true;
  }

  return false;
}

}  // namespace polysmith
