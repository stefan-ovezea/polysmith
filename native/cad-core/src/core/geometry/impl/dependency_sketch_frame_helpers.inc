SketchFeatureParameters::SketchPlaneFrame
to_sketch_plane_frame(const PlaneFrame &frame) {
  return SketchFeatureParameters::SketchPlaneFrame{
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

PlaneFrame from_sketch_plane_frame(
    const SketchFeatureParameters::SketchPlaneFrame &frame) {
  return PlaneFrame{
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

const FeatureEntry *find_feature(const DocumentState &document,
                                 const std::string &feature_id) {
  for (const auto &feature : document.feature_history) {
    if (feature.id == feature_id) {
      return &feature;
    }
  }
  return nullptr;
}

// Hardcoded frames for the three origin reference planes. The basis
// vectors mirror the existing conventions used by `feature_shape.cpp`
// (`to_world_point` and `extrusion_vector`) so a sketch / extrude
// rendered against one of these frames lands at the same world
// position it always has — extending `start_sketch_on_plane` with a
// plane_frame must not move legacy sketches.
//
// Mapping (kept in lockstep with `feature_shape.cpp::to_world_point`):
//   ref-plane-xy: local (x, y) -> world (x, 0, y), normal (0, 1, 0)
//   ref-plane-yz: local (x, y) -> world (0, x, y), normal (1, 0, 0)
//   ref-plane-xz: local (x, y) -> world (x, y, 0), normal (0, 0, 1)
std::optional<PlaneFrame> origin_plane_frame(const std::string &reference_id) {
  if (reference_id == "ref-plane-xy") {
    return PlaneFrame{
        .origin_x = 0.0,
        .origin_y = 0.0,
        .origin_z = 0.0,
        .x_axis_x = 1.0,
        .x_axis_y = 0.0,
        .x_axis_z = 0.0,
        .y_axis_x = 0.0,
        .y_axis_y = 0.0,
        .y_axis_z = 1.0,
        .normal_x = 0.0,
        .normal_y = 1.0,
        .normal_z = 0.0,
    };
  }
  if (reference_id == "ref-plane-yz") {
    return PlaneFrame{
        .origin_x = 0.0,
        .origin_y = 0.0,
        .origin_z = 0.0,
        .x_axis_x = 0.0,
        .x_axis_y = 1.0,
        .x_axis_z = 0.0,
        .y_axis_x = 0.0,
        .y_axis_y = 0.0,
        .y_axis_z = 1.0,
        .normal_x = 1.0,
        .normal_y = 0.0,
        .normal_z = 0.0,
    };
  }
  if (reference_id == "ref-plane-xz") {
    return PlaneFrame{
        .origin_x = 0.0,
        .origin_y = 0.0,
        .origin_z = 0.0,
        .x_axis_x = 1.0,
        .x_axis_y = 0.0,
        .x_axis_z = 0.0,
        .y_axis_x = 0.0,
        .y_axis_y = 1.0,
        .y_axis_z = 0.0,
        .normal_x = 0.0,
        .normal_y = 0.0,
        .normal_z = 1.0,
    };
  }
  return std::nullopt;
}

std::array<double, 3> sketch_local_to_world(const PlaneFrame &frame,
                                            double local_x, double local_y);

std::optional<PlaneFrame>
resolve_sketch_profile_frame(const DocumentState &document,
                             const std::string &profile_id) {
  for (const auto &feature : document.feature_history) {
    if (feature.kind != "sketch" || !feature.sketch_parameters.has_value()) {
      continue;
    }
    const auto &sketch = feature.sketch_parameters.value();
    const auto profile_it =
        std::find_if(sketch.profiles.begin(), sketch.profiles.end(),
                     [&](const SketchProfileRegion &profile) {
                       return profile.id == profile_id;
                     });
    if (profile_it == sketch.profiles.end()) {
      continue;
    }

    std::optional<PlaneFrame> frame =
        sketch.plane_frame.has_value()
            ? std::optional<PlaneFrame>(
                  from_sketch_plane_frame(sketch.plane_frame.value()))
            : origin_plane_frame(sketch.plane_id);
    if (!frame.has_value()) {
      return std::nullopt;
    }

    double local_x = profile_it->center_x;
    double local_y = profile_it->center_y;
    if (profile_it->kind != "circle") {
      if (profile_it->points.empty()) {
        return std::nullopt;
      }
      local_x = 0.0;
      local_y = 0.0;
      for (const auto &point : profile_it->points) {
        local_x += point.x;
        local_y += point.y;
      }
      local_x /= static_cast<double>(profile_it->points.size());
      local_y /= static_cast<double>(profile_it->points.size());
    }

    const auto origin = sketch_local_to_world(frame.value(), local_x, local_y);
    frame->origin_x = origin[0];
    frame->origin_y = origin[1];
    frame->origin_z = origin[2];
    return frame;
  }

  return std::nullopt;
}
