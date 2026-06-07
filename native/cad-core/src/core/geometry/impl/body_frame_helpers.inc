constexpr double kLinearDeflection = 0.1;
constexpr double kAngularDeflection = 0.5;
constexpr double kPi = 3.14159265358979323846;

struct ParsedEdgeId {
  std::string body_id;
  int index = -1;
};

struct ThreadAxis {
  gp_Pnt start;
  gp_Dir direction;
  double length = 0.0;
};

BodyLocalFrame default_body_local_frame() {
  return BodyLocalFrame{};
}

BodyLocalFrame frame_from_copy_parameters(
    const BodyCopyFeatureParameters& params) {
  BodyLocalFrame frame{};
  frame.x_axis_x = params.local_x_axis_x;
  frame.x_axis_y = params.local_x_axis_y;
  frame.x_axis_z = params.local_x_axis_z;
  frame.y_axis_x = params.local_y_axis_x;
  frame.y_axis_y = params.local_y_axis_y;
  frame.y_axis_z = params.local_y_axis_z;
  frame.z_axis_x = params.local_z_axis_x;
  frame.z_axis_y = params.local_z_axis_y;
  frame.z_axis_z = params.local_z_axis_z;
  return frame;
}

TopoDS_Shape deserialize_shape_snapshot(const std::string& serialized_shape) {
  if (serialized_shape.empty()) {
    return TopoDS_Shape{};
  }
  std::istringstream stream(serialized_shape);
  BRep_Builder builder;
  TopoDS_Shape shape;
  try {
    BRepTools::Read(shape, stream, builder);
  } catch (const Standard_Failure&) {
    return TopoDS_Shape{};
  }
  return shape;
}

BodyLocalFrame frame_from_axes(const gp_Vec& raw_x,
                               const gp_Vec& raw_y,
                               const gp_Vec& raw_z) {
  gp_Vec x = raw_x;
  gp_Vec y = raw_y;
  gp_Vec z = raw_z;
  if (x.SquareMagnitude() <= 1.0e-18) {
    x = gp_Vec(1.0, 0.0, 0.0);
  }
  x.Normalize();
  if (y.SquareMagnitude() <= 1.0e-18 ||
      std::abs(x.Normalized().Dot(y.Normalized())) > 0.999) {
    y = gp_Vec(0.0, 1.0, 0.0);
    if (std::abs(x.Dot(y)) > 0.999) {
      y = gp_Vec(0.0, 0.0, 1.0);
    }
  }
  y = y - x.Multiplied(y.Dot(x));
  if (y.SquareMagnitude() <= 1.0e-18) {
    y = gp_Vec(0.0, 1.0, 0.0);
  }
  y.Normalize();
  if (z.SquareMagnitude() <= 1.0e-18) {
    z = x.Crossed(y);
  }
  if (z.SquareMagnitude() <= 1.0e-18) {
    z = gp_Vec(0.0, 0.0, 1.0);
  }
  z.Normalize();
  y = z.Crossed(x);
  if (y.SquareMagnitude() <= 1.0e-18) {
    y = gp_Vec(0.0, 1.0, 0.0);
  }
  y.Normalize();

  return BodyLocalFrame{
      .x_axis_x = x.X(),
      .x_axis_y = x.Y(),
      .x_axis_z = x.Z(),
      .y_axis_x = y.X(),
      .y_axis_y = y.Y(),
      .y_axis_z = y.Z(),
      .z_axis_x = z.X(),
      .z_axis_y = z.Y(),
      .z_axis_z = z.Z(),
  };
}

BodyLocalFrame frame_from_plane_frame(const PlaneFrame& frame) {
  return frame_from_axes(gp_Vec(frame.x_axis_x, frame.x_axis_y, frame.x_axis_z),
                         gp_Vec(frame.y_axis_x, frame.y_axis_y, frame.y_axis_z),
                         gp_Vec(frame.normal_x, frame.normal_y, frame.normal_z));
}

BodyLocalFrame frame_from_plane_id(const std::string& plane_id) {
  if (plane_id == "ref-plane-xy") {
    return frame_from_axes(gp_Vec(1.0, 0.0, 0.0),
                           gp_Vec(0.0, 0.0, 1.0),
                           gp_Vec(0.0, 1.0, 0.0));
  }
  if (plane_id == "ref-plane-yz") {
    return frame_from_axes(gp_Vec(0.0, 1.0, 0.0),
                           gp_Vec(0.0, 0.0, 1.0),
                           gp_Vec(1.0, 0.0, 0.0));
  }
  if (plane_id == "ref-plane-xz") {
    return frame_from_axes(gp_Vec(1.0, 0.0, 0.0),
                           gp_Vec(0.0, 1.0, 0.0),
                           gp_Vec(0.0, 0.0, 1.0));
  }
  return default_body_local_frame();
}

