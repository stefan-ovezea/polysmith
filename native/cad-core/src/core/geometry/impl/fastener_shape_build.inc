TopoDS_Shape build_fastener_shape(const FastenerFeatureParameters& parameters) {
  if (parameters.diameter <= 0.0 || parameters.length <= 0.0) {
    throw std::runtime_error("Fastener diameter and length must be greater than zero");
  }
  const double shaft_radius = parameters.diameter * 0.5;
  const bool modeled_threads =
      parameters.thread_representation == "modeled" &&
      std::min(std::max(parameters.thread_length, 0.0), parameters.length) > 0.0;
  const double thread_length =
      std::min(std::max(parameters.thread_length, 0.0), parameters.length);
  TopoDS_Shape shape = make_fastener_cylinder(shaft_radius,
                                              shaft_radius,
                                              0.0,
                                              shaft_radius,
                                              parameters.length);
  TopoDS_Shape threaded_shape;
  TopoDS_Shape unthreaded_shank;
  if (modeled_threads) {
    threaded_shape =
        build_faceted_fastener_threaded_shaft(parameters, shaft_radius, shaft_radius);
    if (thread_length < parameters.length) {
      unthreaded_shank = make_fastener_cylinder(shaft_radius,
                                                shaft_radius,
                                                thread_length,
                                                shaft_radius,
                                                parameters.length - thread_length);
    }
    if (!threaded_shape.IsNull()) {
      shape = threaded_shape;
    }
  }
  if (shape.IsNull()) {
    throw std::runtime_error("Failed to build fastener shaft");
  }

  const double head_radius =
      parameters.head_type == "hex_bolt" ? parameters.diameter : parameters.diameter * 0.85;
  const double head_height =
      parameters.head_type == "flat" ? parameters.diameter * 0.5 : parameters.diameter * 0.65;
  TopoDS_Shape head;
  if (parameters.head_type == "flat") {
    head = BRepPrimAPI_MakeCone(
               gp_Ax2(gp_Pnt(shaft_radius, parameters.length, shaft_radius),
                      gp_Dir(0.0, 1.0, 0.0)),
               shaft_radius,
               head_radius,
               head_height)
               .Shape();
  } else {
    head = BRepPrimAPI_MakeCylinder(
               gp_Ax2(gp_Pnt(shaft_radius, parameters.length, shaft_radius),
                      gp_Dir(0.0, 1.0, 0.0)),
               head_radius,
               head_height)
               .Shape();
  }
  if (modeled_threads && !threaded_shape.IsNull()) {
    head = apply_fastener_drive_cut(head,
                                    parameters,
                                    shaft_radius,
                                    shaft_radius,
                                    head_radius,
                                    head_height);
    return make_shape_compound({threaded_shape, unthreaded_shank, head});
  }
  shape = fuse_shapes(shape, head);
  return apply_fastener_drive_cut(shape,
                                  parameters,
                                  shaft_radius,
                                  shaft_radius,
                                  head_radius,
                                  head_height);
}
