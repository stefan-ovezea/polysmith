  const ViewportSceneBounds scene_bounds = {
      .center_x = scene_width_with_references / 2.0,
      .center_y = scene_height_with_references / 2.0,
      .center_z = scene_depth_with_references / 2.0,
      .width = scene_width_with_references,
      .height = scene_height_with_references,
      .depth = scene_depth_with_references,
      .max_dimension = std::max({scene_width_with_references,
                                 scene_height_with_references,
                                 scene_depth_with_references}),
  };
