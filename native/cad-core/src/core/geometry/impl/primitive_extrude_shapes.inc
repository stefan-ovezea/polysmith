TopoDS_Shape build_box_shape(const BoxFeatureParameters& parameters) {
  const TopoDS_Shape shape =
      BRepPrimAPI_MakeBox(parameters.width, parameters.height, parameters.depth)
          .Shape();
  if (shape.IsNull()) {
    throw std::runtime_error("Failed to build box shape");
  }
  return shape;
}

TopoDS_Shape build_cylinder_shape(const CylinderFeatureParameters& parameters) {
  const gp_Ax2 axis(gp_Pnt(0.0, 0.0, 0.0),
                    gp_Dir(0.0, 0.0, 1.0));
  const TopoDS_Shape shape =
      BRepPrimAPI_MakeCylinder(axis, parameters.radius, parameters.height).Shape();
  if (shape.IsNull()) {
    throw std::runtime_error("Failed to build cylinder shape");
  }
  return shape;
}

TopoDS_Shape build_extrude_shape(const ExtrudeFeatureParameters& parameters) {
  try {
    const bool symmetric = parameters.extent_mode == "symmetric";
    const double side1_sign = parameters.depth < 0.0 ? -1.0 : 1.0;
    TopoDS_Shape shape =
        build_extrude_side(parameters, parameters.side1, side1_sign, symmetric);

    if (parameters.extent_mode == "symmetric") {
      shape = fuse_shapes(
          shape,
          build_extrude_side(parameters,
                             parameters.side2.value_or(parameters.side1),
                             -side1_sign,
                             true));
    } else if (parameters.extent_mode == "two_sides") {
      shape = fuse_shapes(
          shape,
          build_extrude_side(parameters,
                             parameters.side2.value_or(parameters.side1),
                             -side1_sign,
                             false));
    }

    if (shape.IsNull()) {
      throw std::runtime_error("Failed to build extrude shape");
    }
    return shape;
  } catch (const Standard_Failure&) {
    throw std::runtime_error(
        "Extrude preview failed. Adjust the extent, taper, or source profile.");
  } catch (const std::exception& e) {
    throw std::runtime_error(
        std::string("Extrude preview failed (OCCT): ") + e.what());
  }
}
