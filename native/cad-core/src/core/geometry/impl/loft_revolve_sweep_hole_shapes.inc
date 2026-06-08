TopoDS_Shape build_loft_shape(const LoftFeatureParameters& parameters) {
  if (parameters.sections.size() < 2) {
    throw std::runtime_error("Loft requires at least two profile sections");
  }

  BRepOffsetAPI_ThruSections loft_builder(
      /*isSolid=*/true,
      /*ruled=*/parameters.ruled,
      /*pres3d=*/1.0e-6);
  loft_builder.CheckCompatibility(true);

  for (const auto& section : parameters.sections) {
    if (section.profile_points.size() < 3) {
      throw std::runtime_error("Loft profile section requires at least three points");
    }
    loft_builder.AddWire(make_profile_wire(section, section.profile_points));
  }

  loft_builder.Build();
  if (!loft_builder.IsDone()) {
    throw std::runtime_error("Failed to build loft");
  }

  const TopoDS_Shape shape = loft_builder.Shape();
  if (shape.IsNull()) {
    throw std::runtime_error("Failed to build loft shape");
  }
  return shape;
}

TopoDS_Shape build_revolve_shape(const RevolveFeatureParameters& parameters) {
  if (parameters.profile_points.size() < 3) {
    throw std::runtime_error("Revolve requires a closed profile");
  }
  const double dx = parameters.axis_end_x - parameters.axis_start_x;
  const double dy = parameters.axis_end_y - parameters.axis_start_y;
  const double dz = parameters.axis_end_z - parameters.axis_start_z;
  if (std::sqrt(dx * dx + dy * dy + dz * dz) <= 1.0e-9) {
    throw std::runtime_error("Revolve axis must have non-zero length");
  }

  const TopoDS_Shape face = make_profile_face(
      parameters, parameters.profile_points, parameters.inner_loops);
  const gp_Ax1 axis(gp_Pnt(parameters.axis_start_x,
                           parameters.axis_start_y,
                           parameters.axis_start_z),
                    gp_Dir(dx, dy, dz));
  const double angle_radians = parameters.angle_degrees * kPi / 180.0;
  const TopoDS_Shape shape =
      BRepPrimAPI_MakeRevol(face, axis, angle_radians).Shape();
  if (shape.IsNull()) {
    throw std::runtime_error("Failed to build revolve shape");
  }
  return shape;
}

TopoDS_Shape build_sweep_shape(const SweepFeatureParameters& parameters) {
  if (parameters.profile_points.size() < 3) {
    throw std::runtime_error("Sweep requires a closed profile");
  }
  const auto& segments = parameters.path_segments;
  if (segments.empty()) {
    throw std::runtime_error("Sweep path must have non-zero length");
  }

  const TopoDS_Shape face = make_profile_face(
      parameters, parameters.profile_points, parameters.inner_loops);

  if (segments.size() == 1 && segments.front().kind == "line") {
    const auto& segment = segments.front();
    const double dx = segment.end_x - segment.start_x;
    const double dy = segment.end_y - segment.start_y;
    const double dz = segment.end_z - segment.start_z;
    if (std::sqrt(dx * dx + dy * dy + dz * dz) <= 1.0e-9) {
      throw std::runtime_error("Sweep path must have non-zero length");
    }
    const TopoDS_Shape shape =
        BRepPrimAPI_MakePrism(face, gp_Vec(dx, dy, dz)).Shape();
    if (shape.IsNull()) {
      throw std::runtime_error("Failed to build sweep shape");
    }
    return shape;
  }

  BRepBuilderAPI_MakeWire spine_builder;
  for (const auto& segment : segments) {
    const gp_Pnt start(segment.start_x, segment.start_y, segment.start_z);
    const gp_Pnt end(segment.end_x, segment.end_y, segment.end_z);
    if (start.Distance(end) <= 1.0e-9) {
      continue;
    }
    TopoDS_Edge edge;
    if (segment.kind == "arc") {
      const gp_Pnt mid(segment.mid_x, segment.mid_y, segment.mid_z);
      if (start.Distance(mid) <= 1.0e-9 || mid.Distance(end) <= 1.0e-9) {
        throw std::runtime_error("Sweep arc path segment is degenerate");
      }
      GC_MakeArcOfCircle arc(start, mid, end);
      if (!arc.IsDone()) {
        throw std::runtime_error("Failed to build sweep arc path segment");
      }
      edge = BRepBuilderAPI_MakeEdge(arc.Value()).Edge();
    } else {
      edge = BRepBuilderAPI_MakeEdge(start, end).Edge();
    }
    if (edge.IsNull()) {
      throw std::runtime_error("Failed to build sweep path segment");
    }
    spine_builder.Add(edge);
  }
  if (!spine_builder.IsDone()) {
    throw std::runtime_error("Failed to build sweep path wire");
  }
  BRepOffsetAPI_MakePipe pipe(spine_builder.Wire(), face);
  pipe.Build();
  if (!pipe.IsDone()) {
    throw std::runtime_error("Failed to build swept shape along path");
  }
  const TopoDS_Shape shape = pipe.Shape();
  if (shape.IsNull()) {
    throw std::runtime_error("Failed to build sweep shape");
  }
  return shape;
}

