constexpr double kBoxSpacing = 10.0;
constexpr double kReferencePlaneSize = 120.0;
constexpr double kSketchPlaneOffset = 0.2;
constexpr double kLineDimensionOffset = 12.0;
constexpr double kCircleDimensionOffset = 8.0;
constexpr double kConstraintBadgeOffset = 3.5;
constexpr double kPi = 3.14159265358979323846264338327950288;

struct WorldPoint {
  double x;
  double y;
  double z;
};

struct WorldVector {
  double x;
  double y;
  double z;
};

struct BodyBounds {
  double center_x = 0.0;
  double center_y = 0.0;
  double center_z = 0.0;
  double width = 0.0;
  double height = 0.0;
  double depth = 0.0;
};

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
    Standard_Real min_x = 0.0;
    Standard_Real min_y = 0.0;
    Standard_Real min_z = 0.0;
    Standard_Real max_x = 0.0;
    Standard_Real max_y = 0.0;
    Standard_Real max_z = 0.0;
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

WorldVector subtract_points(const WorldPoint& left, const WorldPoint& right) {
  return WorldVector{
      .x = left.x - right.x,
      .y = left.y - right.y,
      .z = left.z - right.z,
  };
}

WorldVector cross_product(const WorldVector& left, const WorldVector& right) {
  return WorldVector{
      .x = left.y * right.z - left.z * right.y,
      .y = left.z * right.x - left.x * right.z,
      .z = left.x * right.y - left.y * right.x,
  };
}

double vector_length(const WorldVector& vector) {
  return std::sqrt(vector.x * vector.x + vector.y * vector.y +
                   vector.z * vector.z);
}

WorldVector normalize_vector(const WorldVector& vector) {
  const double length = vector_length(vector);
  if (length <= 0.0) {
    return WorldVector{.x = 0.0, .y = 0.0, .z = 0.0};
  }

  return WorldVector{
      .x = vector.x / length,
      .y = vector.y / length,
      .z = vector.z / length,
  };
}

double dot_product(const WorldVector& left, const WorldVector& right) {
  return left.x * right.x + left.y * right.y + left.z * right.z;
}

std::vector<double> make_cosmetic_thread_points(
    double start_x,
    double start_y,
    double start_z,
    double end_x,
    double end_y,
    double end_z,
    double radius,
    double pitch,
    double length,
    double start_offset,
    bool right_handed) {
  if (radius <= 0.0 || pitch <= 0.0 || length <= 0.0) {
    return {};
  }
  const WorldPoint start{.x = start_x, .y = start_y, .z = start_z};
  const WorldPoint end{.x = end_x, .y = end_y, .z = end_z};
  const WorldVector axis = subtract_points(end, start);
  const WorldVector dir = normalize_vector(axis);
  if (vector_length(dir) <= 0.0) {
    return {};
  }

  WorldVector seed{.x = 0.0, .y = 1.0, .z = 0.0};
  if (std::abs(dot_product(dir, seed)) > 0.9) {
    seed = WorldVector{.x = 1.0, .y = 0.0, .z = 0.0};
  }
  WorldVector u = normalize_vector(cross_product(dir, seed));
  if (vector_length(u) <= 0.0) {
    return {};
  }
  const WorldVector v = normalize_vector(cross_product(dir, u));

  const double direction_sign = right_handed ? 1.0 : -1.0;
  const int steps = std::max(
      24, static_cast<int>(std::ceil((length / pitch) * 32.0)));
  std::vector<double> points;
  points.reserve(static_cast<size_t>(steps + 1) * 3);
  for (int i = 0; i <= steps; ++i) {
    const double t = static_cast<double>(i) / static_cast<double>(steps);
    const double along = start_offset + length * t;
    const double angle = direction_sign * 2.0 * kPi * (along / pitch);
    points.push_back(start.x + dir.x * along + u.x * std::cos(angle) * radius +
                     v.x * std::sin(angle) * radius);
    points.push_back(start.y + dir.y * along + u.y * std::cos(angle) * radius +
                     v.y * std::sin(angle) * radius);
    points.push_back(start.z + dir.z * along + u.z * std::cos(angle) * radius +
                     v.z * std::sin(angle) * radius);
  }
  return points;
}

