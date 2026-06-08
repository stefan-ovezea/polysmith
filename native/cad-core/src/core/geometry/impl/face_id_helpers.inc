struct ParsedFaceId {
  std::string owner_id;
  std::string suffix;
};

std::optional<ParsedFaceId> parse_face_id(const std::string& face_id) {
  // Expected layout: "{owner_id}:face:{suffix}".
  const std::string separator = ":face:";
  const auto pos = face_id.find(separator);
  if (pos == std::string::npos) {
    return std::nullopt;
  }
  return ParsedFaceId{
      .owner_id = face_id.substr(0, pos),
      .suffix = face_id.substr(pos + separator.size()),
  };
}

const FeatureEntry* find_feature(const DocumentState& document,
                                 const std::string& feature_id) {
  for (const auto& feature : document.feature_history) {
    if (feature.id == feature_id) {
      return &feature;
    }
  }
  return nullptr;
}
