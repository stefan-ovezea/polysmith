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
      !has_solid_arcs && has_solid_lines && !solid_circles.empty() &&
      any_line_intersects_circle_boundary(line_segments, solid_circles);

  if (!has_solid_arcs) {
    std::vector<ArrangementFace> arrangement_faces;
    if (use_circle_arrangement_profiles) {
      auto segments = line_segments;
      for (const auto* circle : solid_circles) {
        auto circle_segments = circle_boundary_segments(*circle);
        segments.insert(segments.end(), circle_segments.begin(),
                        circle_segments.end());
      }
      arrangement_faces = detect_arrangement_faces(segments);
    } else {
      arrangement_faces = detect_line_arrangement_faces(parameters);
    }

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
                make_polygon_profile_id(face.point_ids),
          .kind = "polygon",
          .point_ids = face.point_ids,
          .line_ids = face.line_ids,
          .points = face.points,
          .inner_loops = {},
          .source_circle_id = unsplit_source_circle_id(face),
          .center_x = 0.0,
          .center_y = 0.0,
          .radius = 0.0,
      });
    }
  } else {
    for (const auto& component : split_edge_components(solid_edges)) {
      if (const auto loop = detect_edge_loop(component); loop.has_value()) {
        profiles.push_back(SketchProfileRegion{
            .id = make_polygon_profile_id(loop->edge_ids),
            .kind = "polygon",
            .point_ids = loop->point_ids,
            // `line_ids` historically held line ids; with arcs in the
            // mix this field carries mixed line + arc edge ids. The
            // downstream UI uses it to compute "which entities does
            // this profile depend on" for cascade-delete, which works
            // as long as the ids resolve to entities.
            .line_ids = loop->edge_ids,
            .points = loop->points,
            .inner_loops = {},
            .source_circle_id = std::nullopt,
            .center_x = 0.0,
            .center_y = 0.0,
            .radius = 0.0,
        });
      }
    }
  }

  apply_nested_polygon_holes(profiles);

  if (use_circle_arrangement_profiles) {
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
          .point_ids = {"point-circle-" + circle->id + "-center"},
          .line_ids = {},
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
        .point_ids = {"point-circle-" + circle->id + "-center"},
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
