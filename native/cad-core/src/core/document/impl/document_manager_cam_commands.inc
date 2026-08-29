  // ── CAM ─────────────────────────────────────────────────────
  // CAM state lives in DocumentState::cam.  Implementations in
  // session_cam_commands.inc.  Mutators follow the canonical document
  // shape (require → validate → push undo → mutate → bump revision).

  DocumentState cam_setup_create(const CamSetup& setup);
  std::optional<CamSetup> cam_setup_get() const;
  DocumentState cam_setup_update(const CamSetup& setup);
  DocumentState cam_machine_settings_set(
      const LaserMachineSettings& machine_settings);

  DocumentState cam_tool_add(const ToolEntry& tool);
  DocumentState cam_tool_update(const std::string& toolId, const ToolEntry& tool);
  DocumentState cam_tool_delete(const std::string& toolId);
  std::vector<ToolEntry> cam_tool_list() const;

  DocumentState cam_operation_add(const CamOperation& op);
  DocumentState cam_operation_update(const std::string& operation_id, const CamOperation& op);
  DocumentState cam_operation_delete(const std::string& operation_id);
  // Re-targets an operation at the given sketch: captures fresh
  // TNP-safe witnesses for every profile of that sketch into
  // machining_regions.  Throws when the operation is unknown or the
  // sketch feature id does not resolve (never silently keeps stale
  // geometry).
  DocumentState cam_operation_set_scope(const std::string& operation_id,
                                        const std::string& sketch_feature_id);

  DocumentState cam_post_processor_set(const PostProcessor& post_processor);
  DocumentState cam_operation_set_generated(const std::string& operation_id,
                                            const CamOperation& op);
