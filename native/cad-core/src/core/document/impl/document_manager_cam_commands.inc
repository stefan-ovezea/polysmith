  // ── CAM ─────────────────────────────────────────────────────
  // Stubs — CAM workspace being rebuilt on cam_types.h types.
  // See session_cam_commands.inc for implementations.

  DocumentState cam_setup_create(const CamSetup& setup);
  std::optional<CamSetup> cam_setup_get() const;
  DocumentState cam_setup_update(const CamSetup& setup);

  DocumentState cam_tool_add(const ToolEntry& tool);
  DocumentState cam_tool_update(const std::string& toolId, const ToolEntry& tool);
  DocumentState cam_tool_delete(const std::string& toolId);
  std::vector<ToolEntry> cam_tool_list() const;

  DocumentState cam_operation_add(const CamOperation& op);
  DocumentState cam_operation_update(const std::string& operation_id, const CamOperation& op);
  DocumentState cam_operation_delete(const std::string& operation_id);
