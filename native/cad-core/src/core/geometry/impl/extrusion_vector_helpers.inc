gp_Vec extrusion_vector(const std::string& plane_id, double depth) {
  if (plane_id == "ref-plane-xy") {
    return gp_Vec(0.0, 0.0, depth);
  }

  if (plane_id == "ref-plane-yz") {
    return gp_Vec(depth, 0.0, 0.0);
  }

  if (plane_id == "ref-plane-xz") {
    return gp_Vec(0.0, depth, 0.0);
  }

  throw std::runtime_error("Unsupported sketch plane for shape: " + plane_id);
}

gp_Vec extrusion_vector(const PlaneFrame& frame, double depth) {
  return gp_Vec(frame.normal_x * depth,
                frame.normal_y * depth,
                frame.normal_z * depth);
}

gp_Vec extrusion_vector(const ExtrudeFeatureParameters& parameters,
                        double depth) {
  return parameters.plane_frame.has_value()
             ? extrusion_vector(parameters.plane_frame.value(), depth)
             : extrusion_vector(parameters.plane_id, depth);
}

gp_Vec extrusion_normal_vector(const ExtrudeFeatureParameters& parameters) {
  return extrusion_vector(parameters, 1.0);
}
