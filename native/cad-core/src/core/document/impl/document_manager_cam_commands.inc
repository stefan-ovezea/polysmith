  // ── CAM ─────────────────────────────────────────────────────
  DocumentState cam_setup_create(const CamSetup& setup);
  std::optional<CamSetup> cam_setup_get() const;
  DocumentState cam_setup_update(const CamSetup& setup);

  DocumentState cam_tool_add(const CamToolDefinition& tool);
  DocumentState cam_tool_update(const std::string& toolId, const CamToolDefinition& tool);
  DocumentState cam_tool_delete(const std::string& toolId);
  std::vector<CamToolDefinition> cam_tool_list() const;

  DocumentState cam_operation_add(const CamOperationEntry& op);
  DocumentState cam_operation_update(const std::string& operation_id, const CamOperationEntry& op);
  DocumentState cam_operation_delete(const std::string& operation_id);
