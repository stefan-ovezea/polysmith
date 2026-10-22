#pragma once

#include <cctype>
#include <optional>
#include <string>
#include <vector>

namespace polysmith::core {

struct BodyAppearanceOverride {
  std::string body_id;
  std::string color;
};

struct FaceAppearanceOverride {
  std::string face_id;
  std::string owner_body_id;
  std::string signature;
  std::string color;
};

struct DocumentAppearance {
  std::vector<BodyAppearanceOverride> body_colors;
  std::vector<FaceAppearanceOverride> face_colors;
};

inline bool is_valid_appearance_color(const std::string& color) {
  if (color.size() != 7 || color[0] != '#') {
    return false;
  }
  for (std::size_t i = 1; i < color.size(); ++i) {
    if (!std::isxdigit(static_cast<unsigned char>(color[i]))) {
      return false;
    }
  }
  return true;
}

inline std::optional<std::string> owner_body_id_from_face_id(
    const std::string& face_id) {
  const auto separator = face_id.find(":face:");
  if (separator == std::string::npos || separator == 0) {
    return std::nullopt;
  }
  return face_id.substr(0, separator);
}

}  // namespace polysmith::core
