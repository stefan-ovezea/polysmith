  // Build pre-computed snap candidates for the active sketch.
  std::vector<SnapCandidate> snap_candidates;
  if (document->active_sketch_feature_id.has_value()) {
    for (const auto& feat : document->feature_history) {
      if (feat.id == document->active_sketch_feature_id.value() &&
          feat.sketch_parameters.has_value()) {
        const auto& sketch = feat.sketch_parameters.value();
        const auto& filter = document->selection_filter;
        // Endpoints
        if (filter.snap_endpoint) {
          for (const auto& line : sketch.lines) {
            if (line.is_construction && !filter.select_construction) continue;
            snap_candidates.push_back({.kind="endpoint",.entity_id=line.id,.vertex_id=line.start_vertex_id,.local_x=line.start_x,.local_y=line.start_y,.distance=0,.label="Endpoint"});
            snap_candidates.push_back({.kind="endpoint",.entity_id=line.id,.vertex_id=line.end_vertex_id,.local_x=line.end_x,.local_y=line.end_y,.distance=0,.label="Endpoint"});
          }
          for (const auto& arc : sketch.arcs) {
            if (arc.is_construction && !filter.select_construction) continue;
            snap_candidates.push_back({.kind="endpoint",.entity_id=arc.id,.vertex_id=arc.start_vertex_id,.local_x=arc.start_x,.local_y=arc.start_y,.distance=0,.label="Endpoint"});
            snap_candidates.push_back({.kind="endpoint",.entity_id=arc.id,.vertex_id=arc.end_vertex_id,.local_x=arc.end_x,.local_y=arc.end_y,.distance=0,.label="Endpoint"});
          }
        }
        // Midpoints
        if (filter.snap_midpoint) {
          for (const auto& line : sketch.lines) {
            if (line.is_construction && !filter.select_construction) continue;
            const double mx = (line.start_x + line.end_x) / 2.0;
            const double my = (line.start_y + line.end_y) / 2.0;
            snap_candidates.push_back({.kind="midpoint",.entity_id=line.id,.vertex_id="",.local_x=mx,.local_y=my,.distance=0,.label="Midpoint"});
          }
        }
        // Centers
        if (filter.snap_center) {
          for (const auto& circle : sketch.circles) {
            if (circle.is_construction && !filter.select_construction) continue;
            snap_candidates.push_back({.kind="center",.entity_id=circle.id,.vertex_id="",.local_x=circle.center_x,.local_y=circle.center_y,.distance=0,.label="Center"});
          }
          for (const auto& arc : sketch.arcs) {
            if (arc.is_construction && !filter.select_construction) continue;
            snap_candidates.push_back({.kind="center",.entity_id=arc.id,.vertex_id="",.local_x=arc.center_x,.local_y=arc.center_y,.distance=0,.label="Center"});
          }
          for (const auto& poly : sketch.polygons) {
            if (poly.is_construction && !filter.select_construction) continue;
            snap_candidates.push_back({.kind="center",.entity_id=poly.id,.vertex_id="",.local_x=poly.center_x,.local_y=poly.center_y,.distance=0,.label="Center"});
          }
        }
        // Quadrant points (4 per circle)
        if (filter.snap_quadrant) {
          for (const auto& circle : sketch.circles) {
            if (circle.is_construction && !filter.select_construction) continue;
            const double cx = circle.center_x;
            const double cy = circle.center_y;
            const double r = circle.radius;
            snap_candidates.push_back({.kind="quadrant",.entity_id=circle.id,.vertex_id="",.local_x=cx+r,.local_y=cy,.distance=0,.label="Quadrant"});
            snap_candidates.push_back({.kind="quadrant",.entity_id=circle.id,.vertex_id="",.local_x=cx,.local_y=cy+r,.distance=0,.label="Quadrant"});
            snap_candidates.push_back({.kind="quadrant",.entity_id=circle.id,.vertex_id="",.local_x=cx-r,.local_y=cy,.distance=0,.label="Quadrant"});
            snap_candidates.push_back({.kind="quadrant",.entity_id=circle.id,.vertex_id="",.local_x=cx,.local_y=cy-r,.distance=0,.label="Quadrant"});
          }
        }
        // Intersection points (line-line and line-arc)
        if (filter.snap_intersection) {
          // Line-line intersections
          for (size_t i = 0; i < sketch.lines.size(); ++i) {
            const auto& a = sketch.lines[i];
            if (a.is_construction && !filter.select_construction) continue;
            for (size_t j = i + 1; j < sketch.lines.size(); ++j) {
              const auto& b = sketch.lines[j];
              if (b.is_construction && !filter.select_construction) continue;
              const double a_dx = a.end_x - a.start_x;
              const double a_dy = a.end_y - a.start_y;
              const double b_dx = b.end_x - b.start_x;
              const double b_dy = b.end_y - b.start_y;
              const double denom = a_dx * b_dy - a_dy * b_dx;
              if (std::abs(denom) < 1e-12) continue;
              const double t = ((b.start_x - a.start_x) * b_dy - (b.start_y - a.start_y) * b_dx) / denom;
              const double u = ((b.start_x - a.start_x) * a_dy - (b.start_y - a.start_y) * a_dx) / denom;
              if (t < 0.0 || t > 1.0 || u < 0.0 || u > 1.0) continue;
              snap_candidates.push_back({.kind="intersection",.entity_id=a.id,.vertex_id="",.local_x=a.start_x+t*a_dx,.local_y=a.start_y+t*a_dy,.distance=0,.label="Intersection"});
            }
          }
          // Line-arc intersections
          for (const auto& line : sketch.lines) {
            if (line.is_construction && !filter.select_construction) continue;
            const double dx = line.end_x - line.start_x;
            const double dy = line.end_y - line.start_y;
            const double len_sq = dx * dx + dy * dy;
            if (len_sq < 1e-12) continue;
            for (const auto& arc : sketch.arcs) {
              if (arc.is_construction && !filter.select_construction) continue;
              const double r = std::hypot(arc.start_x - arc.center_x, arc.start_y - arc.center_y);
              const double fx = line.start_x - arc.center_x;
              const double fy = line.start_y - arc.center_y;
              const double a_val = len_sq;
              const double b_val = 2.0 * (fx * dx + fy * dy);
              const double c_val = fx * fx + fy * fy - r * r;
              double disc = b_val * b_val - 4.0 * a_val * c_val;
              if (disc < 0) continue;
              disc = std::sqrt(disc);
              for (double sign : {-1.0, 1.0}) {
                const double t = (-b_val + sign * disc) / (2.0 * a_val);
                if (t < 0.0 || t > 1.0) continue;
                snap_candidates.push_back({.kind="intersection",.entity_id=line.id,.vertex_id="",.local_x=line.start_x+t*dx,.local_y=line.start_y+t*dy,.distance=0,.label="Intersection"});
              }
            }
          }
        }
        // grid_line, polar, tangent, nearest, and perpendicular are
        // cursor-position-dependent and handled by the TS dynamic snap
        // path — they are not pre-computed here.
        break;
      }
    }
  }
