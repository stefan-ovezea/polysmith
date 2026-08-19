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

  if (!any_boolean || !include_meshes) {
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
