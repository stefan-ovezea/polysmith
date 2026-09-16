  DocumentState select_feature(const std::string& feature_id);
  DocumentState select_reference(const std::string& reference_id);
  DocumentState select_face(const std::string& face_id);
  // Select an edge. When `additive` is false the edge replaces the
  // current edge selection (the typical click). When true, the edge
  // is toggled into the current selection set (shift-click): if it's
  // already selected it is removed, otherwise appended. Other
  // selection categories (face / vertex / reference / feature) are
  // always cleared, matching the single-edge legacy behavior so a new
  // edge selection doesn't leave a stale face highlight.
  DocumentState select_edge(const std::string& edge_id, bool additive);
  // Same toggle / replace semantics as select_edge: shift-click adds
  // (or removes) the vertex from the current vertex selection set;
  // plain click replaces. Other selection categories are cleared.
  DocumentState select_vertex(const std::string& vertex_id, bool additive);
  DocumentState select_sketch_profile(const std::string& profile_id,
                                      bool additive = false);
  // Selects every profile whose boundary includes the given sketch
  // entity (line/circle/arc/ellipse/spline).  A shared edge between
  // two regions selects both — no guessing.  Throws when the entity
  // does not exist or lies on no profile boundary (e.g. construction
  // lines, interior entities).
  // `smallest_only`: a shared outline (e.g. a spoke arc that is also
  // a plate-hole edge) then selects only the smallest owning region —
  // the CAM re-pick flow uses this so an outline click behaves like
  // an interior click instead of toggling every region sharing the
  // boundary.
  DocumentState select_sketch_profile_by_entity(const std::string& entity_id,
                                                bool additive = false,
                                                bool smallest_only = false);
  DocumentState select_sketch_vertex(const std::string& vertex_id,
                                    bool additive = false);
  DocumentState select_sketch_entity(const std::string& entity_id,
                                     bool additive = false);
  // Batch marquee selection: replaces (or toggles per-id in additive
  // mode) the sketch entity selection with ALL ids in ONE command —
  // one revision bump, one document payload. Unknown ids are skipped
  // with a warning instead of aborting the batch (the UI can marquee
  // over a stale scene snapshot).
  DocumentState select_sketch_entities(
      const std::vector<std::string>& entity_ids, bool additive = false);
  // Marquee selection, resolved CORE-side against the exact sketch
  // geometry: selects every entity inside (window mode) or touching
  // (crossing mode) the rectangle. `x1/y1/x2/y2` are sketch-local
  // coordinates of the two drag corners; `window_mode` is the SCREEN
  // drag direction (left->right), passed by the UI so the semantics
  // don't flip when the view is mirrored. The old UI-side screen-space
  // collection could mis-collect against stale scene data — the
  // classic "marquee then Delete removed the perimeter" bug.
  DocumentState select_sketch_rect(double x1, double y1, double x2,
                                   double y2, bool window_mode,
                                   bool additive = false);
  DocumentState select_sketch_dimension(const std::string& dimension_id);
  DocumentState clear_selection();
  // Update the per-session selection filter. The filter controls which
  // geometric element types are visible, selectable, snappable, and
  // constrainable. Stored on DocumentState for persistence in saved
  // files, but in v1 it's also settable per-session from the UI.
  DocumentState update_selection_filter(const SelectionFilter& filter);
