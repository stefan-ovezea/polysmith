gp_Dir plane_normal(const ExtrudeFeatureParameters& parameters) {
  if (parameters.plane_frame.has_value()) {
    const auto& frame = parameters.plane_frame.value();
    return gp_Dir(frame.normal_x, frame.normal_y, frame.normal_z);
  }
  if (parameters.plane_id == "ref-plane-xy") {
    return gp_Dir(0.0, 1.0, 0.0);
  }
  if (parameters.plane_id == "ref-plane-yz") {
    return gp_Dir(1.0, 0.0, 0.0);
  }
  if (parameters.plane_id == "ref-plane-xz") {
    return gp_Dir(0.0, 0.0, 1.0);
  }
  throw std::runtime_error("Unsupported sketch plane for shape: " +
                           parameters.plane_id);
}

gp_Dir profile_wire_normal(const LoftSectionParameters& section) {
  if (section.plane_frame.has_value()) {
    const auto& frame = section.plane_frame.value();
    const double x = frame.x_axis_y * frame.y_axis_z -
                     frame.x_axis_z * frame.y_axis_y;
    const double y = frame.x_axis_z * frame.y_axis_x -
                     frame.x_axis_x * frame.y_axis_z;
    const double z = frame.x_axis_x * frame.y_axis_y -
                     frame.x_axis_y * frame.y_axis_x;
    return gp_Dir(x, y, z);
  }
  if (section.plane_id == "ref-plane-xy") {
    return gp_Dir(0.0, -1.0, 0.0);
  }
  if (section.plane_id == "ref-plane-yz") {
    return gp_Dir(1.0, 0.0, 0.0);
  }
  if (section.plane_id == "ref-plane-xz") {
    return gp_Dir(0.0, 0.0, 1.0);
  }
  throw std::runtime_error("Unsupported sketch plane for loft: " +
                           section.plane_id);
}

gp_Dir profile_wire_normal(const ExtrudeFeatureParameters& parameters) {
  if (parameters.plane_frame.has_value()) {
    const auto& frame = parameters.plane_frame.value();
    const double x = frame.x_axis_y * frame.y_axis_z -
                     frame.x_axis_z * frame.y_axis_y;
    const double y = frame.x_axis_z * frame.y_axis_x -
                     frame.x_axis_x * frame.y_axis_z;
    const double z = frame.x_axis_x * frame.y_axis_y -
                     frame.x_axis_y * frame.y_axis_x;
    return gp_Dir(x, y, z);
  }
  if (parameters.plane_id == "ref-plane-xy") {
    // Sketch-local coordinates on XY are mapped as (x, z) in world space.
    // The local wire normal is therefore X x Z = -Y, while extrusion still
    // uses +Y. Exact circle wires must follow the local wire orientation so
    // hole reversal matches polygon loops built from the same sketch points.
    return gp_Dir(0.0, -1.0, 0.0);
  }
  if (parameters.plane_id == "ref-plane-yz") {
    return gp_Dir(1.0, 0.0, 0.0);
  }
  if (parameters.plane_id == "ref-plane-xz") {
    return gp_Dir(0.0, 0.0, 1.0);
  }
  throw std::runtime_error("Unsupported sketch plane for shape: " +
                           parameters.plane_id);
}

gp_Dir profile_wire_normal(const RevolveFeatureParameters& parameters) {
  if (parameters.plane_frame.has_value()) {
    const auto& frame = parameters.plane_frame.value();
    const double x = frame.x_axis_y * frame.y_axis_z -
                     frame.x_axis_z * frame.y_axis_y;
    const double y = frame.x_axis_z * frame.y_axis_x -
                     frame.x_axis_x * frame.y_axis_z;
    const double z = frame.x_axis_x * frame.y_axis_y -
                     frame.x_axis_y * frame.y_axis_x;
    return gp_Dir(x, y, z);
  }
  if (parameters.plane_id == "ref-plane-xy") {
    return gp_Dir(0.0, -1.0, 0.0);
  }
  if (parameters.plane_id == "ref-plane-yz") {
    return gp_Dir(1.0, 0.0, 0.0);
  }
  if (parameters.plane_id == "ref-plane-xz") {
    return gp_Dir(0.0, 0.0, 1.0);
  }
  throw std::runtime_error("Unsupported sketch plane for revolve: " +
                           parameters.plane_id);
}

gp_Dir profile_wire_normal(const SweepFeatureParameters& parameters) {
  if (parameters.plane_frame.has_value()) {
    const auto& frame = parameters.plane_frame.value();
    const double x = frame.x_axis_y * frame.y_axis_z -
                     frame.x_axis_z * frame.y_axis_y;
    const double y = frame.x_axis_z * frame.y_axis_x -
                     frame.x_axis_x * frame.y_axis_z;
    const double z = frame.x_axis_x * frame.y_axis_y -
                     frame.x_axis_y * frame.y_axis_x;
    return gp_Dir(x, y, z);
  }
  if (parameters.plane_id == "ref-plane-xy") {
    return gp_Dir(0.0, -1.0, 0.0);
  }
  if (parameters.plane_id == "ref-plane-yz") {
    return gp_Dir(1.0, 0.0, 0.0);
  }
  if (parameters.plane_id == "ref-plane-xz") {
    return gp_Dir(0.0, 0.0, 1.0);
  }
  throw std::runtime_error("Unsupported sketch plane for sweep: " +
                           parameters.plane_id);
}
