  if (command.type == "select_sketch_profiles") {
    // Rubber-band over surfaces: one command, one revision, one small event.
    std::vector<std::string> profile_ids;
    if (command.payload.contains("profile_ids") &&
        command.payload.at("profile_ids").is_array()) {
      for (const auto& entry : command.payload.at("profile_ids")) {
        if (entry.is_string()) {
          profile_ids.push_back(entry.get<std::string>());
        }
      }
    }
    bool additive = false;
    if (command.payload.contains("additive") &&
        command.payload.at("additive").is_boolean()) {
      additive = command.payload.at("additive").get<bool>();
    }
    const auto document =
        document_manager().select_sketch_profiles(profile_ids, additive);

    polysmith::protocol::write_message(
        polysmith::protocol::make_selection_changed_event(
            command.id, polysmith::protocol::to_selection_payload(document)));
    return;
  }

  if (command.type == "select_sketch_profile") {
    bool additive = false;
    if (command.payload.contains("additive") &&
        command.payload.at("additive").is_boolean()) {
      additive = command.payload.at("additive").get<bool>();
    }
    // `entity_id` variant: clicking a sketch entity's outline selects
    // the profile(s) whose boundary includes it — the CAM re-pick flow
    // relies on this (interior clicks alone would miss every outline).
    // `smallest_only`: a shared outline (spoke arc that is also a
    // plate-hole edge) then selects only the smallest owning region,
    // matching the interior-click behaviour the CAM re-pick expects.
    const bool by_entity = command.payload.contains("entity_id") &&
                           !command.payload.at("entity_id").is_null();
    bool smallest_only = false;
    if (command.payload.contains("smallest_only") &&
        command.payload.at("smallest_only").is_boolean()) {
      smallest_only = command.payload.at("smallest_only").get<bool>();
    }
    const auto document =
        by_entity
            ? document_manager().select_sketch_profile_by_entity(
                  read_string(command.payload, "entity_id"), additive,
                  smallest_only)
            : document_manager().select_sketch_profile(
                  read_string(command.payload, "profile_id"), additive);

    polysmith::protocol::write_message(
        polysmith::protocol::make_selection_changed_event(command.id, polysmith::protocol::to_selection_payload(document)));
    return;
  }
