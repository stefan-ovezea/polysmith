#include "app/sketch_tool_command_handlers.h"

#include "app/command_context.h"
#include "core/document/document.h"
#include "core/sketch/trim_engine.h"
#include "protocol/serialization.h"

namespace polysmith {

bool handle_sketch_tool_command(
    const polysmith::protocol::CommandMessage& command,
    polysmith::core::DocumentManager& manager) {
  if (command.type == "extend_sketch_entity") {
    const auto document = manager.extend_sketch_entity(
        read_string(command.payload, "entity_id"),
        read_dimension(command.payload, "click_x"),
        read_dimension(command.payload, "click_y"));
    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return true;
  }

  if (command.type == "split_sketch_entity") {
    const auto document = manager.split_sketch_entity(
        read_string(command.payload, "entity_id"),
        read_dimension(command.payload, "click_x"),
        read_dimension(command.payload, "click_y"),
        read_dimension(command.payload, "split2_x"),
        read_dimension(command.payload, "split2_y"));
    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return true;
  }

  if (command.type == "offset_sketch_entity") {
    const auto document = manager.offset_sketch_entity(
        read_string(command.payload, "entity_id"),
        read_dimension(command.payload, "distance"));
    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return true;
  }

  if (command.type == "trim_sketch_stroke") {
    std::vector<polysmith::core::TrimStrokeEntry> entries;
    if (command.payload.contains("entries") &&
        command.payload["entries"].is_array()) {
      entries.reserve(command.payload["entries"].size());
      for (const auto& item : command.payload["entries"]) {
        entries.push_back(polysmith::core::TrimStrokeEntry{
            .entity_id = read_string(item, "entity_id"),
            .click_x = read_dimension(item, "click_x"),
            .click_y = read_dimension(item, "click_y"),
        });
      }
    }
    const auto document = manager.trim_sketch_stroke(entries);
    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return true;
  }

  return false;
}

}  // namespace polysmith
