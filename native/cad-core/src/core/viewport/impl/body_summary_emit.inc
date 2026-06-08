  // Build the body summary list for the UI's target picker. The body's
  // root id maps 1:1 to a feature id, so we look up the human-readable
  // name from feature_history; missing or empty names degrade to the id
  // itself so the picker is always populated.
  for (const auto& body : compiled_bodies.bodies) {
    std::string label = body.id;
    for (const auto& feature : view->feature_history) {
      if (feature.id == body.id && !feature.name.empty()) {
        label = feature.name;
        break;
      }
    }
    const BodyBounds bounds = bounds_for_shape(body.shape);
    bodies.push_back(ViewportBodySummary{
        .id = body.id,
        .label = label,
        .center_x = bounds.center_x,
        .center_y = bounds.center_y,
        .center_z = bounds.center_z,
        .width = bounds.width,
        .height = bounds.height,
        .depth = bounds.depth,
        .local_frame = body.local_frame,
    });
    // Edge picking uses pick_shape when the body has one (set by
    // body_compiler for the duration of a pending fillet/chamfer
    // panel session). This keeps edge ids stable while the user
    // toggles edges, even though body.shape is mutating with each
    // update_*_edges. Vertices and faces still come from the live
    // post-op shape because vertex / face picks aren't part of the
    // pending feature's input set.
    const TopoDS_Shape& edge_pick_shape =
        body.pick_shape.IsNull() ? body.shape : body.pick_shape;
    enumerate_body_edges(edge_pick_shape,
                         body.id,
                         view->selected_edge_ids,
                         edges);
    enumerate_body_vertices(body.shape,
                            body.id,
                            view->selected_vertex_ids,
                            vertices);
    // Look up the body's owning feature kind so the face's owner_kind
    // stays useful to consumers (the UI uses it to label faces).
    std::string body_kind;
    for (const auto& feature : view->feature_history) {
      if (feature.id == body.id) {
        body_kind = feature.kind;
        break;
      }
    }
    // Legacy box/cylinder features render at a `current_x_offset` that
    // body_compiler doesn't know about (their shapes are built at the
    // origin). Body-derived faces would therefore land away from the
    // visual primitive — keep their analytical faces (emitted below in
    // the per-feature loop) and skip body-derived faces for them.
    if (body_kind != "box" && body_kind != "cylinder") {
      enumerate_body_faces(body.shape,
                           body.id,
                           body_kind,
                           *view,
                           view->selected_face_id,
                           solid_faces);
    }
  }
