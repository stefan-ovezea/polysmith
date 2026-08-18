std::string face_owner_id(const std::string& face_id) {
  const auto separator = face_id.find(":face:");
  if (separator == std::string::npos) {
    return "";
  }

  return face_id.substr(0, separator);
}

bool is_supported_sketch_tool(const std::string& tool) {
  return tool == "select" || tool == "line" || tool == "rectangle" ||
         tool == "circle" || tool == "polygon" || tool == "arc" || tool == "fillet" ||
         tool == "trim" || tool == "project" || tool == "dimension" ||
         tool == "move";
}

std::string plane_id_from_frame(
    const SketchFeatureParameters::SketchPlaneFrame& plane_frame) {
  const double abs_x = std::abs(plane_frame.normal_x);
  const double abs_y = std::abs(plane_frame.normal_y);
  const double abs_z = std::abs(plane_frame.normal_z);

  if (abs_x >= abs_y && abs_x >= abs_z) {
    return "ref-plane-yz";
  }

  if (abs_y >= abs_x && abs_y >= abs_z) {
    return "ref-plane-xz";
  }

  return "ref-plane-xy";
}

std::string plane_id_from_frame(const PlaneFrame& plane_frame) {
  const double abs_x = std::abs(plane_frame.normal_x);
  const double abs_y = std::abs(plane_frame.normal_y);
  const double abs_z = std::abs(plane_frame.normal_z);

  if (abs_x >= abs_y && abs_x >= abs_z) {
    return "ref-plane-yz";
  }

  if (abs_y >= abs_x && abs_y >= abs_z) {
    return "ref-plane-xz";
  }

  return "ref-plane-xy";
}

PlaneFrame make_plane_frame(
    const SketchFeatureParameters::SketchPlaneFrame& plane_frame) {
  return PlaneFrame{
      .origin_x = plane_frame.origin_x,
      .origin_y = plane_frame.origin_y,
      .origin_z = plane_frame.origin_z,
      .x_axis_x = plane_frame.x_axis_x,
      .x_axis_y = plane_frame.x_axis_y,
      .x_axis_z = plane_frame.x_axis_z,
      .y_axis_x = plane_frame.y_axis_x,
      .y_axis_y = plane_frame.y_axis_y,
      .y_axis_z = plane_frame.y_axis_z,
      .normal_x = plane_frame.normal_x,
      .normal_y = plane_frame.normal_y,
      .normal_z = plane_frame.normal_z,
  };
}
