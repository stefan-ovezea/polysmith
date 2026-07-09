void set_sketch_midpoint_anchor(FeatureEntry &feature,
                                const std::string &point_id,
                                const std::string &host_line_id) {
  if (feature.kind != "sketch" || !feature.sketch_parameters.has_value()) {
    throw std::runtime_error("Only sketch features can hold midpoint anchors");
  }
  auto &parameters = *feature.sketch_parameters;

  // Drop any pre-existing anchor for the same point so the user can
  // re-target without leaving stale relations behind.
  parameters.midpoint_anchors.erase(
      std::remove_if(parameters.midpoint_anchors.begin(),
                     parameters.midpoint_anchors.end(),
                     [&](const SketchMidpointAnchor &anchor) {
                       return anchor.vertex_id == point_id;
                     }),
      parameters.midpoint_anchors.end());

  if (host_line_id.empty()) {
    refresh_sketch_derived_state(feature);
    return;
  }

  const auto host_it = std::find_if(
      parameters.lines.begin(), parameters.lines.end(),
      [&](const SketchLine &line) { return line.id == host_line_id; });
  if (host_it == parameters.lines.end()) {
    // The host line was deleted between snap-time and commit-time.
    // Silently skip — there is nothing to anchor to.
    return;
  }

  // Use a stable id derived from the bound point so the relation
  // round-trips through serialization without churning.
  parameters.midpoint_anchors.push_back(SketchMidpointAnchor{
      .id = "midpoint-anchor-" + point_id,
      .vertex_id = point_id,
      .line_id = host_line_id,
  });

  refresh_sketch_with_append_focus(feature, {point_id, host_line_id});
}

// Anchor a point to the body of a host line at a specific parametric
// position `t` in [0, 1]. Used by the line-body snap to make the
// bound point ride with the host as it moves. Mirrors
// `set_sketch_midpoint_anchor` byte-for-byte except it also accepts
// (and clamps) the `t` value and stores it on the anchor record.
void set_sketch_point_line_anchor(FeatureEntry &feature,
                                  const std::string &point_id,
                                  const std::string &host_line_id, double t) {
  if (feature.kind != "sketch" || !feature.sketch_parameters.has_value()) {
    throw std::runtime_error(
        "Only sketch features can hold point-line anchors");
  }
  auto &parameters = *feature.sketch_parameters;

  // Drop any pre-existing anchor for the same point. Also clear any
  // midpoint anchor on the same point so the two relations don't
  // fight each other (a midpoint anchor is a degenerate point-line
  // anchor at t=0.5; the explicit point-line anchor wins).
  parameters.point_line_anchors.erase(
      std::remove_if(parameters.point_line_anchors.begin(),
                     parameters.point_line_anchors.end(),
                     [&](const SketchVertexLineAnchor &anchor) {
                       return anchor.vertex_id == point_id;
                     }),
      parameters.point_line_anchors.end());
  parameters.midpoint_anchors.erase(
      std::remove_if(parameters.midpoint_anchors.begin(),
                     parameters.midpoint_anchors.end(),
                     [&](const SketchMidpointAnchor &anchor) {
                       return anchor.vertex_id == point_id;
                     }),
      parameters.midpoint_anchors.end());

  if (host_line_id.empty()) {
    refresh_sketch_derived_state(feature);
    return;
  }

  const auto host_it = std::find_if(
      parameters.lines.begin(), parameters.lines.end(),
      [&](const SketchLine &line) { return line.id == host_line_id; });
  if (host_it == parameters.lines.end()) {
    // The host line was deleted between snap-time and commit-time.
    // Silently skip — there is nothing to anchor to.
    return;
  }

  // Clamp `t` so out-of-range UI input can never produce an anchor
  // that re-projects past the segment's endpoints.
  const double clamped_t = std::max(0.0, std::min(1.0, t));

  parameters.point_line_anchors.push_back(SketchVertexLineAnchor{
      .id = "point-line-anchor-" + point_id,
      .vertex_id = point_id,
      .line_id = host_line_id,
      .t = clamped_t,
  });

  refresh_sketch_with_append_focus(feature, {point_id, host_line_id});
}
