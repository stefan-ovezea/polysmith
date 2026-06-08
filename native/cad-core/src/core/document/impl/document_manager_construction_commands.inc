  // Create a parametric offset construction plane. The source must
  // resolve via `resolve_plane_source_frame` (origin plane,
  // construction plane feature id, or "<body_id>:face:<index>"
  // planar face). The resulting feature is selected and shows up in
  // the timeline and the Construction hierarchy category.
  DocumentState create_offset_plane(const std::string& source_plane_id,
                                    double offset);
  DocumentState create_midplane(const std::string& first_source_id,
                                const std::string& second_source_id);
  DocumentState create_tangent_plane(const std::string& source_face_id);
  DocumentState create_angle_plane(const std::string& source_plane_id,
                                   const std::string& source_axis_id,
                                   double angle_degrees);
  DocumentState create_construction_axis(const std::string& source_id);
  DocumentState create_construction_point(const std::string& source_id);
  // Drive an existing construction plane's offset. The frame is
  // re-derived from the source's current frame, so chained planes
  // / face-based sources update correctly.
  DocumentState update_offset_plane(const std::string& feature_id,
                                    double offset);
  DocumentState update_angle_plane(const std::string& feature_id,
                                   double angle_degrees);
