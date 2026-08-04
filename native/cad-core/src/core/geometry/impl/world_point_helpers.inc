gp_Pnt to_world_point(const std::string& plane_id, double local_x, double local_y) {
  if (plane_id == "ref-plane-xy") {
    return gp_Pnt(local_x, local_y, 0.0);
  }

  if (plane_id == "ref-plane-yz") {
    return gp_Pnt(0.0, local_x, local_y);
  }

  if (plane_id == "ref-plane-xz") {
    return gp_Pnt(local_x, 0.0, local_y);
  }

  throw std::runtime_error("Unsupported sketch plane for shape: " + plane_id);
}

gp_Pnt to_world_point(const PlaneFrame& frame, double local_x, double local_y) {
  return gp_Pnt(frame.origin_x + frame.x_axis_x * local_x +
                    frame.y_axis_x * local_y,
                frame.origin_y + frame.x_axis_y * local_x +
                    frame.y_axis_y * local_y,
                frame.origin_z + frame.x_axis_z * local_x +
                    frame.y_axis_z * local_y);
}

gp_Pnt to_world_point(const ExtrudeFeatureParameters& parameters,
                      double local_x,
                      double local_y) {
  if (parameters.plane_frame.has_value()) {
    return to_world_point(parameters.plane_frame.value(), local_x, local_y);
  }
  return to_world_point(parameters.plane_id, local_x, local_y);
}

gp_Pnt to_world_point(const LoftSectionParameters& section,
                      double local_x,
                      double local_y) {
  if (section.plane_frame.has_value()) {
    return to_world_point(section.plane_frame.value(), local_x, local_y);
  }
  return to_world_point(section.plane_id, local_x, local_y);
}

gp_Pnt to_world_point(const RevolveFeatureParameters& parameters,
                      double local_x,
                      double local_y) {
  if (parameters.plane_frame.has_value()) {
    return to_world_point(parameters.plane_frame.value(), local_x, local_y);
  }
  return to_world_point(parameters.plane_id, local_x, local_y);
}

gp_Pnt to_world_point(const SweepFeatureParameters& parameters,
                      double local_x,
                      double local_y) {
  if (parameters.plane_frame.has_value()) {
    return to_world_point(parameters.plane_frame.value(), local_x, local_y);
  }
  return to_world_point(parameters.plane_id, local_x, local_y);
}
