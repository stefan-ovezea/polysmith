  // ── CAM commands ──────────────────────────────────────────────
  //
  // CAM workspace is being rebuilt on new target-schema types
  // (cam_types.h).  The old command handlers used legacy types that
  // no longer exist.  These stubs accept the commands and return
  // the current document state so the UI doesn't hang, but no CAM
  // state is mutated yet.

  if (command.type == "cam_operation_create" ||
      command.type == "cam_operation_update" ||
      command.type == "cam_operation_delete" ||
      command.type == "cam_setup_create" ||
      command.type == "cam_setup_get" ||
      command.type == "cam_setup_update" ||
      command.type == "cam_tool_add" ||
      command.type == "cam_tool_list" ||
      command.type == "cam_tool_delete" ||
      command.type == "cam_stock_set" ||
      command.type == "cam_stock_get") {
    const auto document = document_manager().get_document().value_or(
        core::DocumentState{});
    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }
