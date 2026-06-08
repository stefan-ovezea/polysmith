DetectedSketchProfiles detect_sketch_profiles(const FeatureEntry& feature) {
  DetectedSketchProfiles profiles;

  if (feature.kind != "sketch" || !feature.sketch_parameters.has_value()) {
    return profiles;
  }

  const auto& sketch = feature.sketch_parameters.value();

  for (const auto& profile : sketch.profiles) {
    if (profile.kind == "polygon") {
      profiles.polygons.push_back(PolygonSketchProfile{
          .id = profile.id,
          .plane_id = sketch.plane_id,
          .plane_frame = sketch.plane_frame,
          .points = profile.points,
          .inner_loops = profile.inner_loops,
      });
      continue;
    }

    if (profile.kind == "circle") {
      profiles.circles.push_back(CircleSketchProfile{
          .id = profile.id,
          .plane_id = sketch.plane_id,
          .plane_frame = sketch.plane_frame,
          .center_x = profile.center_x,
          .center_y = profile.center_y,
          .radius = profile.radius,
      });
    }
  }

  return profiles;
}
