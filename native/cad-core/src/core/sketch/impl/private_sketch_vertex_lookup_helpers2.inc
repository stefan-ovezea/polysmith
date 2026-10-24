SketchVertex* find_sketch_vertex(SketchFeatureParameters& parameters,
                               const std::string& vertex_id) {
  const auto vertex_it = std::find_if(
      parameters.vertices.begin(),
      parameters.vertices.end(),
      [&](const SketchVertex& v) { return v.id == vertex_id; });
  return vertex_it == parameters.vertices.end() ? nullptr : &(*vertex_it);
}

const SketchVertex* find_sketch_vertex(const SketchFeatureParameters& parameters,
                                     const std::string& vertex_id) {
  const auto vertex_it = std::find_if(
      parameters.vertices.begin(),
      parameters.vertices.end(),
      [&](const SketchVertex& v) { return v.id == vertex_id; });
  return vertex_it == parameters.vertices.end() ? nullptr : &(*vertex_it);
}

bool vertex_is_fixed(const SketchFeatureParameters& parameters,
                    const std::string& vertex_id) {
  const SketchVertex* v = find_sketch_vertex(parameters, vertex_id);
  return v != nullptr && v->is_fixed;
}

// True iff `vertex_id` is anchored to a host line (midpoint or
// parametric). Used by the H/V rigid-translation branch in
// `propagate_connected_point_move` to suppress its
// length-preserving move of the *other* endpoint when that other
// endpoint is itself going to be re-pulled by an anchor — without
// this guard, the rigid translation overrides the anchor's target
// position and the line keeps its old length, drifting out of the
// host geometry (see the rectangle midpoint-line repro).
bool vertex_is_anchored_to_line(const SketchFeatureParameters& parameters,
                               const std::string& vertex_id) {
  for (const auto& anchor : parameters.midpoint_anchors) {
    if (anchor.vertex_id == vertex_id) {
      return true;
    }
  }
  for (const auto& anchor : parameters.point_line_anchors) {
    if (anchor.vertex_id == vertex_id) {
      return true;
    }
  }
  return false;
}

// True iff a stored relation pins this line's DIRECTION — a parallel or
// perpendicular pair fixes the angle it must keep, exactly the way a
// horizontal/vertical constraint does. `propagate_connected_point_move` uses
// this to carry the far endpoint along when a shared corner moves: translating
// a direction-constrained line keeps its length AND its angle, while moving only
// the shared corner would rotate it (and then the relation pass would "repair"
// the rotation by preserving the stretched length — which is how widening a
// rectangle used to shrink its height).
bool line_direction_is_related(const SketchFeatureParameters& parameters,
                               const std::string& line_id) {
  for (const auto& relation : parameters.line_relations) {
    if (relation.kind != "parallel" && relation.kind != "perpendicular") {
      continue;
    }
    if (relation.first_line_id == line_id ||
        relation.second_line_id == line_id) {
      return true;
    }
  }
  return false;
}