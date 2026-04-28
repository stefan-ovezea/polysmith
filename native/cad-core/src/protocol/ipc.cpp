#include "protocol/ipc.h"

#include <iostream>
#include <stdexcept>

namespace polysmith::protocol {

void write_message(const json& message) {
  std::cout << message.dump() << std::endl;
}

void write_log(const std::string& message) {
  std::cerr << message << std::endl;
}

CommandMessage parse_command(const std::string& line) {
  const json message = json::parse(line);

  if (!message.contains("type") || !message.at("type").is_string()) {
    throw std::runtime_error("Command message is missing string field 'type'");
  }

  CommandMessage command{
      .id = message.value("id", ""),
      .type = message.at("type").get<std::string>(),
      .payload = message.value("payload", json::object()),
  };

  if (command.id.empty() && command.type != "shutdown") {
    throw std::runtime_error("Command message is missing string field 'id'");
  }

  if (!command.payload.is_object()) {
    throw std::runtime_error("Command message field 'payload' must be an object");
  }

  return command;
}

json make_hello_event() {
  return {
      {"type", "hello"},
      {"payload",
       {
           {"service", "cad_core"},
           {"version", "0.1.0"},
       }},
  };
}

json make_pong_event(const std::string& id) {
  return {
      {"id", id},
      {"type", "pong"},
      {"payload",
       {
           {"version", "0.1.0"},
       }},
  };
}

json make_document_created_event(const std::string& id, const json& document) {
  return {
      {"id", id},
      {"type", "document_created"},
      {"payload", document},
  };
}

json make_document_state_event(const std::string& id, const json& document) {
  return {
      {"id", id},
      {"type", "document_state"},
      {"payload", document},
  };
}

json make_session_state_event(const std::string& id, const json& session) {
  return {
      {"id", id},
      {"type", "session_state"},
      {"payload", session},
  };
}

json make_viewport_state_event(const std::string& id, const json& viewport) {
  return {
      {"id", id},
      {"type", "viewport_state"},
      {"payload", viewport},
  };
}

json make_document_exported_event(const std::string& id,
                                  const json& export_result) {
  return {
      {"id", id},
      {"type", "document_exported"},
      {"payload", export_result},
  };
}

json make_document_saved_event(const std::string& id,
                               const std::string& file_path) {
  return {
      {"id", id},
      {"type", "document_saved"},
      {"payload",
       {
           {"file_path", file_path},
       }},
  };
}

json make_error_event(const std::string& id,
                      const std::string& code,
                      const std::string& message) {
  json payload = {
      {"code", code},
      {"message", message},
  };

  json error = {
      {"type", "error"},
      {"payload", payload},
  };

  if (!id.empty()) {
    error["id"] = id;
  }

  return error;
}

}  // namespace polysmith::protocol
