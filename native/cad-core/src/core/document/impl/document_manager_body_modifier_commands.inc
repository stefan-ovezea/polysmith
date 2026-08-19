  // Create a fillet feature on one or more edges of an existing body.
  // The edge ids must already exist in the current viewport_state.edges
  // (the viewport is the source of truth for available edge ids); we
  // parse the body owner out of the first edge id and require every
  // edge to share the same owner. Default radius 1.0mm — the UI panel
  // updates it via update_fillet_radius for live preview.
  DocumentState create_fillet(const std::vector<std::string>& edge_ids,
                              double radius);
  DocumentState update_fillet_radius(const std::string& feature_id,
                                     double radius);
  // Replace the edge set on an existing fillet feature. Used by the
  // floating panel's "edit edges" interaction: the user clicks edges
  // in the viewport while the panel is open and the UI re-issues the
  // full set on each toggle. The new set must be non-empty and every
  // edge must belong to the feature's existing target body — adding
  // edges from a different body has no well-defined fillet semantics.
  DocumentState update_fillet_edges(const std::string& feature_id,
                                    const std::vector<std::string>& edge_ids);
  // Flip an in-progress fillet's `is_pending` flag to false. Called by
  // the UI's panel-Confirm path once the user is done picking edges /
  // dialing in the radius. After this point the body's edge identity
  // follows the post-fillet topology again. Does not push an undo
  // step — the original create_fillet's push covers the entire panel
  // session in one shot.
  DocumentState confirm_fillet(const std::string& feature_id);
  DocumentState create_chamfer(const std::vector<std::string>& edge_ids,
                               double distance);
  DocumentState update_chamfer_distance(const std::string& feature_id,
                                        double distance);
  DocumentState update_chamfer_edges(
      const std::string& feature_id,
      const std::vector<std::string>& edge_ids);
  // See `confirm_fillet` for semantics.
  DocumentState confirm_chamfer(const std::string& feature_id);
  DocumentState create_shell(const std::string& face_id, double thickness);
  DocumentState update_shell_thickness(const std::string& feature_id,
                                       double thickness);
  DocumentState confirm_shell(const std::string& feature_id);
  DocumentState create_hole(const std::string& face_id,
                            double center_x,
                            double center_y,
                            double center_z,
                            const HoleFeatureParameters& parameters);
  DocumentState update_hole_parameters(const std::string& feature_id,
                                       const HoleFeatureParameters& parameters);
  DocumentState confirm_hole(const std::string& feature_id);
  DocumentState create_helix(const std::string& axis_source_id,
                             const HelixFeatureParameters& parameters);
  DocumentState update_helix_parameters(const std::string& feature_id,
                                        const HelixFeatureParameters& parameters);
  DocumentState create_thread(const ThreadFeatureParameters& parameters);
  DocumentState update_thread_parameters(const std::string& feature_id,
                                         const ThreadFeatureParameters& parameters);
  DocumentState confirm_thread(const std::string& feature_id);
  DocumentState create_fastener(const FastenerFeatureParameters& parameters);
  DocumentState update_fastener_parameters(
      const std::string& feature_id,
      const FastenerFeatureParameters& parameters);
  DocumentState create_move(const std::string& target_body_id,
                            const MoveFeatureParameters& parameters);
  DocumentState update_move_parameters(const std::string& feature_id,
                                       const MoveFeatureParameters& parameters);
  DocumentState confirm_move(const std::string& feature_id);
  DocumentState create_body_copy(const std::string& source_body_id,
                                 const std::string& copy_mode);
  DocumentState unlink_body_copy(const std::string& feature_id);
  // Import an STL file as a mesh body. The document stores only the
  // source path (+ scale); the mesh is re-read from disk on every
  // compile and degrades via dependency_broken when the file is gone.
  DocumentState import_stl(const std::string& file_path, double scale = 1.0);
  // Convert a mesh_import body into a regular solid body alongside it.
  DocumentState convert_mesh_to_body(const std::string& source_body_id);
  // Remove every live projection link sourced from this body (face /
  // edge / vertex / body projections). The generated sketch entities
  // stay as fixed lines — only the link goes away, so the body can be
  // deleted without breaking the sketches built on the projection.
  DocumentState detach_body_projections(const std::string& body_id);
  DocumentState set_body_color(const std::string& body_id,
                               const std::string& color);
  DocumentState set_face_color(const std::string& face_id,
                               const std::string& color);
  DocumentState clear_body_color(const std::string& body_id);
  DocumentState clear_face_color(const std::string& face_id);
  DocumentState clear_appearance_overrides();
