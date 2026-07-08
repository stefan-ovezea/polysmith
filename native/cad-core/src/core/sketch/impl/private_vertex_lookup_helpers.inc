// ── Phase 4.2: vertex-lookup helpers shared across translation units ──
// Included by sketch_feature.cpp, document.cpp, and constraint_solver.cpp.
//
// Each including site must have SketchFeatureParameters in scope and
// the polysmith::core types visible.

// Given a point ID, find the circle that owns it via geometry_owner_ids.
// Returns nullptr if the point doesn't belong to any circle.
inline const polysmith::core::SketchCircle* find_circle_for_point(
    const polysmith::core::SketchFeatureParameters& parameters,
    const std::string& point_id) {
  for (const auto& v : parameters.vertices) {
    if (v.id != point_id) continue;
    for (const auto& owner_id : v.geometry_owner_ids) {
      for (const auto& circle : parameters.circles) {
        if (circle.id == owner_id) return &circle;
      }
    }
  }
  return nullptr;
}

// Given a point ID, find the arc that owns it via geometry_owner_ids.
inline const polysmith::core::SketchArc* find_arc_for_point(
    const polysmith::core::SketchFeatureParameters& parameters,
    const std::string& point_id) {
  for (const auto& v : parameters.vertices) {
    if (v.id != point_id) continue;
    for (const auto& owner_id : v.geometry_owner_ids) {
      for (const auto& arc : parameters.arcs) {
        if (arc.id == owner_id) return &arc;
      }
    }
  }
  return nullptr;
}

// Get the center point's primary ID (the `id` field) for a circle.
// During the Phase 4 transition, this still uses the legacy
// "point-circle-{id}-center" format.  After Phase 6 (when vertex-N
// becomes the primary id), this will switch to center_vertex_id.
inline std::string circle_center_vertex_id(
    const polysmith::core::SketchCircle& circle) {
  // Phase 6 (future): return circle.center_vertex_id;
  return "point-circle-" + circle.id + "-center";
}

// Convenience overload: given a circle ID, find the circle and return
// its center point ID.
inline std::string circle_center_vertex_id_for(
    const polysmith::core::SketchFeatureParameters& parameters,
    const std::string& circle_id) {
  for (const auto& circle : parameters.circles) {
    if (circle.id == circle_id) return circle_center_vertex_id(circle);
  }
  return "";
}

// Get the center point ID for an arc.
inline std::string arc_center_vertex_id(
    const polysmith::core::SketchArc& arc) {
  // Phase 6 (future): return arc.center_vertex_id;
  return "point-arc-" + arc.id + "-center";
}