BodyLocalFrame initial_frame_for_feature(const FeatureEntry& feature) {
  if (feature.kind == "extrude" && feature.extrude_parameters.has_value()) {
    const auto& params = feature.extrude_parameters.value();
    return params.plane_frame.has_value()
               ? frame_from_plane_frame(params.plane_frame.value())
               : frame_from_plane_id(params.plane_id);
  }
  if (feature.kind == "loft" && feature.loft_parameters.has_value() &&
      !feature.loft_parameters->sections.empty()) {
    const auto& section = feature.loft_parameters->sections.front();
    return section.plane_frame.has_value()
               ? frame_from_plane_frame(section.plane_frame.value())
               : frame_from_plane_id(section.plane_id);
  }
  if (feature.kind == "revolve" && feature.revolve_parameters.has_value()) {
    const auto& params = feature.revolve_parameters.value();
    return params.plane_frame.has_value()
               ? frame_from_plane_frame(params.plane_frame.value())
               : frame_from_plane_id(params.plane_id);
  }
  if (feature.kind == "sweep" && feature.sweep_parameters.has_value()) {
    const auto& params = feature.sweep_parameters.value();
    return params.plane_frame.has_value()
               ? frame_from_plane_frame(params.plane_frame.value())
               : frame_from_plane_id(params.plane_id);
  }
  return default_body_local_frame();
}

gp_Vec frame_x_axis(const BodyLocalFrame& frame) {
  return gp_Vec(frame.x_axis_x, frame.x_axis_y, frame.x_axis_z);
}

gp_Vec frame_y_axis(const BodyLocalFrame& frame) {
  return gp_Vec(frame.y_axis_x, frame.y_axis_y, frame.y_axis_z);
}

gp_Vec frame_z_axis(const BodyLocalFrame& frame) {
  return gp_Vec(frame.z_axis_x, frame.z_axis_y, frame.z_axis_z);
}

gp_Pnt shape_bounds_center(const TopoDS_Shape& shape) {
  if (shape.IsNull()) {
    return gp_Pnt(0.0, 0.0, 0.0);
  }
  Bnd_Box bounds;
  try {
    BRepBndLib::Add(shape, bounds);
    if (bounds.IsVoid()) {
      return gp_Pnt(0.0, 0.0, 0.0);
    }
    double min_x = 0.0;
    double min_y = 0.0;
    double min_z = 0.0;
    double max_x = 0.0;
    double max_y = 0.0;
    double max_z = 0.0;
    bounds.Get(min_x, min_y, min_z, max_x, max_y, max_z);
    return gp_Pnt((min_x + max_x) * 0.5,
                  (min_y + max_y) * 0.5,
                  (min_z + max_z) * 0.5);
  } catch (const std::exception&) {
    return gp_Pnt(0.0, 0.0, 0.0);
  }
}

void transform_frame_axes(BodyLocalFrame& frame, const gp_Trsf& transform) {
  gp_Vec x = frame_x_axis(frame);
  gp_Vec y = frame_y_axis(frame);
  gp_Vec z = frame_z_axis(frame);
  x.Transform(transform);
  y.Transform(transform);
  z.Transform(transform);
  frame = frame_from_axes(x, y, z);
}

TopoDS_Shape apply_transform(const TopoDS_Shape& shape, const gp_Trsf& transform) {
  if (shape.IsNull()) {
    return shape;
  }
  try {
    const TopoDS_Shape transformed =
        BRepBuilderAPI_Transform(shape, transform, true).Shape();
    return transformed.IsNull() ? shape : transformed;
  } catch (const Standard_Failure&) {
    return shape;
  } catch (const std::exception&) {
    return shape;
  }
}

TopoDS_Shape apply_move(const TopoDS_Shape& body_shape,
                        BodyLocalFrame& frame,
                        const MoveFeatureParameters& params) {
  if (body_shape.IsNull()) {
    return body_shape;
  }

  TopoDS_Shape next_shape = body_shape;
  BodyLocalFrame next_frame = frame;
  const gp_Pnt pivot = shape_bounds_center(body_shape);

  const auto rotate_about_axis = [&](const gp_Vec& axis, double degrees) {
    if (std::abs(degrees) <= 1.0e-9 || axis.SquareMagnitude() <= 1.0e-18) {
      return;
    }
    gp_Trsf rotation;
    rotation.SetRotation(gp_Ax1(pivot, gp_Dir(axis)),
                         degrees * kPi / 180.0);
    next_shape = apply_transform(next_shape, rotation);
    transform_frame_axes(next_frame, rotation);
  };

  rotate_about_axis(frame_x_axis(next_frame), params.rotation_x_degrees);
  rotate_about_axis(frame_y_axis(next_frame), params.rotation_y_degrees);
  rotate_about_axis(frame_z_axis(next_frame), params.rotation_z_degrees);

  gp_Vec translation(0.0, 0.0, 0.0);
  translation += frame_x_axis(next_frame).Multiplied(params.translation_x);
  translation += frame_y_axis(next_frame).Multiplied(params.translation_y);
  translation += frame_z_axis(next_frame).Multiplied(params.translation_z);
  if (translation.SquareMagnitude() > 1.0e-18) {
    gp_Trsf move;
    move.SetTranslation(translation);
    next_shape = apply_transform(next_shape, move);
  }

  frame = next_frame;
  return next_shape;
}
