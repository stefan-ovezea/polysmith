  if (command.type == "pcb_capture_layer_reference") {
    // The UI asks the core to mint the reference an operation will store.
    // The witness comes from the layer's own summary (produced by the
    // importer from the file), never from TypeScript: the renderer picks a
    // row, the core decides what identifies it.
    const std::string feature_id = command.payload.value("feature_id", "");
    const auto document = document_manager().get_document();
    if (!document.has_value()) {
      polysmith::protocol::write_message(polysmith::protocol::make_error_event(
          command.id, "NO_ACTIVE_DOCUMENT", "No active document"));
      return;
    }
    const polysmith::core::FeatureEntry* layer = nullptr;
    for (const auto& feature : document->feature_history) {
      if (feature.id == feature_id) {
        layer = &feature;
        break;
      }
    }
    if (layer == nullptr || !layer->pcb_layer_parameters.has_value()) {
      polysmith::protocol::write_message(polysmith::protocol::make_error_event(
          command.id, "PCB_LAYER_NOT_FOUND",
          "No PCB layer matches " + feature_id + "."));
      return;
    }
    const auto& params = *layer->pcb_layer_parameters;
    polysmith::core::PcbLayerAttestation attestation;
    attestation.layer_feature_id = layer->id;
    attestation.role = polysmith::core::pcb_role_id(params.role);
    attestation.ring_count = params.ring_count;
    attestation.min_x = params.min_x;
    attestation.min_y = params.min_y;
    attestation.max_x = params.max_x;
    attestation.max_y = params.max_y;
    polysmith::protocol::write_message(
        polysmith::protocol::make_pcb_layer_attestation_event(
            command.id,
            {{"persistent_id", layer->id},
             {"attestation", polysmith::protocol::to_payload(attestation)}}));
    return;
  }

  if (command.type == "pcb_import_layer") {
    // The payload's optional `role` is the layer table's explicit override;
    // absent means "resolve from the file's own attributes, then its name".
    const std::optional<std::string> role =
        command.payload.contains("role") && command.payload.at("role").is_string()
            ? std::optional<std::string>(command.payload.at("role").get<std::string>())
            : std::nullopt;
    const auto document = document_manager().import_pcb_layer(
        read_string(command.payload, "file_path"), role);

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "pcb_update_layer") {
    const std::optional<std::string> role =
        command.payload.contains("role") && command.payload.at("role").is_string()
            ? std::optional<std::string>(command.payload.at("role").get<std::string>())
            : std::nullopt;
    const std::optional<bool> mirror_x =
        command.payload.contains("mirror_x") && command.payload.at("mirror_x").is_boolean()
            ? std::optional<bool>(command.payload.at("mirror_x").get<bool>())
            : std::nullopt;
    const auto document = document_manager().update_pcb_layer(
        read_string(command.payload, "feature_id"), role, mirror_x);

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "pcb_remove_layer") {
    const auto document = document_manager().remove_pcb_layer(
        read_string(command.payload, "feature_id"));

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }
