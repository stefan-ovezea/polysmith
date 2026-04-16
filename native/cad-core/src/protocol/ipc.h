#pragma once

#include <nlohmann/json.hpp>
#include <string>

namespace polysmith::protocol {

using json = nlohmann::json;

struct CommandMessage {
  std::string id;
  std::string type;
  json payload;
};

void write_message(const json& message);
void write_log(const std::string& message);
CommandMessage parse_command(const std::string& line);
json make_hello_event();
json make_pong_event(const std::string& id);
json make_document_created_event(const std::string& id, const json& document);
json make_document_state_event(const std::string& id, const json& document);
json make_session_state_event(const std::string& id, const json& session);
json make_viewport_state_event(const std::string& id, const json& viewport);
json make_error_event(const std::string& id,
                      const std::string& code,
                      const std::string& message);

}  // namespace polysmith::protocol
