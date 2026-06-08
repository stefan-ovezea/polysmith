  DocumentState start_sketch_on_plane(const std::string& reference_id);
  DocumentState start_sketch_on_face(
      const std::string& face_id,
      const SketchFeatureParameters::SketchPlaneFrame& plane_frame);
  DocumentState set_sketch_tool(const std::string& tool);
  // Mirror tool — contextual modeling pending preview lifecycle. Each
  // method maps directly onto the sketch_feature core ops; the
  // wrapping here is bookkeeping (undo, refresh, selection).
  // Note: only `commit` writes a permanent change, so it's the
  // only one that pushes an undo state.
  DocumentState start_mirror_preview();
  DocumentState update_mirror_preview_axis(const std::string& axis_line_id);
  DocumentState update_mirror_preview_objects(
      const std::vector<std::string>& object_ids);
  DocumentState commit_mirror_preview(bool persistent = false);
  DocumentState cancel_mirror_preview();
  DocumentState finish_sketch();
  DocumentState reenter_sketch(const std::string& feature_id);
