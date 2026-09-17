  FeatureEntry make_root_feature();
  void require_document() const;
  // Defaulted std::source_location is evaluated at the CALL SITE, so
  // the step name comes from the mutating method, not from
  // push_undo_state itself.
  void push_undo_state(
      const std::source_location& location = std::source_location::current());
  void clear_redo_stack();
  // Re-resolve face-based plane frames, propagate broken-dependency
  // state, and bump the document revision. Use this from any mutator
  // that can affect upstream body geometry (extrudes, fillets/chamfers,
  // suppress, delete, undo/redo, sketch profile edits, etc.). Pure
  // selection mutators stick with the plain revision bump because the
  // refresh would be a no-op.
  void bump_geometry_revision();
  // Erase sketch-selection ids (singular + plural + text) that no
  // longer exist in any sketch feature. Runs on every geometry revision
  // so deleted or regenerated entities (trim-as-delete, delete
  // dimension/fillet/text, zero-length cleanup, fabricated dim ids,
  // load) can never leave a dangling highlight. Pure selection
  // mutators do not run it.
  void prune_document_selection();
  // Re-derive every sketch id counter from the live feature history
  // (monotonic — never lowers).  Sketch-layer minting (array copies,
  // trims, offsets — all via next_trim_entity_index) bypasses these
  // counters, so a later document-layer mint (fillet / chamfer) would
  // otherwise reuse an id already in the sketch and corrupt wires.
  // Called from bump_geometry_revision — the choke point every
  // mutator passes through.
  void sync_sketch_id_counters();
  // Copies the named stacks (most recent first) into the document's
  // undo_step_names / redo_step_names fields for the payload.
  void sync_undo_step_names();

  int next_document_id_ = 1;
  int next_feature_id_ = 1;
  int next_sketch_line_id_ = 1;
  int next_sketch_circle_id_ = 1;
  int next_sketch_polygon_id_ = 1;
  // Independent counter for arc ids ("arc-N"). Endpoint point ids
  // ("vertex-N") still come from `next_sketch_line_id_` so arc and
  // line endpoints share the same id space, keeping the points
  // graph uniform.
  int next_sketch_arc_id_ = 1;
  int next_sketch_ellipse_id_ = 1;
  int next_sketch_spline_id_ = 1;
  int next_sketch_slot_id_ = 1;
  // Independent counter for sketch fillet ids ("fillet-N"). Trim
  // points and the generated arc still come from the existing line /
  // arc counters so the id namespaces stay uniform.
  int next_sketch_fillet_id_ = 1;
  int next_sketch_chamfer_id_ = 1;
  // Independent counter for the Project tool's standalone projected
  // points ("projected-point-N"). Kept separate from the line / arc
  // endpoint counter so user-visible debug ids stay readable.
  int next_sketch_projected_point_id_ = 1;
  // Counter for `SketchProjection` record ids ("projection-N"). The
  // record itself isn't user-visible (it's an internal link between
  // a body source and the sketch entities it generated), but a
  // monotonic id keeps debugging and serialization predictable.
  int next_sketch_projection_id_ = 1;
  // Counter for sketch text entity ids ("text-N"). The generated glyph
  // lines / vertices use their own deterministic id namespace
  // (line-text-* / vertex-text-*), so they never consume user counters.
  int next_sketch_text_id_ = 1;
  // CAM id counters ("cam-setup-N" / "tool-N" / "cam-op-N"), restored
  // on load by scanning trailing integers like the feature counters.
  int next_cam_setup_id_ = 1;
  int next_cam_tool_id_ = 1;
  int next_cam_op_id_ = 1;
  // Independent counter for drilling point references ("pt-N"),
  // restored on load by scanning machining_regions like the other
  // CAM counters.
  int next_cam_point_ref_id_ = 1;
  int document_count_ = 0;
  std::optional<DocumentState> document_;
  std::vector<DocumentState> undo_stack_;
  std::vector<DocumentState> redo_stack_;
  // Human-readable step names parallel to the stacks ("Add Sketch
  // Line", "Sketch Edit") for the undo/redo tooltips and the history
  // dropdown.  Derived from the pushing method name by default, set
  // explicitly by undo groups.
  std::vector<std::string> undo_names_;
  std::vector<std::string> redo_names_;
  // Configurable history depth (D8).  Oldest entries are purged first.
  int max_undo_steps_ = 30;
  // Open undo groups (D2), outermost first.  While a group is open,
  // pushes land on the main stack above the group's marker and
  // collapse into ONE entry at end_undo_group; undo/redo only walk
  // steps above the marker (per-action undo inside the session).
  struct UndoGroupFrame {
    std::string name;
    DocumentState start_snapshot;
    size_t undo_start_index = 0;
    size_t redo_start_index = 0;
    // Set by mark_undo_group_changed() for mutations that don't push
    // (bookkeeping sweeps).  end_undo_group also compares stack sizes
    // and revisions, so an entry only appears when state changed.
    bool changed = false;
  };
  std::vector<UndoGroupFrame> undo_group_stack_;
