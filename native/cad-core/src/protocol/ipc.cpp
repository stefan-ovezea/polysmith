#include "protocol/ipc.h"

#include <cstdio>
#include <stdexcept>

namespace polysmith::protocol {

void write_message(const json& message) {
  const std::string str = message.dump() + "\n";
  fwrite(str.data(), 1, str.size(), stdout);
  fflush(stdout);
}

void write_log(const std::string& message) {
  const std::string str = message + "\n";
  fwrite(str.data(), 1, str.size(), stderr);
  fflush(stderr);
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

json make_log_event(const std::string& level,
                    const std::string& source,
                    const std::string& message,
                    const std::string& timestamp) {
  return {
      {"type", "log"},
      {"payload",
       {
           {"level", level},
           {"source", source},
           {"message", message},
           {"timestamp", timestamp},
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

json make_cam_generation_progress_event(const std::string& id,
                                        const std::string& op_id,
                                        int percent) {
  return {
      {"id", id},
      {"type", "cam_generation_progress"},
      {"payload",
       {
           {"op_id", op_id},
           {"percent", percent},
       }},
  };
}

json make_cam_post_list_event(const std::string& id, const json& posts) {
  return {
      {"id", id},
      {"type", "cam_post_list_result"},
      {"payload",
       {
           {"posts", posts},
       }},
  };
}

}  // namespace polysmith::protocol