std::optional<std::string> body_appearance_color(
    const DocumentState& document,
    const std::string& body_id) {
  const auto entry = std::find_if(
      document.appearance.body_colors.begin(),
      document.appearance.body_colors.end(),
      [&](const BodyAppearanceOverride& override_entry) {
        return override_entry.body_id == body_id;
      });
  if (entry == document.appearance.body_colors.end()) {
    return std::nullopt;
  }
  return entry->color;
}

std::optional<std::string> face_appearance_color(
    const DocumentState& document,
    const std::string& face_id,
    const std::string& signature) {
  const auto entry = std::find_if(
      document.appearance.face_colors.begin(),
      document.appearance.face_colors.end(),
      [&](const FaceAppearanceOverride& override_entry) {
        return override_entry.face_id == face_id &&
               override_entry.signature == signature;
      });
  if (entry == document.appearance.face_colors.end()) {
    return std::nullopt;
  }
  return entry->color;
}

ViewportSolidFace make_solid_face(
    const std::string& owner_id,
    const std::string& owner_kind,
    const std::string& face_suffix,
    const std::string& label,
    const std::string& sketchability,
    double center_x,
    double center_y,
    double center_z,
    double normal_x,
    double normal_y,
    double normal_z,
    const ViewportSolidFace::PlaneFrame& plane_frame,
    double width,
    double height,
    double radius,
    bool is_selected) {
  return ViewportSolidFace{
      .face_id = owner_id + ":face:" + face_suffix,
      .owner_id = owner_id,
      .owner_kind = owner_kind,
      .label = label,
      .sketchability = sketchability,
      .center_x = center_x,
      .center_y = center_y,
      .center_z = center_z,
      .normal_x = normal_x,
      .normal_y = normal_y,
      .normal_z = normal_z,
      .plane_frame = plane_frame,
      .width = width,
      .height = height,
      .radius = radius,
      .is_selected = is_selected,
  };
}

ViewportSolidFace::PlaneFrame make_plane_frame(double origin_x,
                                               double origin_y,
                                               double origin_z,
                                               double x_axis_x,
                                               double x_axis_y,
                                               double x_axis_z,
                                               double y_axis_x,
                                               double y_axis_y,
                                               double y_axis_z,
                                               double normal_x,
                                               double normal_y,
                                               double normal_z) {
  return ViewportSolidFace::PlaneFrame{
      .origin_x = origin_x,
      .origin_y = origin_y,
      .origin_z = origin_z,
      .x_axis_x = x_axis_x,
      .x_axis_y = x_axis_y,
      .x_axis_z = x_axis_z,
      .y_axis_x = y_axis_x,
      .y_axis_y = y_axis_y,
      .y_axis_z = y_axis_z,
      .normal_x = normal_x,
      .normal_y = normal_y,
      .normal_z = normal_z,
  };
}

ViewportSolidFace::PlaneFrame make_plane_frame(
    const SketchFeatureParameters::SketchPlaneFrame& frame) {
  return make_plane_frame(frame.origin_x,
                          frame.origin_y,
                          frame.origin_z,
                          frame.x_axis_x,
                          frame.x_axis_y,
                          frame.x_axis_z,
                          frame.y_axis_x,
                          frame.y_axis_y,
                          frame.y_axis_z,
                          frame.normal_x,
                          frame.normal_y,
                          frame.normal_z);
}

ViewportSketchPlaneFrame make_sketch_plane_frame(
    const SketchFeatureParameters::SketchPlaneFrame& frame) {
  return ViewportSketchPlaneFrame{
      .origin_x = frame.origin_x,
      .origin_y = frame.origin_y,
      .origin_z = frame.origin_z,
      .x_axis_x = frame.x_axis_x,
      .x_axis_y = frame.x_axis_y,
      .x_axis_z = frame.x_axis_z,
      .y_axis_x = frame.y_axis_x,
      .y_axis_y = frame.y_axis_y,
      .y_axis_z = frame.y_axis_z,
      .normal_x = frame.normal_x,
      .normal_y = frame.normal_y,
      .normal_z = frame.normal_z,
  };
}

