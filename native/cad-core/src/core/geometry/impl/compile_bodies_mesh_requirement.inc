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
    if (feature.kind == "plugin_feature") {
      any_boolean = true;
      break;
    }
  }
