// After circle→arc conversion, ensure that any line that the arc
// endpoints lie on (in the interior, not at existing endpoints) is
// split at those positions so the arc and line share point IDs.
// Without this the profile loop detector sees the arc and line as
// disconnected components and won't detect the closed region.
{
  // Pre-compute the arc endpoint coords + vertex ids — the arc
  // reference may be invalidated if we push to params.arcs (we don't
  // here, but be safe).
  const double asx = params.arcs.back().start_x;
  const double asy = params.arcs.back().start_y;
  const double aex = params.arcs.back().end_x;
  const double aey = params.arcs.back().end_y;
  const std::string as_id = params.arcs.back().start_vertex_id;
  const std::string ae_id = params.arcs.back().end_vertex_id;

  // The split point IS the arc endpoint — the whole point of splitting
  // the line is that the two entities share one vertex. Reuse the arc
  // endpoint's resolved vertex id instead of minting a fresh one (and
  // never fall back to a size-based scheme: next_trim_entity_index
  // exists precisely because ids must not be reused after trims).
  auto split_line_at_interior_point =
      [&](double px, double py, const std::string& split_pt_id) -> bool {
    // Index-based iteration — avoid iterator invalidation when we
    // push_back a new line inside the loop.
    const size_t n = params.lines.size();
    for (size_t i = 0; i < n; ++i) {
      auto& line = params.lines[i];
      const double dx = line.end_x - line.start_x;
      const double dy = line.end_y - line.start_y;
      const double len_sq = dx * dx + dy * dy;
      if (len_sq < 1e-12) continue;

      const double t =
          ((px - line.start_x) * dx + (py - line.start_y) * dy) / len_sq;
      // Only split interior points — endpoints are already handled by
      // the shared-point resolution above.
      if (t <= 0.001 || t >= 0.999) continue;

      const double proj_x = line.start_x + t * dx;
      const double proj_y = line.start_y + t * dy;
      if (!points_match(proj_x, proj_y, px, py)) continue;

      // A line that is TANGENT to the circle here only TOUCHES it. Splitting
      // such a line leaves a collinear stub past the tangent point — the
      // "trim adds 2 small segments" report: a snapped tangent line usually
      // overshoots the true tangent point by a fraction of a mm to a few mm,
      // which is enough for the split to fire. Truncate the overshooting end
      // to the tangent point instead: the line stays a single piece, still
      // shares the arc's endpoint vertex, and the stub never appears.
      {
        const double len = std::sqrt(len_sq);
        const double ldx = dx / len;
        const double ldy = dy / len;
        const auto& new_arc = params.arcs.back();
        const double centre_dist =
            std::abs((new_arc.center_x - line.start_x) * ldy -
                     (new_arc.center_y - line.start_y) * ldx);
        // The relation is the authoritative "this line is tangent" signal;
        // the geometric test catches one that merely snaps close (its
        // point_on_circle pin keeps the endpoint on the boundary but does
        // not guarantee exact tangency).
        bool tangent = std::abs(centre_dist - new_arc.radius) < 0.5;
        if (!tangent) {
          for (const auto& rel : params.line_relations) {
            if (rel.kind == "tangent_line_circle" &&
                rel.first_line_id == line.id &&
                rel.second_line_id == entity_id) {
              tangent = true;
              break;
            }
          }
        }
        if (tangent) {
          if (t <= 0.5) {
            // Overshoot is on the start side — keep the long end.
            line.start_x = px;
            line.start_y = py;
            line.start_vertex_id = split_pt_id;
          } else {
            // Overshoot is on the end side — keep the long start.
            line.end_x = px;
            line.end_y = py;
            line.end_vertex_id = split_pt_id;
          }
          line.constraint = std::nullopt;
          return true;
        }
      }

      // Split: original line shortens to split point, new line
      // continues from split point to old end.
      const double old_end_x = line.end_x;
      const double old_end_y = line.end_y;
      const std::string old_end_id = line.end_vertex_id;

      line.end_x = px;
      line.end_y = py;
      line.end_vertex_id = split_pt_id;
      line.constraint = std::nullopt;

      std::vector<std::string> line_ids;
      line_ids.reserve(params.lines.size());
      for (const auto& l : params.lines) line_ids.push_back(l.id);
      const int next_idx = next_trim_entity_index(line_ids);
      params.lines.push_back(SketchLine{
          .id = "line-" + std::to_string(next_idx),
          .start_vertex_id = split_pt_id,
          .end_vertex_id = old_end_id,
          .start_x = px,
          .start_y = py,
          .end_x = old_end_x,
          .end_y = old_end_y,
          .constraint = std::nullopt,
          .is_construction = line.is_construction,
      });

      return true;
    }
    return false;
  };

  // Check and fix both arc endpoints.  Do start first since end may
  // lie on the same line; after the first split the second check will
  // find the right segment (original shortened or newly-created half).
  split_line_at_interior_point(asx, asy, as_id);
  split_line_at_interior_point(aex, aey, ae_id);
}