ViewportSketchPlaneFrame make_sketch_plane_frame(
    const PlaneFrame& frame) {
  return ViewportSketchPlaneFrame{
      .origin_x = frame.origin_x,
      .origin_y = frame.origin_y,
      .origin_z = frame.origin_z,
      .x_axis_x = frame.x_axis_x,
      .x_axis_y = frame.x_axis_y,
      .x_axis_z = frame.x_axis_z,
      .y_axis_x = frame.y_axis_x,
      .y_axis_y = frame.y_axis_y,
      .y_axis_z = frame.y_axis_z,
      .normal_x = frame.normal_x,
      .normal_y = frame.normal_y,
      .normal_z = frame.normal_z,
  };
}

WorldPoint to_world_point(const std::string& plane_id,
                          double local_x,
                          double local_y,
                          double offset = kSketchPlaneOffset) {
  if (plane_id == "ref-plane-xy") {
    return WorldPoint{.x = local_x, .y = offset, .z = local_y};
  }

  if (plane_id == "ref-plane-yz") {
    return WorldPoint{.x = offset, .y = local_x, .z = local_y};
  }

  return WorldPoint{.x = local_x, .y = local_y, .z = offset};
}

WorldPoint to_world_point(const SketchFeatureParameters& parameters,
                          double local_x,
                          double local_y,
                          double offset = kSketchPlaneOffset) {
  if (parameters.plane_frame.has_value()) {
    const auto& frame = parameters.plane_frame.value();
    return WorldPoint{
        .x = frame.origin_x + frame.x_axis_x * local_x +
             frame.y_axis_x * local_y + frame.normal_x * offset,
        .y = frame.origin_y + frame.x_axis_y * local_x +
             frame.y_axis_y * local_y + frame.normal_y * offset,
        .z = frame.origin_z + frame.x_axis_z * local_x +
             frame.y_axis_z * local_y + frame.normal_z * offset,
    };
  }

  return to_world_point(parameters.plane_id, local_x, local_y, offset);
}

WorldPoint to_world_point(const SketchFeatureParameters::SketchPlaneFrame& frame,
                          double local_x,
                          double local_y,
                          double offset = 0.0) {
  return WorldPoint{
      .x = frame.origin_x + frame.x_axis_x * local_x +
           frame.y_axis_x * local_y + frame.normal_x * offset,
      .y = frame.origin_y + frame.x_axis_y * local_x +
           frame.y_axis_y * local_y + frame.normal_y * offset,
      .z = frame.origin_z + frame.x_axis_z * local_x +
           frame.y_axis_z * local_y + frame.normal_z * offset,
  };
}

WorldPoint to_world_point(const PlaneFrame& frame,
                          double local_x,
                          double local_y,
                          double offset = 0.0) {
  return WorldPoint{
      .x = frame.origin_x + frame.x_axis_x * local_x +
           frame.y_axis_x * local_y + frame.normal_x * offset,
      .y = frame.origin_y + frame.x_axis_y * local_x +
           frame.y_axis_y * local_y + frame.normal_y * offset,
      .z = frame.origin_z + frame.x_axis_z * local_x +
           frame.y_axis_z * local_y + frame.normal_z * offset,
  };
}

ViewportSolidFace::PlaneFrame make_face_frame_for_plane(const std::string& plane_id,
                                                        double origin_x,
                                                        double origin_y,
                                                        double origin_z) {
  if (plane_id == "ref-plane-xy") {
    return make_plane_frame(origin_x,
                            origin_y,
                            origin_z,
                            1.0,
                            0.0,
                            0.0,
                            0.0,
                            0.0,
                            1.0,
                            0.0,
                            1.0,
                            0.0);
  }

  if (plane_id == "ref-plane-yz") {
    return make_plane_frame(origin_x,
                            origin_y,
                            origin_z,
                            0.0,
                            1.0,
                            0.0,
                            0.0,
                            0.0,
                            1.0,
                            1.0,
                            0.0,
                            0.0);
  }

  return make_plane_frame(origin_x,
                          origin_y,
                          origin_z,
                          1.0,
                          0.0,
                          0.0,
                          0.0,
                          1.0,
                          0.0,
                          0.0,
                          0.0,
                          1.0);
}
