#include "app.h"

#include <exception>
#include <string>

#include <BRepPrimAPI_MakeBox.hxx>
#include <TopoDS_Shape.hxx>

#include "core/document.h"
#include "core/viewport.h"
#include "protocol/ipc.h"
#include "protocol/serialization.h"

namespace polysmith {
namespace {

using polysmith::core::DocumentManager;
using polysmith::core::BoxFeatureParameters;
using polysmith::protocol::CommandMessage;

DocumentManager& document_manager() {
  static DocumentManager manager;
  return manager;
}

double read_dimension(const polysmith::protocol::json& payload,
                      const char* key) {
  if (!payload.contains(key) || !payload.at(key).is_number()) {
    throw std::runtime_error(std::string("Command payload is missing numeric field '") +
                             key + "'");
  }

  return payload.at(key).get<double>();
}

std::string read_string(const polysmith::protocol::json& payload,
                        const char* key) {
  if (!payload.contains(key) || !payload.at(key).is_string()) {
    throw std::runtime_error(std::string("Command payload is missing string field '") +
                             key + "'");
  }

  return payload.at(key).get<std::string>();
}

}  // namespace

void CadCoreApp::init_occt() const {
  polysmith::protocol::write_log("Starting OCCT smoke test...");

  const TopoDS_Shape box = BRepPrimAPI_MakeBox(10.0, 20.0, 30.0).Shape();

  if (box.IsNull()) {
    polysmith::protocol::write_log("OCCT smoke test failed: shape is null");
    return;
  }

  polysmith::protocol::write_log("OCCT box created successfully");
}

void CadCoreApp::handle_command_line(const std::string& line) {
  const CommandMessage command = polysmith::protocol::parse_command(line);

  if (command.type == "ping") {
    polysmith::protocol::write_message(
        polysmith::protocol::make_pong_event(command.id));
    return;
  }

  if (command.type == "create_document") {
    const auto document = document_manager().create_document();
    polysmith::protocol::write_message(
        polysmith::protocol::make_document_created_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "get_document_state") {
    const auto document = document_manager().get_document();

    if (!document.has_value()) {
      polysmith::protocol::write_message(polysmith::protocol::make_error_event(
          command.id, "NO_ACTIVE_DOCUMENT", "No active document"));
      return;
    }

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document.value())));
    return;
  }

  if (command.type == "get_session_state") {
    polysmith::protocol::write_message(
        polysmith::protocol::make_session_state_event(
            command.id,
            polysmith::protocol::to_payload(
                document_manager().get_session_state())));
    return;
  }

  if (command.type == "get_viewport_state") {
    polysmith::protocol::write_message(
        polysmith::protocol::make_viewport_state_event(
            command.id,
            polysmith::protocol::to_payload(polysmith::core::build_viewport_state(
                document_manager().get_document()))));
    return;
  }

  if (command.type == "add_box_feature") {
    const auto document = document_manager().add_box_feature(BoxFeatureParameters{
        .width = read_dimension(command.payload, "width"),
        .height = read_dimension(command.payload, "height"),
        .depth = read_dimension(command.payload, "depth"),
    });

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "update_box_feature") {
    const auto document = document_manager().update_box_feature(
        read_string(command.payload, "feature_id"),
        BoxFeatureParameters{
            .width = read_dimension(command.payload, "width"),
            .height = read_dimension(command.payload, "height"),
            .depth = read_dimension(command.payload, "depth"),
        });

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "rename_feature") {
    const auto document = document_manager().rename_feature(
        read_string(command.payload, "feature_id"),
        read_string(command.payload, "name"));

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "delete_feature") {
    const auto document =
        document_manager().delete_feature(read_string(command.payload, "feature_id"));

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "undo") {
    const auto document = document_manager().undo();

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "redo") {
    const auto document = document_manager().redo();

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "select_feature") {
    const auto document =
        document_manager().select_feature(read_string(command.payload, "feature_id"));

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "clear_selection") {
    const auto document = document_manager().clear_selection();

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "shutdown") {
    throw std::runtime_error("__POLYSMITH_SHUTDOWN__");
  }

  polysmith::protocol::write_message(polysmith::protocol::make_error_event(
      command.id, "UNKNOWN_COMMAND", "Unknown command: " + command.type));
}

void CadCoreApp::run() {
  init_occt();
  polysmith::protocol::write_message(polysmith::protocol::make_hello_event());

  std::string line;
  while (std::getline(std::cin, line)) {
    if (line.empty()) {
      continue;
    }

    try {
      handle_command_line(line);
    } catch (const std::runtime_error& error) {
      if (std::string(error.what()) == "__POLYSMITH_SHUTDOWN__") {
        break;
      }

      polysmith::protocol::write_message(polysmith::protocol::make_error_event(
          "", "INVALID_COMMAND", error.what()));
    } catch (const std::exception& error) {
      polysmith::protocol::write_message(polysmith::protocol::make_error_event(
          "", "INVALID_JSON", error.what()));
    }
  }
}

}  // namespace polysmith
