    const TopoDS_Shape shape = build_feature_shape(feature);
    if (shape.IsNull()) {
      continue;
    }

    std::string mode = "new_body";
    if (feature.kind == "extrude" && feature.extrude_parameters.has_value()) {
      mode = feature.extrude_parameters->mode;
    }

    if (mode != "new_body" && !body_order.empty()) {
      // Honor an explicit target_body_id when set and still present in
      // the current body set; otherwise fall back to the most recent
      // body. This keeps boolean ops well-defined even when prior body
      // roots were consumed by a more recent boolean further upstream.
      std::string target_id = body_order.back();
      if (feature.kind == "extrude" &&
          feature.extrude_parameters.has_value() &&
          feature.extrude_parameters->target_body_id.has_value()) {
        const std::string& requested =
            feature.extrude_parameters->target_body_id.value();
        if (body_shapes.find(requested) != body_shapes.end()) {
          target_id = requested;
        }
      }
      const TopoDS_Shape target_shape = body_shapes[target_id];

      TopoDS_Shape combined;
      try {
        if (mode == "join") {
          combined = BRepAlgoAPI_Fuse(target_shape, shape).Shape();
        } else if (mode == "cut") {
          combined = BRepAlgoAPI_Cut(target_shape, shape).Shape();
        } else if (mode == "intersect") {
          combined = BRepAlgoAPI_Common(target_shape, shape).Shape();
        } else {
          throw std::runtime_error("Unknown extrude mode: " + mode);
        }
      } catch (const std::exception&) {
        // Swallow boolean failures — fall back to a fresh body so the
        // user still sees their geometry instead of an opaque error.
        combined = shape;
      }

      if (combined.IsNull()) {
        // Boolean failure: degrade to an independent body so the user
        // still sees the new geometry.
        body_shapes[feature.id] = shape;
        body_frames[feature.id] = initial_frame_for_feature(feature);
        body_order.push_back(feature.id);
        continue;
      }
      combined = unify_same_domain(combined);

      const bool intersect_as_new_body =
          mode == "intersect" &&
          feature.kind == "extrude" &&
          feature.extrude_parameters.has_value() &&
          feature.extrude_parameters->intersect_result == "new_body";
      if (intersect_as_new_body) {
        body_shapes[feature.id] = combined;
        body_frames[feature.id] = initial_frame_for_feature(feature);
        body_order.push_back(feature.id);
        result.consumed_feature_ids.insert(feature.id);
        continue;
      }

      body_shapes[target_id] = combined;
      // A boolean op replaced the body's topology, so any stale
      // pending-fillet pick shape no longer reflects what the user is
      // looking at. Clear it so edge picks resolve against the current
      // body shape from here on.
      body_pick_shapes.erase(target_id);
      result.consumed_feature_ids.insert(feature.id);
      result.consumed_feature_ids.insert(target_id);
      // The combined body is now the most recent body for the purpose
      // of "most recent" fallbacks downstream — bring its id to the
      // back of body_order if it's not already there.
      if (!body_order.empty() && body_order.back() != target_id) {
        for (auto it = body_order.begin(); it != body_order.end(); ++it) {
          if (*it == target_id) {
            body_order.erase(it);
            break;
          }
        }
        body_order.push_back(target_id);
      }
    } else {
      body_shapes[feature.id] = shape;
      body_frames[feature.id] = initial_frame_for_feature(feature);
      body_order.push_back(feature.id);
    }
  }
