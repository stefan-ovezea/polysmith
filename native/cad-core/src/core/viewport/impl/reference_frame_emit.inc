  scene_width = std::max(
      scene_width,
      (boxes.empty() && cylinders.empty()) ? 0.0 : current_x_offset - kBoxSpacing);
  const double reference_extent =
      std::max({kReferencePlaneSize, scene_width, max_height, max_depth, 1.0});
  const double scene_width_with_references = std::max(scene_width, reference_extent);
  const double scene_height_with_references = std::max(max_height, reference_extent);
  const double scene_depth_with_references = std::max(max_depth, reference_extent);

  reference_planes.push_back(ViewportReferencePlane{
      .id = "ref-plane-xy",
      .label = "XY Plane",
      .orientation = "xy",
      .center_x = reference_extent / 2.0,
      .center_y = reference_extent / 2.0,
      .center_z = 0.0,
      .width = reference_extent,
      .height = reference_extent,
      .is_selected = view->selected_reference_id.has_value() &&
                     view->selected_reference_id.value() == "ref-plane-xy",
      .is_active_sketch_plane = view->active_sketch_plane_id.has_value() &&
                                view->active_sketch_plane_id.value() ==
                                    "ref-plane-xy",
  });
  reference_planes.push_back(ViewportReferencePlane{
      .id = "ref-plane-yz",
      .label = "YZ Plane",
      .orientation = "yz",
      .center_x = 0.0,
      .center_y = reference_extent / 2.0,
      .center_z = reference_extent / 2.0,
      .width = reference_extent,
      .height = reference_extent,
      .is_selected = view->selected_reference_id.has_value() &&
                     view->selected_reference_id.value() == "ref-plane-yz",
      .is_active_sketch_plane = view->active_sketch_plane_id.has_value() &&
                                view->active_sketch_plane_id.value() ==
                                    "ref-plane-yz",
  });
  reference_planes.push_back(ViewportReferencePlane{
      .id = "ref-plane-xz",
      .label = "XZ Plane",
      .orientation = "xz",
      .center_x = reference_extent / 2.0,
      .center_y = 0.0,
      .center_z = reference_extent / 2.0,
      .width = reference_extent,
      .height = reference_extent,
      .is_selected = view->selected_reference_id.has_value() &&
                     view->selected_reference_id.value() == "ref-plane-xz",
      .is_active_sketch_plane = view->active_sketch_plane_id.has_value() &&
                                view->active_sketch_plane_id.value() ==
                                    "ref-plane-xz",
  });

  reference_axes.push_back(ViewportReferenceAxis{
      .id = "ref-axis-x",
      .label = "X",
      .axis = "x",
      .start_x = 0.0,
      .start_y = 0.0,
      .start_z = 0.0,
      .end_x = reference_extent,
      .end_y = 0.0,
      .end_z = 0.0,
  });
  reference_axes.push_back(ViewportReferenceAxis{
      .id = "ref-axis-y",
      .label = "Y",
      .axis = "y",
      .start_x = 0.0,
      .start_y = 0.0,
      .start_z = 0.0,
      .end_x = 0.0,
      .end_y = reference_extent,
      .end_z = 0.0,
  });
  reference_axes.push_back(ViewportReferenceAxis{
      .id = "ref-axis-z",
      .label = "Z",
      .axis = "z",
      .start_x = 0.0,
      .start_y = 0.0,
      .start_z = 0.0,
      .end_x = 0.0,
      .end_y = 0.0,
      .end_z = reference_extent,
  });

  // Emit construction-plane features as additional reference planes.
  // We reuse `ViewportReferencePlane` (rather than introducing a new
  // viewport struct) so existing viewport machinery — selection,
  // active-sketch-plane highlighting, hierarchy filtering — keeps
  // working unchanged. Construction planes ship the cached world-space
  // `plane_frame`; the renderer uses it instead of the legacy
  // orientation rotation.
  for (const auto& feature : view->feature_history) {
    if (feature.suppressed) {
      continue;
    }
    if (feature.kind != "construction_plane" ||
        !feature.construction_plane_parameters.has_value()) {
      continue;
    }
    const auto& params = feature.construction_plane_parameters.value();
    reference_planes.push_back(ViewportReferencePlane{
        .id = feature.id,
        .label = feature.name,
        .orientation = "custom",
        .center_x = params.plane_frame.origin_x,
        .center_y = params.plane_frame.origin_y,
        .center_z = params.plane_frame.origin_z,
        .width = kReferencePlaneSize,
        .height = kReferencePlaneSize,
        .is_selected =
            view->selected_reference_id.has_value() &&
            view->selected_reference_id.value() == feature.id,
        .is_active_sketch_plane =
            view->active_sketch_plane_id.has_value() &&
            view->active_sketch_plane_id.value() == feature.id,
        .plane_frame = params.plane_frame,
    });
  }
