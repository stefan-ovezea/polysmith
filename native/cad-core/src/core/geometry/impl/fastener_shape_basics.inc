double estimate_fastener_thread_pitch(const FastenerFeatureParameters& parameters) {
  return parameters.pitch > 0.0
             ? parameters.pitch
             : std::max(parameters.diameter * 0.16, 0.2);
}

double modeled_fastener_thread_depth(const FastenerFeatureParameters& parameters) {
  const double major_radius = parameters.diameter * 0.5;
  if (parameters.minor_diameter > 0.0 &&
      parameters.minor_diameter < parameters.diameter) {
    return std::max((parameters.diameter - parameters.minor_diameter) * 0.5,
                    0.02);
  }
  const double pitch = estimate_fastener_thread_pitch(parameters);
  return std::min({std::max(pitch * 0.38, 0.08), major_radius * 0.22});
}

TopoDS_Shape make_fastener_cylinder(double center_x,
                                    double center_y,
                                    double start_z,
                                    double radius,
                                    double height) {
  if (radius <= 0.0 || height <= 0.0) {
    return TopoDS_Shape{};
  }
  return BRepPrimAPI_MakeCylinder(gp_Ax2(gp_Pnt(center_x, center_y, start_z),
                                         gp_Dir(0.0, 0.0, 1.0)),
                                  radius,
                                  height)
      .Shape();
}
