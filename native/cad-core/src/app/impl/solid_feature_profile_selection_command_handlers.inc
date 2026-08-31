  if (command.type == "select_sketch_profile") {
    bool additive = false;
    if (command.payload.contains("additive") &&
        command.payload.at("additive").is_boolean()) {
      additive = command.payload.at("additive").get<bool>();
    }
    // `entity_id` variant: clicking a sketch entity's outline selects
    // the profile(s) whose boundary includes it — the CAM re-pick flow
    // relies on this (interior clicks alone would miss every outline).
    const bool by_entity = command.payload.contains("entity_id") &&
                           !command.payload.at("entity_id").is_null();
    const auto document =
        by_entity
            ? document_manager().select_sketch_profile_by_entity(
                  read_string(command.payload, "entity_id"), additive)
            : document_manager().select_sketch_profile(
                  read_string(command.payload, "profile_id"), additive);

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }
