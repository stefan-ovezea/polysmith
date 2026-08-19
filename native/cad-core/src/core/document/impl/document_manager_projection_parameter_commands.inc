  DocumentState project_face_into_sketch(const std::string& face_id);
  // Modal Project tool: dispatch one of these per click depending on
  // what the raycaster hit. Each routes through `project_*_into_sketch`
  // and shares the active-sketch validation + idempotency check (see
  // `projected_sources` / `projected_points`).
  DocumentState project_profile_into_sketch(const std::string& profile_id);
  DocumentState project_edge_into_sketch(const std::string& edge_id);
  DocumentState project_vertex_into_sketch(const std::string& vertex_id);
  // Project a mesh body onto the active sketch plane as fixed sketch
  // lines: "section" = cross-section at the sketch plane, "silhouette"
  // = visible outline projected along the plane normal.
  DocumentState project_body_into_sketch(const std::string& body_id,
                                         const std::string& mode);
  // Parameter CRUD — document-scoped named numeric parameters that can
  // be referenced by name in sketch dimension expressions.
  DocumentState add_parameter(const std::string& name,
                              const std::string& expression,
                              const std::string& kind = "length");
  DocumentState update_parameter(const std::string& name,
                                 const std::string& expression,
                                 const std::string& kind = "length");
  DocumentState delete_parameter(const std::string& name);
