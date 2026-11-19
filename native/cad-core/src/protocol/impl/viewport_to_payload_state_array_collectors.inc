  json boxes = json::array();
  for (const auto& box : viewport.boxes) {
    boxes.push_back(to_payload(box));
  }
  json solid_faces = to_payload(viewport.solid_faces);

  // Selection moves without the revision changing, so it travels on its own: a
  // click that changes no geometry sends three id arrays instead of the scene.
  json selected_faces = json::array();
  for (const auto& face : viewport.solid_faces) {
    if (face.is_selected) {
      selected_faces.push_back(face.face_id);
    }
  }


  json cylinders = json::array();
  for (const auto& cylinder : viewport.cylinders) {
    cylinders.push_back(to_payload(cylinder));
  }

  json polygon_extrudes = json::array();
  for (const auto& polygon_extrude : viewport.polygon_extrudes) {
    polygon_extrudes.push_back(to_payload(polygon_extrude));
  }

  json reference_planes = json::array();
  for (const auto& plane : viewport.reference_planes) {
    reference_planes.push_back(to_payload(plane));
  }

  json reference_axes = json::array();
  for (const auto& axis : viewport.reference_axes) {
    reference_axes.push_back(to_payload(axis));
  }

  json reference_points = json::array();
  for (const auto& point : viewport.reference_points) {
    reference_points.push_back(to_payload(point));
  }

  json helices = json::array();
  for (const auto& helix : viewport.helices) {
    helices.push_back(to_payload(helix));
  }

  json sketch_lines = json::array();
  for (const auto& line : viewport.sketch_lines) {
    sketch_lines.push_back(to_payload(line));
  }

  json sketch_circles = json::array();
  for (const auto& circle : viewport.sketch_circles) {
    sketch_circles.push_back(to_payload(circle));
  }

  json sketch_polygons = json::array();
  for (const auto& polygon : viewport.sketch_polygons) {
    sketch_polygons.push_back(to_payload(polygon));
  }

  json sketch_ellipses = json::array();
  for (const auto& ellipse : viewport.sketch_ellipses) {
    sketch_ellipses.push_back(to_payload(ellipse));
  }
  json sketch_splines = json::array();
  for (const auto& spline : viewport.sketch_splines) {
    sketch_splines.push_back(to_payload(spline));
  }

  json sketch_arcs = json::array();
  for (const auto& arc : viewport.sketch_arcs) {
    sketch_arcs.push_back(to_payload(arc));
  }

  json sketch_vertices = json::array();
  for (const auto& point : viewport.sketch_vertices) {
    sketch_vertices.push_back(to_payload(point));
  }

  json sketch_dimensions = json::array();
  for (const auto& dimension : viewport.sketch_dimensions) {
    sketch_dimensions.push_back(to_payload(dimension));
  }

  json sketch_constraints = json::array();
  for (const auto& constraint : viewport.sketch_constraints) {
    sketch_constraints.push_back(to_payload(constraint));
  }

  json sketch_profiles = json::array();
  for (const auto& profile : viewport.sketch_profiles) {
    sketch_profiles.push_back(to_payload(profile));
  }

  // Mesh delta. A body's fingerprint is a cheap sample hash of its geometry:
  // sizes plus every 64th coordinate, which catches a retessellation and a
  // moved body (same triangle count, different vertices). The client sends the
  // fingerprints it holds, so a selection or a sketch edit - neither of which
  // changes a triangle - transfers no triangles at all.
  const auto mesh_fingerprint = [](const polysmith::core::ViewportMeshPrimitive& mesh) {
    std::uint64_t hash = 1469598103934665603ull;
    const auto mix = [&hash](std::uint64_t value) {
      hash ^= value;
      hash *= 1099511628211ull;
    };
    mix(mesh.positions.size());
    mix(mesh.indices.size());
    mix(mesh.normals.size());
    mix(mesh.face_ids.size());
    const auto sample = [&mix](const std::vector<double>& values) {
      if (values.empty()) {
        return;
      }
      const std::size_t step = std::max<std::size_t>(1, values.size() / 64);
      for (std::size_t index = 0; index < values.size(); index += step) {
        std::uint64_t bits = 0;
        std::memcpy(&bits, &values[index], sizeof(double));
        mix(bits);
      }
    };
    sample(mesh.positions);
    sample(mesh.normals);
    char buffer[17];
    std::snprintf(buffer, sizeof(buffer), "%016llx",
                  static_cast<unsigned long long>(hash));
    return std::string(buffer);
  };

  json meshes = json::array();
  json mesh_revisions = json::object();
  json body_states = json::object();
  std::set<std::string> current_mesh_ids;
  for (const auto& mesh : viewport.meshes) {
    current_mesh_ids.insert(mesh.id);
    // Selection and colour are tiny and change without the geometry, so they
    // travel separately: a click updates ody_states and sends no triangles.
    body_states[mesh.id] = {
        {"is_selected", mesh.is_selected},
        {"appearance_color",
         mesh.appearance_color.has_value()
             ? json(mesh.appearance_color.value())
             : json(nullptr)},
    };
    const std::string fingerprint = mesh_fingerprint(mesh);
    mesh_revisions[mesh.id] = fingerprint;
    const auto known = known_mesh_revisions.find(mesh.id);
    if (known != known_mesh_revisions.end() && known->second == fingerprint) {
      continue;  // the client already has exactly these triangles
    }
    meshes.push_back({
        {"primitive_id", mesh.id},
        // Binary (phase E4): float32/uint32 base64 blobs decode straight into
        // Float32Array/Uint32Array on the UI, 3-5x smaller than JSON number
        // arrays.  The field names are unchanged — the UI treats a string as
        // base64 and a number array as the legacy JSON form.
        {"positions", encode_f32(mesh.positions)},
        {"normals", encode_f32(mesh.normals)},
        {"indices", encode_u32(mesh.indices)},
        // Pick map: one face ordinal per triangle, so the UI builds its pick
        // mesh from THIS geometry instead of a second copy of the triangles.
        {"face_ids", encode_u32(mesh.face_ids)},
        // Empty for single-colour bodies; the UI then uses the material
        // colour. Per-vertex colours carry the file's face styles.
        {"colors", encode_f32(mesh.colors)},
        {"is_selected", mesh.is_selected},
        {"appearance_color",
         mesh.appearance_color.has_value()
             ? json(mesh.appearance_color.value())
             : json(nullptr)},
    });
  }
  json removed_meshes = json::array();
  for (const auto& [known_id, revision] : known_mesh_revisions) {
    (void)revision;
    if (current_mesh_ids.find(known_id) == current_mesh_ids.end()) {
      removed_meshes.push_back(known_id);
    }
  }
