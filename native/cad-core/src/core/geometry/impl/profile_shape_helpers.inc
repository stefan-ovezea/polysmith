TopoDS_Shape make_profile_face(
    const RevolveFeatureParameters& parameters,
    const std::vector<SketchProfilePoint>& outer_points,
    const std::vector<std::vector<SketchProfilePoint>>& inner_loops) {
  BRepBuilderAPI_MakeFace face_builder(
      make_profile_wire(parameters, outer_points));
  for (const auto& loop : inner_loops) {
    if (loop.size() < 3) {
      continue;
    }
    TopoDS_Wire hole_wire = make_profile_wire(parameters, loop);
    hole_wire.Reverse();
    face_builder.Add(hole_wire);
  }

  const TopoDS_Shape face = face_builder.Shape();
  if (face.IsNull()) {
    throw std::runtime_error("Failed to build revolve profile face");
  }
  return face;
}

TopoDS_Shape make_profile_face(
    const SweepFeatureParameters& parameters,
    const std::vector<SketchProfilePoint>& outer_points,
    const std::vector<std::vector<SketchProfilePoint>>& inner_loops) {
  BRepBuilderAPI_MakeFace face_builder(make_profile_wire(parameters, outer_points));
  for (const auto& loop : inner_loops) {
    if (loop.size() < 3) {
      continue;
    }
    TopoDS_Wire hole_wire = make_profile_wire(parameters, loop);
    hole_wire.Reverse();
    face_builder.Add(hole_wire);
  }

  const TopoDS_Shape face = face_builder.Shape();
  if (face.IsNull()) {
    throw std::runtime_error("Failed to build sweep profile face");
  }
  return face;
}

TopoDS_Shape make_polygon_prism_shape(
    const ExtrudeFeatureParameters& parameters,
    const std::vector<SketchProfilePoint>& outer_points,
    const std::vector<std::vector<SketchProfilePoint>>& inner_loops) {
  BRepBuilderAPI_MakeFace face_builder(
      make_profile_wire(parameters, outer_points));
  for (const auto& loop : inner_loops) {
    if (loop.size() < 3) {
      continue;
    }
    TopoDS_Wire hole_wire = make_profile_wire(parameters, loop);
    hole_wire.Reverse();
    face_builder.Add(hole_wire);
  }

  const TopoDS_Shape face = face_builder.Shape();
  if (face.IsNull()) {
    throw std::runtime_error("Failed to build polygon face");
  }

  const gp_Vec direction =
      parameters.plane_frame.has_value()
          ? extrusion_vector(parameters.plane_frame.value(), parameters.depth)
          : extrusion_vector(parameters.plane_id, parameters.depth);

  const TopoDS_Shape shape = BRepPrimAPI_MakePrism(face, direction).Shape();
  if (shape.IsNull()) {
    throw std::runtime_error("Failed to build polygon extrude");
  }

  return shape;
}
