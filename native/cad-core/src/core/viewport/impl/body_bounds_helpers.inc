BodyBounds bounds_for_shape(const TopoDS_Shape& shape) {
  if (shape.IsNull()) {
    return BodyBounds{};
  }
  try {
    Bnd_Box bounds;
    BRepBndLib::Add(shape, bounds);
    if (bounds.IsVoid()) {
      return BodyBounds{};
    }
    double min_x = 0.0;
    double min_y = 0.0;
    double min_z = 0.0;
    double max_x = 0.0;
    double max_y = 0.0;
    double max_z = 0.0;
    bounds.Get(min_x, min_y, min_z, max_x, max_y, max_z);
    return BodyBounds{
        .center_x = (min_x + max_x) * 0.5,
        .center_y = (min_y + max_y) * 0.5,
        .center_z = (min_z + max_z) * 0.5,
        .width = max_x - min_x,
        .height = max_y - min_y,
        .depth = max_z - min_z,
    };
  } catch (const std::exception&) {
    return BodyBounds{};
  }
}
