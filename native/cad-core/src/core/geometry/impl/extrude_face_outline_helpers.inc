std::optional<FaceOutline> outline_for_extrude(
    const ExtrudeFeatureParameters& parameters,
    const std::string& suffix) {
  if (!parameters.plane_frame.has_value()) {
    // Legacy origin-plane extrudes (no plane_frame) are not supported by
    // the projection helper yet.
    return std::nullopt;
  }
  const PlaneFrame& frame = parameters.plane_frame.value();
  const double depth = parameters.depth;

  if (parameters.profile_kind == "rectangle") {
    const double u0 = parameters.start_x;
    const double v0 = parameters.start_y;
    const double u1 = parameters.start_x + parameters.width;
    const double v1 = parameters.start_y + parameters.height;

    auto rectangle_at_offset = [&](double offset) {
      FaceOutline outline{};
      outline.kind = "rectangle";
      outline.rectangle_corners = {
          plane_to_world(frame, u0, v0, offset),
          plane_to_world(frame, u1, v0, offset),
          plane_to_world(frame, u1, v1, offset),
          plane_to_world(frame, u0, v1, offset),
      };
      return outline;
    };

    if (suffix == "base") {
      return rectangle_at_offset(0.0);
    }
    if (suffix == "top") {
      return rectangle_at_offset(depth);
    }

    // Side faces of a rectangular extrude. Each side spans one base edge
    // (length L) and the depth axis (height D), so the four corners are:
    //   (edge_start, 0), (edge_end, 0), (edge_end, depth), (edge_start, depth)
    // expressed in the (u/v base plane, normal) frame.
    auto side_face = [&](double su, double sv, double eu, double ev) {
      FaceOutline outline{};
      outline.kind = "rectangle";
      outline.rectangle_corners = {
          plane_to_world(frame, su, sv, 0.0),
          plane_to_world(frame, eu, ev, 0.0),
          plane_to_world(frame, eu, ev, depth),
          plane_to_world(frame, su, sv, depth),
      };
      return outline;
    };

    if (suffix == "front") {
      return side_face(u0, v0, u1, v0);
    }
    if (suffix == "back") {
      return side_face(u0, v1, u1, v1);
    }
    if (suffix == "left") {
      return side_face(u0, v0, u0, v1);
    }
    if (suffix == "right") {
      return side_face(u1, v0, u1, v1);
    }

    return std::nullopt;
  }

  if (parameters.profile_kind == "circle") {
    if (suffix != "top" && suffix != "base") {
      return std::nullopt;
    }
    const double offset = suffix == "top" ? depth : 0.0;
    FaceOutline outline{};
    outline.kind = "circle";
    outline.circle_center =
        plane_to_world(frame, parameters.start_x, parameters.start_y, offset);
    outline.circle_axis = FaceOutlinePoint{
        .x = frame.normal_x,
        .y = frame.normal_y,
        .z = frame.normal_z,
    };
    outline.circle_radius = parameters.radius;
    return outline;
  }

  // Polygon profiles are not yet supported by Project.
  return std::nullopt;
}
