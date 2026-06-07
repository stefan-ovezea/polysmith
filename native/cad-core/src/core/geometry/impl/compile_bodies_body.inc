  CompiledBodies result;

  // Body root id, in insertion order. We use a parallel vector instead of
  // an ordered map because we need stable "most recent body" semantics:
  // when an extrude is in cut/join mode it targets the body that was the
  // most-recently created or modified.
  std::vector<std::string> body_order;
  std::unordered_map<std::string, TopoDS_Shape> body_shapes;
  std::unordered_map<std::string, BodyLocalFrame> body_frames;
  // When a fillet/chamfer feature with `is_pending=true` is replayed
  // we capture the body shape *before* the op was applied. The
  // viewport later enumerates body edges from this shape so edge ids
  // stay stable for the duration of the panel session, even though
  // the post-op shape (used for visual rendering) keeps mutating as
  // the user toggles edges. Cleared per-body when a non-pending op
  // overwrites that body. See FilletFeatureParameters::is_pending.
  std::unordered_map<std::string, TopoDS_Shape> body_pick_shapes;

  // First pass: detect whether any body needs the native mesh path. If
  // not we still need per-body shapes for downstream callers (export),
  // but we skip tessellation since legacy primitives will render the
  // viewport. This keeps the cost path-dependent.
  bool any_boolean = false;
  for (const auto& feature : document.feature_history) {
    if (feature.suppressed) {
      continue;
    }
    if (feature.dependency_broken || feature.status == "warning") {
      continue;
    }
    if (feature.kind == "extrude" &&
        feature.extrude_parameters.has_value()) {
      if (feature.extrude_parameters->mode != "new_body") {
        any_boolean = true;
        break;
      }
      // Negative-depth extrudes also force the mesh path: the legacy
      // polygon-extrude primitive renderer assumes a positive depth in
      // the +normal direction, so a negative depth would render in the
      // wrong place. The body_compiler always produces the right body
      // shape regardless of sign.
      if (feature.extrude_parameters->depth < 0.0) {
        any_boolean = true;
        break;
      }
      // Circle extrudes can be created on arbitrary sketch planes.
      // The legacy cylinder primitive only knows the old world-Y axis
      // layout, so use OCCT tessellation for circular profiles.
      if (feature.extrude_parameters->profile_kind == "circle") {
        any_boolean = true;
        break;
      }
      // Profile holes, multi-profile, thin, and side-based extrudes are
      // real native-core topology. The legacy three.js polygon-extrude
      // preview can show simple prisms, but OCCT tessellation is the
      // authoritative path for rings, disjoint same-plane regions, and
      // advanced extents.
      // topology. The legacy three.js polygon-extrude preview can show
      // simple prisms, but OCCT tessellation is the authoritative path
      // for rings and disjoint same-plane regions.
      const auto& params = feature.extrude_parameters.value();
      const bool has_additional_holes = std::any_of(
          params.additional_inner_loops.begin(),
          params.additional_inner_loops.end(),
          [](const auto& loops) { return !loops.empty(); });
      if (!params.inner_loops.empty() ||
          !params.additional_profile_points.empty() ||
          has_additional_holes ||
          params.thin.enabled ||
          params.extent_mode != "one_side" ||
          params.side1.start_offset != 0.0 ||
          params.side1.taper_angle_degrees != 0.0 ||
          params.side1.extent_type != "distance" ||
          params.side2.has_value()) {
        any_boolean = true;
        break;
      }
    }
    if (feature.kind == "fillet" || feature.kind == "chamfer" ||
        feature.kind == "shell" || feature.kind == "hole" ||
        feature.kind == "move" || feature.kind == "body_copy" ||
        feature.kind == "fastener") {
      // Body-modifying features always produce a mesh body (they modify an
      // existing OCCT shape in ways the legacy primitive renderers
      // can't replicate), so they always trigger the mesh path.
      any_boolean = true;
      break;
    }
    if (feature.kind == "thread" && feature.thread_parameters.has_value() &&
        feature.thread_parameters->representation == "modeled") {
      any_boolean = true;
      break;
    }
    if (feature.kind == "loft" || feature.kind == "revolve" ||
        feature.kind == "sweep") {
      any_boolean = true;
      break;
    }
  }

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

  for (const auto& body_id : body_order) {
    CompiledBody body{};
    body.id = body_id;
    body.shape = body_shapes[body_id];
    const auto frame_it = body_frames.find(body_id);
    body.local_frame = frame_it != body_frames.end()
                           ? frame_it->second
                           : default_body_local_frame();
    const auto pick_it = body_pick_shapes.find(body_id);
    if (pick_it != body_pick_shapes.end()) {
      body.pick_shape = pick_it->second;
    }
    result.bodies.push_back(body);
  }

  if (!any_boolean) {
    return result;
  }

  // Tessellate every body — including independent ones — so the viewport
  // can render them all consistently when at least one boolean op is in
  // play. (Mixing legacy primitives and meshes for the same scene is
  // jarring, and the cost is dominated by the boolean op anyway.)
  for (const auto& body : result.bodies) {
    BodyMesh mesh{};
    mesh.body_id = body.id;
    try {
      tessellate_shape(body.shape, mesh);
    } catch (const std::exception&) {
      // Skip the failing body rather than aborting the whole viewport
      // build; users will see other bodies render correctly.
      mesh.vertices.clear();
      mesh.indices.clear();
      mesh.normals.clear();
    }
    if (!mesh.vertices.empty() && !mesh.indices.empty()) {
      result.meshes.push_back(std::move(mesh));
      // Once a mesh exists for this body, suppress the legacy primitive
      // for its root feature so we don't double-render.
      result.consumed_feature_ids.insert(body.id);
    }
  }

  return result;
