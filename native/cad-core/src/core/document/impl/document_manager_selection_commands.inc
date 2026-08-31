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
  DocumentState select_sketch_profile_by_entity(const std::string& entity_id,
                                                bool additive = false);
  DocumentState select_sketch_vertex(const std::string& vertex_id,
                                    bool additive = false);
  DocumentState select_sketch_entity(const std::string& entity_id,
                                     bool additive = false);
  DocumentState select_sketch_dimension(const std::string& dimension_id);
  DocumentState clear_selection();
  // Update the per-session selection filter. The filter controls which
  // geometric element types are visible, selectable, snappable, and
  // constrainable. Stored on DocumentState for persistence in saved
  // files, but in v1 it's also settable per-session from the UI.
  DocumentState update_selection_filter(const SelectionFilter& filter);
