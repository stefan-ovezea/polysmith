int parse_edge_index(const std::string& edge_id) {
  const std::string marker = ":edge:";
  const auto pos = edge_id.rfind(marker);
  if (pos == std::string::npos) {
    return -1;
  }
  try {
    return std::stoi(edge_id.substr(pos + marker.size()));
  } catch (const std::exception&) {
    return -1;
  }
}

int parse_face_index(const std::string& face_id) {
  const std::string marker = ":face:";
  const auto pos = face_id.rfind(marker);
  if (pos == std::string::npos) {
    return -1;
  }
  try {
    return std::stoi(face_id.substr(pos + marker.size()));
  } catch (const std::exception&) {
    return -1;
  }
}

// Apply a fillet feature onto an existing body shape. Returns the new
// shape on success, or `body_shape` unchanged on any failure (missing
// edges, OCCT failure, etc.) so the document keeps rendering.
TopoDS_Shape apply_fillet(const TopoDS_Shape& body_shape,
                          const FilletFeatureParameters& params) {
  if (body_shape.IsNull() || params.edge_ids.empty() || params.radius <= 0.0) {
    return body_shape;
  }

  NCollection_IndexedMap<TopoDS_Shape, TopTools_ShapeMapHasher> edge_map;
  TopExp::MapShapes(body_shape, TopAbs_EDGE, edge_map);

  try {
    BRepFilletAPI_MakeFillet maker(body_shape);
    bool any_added = false;
    for (const std::string& edge_id : params.edge_ids) {
      const int zero_based = parse_edge_index(edge_id);
      const int one_based = zero_based + 1;
      if (zero_based < 0 || one_based < 1 || one_based > edge_map.Extent()) {
        continue;
      }
      const TopoDS_Edge edge = TopoDS::Edge(edge_map(one_based));
      maker.Add(params.radius, edge);
      any_added = true;
    }
    if (!any_added) {
      return body_shape;
    }
    maker.Build();
    if (!maker.IsDone()) {
      return body_shape;
    }
    const TopoDS_Shape result = maker.Shape();
    if (result.IsNull()) {
      return body_shape;
    }
    return result;
  } catch (const std::exception&) {
    return body_shape;
  }
}

TopoDS_Shape apply_chamfer(const TopoDS_Shape& body_shape,
                           const ChamferFeatureParameters& params) {
  if (body_shape.IsNull() || params.edge_ids.empty() ||
      params.distance <= 0.0) {
    return body_shape;
  }

  NCollection_IndexedMap<TopoDS_Shape, TopTools_ShapeMapHasher> edge_map;
  TopExp::MapShapes(body_shape, TopAbs_EDGE, edge_map);

  try {
    BRepFilletAPI_MakeChamfer maker(body_shape);
    bool any_added = false;
    for (const std::string& edge_id : params.edge_ids) {
      const int zero_based = parse_edge_index(edge_id);
      const int one_based = zero_based + 1;
      if (zero_based < 0 || one_based < 1 || one_based > edge_map.Extent()) {
        continue;
      }
      const TopoDS_Edge edge = TopoDS::Edge(edge_map(one_based));
      // Symmetric chamfer: same distance on both adjacent faces. The
      // single-arg Add overload uses the edge's first adjacent face,
      // which is fine for symmetric chamfers.
      maker.Add(params.distance, edge);
      any_added = true;
    }
    if (!any_added) {
      return body_shape;
    }
    maker.Build();
    if (!maker.IsDone()) {
      return body_shape;
    }
    const TopoDS_Shape result = maker.Shape();
    if (result.IsNull()) {
      return body_shape;
    }
    return result;
  } catch (const std::exception&) {
    return body_shape;
  }
}

