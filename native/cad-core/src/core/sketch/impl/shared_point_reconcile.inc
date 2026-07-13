// After constraint enforcement, multiple lines that share a point ID may
// have been driven to different (x, y) coordinates for that shared point.
// `rebuild_sketch_vertices` deduplicates by point ID and renders a single
// sphere at whichever line's coordinates it encounters first — leaving the
// other line's endpoint visually orphaned.  This pass forces every shared
// point to a single canonical position before the rebuild so the sphere
// matches all lines that reference it.
void reconcile_shared_point_positions(SketchFeatureParameters& parameters) {
  // Build a map from point_id → list of (line_index, is_start_endpoint)
  struct Reference { size_t line_index; bool is_start; };
  std::unordered_map<std::string, std::vector<Reference>> shared;

  for (size_t i = 0; i < parameters.lines.size(); ++i) {
    const auto& line = parameters.lines[i];
    shared[line.start_vertex_id].push_back({i, true});
    shared[line.end_vertex_id].push_back({i, false});
  }

  for (const auto& [point_id, refs] : shared) {
    if (refs.size() <= 1) continue;

    // Pick the canonical position.  Priority:
    //   1. A line with a fixed endpoint at this point (authoritative).
    //   2. A line with an H/V constraint (angle is determined).
    //   3. The first reference (fallback).
    size_t canonical_ref = 0;
    bool found_authoritative = false;
    for (size_t r = 0; r < refs.size(); ++r) {
      const auto& l = parameters.lines[refs[r].line_index];
      const std::string& pt =
          refs[r].is_start ? l.start_vertex_id : l.end_vertex_id;
      if (vertex_is_fixed(parameters, pt)) {
        canonical_ref = r;
        found_authoritative = true;
        break;
      }
    }
    if (!found_authoritative) {
      for (size_t r = 0; r < refs.size(); ++r) {
        if (parameters.lines[refs[r].line_index].constraint.has_value()) {
          canonical_ref = r;
          break;
        }
      }
    }

    const auto& canonical_line = parameters.lines[refs[canonical_ref].line_index];
    const double canonical_x = refs[canonical_ref].is_start
        ? canonical_line.start_x : canonical_line.end_x;
    const double canonical_y = refs[canonical_ref].is_start
        ? canonical_line.start_y : canonical_line.end_y;

    for (size_t r = 1; r < refs.size(); ++r) {
      auto& line = parameters.lines[refs[r].line_index];
      const double current_x = refs[r].is_start ? line.start_x : line.end_x;
      const double current_y = refs[r].is_start ? line.start_y : line.end_y;

      if (!points_match(canonical_x, canonical_y, current_x, current_y)) {
        if (refs[r].is_start) {
          line.start_x = canonical_x;
          line.start_y = canonical_y;
        } else {
          line.end_x = canonical_x;
          line.end_y = canonical_y;
        }
      }
    }
  }
}
