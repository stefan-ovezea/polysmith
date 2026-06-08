std::optional<std::string> body_appearance_color(
    const DocumentState& document,
    const std::string& body_id) {
  const auto entry = std::find_if(
      document.appearance.body_colors.begin(),
      document.appearance.body_colors.end(),
      [&](const BodyAppearanceOverride& override_entry) {
        return override_entry.body_id == body_id;
      });
  if (entry == document.appearance.body_colors.end()) {
    return std::nullopt;
  }
  return entry->color;
}

std::optional<std::string> face_appearance_color(
    const DocumentState& document,
    const std::string& face_id,
    const std::string& signature) {
  const auto entry = std::find_if(
      document.appearance.face_colors.begin(),
      document.appearance.face_colors.end(),
      [&](const FaceAppearanceOverride& override_entry) {
        return override_entry.face_id == face_id &&
               override_entry.signature == signature;
      });
  if (entry == document.appearance.face_colors.end()) {
    return std::nullopt;
  }
  return entry->color;
}