TopoDS_Shape apply_shell(const TopoDS_Shape& body_shape,
                         const ShellFeatureParameters& params) {
  if (body_shape.IsNull() || params.thickness <= 0.0 ||
      params.removed_face_ids.empty()) {
    return body_shape;
  }

  NCollection_IndexedMap<TopoDS_Shape, TopTools_ShapeMapHasher> face_map;
  TopExp::MapShapes(body_shape, TopAbs_FACE, face_map);

  try {
    NCollection_List<TopoDS_Shape> closing_faces;
    for (const std::string& face_id : params.removed_face_ids) {
      const int zero_based = parse_face_index(face_id);
      const int one_based = zero_based + 1;
      if (zero_based < 0 || one_based < 1 || one_based > face_map.Extent()) {
        continue;
      }
      closing_faces.Append(TopoDS::Face(face_map(one_based)));
    }
    if (closing_faces.IsEmpty()) {
      return body_shape;
    }

    BRepOffsetAPI_MakeThickSolid maker;
    maker.MakeThickSolidByJoin(body_shape,
                               closing_faces,
                               -std::abs(params.thickness),
                               1.0e-3,
                               BRepOffset_Skin,
                               false,
                               false,
                               GeomAbs_Arc,
                               true);
    maker.Build();
    if (!maker.IsDone()) {
      return body_shape;
    }
    const TopoDS_Shape result = maker.Shape();
    if (result.IsNull()) {
      return body_shape;
    }
    return result;
  } catch (const std::exception&) {
    return body_shape;
  }
}

TopoDS_Shape apply_hole(const TopoDS_Shape& body_shape,
                        const HoleFeatureParameters& params) {
  if (body_shape.IsNull()) {
    return body_shape;
  }
  try {
    const TopoDS_Shape cutter = build_hole_cutter_shape(params);
    if (cutter.IsNull()) {
      return body_shape;
    }
    TopoDS_Shape result = BRepAlgoAPI_Cut(body_shape, cutter).Shape();
    if (result.IsNull()) {
      return body_shape;
    }
    result = unify_same_domain(result);
    if (!params.thread_enabled ||
        params.thread_representation != "modeled" ||
        params.thread_pitch <= 0.0 ||
        params.major_diameter <= 0.0 ||
        params.minor_diameter <= 0.0) {
      return result;
    }

    const gp_Pnt face_center(
        params.plane_frame.origin_x +
            params.plane_frame.x_axis_x * params.center_x +
            params.plane_frame.y_axis_x * params.center_y,
        params.plane_frame.origin_y +
            params.plane_frame.x_axis_y * params.center_x +
            params.plane_frame.y_axis_y * params.center_y,
        params.plane_frame.origin_z +
            params.plane_frame.x_axis_z * params.center_x +
            params.plane_frame.y_axis_z * params.center_y);
    gp_Vec inward(-params.plane_frame.normal_x,
                  -params.plane_frame.normal_y,
                  -params.plane_frame.normal_z);
    if (inward.SquareMagnitude() <= 1.0e-18) {
      return result;
    }
    inward.Normalize();
    ThreadFeatureParameters thread{};
    thread.target_body_id = params.target_body_id;
    thread.axis_source_id = params.source_face_id;
    thread.mode = "internal";
    thread.standard = params.standard;
    thread.size = params.standard_size;
    thread.major_diameter = params.major_diameter;
    thread.minor_diameter = params.minor_diameter;
    thread.pitch = params.thread_pitch;
    thread.length = std::min(std::abs(params.thread_depth),
                             params.extent_type == "through_all"
                                 ? std::max(10000.0, std::abs(params.depth))
                                 : std::abs(params.depth));
    thread.thread_angle_degrees = 60.0;
    thread.start_offset = 0.0;
    thread.handedness = "right";
    thread.representation = "modeled";
    const ThreadAxis axis{
        .start = face_center,
        .direction = gp_Dir(inward),
        .length = thread.length,
    };
    return apply_thread(result, thread, axis);
  } catch (const std::exception&) {
    return body_shape;
  }
}
