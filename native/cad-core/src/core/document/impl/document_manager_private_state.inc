  FeatureEntry make_root_feature();
  void require_document() const;
  void push_undo_state();
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
  // Independent counter for sketch fillet ids ("fillet-N"). Trim
  // points and the generated arc still come from the existing line /
  // arc counters so the id namespaces stay uniform.
  int next_sketch_fillet_id_ = 1;
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
  int document_count_ = 0;
  std::optional<DocumentState> document_;
  std::vector<DocumentState> undo_stack_;
  std::vector<DocumentState> redo_stack_;
