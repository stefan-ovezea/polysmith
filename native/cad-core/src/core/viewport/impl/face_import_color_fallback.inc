  // ── Imported-file display colours ─────────────────────────────────
  // A STEP file can carry its own presentation styles (a PCB's soldermask,
  // pad plating, white silkscreen lettering, component bodies). They are
  // geometry-independent, so they were captured next to the B-rep snapshot
  // and are applied here — under the user's paint, never over it: the
  // appearance passes above already had the first word, this only fills
  // faces that still have no colour.
  //
  // Without this an imported board renders as one flat body colour: its
  // markings and component colours only exist in the file's styles.
  if (!solid_faces.empty()) {
    // Cache the imported colour tables by owning feature so a 24k-face
    // board does not rescan the feature history per face.
    std::map<std::string, const std::vector<std::uint32_t>*> import_colors;
    for (auto& face : solid_faces) {
      if (face.appearance_color.has_value()) {
        continue;
      }
      auto cached = import_colors.find(face.owner_id);
      const std::vector<std::uint32_t>* colors = nullptr;
      if (cached == import_colors.end()) {
        for (const auto& feature : view->feature_history) {
          if (feature.id != face.owner_id) {
            continue;
          }
          if (feature.step_import_parameters.has_value() &&
              !feature.step_import_parameters->face_colors.empty()) {
            colors = &feature.step_import_parameters->face_colors;
          }
          break;
        }
        import_colors.emplace(face.owner_id, colors);
      } else {
        colors = cached->second;
      }
      if (colors == nullptr) {
        continue;
      }

      const std::string& face_id = face.face_id;
      const auto separator = face_id.rfind(":face:");
      if (separator == std::string::npos) {
        continue;
      }
      int face_index = -1;
      try {
        face_index = std::stoi(face_id.substr(separator + 6));
      } catch (const std::exception&) {
        continue;
      }
      if (face_index < 0 ||
          static_cast<std::size_t>(face_index) >= colors->size()) {
        continue;
      }
      const std::uint32_t packed = (*colors)[static_cast<std::size_t>(face_index)];
      // High byte set = the file styled this face (see
      // StepImportFeatureParameters::face_colors).
      if ((packed & 0xFF000000u) == 0u) {
        continue;
      }
      char buffer[8];
      std::snprintf(buffer, sizeof(buffer), "#%06X",
                    static_cast<unsigned>(packed & 0x00FFFFFFu));
      face.appearance_color = std::string(buffer);
    }
  }
