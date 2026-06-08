  for (const auto& feature : document.feature_history) {
    // Suppressed features participate neither in body building nor in
    // the consumed-feature accounting downstream. The legacy primitive
    // renderer skips them via the same flag, so a suppressed box just
    // disappears until unsuppressed.
    if (feature.suppressed) {
      continue;
    }
    if (feature.dependency_broken || feature.status == "warning") {
      continue;
    }
    // Fillet / chamfer modify an existing body in place rather than
    // emitting a new shape, so they're handled here before falling
    // through to the shape-building path used for box/cylinder/extrude.
    if (feature.kind == "fillet" && feature.fillet_parameters.has_value()) {
      const auto& params = feature.fillet_parameters.value();
      // Resolve target body: prefer the explicit target, otherwise fall
      // back to the most recent body. If neither resolves to an existing
      // body the feature is a no-op (the user likely deleted the body).
      std::string target_id;
      if (!params.target_body_id.empty() &&
          body_shapes.find(params.target_body_id) != body_shapes.end()) {
        target_id = params.target_body_id;
      } else if (!body_order.empty()) {
        target_id = body_order.back();
      } else {
        continue;
      }
      const TopoDS_Shape pre_shape = body_shapes[target_id];
      const TopoDS_Shape next = apply_fillet(pre_shape, params);
      body_shapes[target_id] = next;
      // Pending feature: keep the pre-op shape around so the viewport
      // can pick edges against a stable topology. Non-pending features
      // overwrite (and effectively clear) any prior pick_shape on this
      // body, since once confirmed the body's edge identity follows
      // the new post-op topology.
      if (params.is_pending) {
        body_pick_shapes[target_id] = pre_shape;
      } else {
        body_pick_shapes.erase(target_id);
      }
      result.consumed_feature_ids.insert(feature.id);
      // Refresh "most recent body" so subsequent boolean ops target the
      // post-fillet shape.
      if (!body_order.empty() && body_order.back() != target_id) {
        for (auto it = body_order.begin(); it != body_order.end(); ++it) {
          if (*it == target_id) {
            body_order.erase(it);
            break;
          }
        }
        body_order.push_back(target_id);
      }
      continue;
    }
    if (feature.kind == "chamfer" && feature.chamfer_parameters.has_value()) {
      const auto& params = feature.chamfer_parameters.value();
      std::string target_id;
      if (!params.target_body_id.empty() &&
          body_shapes.find(params.target_body_id) != body_shapes.end()) {
        target_id = params.target_body_id;
      } else if (!body_order.empty()) {
        target_id = body_order.back();
      } else {
        continue;
      }
      const TopoDS_Shape pre_shape = body_shapes[target_id];
      const TopoDS_Shape next = apply_chamfer(pre_shape, params);
      body_shapes[target_id] = next;
      // Same pending-shape logic as fillet — see the matching comment
      // a few lines above.
      if (params.is_pending) {
        body_pick_shapes[target_id] = pre_shape;
      } else {
        body_pick_shapes.erase(target_id);
      }
      result.consumed_feature_ids.insert(feature.id);
      if (!body_order.empty() && body_order.back() != target_id) {
        for (auto it = body_order.begin(); it != body_order.end(); ++it) {
          if (*it == target_id) {
            body_order.erase(it);
            break;
          }
        }
        body_order.push_back(target_id);
      }
      continue;
    }
    if (feature.kind == "shell" && feature.shell_parameters.has_value()) {
      const auto& params = feature.shell_parameters.value();
      std::string target_id;
      if (!params.target_body_id.empty() &&
          body_shapes.find(params.target_body_id) != body_shapes.end()) {
        target_id = params.target_body_id;
      } else if (!body_order.empty()) {
        target_id = body_order.back();
      } else {
        continue;
      }
      const TopoDS_Shape pre_shape = body_shapes[target_id];
      const TopoDS_Shape next = apply_shell(pre_shape, params);
      body_shapes[target_id] = next;
      if (params.is_pending) {
        body_pick_shapes[target_id] = pre_shape;
      } else {
        body_pick_shapes.erase(target_id);
      }
      result.consumed_feature_ids.insert(feature.id);
      if (!body_order.empty() && body_order.back() != target_id) {
        for (auto it = body_order.begin(); it != body_order.end(); ++it) {
          if (*it == target_id) {
            body_order.erase(it);
            break;
          }
        }
        body_order.push_back(target_id);
      }
      continue;
    }
    if (feature.kind == "hole" && feature.hole_parameters.has_value()) {
      const auto& params = feature.hole_parameters.value();
      std::string target_id;
      if (!params.target_body_id.empty() &&
          body_shapes.find(params.target_body_id) != body_shapes.end()) {
        target_id = params.target_body_id;
      } else if (!body_order.empty()) {
        target_id = body_order.back();
      } else {
        continue;
      }
      const TopoDS_Shape pre_shape = body_shapes[target_id];
      body_shapes[target_id] = apply_hole(pre_shape, params);
      result.consumed_feature_ids.insert(feature.id);
      result.consumed_feature_ids.insert(target_id);
      body_pick_shapes.erase(target_id);
      if (!body_order.empty() && body_order.back() != target_id) {
        for (auto it = body_order.begin(); it != body_order.end(); ++it) {
          if (*it == target_id) {
            body_order.erase(it);
            break;
          }
        }
        body_order.push_back(target_id);
      }
      continue;
    }
    if (feature.kind == "move" && feature.move_parameters.has_value()) {
      const auto& params = feature.move_parameters.value();
      if (params.target_body_id.empty() ||
          body_shapes.find(params.target_body_id) == body_shapes.end()) {
        result.consumed_feature_ids.insert(feature.id);
        continue;
      }
      BodyLocalFrame frame = body_frames[params.target_body_id];
      body_shapes[params.target_body_id] =
          apply_move(body_shapes[params.target_body_id], frame, params);
      body_frames[params.target_body_id] = frame;
      body_pick_shapes.erase(params.target_body_id);
      result.consumed_feature_ids.insert(feature.id);
      if (!body_order.empty() && body_order.back() != params.target_body_id) {
        for (auto it = body_order.begin(); it != body_order.end(); ++it) {
          if (*it == params.target_body_id) {
            body_order.erase(it);
            break;
          }
        }
        body_order.push_back(params.target_body_id);
      }
      continue;
    }
    if (feature.kind == "body_copy" && feature.body_copy_parameters.has_value()) {
      const auto& params = feature.body_copy_parameters.value();
      if (params.copy_mode == "standalone") {
        const TopoDS_Shape shape =
            deserialize_shape_snapshot(params.serialized_shape);
        if (shape.IsNull()) {
          continue;
        }
        body_shapes[feature.id] = shape;
        body_frames[feature.id] = frame_from_copy_parameters(params);
        body_pick_shapes.erase(feature.id);
        body_order.push_back(feature.id);
        continue;
      }
      if (params.source_body_id.empty() ||
          body_shapes.find(params.source_body_id) == body_shapes.end()) {
        continue;
      }
      body_shapes[feature.id] = body_shapes[params.source_body_id];
      body_frames[feature.id] = body_frames[params.source_body_id];
      body_pick_shapes.erase(feature.id);
      body_order.push_back(feature.id);
      continue;
    }
    if (feature.kind == "thread" && feature.thread_parameters.has_value()) {
      const auto& params = feature.thread_parameters.value();
      if (params.representation == "modeled") {
        std::string target_id;
        if (!params.target_body_id.empty() &&
            body_shapes.find(params.target_body_id) != body_shapes.end()) {
          target_id = params.target_body_id;
        } else if (!body_order.empty()) {
          target_id = body_order.back();
        } else {
          result.consumed_feature_ids.insert(feature.id);
          continue;
        }
        const auto axis = resolve_thread_axis(document,
                                              params.axis_source_id,
                                              body_shapes);
        if (axis.has_value()) {
          body_shapes[target_id] =
              apply_thread(body_shapes[target_id], params, axis.value());
          body_pick_shapes.erase(target_id);
          if (!body_order.empty() && body_order.back() != target_id) {
            for (auto it = body_order.begin(); it != body_order.end(); ++it) {
              if (*it == target_id) {
                body_order.erase(it);
                break;
              }
            }
            body_order.push_back(target_id);
          }
        }
        result.consumed_feature_ids.insert(target_id);
      }
      result.consumed_feature_ids.insert(feature.id);
      continue;
    }
