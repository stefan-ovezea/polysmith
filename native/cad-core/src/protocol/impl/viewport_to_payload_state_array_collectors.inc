  json boxes = json::array();
  for (const auto& box : viewport.boxes) {
    boxes.push_back(to_payload(box));
  }
  json solid_faces = json::array();
  for (const auto& face : viewport.solid_faces) {
    solid_faces.push_back(to_payload(face));
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

  json sketch_arcs = json::array();
  for (const auto& arc : viewport.sketch_arcs) {
    sketch_arcs.push_back(to_payload(arc));
  }

  json sketch_points = json::array();
  for (const auto& point : viewport.sketch_points) {
    sketch_points.push_back(to_payload(point));
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

  json meshes = json::array();
  for (const auto& mesh : viewport.meshes) {
    meshes.push_back({
        {"primitive_id", mesh.id},
        {"positions", mesh.positions},
        {"normals", mesh.normals},
        {"indices", mesh.indices},
        {"is_selected", mesh.is_selected},
        {"appearance_color",
         mesh.appearance_color.has_value()
             ? json(mesh.appearance_color.value())
             : json(nullptr)},
    });
  }
