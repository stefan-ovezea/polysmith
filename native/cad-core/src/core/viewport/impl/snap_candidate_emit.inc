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
          for (const auto& arc : sketch.arcs) {
            if (arc.is_construction && !filter.select_construction) continue;
            const double r = arc.radius;
            const double start_angle = std::atan2(arc.start_y - arc.center_y, arc.start_x - arc.center_x);
            const double end_angle   = std::atan2(arc.end_y   - arc.center_y, arc.end_x   - arc.center_x);
            double sweep = end_angle - start_angle;
            if (arc.ccw) {
              while (sweep <= 0.0) sweep += 2.0 * M_PI;
            } else {
              while (sweep >= 0.0) sweep -= 2.0 * M_PI;
            }
            const double mid_angle = start_angle + sweep * 0.5;
            snap_candidates.push_back({.kind="midpoint",.entity_id=arc.id,.vertex_id="",
                .local_x=arc.center_x + r * std::cos(mid_angle),
                .local_y=arc.center_y + r * std::sin(mid_angle),
                .distance=0,.label="Midpoint"});
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
        // Quadrant points (4 per circle, plus cardinal points within arc sweeps)
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
          for (const auto& arc : sketch.arcs) {
            if (arc.is_construction && !filter.select_construction) continue;
            const double cx = arc.center_x;
            const double cy = arc.center_y;
            const double r = arc.radius;
            // Normalise to [0, 2π) so angles at the ±π boundary don't
            // bounce and break the sweep test.
            auto norm = [](double angle) {
              angle = std::fmod(angle, 2.0 * M_PI);
              if (angle < 0.0) angle += 2.0 * M_PI;
              return angle;
            };
            const double ns = norm(std::atan2(arc.start_y - cy, arc.start_x - cx));
            const double ne = norm(std::atan2(arc.end_y   - cy, arc.end_x   - cx));
            // Cardinal angles in sketch-plane coords: 0°=east, 90°=north, 180°=west, 270°=south.
            const double cardinals[4] = {0.0, M_PI_2, M_PI, M_PI + M_PI_2};
            for (const double angle : cardinals) {
              const double a = norm(angle);
              // Check whether `a` lies within the sweep from start to end.
              bool on_arc = false;
              if (arc.ccw) {
                if (ns <= ne) {
                  on_arc = a >= ns && a <= ne;
                } else {
                  on_arc = a >= ns || a <= ne;
                }
              } else {
                if (ns >= ne) {
                  on_arc = a <= ns && a >= ne;
                } else {
                  on_arc = a <= ns || a >= ne;
                }
              }
              if (!on_arc) continue;
              snap_candidates.push_back({.kind="quadrant",.entity_id=arc.id,.vertex_id="",
                  .local_x=cx + r * std::cos(a),.local_y=cy + r * std::sin(a),
                  .distance=0,.label="Quadrant"});
            }
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
          // Arc-arc intersections
          for (size_t i = 0; i < sketch.arcs.size(); ++i) {
            const auto& a = sketch.arcs[i];
            if (a.is_construction && !filter.select_construction) continue;
            for (size_t j = i + 1; j < sketch.arcs.size(); ++j) {
              const auto& b = sketch.arcs[j];
              if (b.is_construction && !filter.select_construction) continue;
              const double dx = b.center_x - a.center_x;
              const double dy = b.center_y - a.center_y;
              const double d = std::hypot(dx, dy);
              if (d < 1e-12) continue;                      // concentric
              if (d > a.radius + b.radius + 1e-12) continue; // too far
              if (d < std::abs(a.radius - b.radius) - 1e-12) continue; // one inside other
              const double d2 = d * d;
              const double r1sq = a.radius * a.radius;
              const double r2sq = b.radius * b.radius;
              const double a_val = (r1sq - r2sq + d2) / (2.0 * d);
              const double h_sq = r1sq - a_val * a_val;
              if (h_sq < 0.0) continue;
              const double h = std::sqrt(h_sq > 0.0 ? h_sq : 0.0);
              const double px = a.center_x + a_val * dx / d;
              const double py = a.center_y + a_val * dy / d;
              for (double sign : {-1.0, 1.0}) {
                double ix = px + sign * h * (-dy / d);
                double iy = py + sign * h * ( dx / d);
                // Check intersection lies on both arcs.
                auto angle_on_arc = [](const auto& arc, double x, double y) {
                  double angle = std::atan2(y - arc.center_y, x - arc.center_x);
                  double start_a = std::atan2(arc.start_y - arc.center_y, arc.start_x - arc.center_x);
                  double end_a   = std::atan2(arc.end_y   - arc.center_y, arc.end_x   - arc.center_x);
                  double sweep = end_a - start_a;
                  if (arc.ccw) {
                    while (sweep <= 0.0) sweep += 2.0 * M_PI;
                    double da = angle - start_a;
                    while (da < 0.0) da += 2.0 * M_PI;
                    return da <= sweep;
                  }
                  while (sweep >= 0.0) sweep -= 2.0 * M_PI;
                  double da = angle - start_a;
                  while (da > 0.0) da -= 2.0 * M_PI;
                  return da >= sweep;
                };
                if (!angle_on_arc(a, ix, iy) || !angle_on_arc(b, ix, iy)) continue;
                snap_candidates.push_back({.kind="intersection",.entity_id=a.id,.vertex_id="",.local_x=ix,.local_y=iy,.distance=0,.label="Intersection"});
              }
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