gp_Pnt local_point_on_frame(const PlaneFrame& frame, double local_x, double local_y) {
  return gp_Pnt(frame.origin_x + frame.x_axis_x * local_x + frame.y_axis_x * local_y,
                frame.origin_y + frame.x_axis_y * local_x + frame.y_axis_y * local_y,
                frame.origin_z + frame.x_axis_z * local_x + frame.y_axis_z * local_y);
}

TopoDS_Shape build_hole_cutter_shape(const HoleFeatureParameters& parameters) {
  if (parameters.diameter <= 0.0) {
    throw std::runtime_error("Hole diameter must be greater than zero");
  }
  const double main_depth =
      parameters.extent_type == "through_all"
          ? std::max(10000.0, std::abs(parameters.depth))
          : std::abs(parameters.depth);
  if (main_depth <= 0.0) {
    throw std::runtime_error("Hole depth must be greater than zero");
  }

  const PlaneFrame& frame = parameters.plane_frame;
  const gp_Pnt face_center =
      local_point_on_frame(frame, parameters.center_x, parameters.center_y);
  const gp_Vec outward(frame.normal_x, frame.normal_y, frame.normal_z);
  if (outward.SquareMagnitude() <= 1.0e-18) {
    throw std::runtime_error("Hole source plane normal is invalid");
  }
  gp_Vec inward = outward.Reversed();
  inward.Normalize();

  const double overshoot = 1.0;
  gp_Vec outward_unit = outward;
  outward_unit.Normalize();
  const gp_Pnt start = face_center.Translated(outward_unit * overshoot);
  const gp_Ax2 axis(start, gp_Dir(inward));
  TopoDS_Shape cutter =
      BRepPrimAPI_MakeCylinder(axis,
                               parameters.diameter * 0.5,
                               main_depth + overshoot * 2.0)
          .Shape();

  if (parameters.hole_type == "counterbore" ||
      parameters.hole_type == "spotface") {
    const double cb_diameter =
        std::max(parameters.counterbore_diameter, parameters.diameter);
    const double cb_depth = std::abs(parameters.counterbore_depth);
    if (cb_depth > 0.0) {
      cutter = fuse_shapes(
          cutter,
          BRepPrimAPI_MakeCylinder(axis,
                                   cb_diameter * 0.5,
                                   cb_depth + overshoot)
              .Shape());
    }
  } else if (parameters.hole_type == "countersink") {
    const double sink_diameter =
        std::max(parameters.countersink_diameter, parameters.diameter);
    const double half_angle =
        std::max(1.0, parameters.countersink_angle_degrees) * kPi / 360.0;
    const double sink_depth =
        (sink_diameter - parameters.diameter) * 0.5 / std::tan(half_angle);
    if (sink_depth > 0.0) {
      cutter = fuse_shapes(
          cutter,
          BRepPrimAPI_MakeCone(axis,
                               sink_diameter * 0.5,
                               parameters.diameter * 0.5,
                               sink_depth + overshoot)
              .Shape());
    }
  }

  if (cutter.IsNull()) {
    throw std::runtime_error("Failed to build hole cutter");
  }
  return cutter;
}
