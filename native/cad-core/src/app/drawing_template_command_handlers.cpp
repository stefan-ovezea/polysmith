#include "app/drawing_template_command_handlers.h"

#include <fstream>

#include "app/command_context.h"
#include "core/diagnostics/logger.h"
#include "core/document/document.h"
#include "core/drawing/drawing_templates.h"

namespace polysmith {

bool handle_drawing_template_command(
    const polysmith::protocol::CommandMessage& command,
    polysmith::core::DocumentManager& manager) {
  (void)manager;  // template commands only touch the file system

  if (command.type == "drawing_template_save") {
    const std::string path = read_string(command.payload, "file_path");
    if (!command.payload.contains("template") ||
        !command.payload.at("template").is_object()) {
      polysmith::protocol::write_message(polysmith::protocol::make_error_event(
          command.id, "DRAWING_TEMPLATE_SAVE_FAILED",
          "drawing_template_save needs a template object"));
      return true;
    }
    std::string error;
    const polysmith::core::DrawingTemplate template_ =
        polysmith::core::drawing_template_from_json(
            command.payload.at("template"), error);
    if (!error.empty()) {
      polysmith::protocol::write_message(polysmith::protocol::make_error_event(
          command.id, "DRAWING_TEMPLATE_SAVE_FAILED", error));
      return true;
    }
    std::ofstream stream(path);
    if (!stream) {
      polysmith::protocol::write_message(polysmith::protocol::make_error_event(
          command.id, "DRAWING_TEMPLATE_SAVE_FAILED",
          "Could not write the template file: " + path));
      return true;
    }
    stream << polysmith::core::drawing_template_to_json(template_).dump(2);
    stream << "\n";
    polysmith::core::log_info(
        "drawing", "template saved: " + path + " (" +
                       std::to_string(template_.sheets.size()) + " sheet(s))");
    polysmith::protocol::write_message(
        polysmith::protocol::make_drawing_template_save_result_event(
            command.id, polysmith::protocol::json{{"file_path", path}}));
    return true;
  }

  if (command.type == "drawing_template_load") {
    const std::string path = read_string(command.payload, "file_path");
    std::ifstream stream(path);
    if (!stream) {
      polysmith::protocol::write_message(polysmith::protocol::make_error_event(
          command.id, "DRAWING_TEMPLATE_LOAD_FAILED",
          "Could not read the template file: " + path));
      return true;
    }
    polysmith::protocol::json payload;
    try {
      stream >> payload;
    } catch (const std::exception& e) {
      polysmith::protocol::write_message(polysmith::protocol::make_error_event(
          command.id, "DRAWING_TEMPLATE_LOAD_FAILED",
          "The template file is not valid JSON: " + std::string(e.what())));
      return true;
    }
    std::string error;
    const polysmith::core::DrawingTemplate template_ =
        polysmith::core::drawing_template_from_json(payload, error);
    if (!error.empty()) {
      polysmith::protocol::write_message(polysmith::protocol::make_error_event(
          command.id, "DRAWING_TEMPLATE_LOAD_FAILED", error));
      return true;
    }
    polysmith::protocol::json sheets = polysmith::protocol::json::array();
    for (const auto& sheet : template_.sheets) {
      sheets.push_back(polysmith::protocol::to_payload(sheet));
    }
    polysmith::protocol::write_message(
        polysmith::protocol::make_drawing_template_load_result_event(
            command.id,
            polysmith::protocol::json{
                {"template",
                 polysmith::protocol::json{
                     {"name", template_.name},
                     {"sheets", std::move(sheets)},
                 }},
            }));
    return true;
  }

  return false;
}

}  // namespace polysmith
