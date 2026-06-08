DocumentState DocumentManager::set_body_color(const std::string& body_id,
                                              const std::string& color) {
  require_document();
  if (!is_valid_appearance_color(color)) {
    throw std::runtime_error("Body color must be an opaque #RRGGBB value");
  }
  if (!compiled_body_exists(*document_, body_id)) {
    throw std::runtime_error("Body not found: " + body_id);
  }

  push_undo_state();
  clear_redo_stack();
  auto& overrides = document_->appearance.body_colors;
  const auto existing = std::find_if(
      overrides.begin(),
      overrides.end(),
      [&](const BodyAppearanceOverride& entry) {
        return entry.body_id == body_id;
      });
  if (existing == overrides.end()) {
    overrides.push_back(BodyAppearanceOverride{.body_id = body_id,
                                               .color = color});
  } else {
    existing->color = color;
  }
  bump_geometry_revision();
  return document_.value();
}

DocumentState DocumentManager::set_face_color(const std::string& face_id,
                                              const std::string& color) {
  require_document();
  if (!is_valid_appearance_color(color)) {
    throw std::runtime_error("Face color must be an opaque #RRGGBB value");
  }
  const auto owner_id = owner_body_id_from_face_id(face_id);
  const auto face = resolve_face_for_appearance(*document_, face_id);
  if (!owner_id.has_value() || !compiled_body_exists(*document_, owner_id.value())) {
    throw std::runtime_error("Face not found: " + face_id);
  }
  const std::string signature = face.has_value()
                                    ? appearance_face_signature(face.value())
                                    : "semantic:" + face_id;
  if (signature.empty()) {
    throw std::runtime_error("Face appearance signature could not be resolved");
  }

  push_undo_state();
  clear_redo_stack();
  auto& overrides = document_->appearance.face_colors;
  const auto existing = std::find_if(
      overrides.begin(),
      overrides.end(),
      [&](const FaceAppearanceOverride& entry) {
        return entry.face_id == face_id;
      });
  if (existing == overrides.end()) {
    overrides.push_back(FaceAppearanceOverride{
        .face_id = face_id,
        .owner_body_id = owner_id.value(),
        .signature = signature,
        .color = color,
    });
  } else {
    existing->owner_body_id = owner_id.value();
    existing->signature = signature;
    existing->color = color;
  }
  bump_geometry_revision();
  return document_.value();
}

DocumentState DocumentManager::clear_body_color(const std::string& body_id) {
  require_document();
  auto& overrides = document_->appearance.body_colors;
  const auto existing = std::find_if(
      overrides.begin(),
      overrides.end(),
      [&](const BodyAppearanceOverride& entry) {
        return entry.body_id == body_id;
      });
  if (existing == overrides.end()) {
    return document_.value();
  }
  push_undo_state();
  clear_redo_stack();
  overrides.erase(std::remove_if(overrides.begin(),
                                 overrides.end(),
                                 [&](const BodyAppearanceOverride& entry) {
                                   return entry.body_id == body_id;
                                 }),
                  overrides.end());
  bump_geometry_revision();
  return document_.value();
}

DocumentState DocumentManager::clear_face_color(const std::string& face_id) {
  require_document();
  auto& overrides = document_->appearance.face_colors;
  const auto existing = std::find_if(
      overrides.begin(),
      overrides.end(),
      [&](const FaceAppearanceOverride& entry) {
        return entry.face_id == face_id;
      });
  if (existing == overrides.end()) {
    return document_.value();
  }
  push_undo_state();
  clear_redo_stack();
  overrides.erase(std::remove_if(overrides.begin(),
                                 overrides.end(),
                                 [&](const FaceAppearanceOverride& entry) {
                                   return entry.face_id == face_id;
                                 }),
                  overrides.end());
  bump_geometry_revision();
  return document_.value();
}

DocumentState DocumentManager::clear_appearance_overrides() {
  require_document();
  if (document_->appearance.body_colors.empty() &&
      document_->appearance.face_colors.empty()) {
    return document_.value();
  }
  push_undo_state();
  clear_redo_stack();
  document_->appearance.body_colors.clear();
  document_->appearance.face_colors.clear();
  bump_geometry_revision();
  return document_.value();
}
