std::vector<SketchProfileRegion> build_sketch_profile_regions(
    const SketchFeatureParameters& parameters) {
  std::vector<SketchProfileRegion> profiles;

  // Construction lines are reference geometry only — they should not
  // close profile loops. Arcs don't have a construction-line analogue
  // yet (v1 ships with solid arcs only), but the future flag would
  // be applied here in the same way.
  std::vector<ProfileEdge> solid_edges;
  solid_edges.reserve(parameters.lines.size() + parameters.arcs.size());
  std::vector<const SketchCircle*> solid_circles;
  solid_circles.reserve(parameters.circles.size());
  for (const auto& line : parameters.lines) {
    if (!line.is_construction) {
      solid_edges.push_back(profile_edge_from_line(line));
    }
  }
  for (const auto& arc : parameters.arcs) {
    if (!arc.is_construction) {
      solid_edges.push_back(profile_edge_from_arc(arc));
    }
  }
  for (const auto& circle : parameters.circles) {
    if (!circle.is_construction) {
      solid_circles.push_back(&circle);
    }
  }

  const bool has_solid_arcs = std::any_of(
      solid_edges.begin(), solid_edges.end(), [](const ProfileEdge& edge) {
        return edge.kind == ProfileEdge::Kind::Arc;
      });
  const bool has_solid_lines = std::any_of(
      solid_edges.begin(), solid_edges.end(), [](const ProfileEdge& edge) {
        return edge.kind == ProfileEdge::Kind::Line;
      });
  auto line_segments = solid_line_segments(parameters);
  const bool use_circle_arrangement_profiles =
      has_solid_lines && !solid_circles.empty() &&
      any_line_intersects_circle_boundary(line_segments, solid_circles);

  // Build a common arrangement for lines + circles + arcs.  Circles and
  // arcs are both approximated as polygon segments so that tangent lines
  // (which don't share vertex IDs with the host curve) still connect
  // through the shared coordinate graph.
  const bool use_arrangement =
      has_solid_lines &&
      (!solid_circles.empty() || has_solid_arcs);

  if (use_arrangement) {
    auto segments = line_segments;

    // Circle boundary polygons.
    for (const auto* circle : solid_circles) {
      auto circle_segments = circle_boundary_segments(*circle);
      split_circle_segments_at_line_endpoints(circle_segments, *circle,
                                              parameters.lines);
      segments.insert(segments.end(), circle_segments.begin(),
                      circle_segments.end());
    }

    // Arc boundary polygons — sample only the arc's sweep and split
    // at line endpoints so tangent lines connect through shared
    // coordinates (same strategy as split_circle_segments_at_line_endpoints
    // for circles).  Without the split the line endpoint and nearest
    // polyline vertex may be further apart than kProfileTolerance.
    for (const auto& arc : parameters.arcs) {
      if (arc.is_construction) continue;
      if (arc.radius <= kProfileTolerance) continue;

      const double start_angle = std::atan2(arc.start_y - arc.center_y,
                                             arc.start_x - arc.center_x);
      const double end_angle   = std::atan2(arc.end_y   - arc.center_y,
                                             arc.end_x   - arc.center_x);
      double sweep = end_angle - start_angle;
      if (arc.ccw) {
        while (sweep <= 0.0) sweep += 2.0 * kPi;
      } else {
        while (sweep >= 0.0) sweep -= 2.0 * kPi;
      }

      // Collect tangent point angles for this arc.
      auto norm = [](double a) {
        a = std::fmod(a, 2.0 * kPi);
        return a < 0.0 ? a + 2.0 * kPi : a;
      };
      auto in_sweep = [&](double a) {
        const double ns = norm(start_angle);
        const double ne = norm(end_angle);
        const double na = norm(a);
        if (arc.ccw) {
          if (ns <= ne) return na >= ns && na <= ne;
          return na >= ns || na <= ne;
        }
        if (ns >= ne) return na <= ns && na >= ne;
        return na <= ns || na >= ne;
      };

      std::vector<double> split_angles;
      for (const auto& line : parameters.lines) {
        if (line.is_construction) continue;
        {
          const double px = line.start_x;
          const double py = line.start_y;
          const double dx = px - arc.center_x;
          const double dy = py - arc.center_y;
          const double dist = std::sqrt(dx * dx + dy * dy);
          if (std::abs(dist - arc.radius) <= kProfileTolerance) {
            const double a = std::atan2(dy, dx);
            if (in_sweep(a)) split_angles.push_back(a);
          }
        }
        {
          const double px = line.end_x;
          const double py = line.end_y;
          const double dx = px - arc.center_x;
          const double dy = py - arc.center_y;
          const double dist = std::sqrt(dx * dx + dy * dy);
          if (std::abs(dist - arc.radius) > kProfileTolerance) continue;
          const double a = std::atan2(dy, dx);
          if (!in_sweep(a)) continue;
          split_angles.push_back(a);
        }
      }

      constexpr int kArcSegments = 64;
      std::vector<SketchProfilePoint> pts;
      pts.reserve(kArcSegments + 1 + split_angles.size());
      for (int i = 0; i <= kArcSegments; ++i) {
        const double t = static_cast<double>(i) / kArcSegments;
        const double a = start_angle + sweep * t;
        pts.push_back(SketchProfilePoint{
            .x = arc.center_x + arc.radius * std::cos(a),
            .y = arc.center_y + arc.radius * std::sin(a),
        });
      }

      // Insert split points sorted by angular position along the sweep.
      if (!split_angles.empty()) {
        std::sort(split_angles.begin(), split_angles.end(),
                  [&](double a, double b) {
                    auto dist = [&](double ang) {
                      double da = norm(ang) - norm(start_angle);
                      if (arc.ccw) { while (da < 0.0) da += 2.0 * kPi; }
                      else         { while (da > 0.0) da -= 2.0 * kPi; }
                      return da;
                    };
                    return dist(a) < dist(b);
                  });
        // Deduplicate and filter those already at endpoints.
        split_angles.erase(
            std::unique(split_angles.begin(), split_angles.end(),
                        [&](double a, double b) {
                          return std::abs(norm(a) - norm(b)) < 1e-12;
                        }),
            split_angles.end());
        // Insert each split point as a new vertex.
        std::vector<SketchProfilePoint> merged;
        merged.reserve(pts.size() + split_angles.size());
        size_t si = 0;
        for (const auto& pt : pts) {
          // Insert any split angles that fall between the previous and
          // current sample point along the sweep direction.
          double pt_a = std::atan2(pt.y - arc.center_y, pt.x - arc.center_x);
          while (si < split_angles.size()) {
            auto dist = [&](double ang) {
              double da = norm(ang) - norm(start_angle);
              if (arc.ccw) { while (da < 0.0) da += 2.0 * kPi; }
              else         { while (da > 0.0) da -= 2.0 * kPi; }
              return da;
            };
            if (dist(split_angles[si]) < dist(pt_a) + 1e-12) {
              merged.push_back(SketchProfilePoint{
                  .x = arc.center_x + arc.radius * std::cos(split_angles[si]),
                  .y = arc.center_y + arc.radius * std::sin(split_angles[si]),
              });
              ++si;
            } else {
              break;
            }
          }
          merged.push_back(pt);
        }
        pts = std::move(merged);
      }

      for (size_t i = 0; i < pts.size() - 1; ++i) {
        segments.push_back(LineSegment{
            .line_id = arc.id,
            .start = pts[i],
            .end = pts[i + 1],
        });
      }
    }

    const auto arrangement_faces = detect_arrangement_faces(segments);

    auto unsplit_source_circle_id =
        [&](const ArrangementFace& face) -> std::optional<std::string> {
      if (face.line_ids.size() != 1) {
        return std::nullopt;
      }
      const std::string& only_id = face.line_ids.front();
      const auto circle_it = std::find_if(
          solid_circles.begin(), solid_circles.end(),
          [&](const SketchCircle* circle) { return circle->id == only_id; });
      if (circle_it == solid_circles.end()) {
        return std::nullopt;
      }
      return only_id;
    };

    for (const auto& face : arrangement_faces) {
      profiles.push_back(SketchProfileRegion{
          .id = make_polygon_profile_id(face.line_ids) + "-" +
                make_polygon_profile_id(face.vertex_ids),
          .kind = "polygon",
          .vertex_ids = face.vertex_ids,
          .line_ids = face.line_ids,
          .ordered_edge_ids = face.ordered_line_ids,
          .points = face.points,
          .inner_loops = {},
          .source_circle_id = unsplit_source_circle_id(face),
          .center_x = 0.0,
          .center_y = 0.0,
          .radius = 0.0,
      });
    }
  } else if (!has_solid_arcs) {
    const auto arrangement_faces =
        detect_line_arrangement_faces(parameters);
    for (const auto& face : arrangement_faces) {
      profiles.push_back(SketchProfileRegion{
          .id = make_polygon_profile_id(face.line_ids) + "-" +
                make_polygon_profile_id(face.vertex_ids),
          .kind = "polygon",
          .vertex_ids = face.vertex_ids,
          .line_ids = face.line_ids,
          .ordered_edge_ids = face.ordered_line_ids,
          .points = face.points,
          .inner_loops = {},
          .source_circle_id = std::nullopt,
          .center_x = 0.0,
          .center_y = 0.0,
          .radius = 0.0,
      });
    }
  }

  // When the arrangement path is active the arrangement already
  // handles all profile regions including arcs — the edge-loop pass
  // only creates duplicate profiles that confuse hole nesting.
  if (!use_arrangement) {
    // Split arc edges at line endpoints that lie on the arc, so tangent
    // lines connect to the arc in the edge-loop graph (same principle as
    // split_circle_segments_at_line_endpoints for circles).
    {
    auto norm_angle = [](double a) {
      a = std::fmod(a, 2.0 * kPi);
      if (a < 0.0) a += 2.0 * kPi;
      return a;
    };
    auto angle_in_sweep = [&](double a, double ns, double ne, bool ccw) {
      double na = norm_angle(a);
      if (ccw) {
        if (ns <= ne) return na >= ns && na <= ne;
        return na >= ns || na <= ne;
      }
      if (ns >= ne) return na <= ns && na >= ne;
      return na <= ns || na >= ne;
    };

    std::vector<ProfileEdge> split_arc_edges;
    for (const auto& edge : solid_edges) {
      if (edge.kind != ProfileEdge::Kind::Arc) continue;

      const double ns = norm_angle(std::atan2(edge.start_y - edge.center_y, edge.start_x - edge.center_x));
      const double ne = norm_angle(std::atan2(edge.end_y   - edge.center_y, edge.end_x   - edge.center_x));

      // Collect split points (angles of line endpoints on this arc).
      std::vector<double> split_angles;
      for (const auto& line : parameters.lines) {
        if (line.is_construction) continue;
        for (bool is_start : {true, false}) {
          const double px = is_start ? line.start_x : line.end_x;
          const double py = is_start ? line.start_y : line.end_y;
          const double dx = px - edge.center_x;
          const double dy = py - edge.center_y;
          const double dist = std::sqrt(dx * dx + dy * dy);
          if (std::abs(dist - edge.radius) > kProfileTolerance) continue;
          const double pt_angle = std::atan2(py - edge.center_y, px - edge.center_x);
          if (!angle_in_sweep(pt_angle, ns, ne, edge.ccw)) continue;
          if (points_match(SketchProfilePoint{.x=px,.y=py},
                           SketchProfilePoint{.x=edge.start_x,.y=edge.start_y}) ||
              points_match(SketchProfilePoint{.x=px,.y=py},
                           SketchProfilePoint{.x=edge.end_x,.y=edge.end_y})) continue;
          split_angles.push_back(pt_angle);
        }
      }

      if (split_angles.empty()) {
        split_arc_edges.push_back(edge);
        continue;
      }

      // Sort by angular position along the sweep.
      const double sns = ns;
      const bool sccw = edge.ccw;
      std::sort(split_angles.begin(), split_angles.end(),
                [&](double a, double b) {
                  auto dist_from_start = [sns, sccw](double ang) {
                    double da = std::fmod(ang, 2.0 * kPi) - sns;
                    if (da < 0.0) da += 2.0 * kPi;
                    if (sccw) { while (da < 0.0) da += 2.0 * kPi; }
                    else      { while (da > 0.0) da -= 2.0 * kPi; }
                    return da;
                  };
                  return dist_from_start(a) < dist_from_start(b);
                });
      // Deduplicate.
      split_angles.erase(std::unique(split_angles.begin(), split_angles.end(),
                                     [](double a, double b) {
                                       auto n = [](double v) {
                                         v = std::fmod(v, 2.0 * kPi);
                                         return v < 0.0 ? v + 2.0 * kPi : v;
                                       };
                                       return std::abs(n(a) - n(b)) < 1e-12;
                                     }),
                         split_angles.end());

      // Build edge pieces.
      double prev_x = edge.start_x;
      double prev_y = edge.start_y;
      for (double a : split_angles) {
        const double sx = edge.center_x + edge.radius * std::cos(a);
        const double sy = edge.center_y + edge.radius * std::sin(a);
        split_arc_edges.push_back(ProfileEdge{
            .id = edge.id, .kind = ProfileEdge::Kind::Arc,
            .start_vertex_id = "", .end_vertex_id = "",
            .start_x = prev_x, .start_y = prev_y,
            .end_x = sx, .end_y = sy,
            .center_x = edge.center_x, .center_y = edge.center_y,
            .radius = edge.radius, .ccw = edge.ccw,
        });
        prev_x = sx;
        prev_y = sy;
      }
      split_arc_edges.push_back(ProfileEdge{
          .id = edge.id, .kind = ProfileEdge::Kind::Arc,
          .start_vertex_id = "", .end_vertex_id = "",
          .start_x = prev_x, .start_y = prev_y,
          .end_x = edge.end_x, .end_y = edge.end_y,
          .center_x = edge.center_x, .center_y = edge.center_y,
          .radius = edge.radius, .ccw = edge.ccw,
      });
    }
    // Replace arc edges with split versions, keeping line edges as-is.
    std::vector<ProfileEdge> new_edges;
    for (const auto& edge : solid_edges) {
      if (edge.kind != ProfileEdge::Kind::Arc) {
        new_edges.push_back(edge);
      }
    }
    new_edges.insert(new_edges.end(), split_arc_edges.begin(), split_arc_edges.end());
    solid_edges = std::move(new_edges);
  }

  // Arc / line edge-loop profiles (when solid arcs are present).
  for (const auto& component : split_edge_components(solid_edges)) {
    if (const auto loop = detect_edge_loop(component); loop.has_value()) {
      profiles.push_back(SketchProfileRegion{
          .id = make_polygon_profile_id(loop->edge_ids),
          .kind = "polygon",
          .vertex_ids = loop->vertex_ids,
          .line_ids = loop->edge_ids,
          .ordered_edge_ids = loop->edge_ids,
          .points = loop->points,
          .inner_loops = {},
          .source_circle_id = std::nullopt,
          .center_x = 0.0,
          .center_y = 0.0,
          .radius = 0.0,
      });
    }
  }
  }  // if (!use_arrangement)

  apply_nested_polygon_holes(profiles);

  if (use_arrangement) {
    return profiles;
  }

  std::map<std::string, std::vector<std::vector<SketchProfilePoint>>>
      circle_inner_loops;
  std::map<std::string, std::vector<std::string>> circle_hole_ids;

  for (const auto* circle : solid_circles) {
    const SketchProfilePoint center{.x = circle->center_x, .y = circle->center_y};
    SketchProfileRegion* containing_profile = nullptr;
    double containing_area = std::numeric_limits<double>::max();
    for (auto& profile : profiles) {
      if (profile.kind != "polygon" || !point_in_polygon(center, profile.points)) {
        continue;
      }
      const double area = std::abs(polygon_signed_area(profile.points));
      if (area < containing_area) {
        containing_profile = &profile;
        containing_area = area;
      }
    }

    const SketchCircle* containing_circle = nullptr;
    double containing_circle_area = std::numeric_limits<double>::max();
    for (const auto* candidate : solid_circles) {
      if (!circle_contains_circle(*candidate, *circle)) {
        continue;
      }
      const double area = circle_area(*candidate);
      if (area < containing_circle_area) {
        containing_circle = candidate;
        containing_circle_area = area;
      }
    }

    if (containing_circle != nullptr &&
        containing_circle_area < containing_area) {
      circle_inner_loops[containing_circle->id].push_back(
          sample_circle_loop(*circle));
      circle_hole_ids[containing_circle->id].push_back(circle->id);
    } else if (containing_profile != nullptr) {
      containing_profile->inner_loops.push_back(sample_circle_loop(*circle));
      containing_profile->id += "-hole-" + circle->id;
    }
  }

  for (const auto* circle : solid_circles) {
    const auto hole_it = circle_inner_loops.find(circle->id);
    if (hole_it != circle_inner_loops.end()) {
      std::string profile_id = "profile-circle-" + circle->id;
      const auto hole_id_it = circle_hole_ids.find(circle->id);
      if (hole_id_it != circle_hole_ids.end()) {
        for (const auto& hole_id : hole_id_it->second) {
          profile_id += "-hole-" + hole_id;
        }
      }
      profiles.push_back(SketchProfileRegion{
          .id = profile_id,
          .kind = "polygon",
          .vertex_ids = {circle_center_vertex_id(*circle)},
          .line_ids = {},
          .ordered_edge_ids = {circle->id},
          .points = sample_circle_loop(*circle),
          .inner_loops = hole_it->second,
          .source_circle_id = circle->id,
          .center_x = 0.0,
          .center_y = 0.0,
          .radius = 0.0,
      });
      continue;
    }
    profiles.push_back(SketchProfileRegion{
        .id = "profile-circle-" + circle->id,
        .kind = "circle",
        .vertex_ids = {circle_center_vertex_id(*circle)},
        .line_ids = {},
        .points = {},
        .inner_loops = {},
        .source_circle_id = circle->id,
        .center_x = circle->center_x,
        .center_y = circle->center_y,
        .radius = circle->radius,
    });
  }

  return profiles;
}
