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

int read_int(const polysmith::protocol::json& payload, const char* key) {
  if (!payload.contains(key) || !payload.at(key).is_number_integer()) {
    throw std::runtime_error(std::string("Command payload is missing integer field '") +
                             key + "'");
  }

  return payload.at(key).get<int>();
}

std::string read_string(const polysmith::protocol::json& payload,
                        const char* key) {
  if (!payload.contains(key) || !payload.at(key).is_string()) {
    throw std::runtime_error(std::string("Command payload is missing string field '") +
                             key + "'");
  }

  return payload.at(key).get<std::string>();
}

std::optional<std::string> read_optional_string(
    const polysmith::protocol::json& payload,
    const char* key) {
  if (!payload.contains(key) || payload.at(key).is_null()) {
    return std::nullopt;
  }
  if (!payload.at(key).is_string()) {
    throw std::runtime_error(std::string("Command payload field is not a string '") +
                             key + "'");
  }
  return payload.at(key).get<std::string>();
}

double read_optional_dimension(const polysmith::protocol::json& payload,
                               const char* key,
                               double fallback) {
  if (!payload.contains(key) || payload.at(key).is_null()) {
    return fallback;
  }
  if (!payload.at(key).is_number()) {
    throw std::runtime_error(std::string("Command payload field is not numeric '") +
                             key + "'");
  }
  return payload.at(key).get<double>();
}
