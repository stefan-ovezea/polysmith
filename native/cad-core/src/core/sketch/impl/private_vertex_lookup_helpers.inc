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
// Phase 6: returns center_vertex_id (now vertex-N) when set.
// Falls back to legacy format for old documents before first rebuild.
inline std::string circle_center_vertex_id(
    const polysmith::core::SketchCircle& circle) {
  if (!circle.center_vertex_id.empty()) return circle.center_vertex_id;
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
// Phase 6: returns center_vertex_id (now vertex-N) when set.
inline std::string arc_center_vertex_id(
    const polysmith::core::SketchArc& arc) {
  if (!arc.center_vertex_id.empty()) return arc.center_vertex_id;
  return "point-arc-" + arc.id + "-center";
}
