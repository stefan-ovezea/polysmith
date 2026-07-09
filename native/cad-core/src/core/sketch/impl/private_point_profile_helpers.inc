struct LineEndpointRef {
  size_t line_index;
  bool is_start;
};

std::string endpoint_vertex_id(const SketchLine& line, bool is_start) {
  return is_start ? line.start_point_id : line.end_point_id;
}

void set_endpoint_vertex_id(SketchLine& line,
                           bool is_start,
                           const std::string& point_id) {
  if (is_start) {
    line.start_vertex_id = point_id;
    return;
  }

  line.end_vertex_id = point_id;
}

double endpoint_x(const SketchLine& line, bool is_start) {
  return is_start ? line.start_x : line.end_x;
}

double endpoint_y(const SketchLine& line, bool is_start) {
  return is_start ? line.start_y : line.end_y;
}

std::optional<std::tuple<std::string, double, double>> find_coincident_endpoint(
    const SketchFeatureParameters& parameters,
    const std::string& ignored_entity_id,
    double x,
    double y) {
  auto match_line = [&](const SketchLine& candidate) -> bool {
    return candidate.id == ignored_entity_id;
  };
  auto match_arc = [&](const SketchArc& candidate) -> bool {
    return candidate.id == ignored_entity_id;
  };

  for (const auto& candidate : parameters.lines) {
    if (match_line(candidate)) continue;
    if (points_match(candidate.start_x, candidate.start_y, x, y))
      return std::tuple<std::string, double, double>{
          candidate.start_vertex_id, candidate.start_x, candidate.start_y};
    if (points_match(candidate.end_x, candidate.end_y, x, y))
      return std::tuple<std::string, double, double>{
          candidate.end_vertex_id, candidate.end_x, candidate.end_y};
  }
  for (const auto& candidate : parameters.arcs) {
    if (match_arc(candidate)) continue;
    if (points_match(candidate.start_x, candidate.start_y, x, y))
      return std::tuple<std::string, double, double>{
          candidate.start_vertex_id, candidate.start_x, candidate.start_y};
    if (points_match(candidate.end_x, candidate.end_y, x, y))
      return std::tuple<std::string, double, double>{
          candidate.end_vertex_id, candidate.end_x, candidate.end_y};
  }
  return std::nullopt;
}

void rebuild_sketch_vertices(SketchFeatureParameters& parameters) {
  parameters.vertices.clear();

  const auto append_point = [&](const std::string& point_id,
                                const std::string& kind,
                                double x,
                                double y) {
    const auto existing_it = std::find_if(
        parameters.vertices.begin(),
        parameters.vertices.end(),
        [&](const SketchVertex& point) { return point.id == point_id; });
    if (existing_it != parameters.vertices.end()) {
      return;
    }

    parameters.vertices.push_back(SketchVertex{
        .id = point_id,
        .kind = kind,
        .x = x,
        .y = y,
        .is_fixed = false,
    });
  };

  for (const auto& line : parameters.lines) {
    append_point(line.start_vertex_id, "endpoint", line.start_x, line.start_y);
    append_point(line.end_vertex_id, "endpoint", line.end_x, line.end_y);
  }

  for (const auto& arc : parameters.arcs) {
    // Arc endpoints share the line "endpoint" kind so the same
    // hover / snapping / loop-detection logic that already walks
    // endpoint points just works on arcs without special-casing.
    // Coords come from the cached arc params (which are kept in
    // sync at creation; v1 freezes them after that).
    append_point(arc.start_vertex_id, "endpoint", arc.start_x, arc.start_y);
    append_point(arc.end_vertex_id, "endpoint", arc.end_x, arc.end_y);
  }

  // ── Phase 6: centers use stable vertex-N IDs ──────────────────
  for (auto& circle : parameters.circles) {
    // Generate or reuse a stable vertex-N ID for the center.
    if (circle.center_vertex_id.empty()) {
      circle.center_vertex_id =
          "vertex-" + std::to_string(parameters.next_vertex_index++);
    }
    append_point(circle.center_vertex_id, "center",
                 circle.center_x, circle.center_y);

    // Derived quadrant points (cardinal directions on the circle).
    // Quadrants keep the legacy point-circle-*-quadrant-N format for
    // now — they are regenerated on every rebuild and need stable
    // predictable IDs for trim live-point tracking.
    // TODO(phase-6-followup): give quadrants stable vertex-N IDs.
    const double cx = circle.center_x;
    const double cy = circle.center_y;
    const double r = circle.radius;
    parameters.vertices.push_back(SketchVertex{
        .id = "point-circle-" + circle.id + "-quadrant-0",
        .kind = "quadrant",
        .x = cx + r,
        .y = cy,
        .is_fixed = false,
    });
    parameters.vertices.push_back(SketchVertex{
        .id = "point-circle-" + circle.id + "-quadrant-1",
        .kind = "quadrant",
        .x = cx,
        .y = cy + r,
        .is_fixed = false,
    });
    parameters.vertices.push_back(SketchVertex{
        .id = "point-circle-" + circle.id + "-quadrant-2",
        .kind = "quadrant",
        .x = cx - r,
        .y = cy,
        .is_fixed = false,
    });
    parameters.vertices.push_back(SketchVertex{
        .id = "point-circle-" + circle.id + "-quadrant-3",
        .kind = "quadrant",
        .x = cx,
        .y = cy - r,
        .is_fixed = false,
    });
  }

  // Re-emit arc center points with stable vertex-N IDs.
  for (auto& arc : parameters.arcs) {
    if (arc.center_vertex_id.empty()) {
      arc.center_vertex_id =
          "vertex-" + std::to_string(parameters.next_vertex_index++);
    }
    append_point(arc.center_vertex_id, "center",
                 arc.center_x, arc.center_y);
  }

  // Re-emit the original corner point of every fillet. After
  // `add_sketch_fillet` mutates lines A and B to reference the new
  // trim points, no line / arc / circle references the original
  // corner anymore, so without this pass `rebuild_sketch_vertices`
  // would silently drop it. The fillet record carries the cached
  // (`corner_x`, `corner_y`) precisely for this purpose; they're
  // refreshed every recompute by `enforce_sketch_fillets`.
  for (const auto& fillet : parameters.fillets) {
    append_point(fillet.corner_point_id,
                 "endpoint",
                 fillet.corner_x,
                 fillet.corner_y);
  }

  // Free-standing points placed by the Project tool. Same machinery
  // as the fillet corner re-emit above: nothing else references
  // these ids, so without this loop they would vanish on the next
  // recompute. Marked `kind = "projected"` so the renderer / hit
  // testing can give them a distinct visual (a small cross, similar
  // to common CAD behavior). `is_fixed` is forced true below by
  // `enforce_derived_points_fixed` so the user can't drag them.
  for (const auto& projected : parameters.projected_points) {
    append_point(projected.id, "projected", projected.x, projected.y);
  }

  // ── Phase 2: populate geometry_owner_ids ────────────────────
  // Walk every point and record which geometry entities
  // (lines, arcs, circles) own it.  Shared endpoints accumulate
  // multiple owner ids.  This is the foundation for Phase 4's
  // vertex-ID renaming — consumers can look up the owning entity
  // without regex-parsing the point id.

  auto find_point = [&](const std::string& pid) -> SketchVertex* {
    for (auto& p : parameters.vertices) {
      if (p.id == pid) return &p;
    }
    return nullptr;
  };

  // Line endpoints.
  for (const auto& line : parameters.lines) {
    if (auto* sp = find_point(line.start_vertex_id))
      sp->geometry_owner_ids.push_back(line.id);
    if (auto* sp = find_point(line.end_vertex_id))
      sp->geometry_owner_ids.push_back(line.id);
  }

  // Arc endpoints and centers.
  for (const auto& arc : parameters.arcs) {
    if (auto* sp = find_point(arc.start_vertex_id))
      sp->geometry_owner_ids.push_back(arc.id);
    if (auto* sp = find_point(arc.end_vertex_id))
      sp->geometry_owner_ids.push_back(arc.id);
    // Phase 6: center ID is now center_vertex_id (vertex-N)
    if (!arc.center_vertex_id.empty())
      if (auto* sp = find_point(arc.center_vertex_id))
        sp->geometry_owner_ids.push_back(arc.id);
  }

  // Circle centers and quadrants.
  for (const auto& circle : parameters.circles) {
    // Phase 6: center ID is now center_vertex_id (vertex-N)
    if (!circle.center_vertex_id.empty())
      if (auto* sp = find_point(circle.center_vertex_id))
        sp->geometry_owner_ids.push_back(circle.id);
    // Quadrants still use legacy format (see TODO above).
    for (int q = 0; q < 4; ++q) {
      const auto qid = "point-circle-" + circle.id + "-quadrant-" + std::to_string(q);
      if (auto* sp = find_point(qid))
        sp->geometry_owner_ids.push_back(circle.id);
    }
  }

  // ── Phase 1 (kept): projection metadata ─────────────────────
  for (auto& point : parameters.vertices) {
    if (point.kind == "projected") {
      point.is_projected = true;
      for (const auto& proj : parameters.projections) {
        if (proj.generated_point_id == point.id ||
            proj.source_id == point.id) {
          point.source_type = proj.source_kind;
          const auto colon1 = proj.source_id.find(':');
          if (colon1 != std::string::npos) {
            point.source_feature_id = proj.source_id.substr(0, colon1);
          }
          break;
        }
      }
      // Projection-generated lines already handled above; this
      // catches standalone projected points that map to a line.
      for (const auto& proj : parameters.projections) {
        for (const auto& line_id : proj.generated_line_ids) {
          if (line_id == point.id) {
            if (point.geometry_owner_ids.empty())
              point.geometry_owner_ids = {line_id};
            break;
          }
        }
      }
    }
  }

  // ── Phase 4.1: assign vertex-N IDs ───────────────────────────
  // New entity-created points already have vertex-N as their id;
  // old/derived points get vertex-N assigned here.
  for (auto& point : parameters.vertices) {
    if (point.vertex_id.empty()) {
      if (point.id.rfind("vertex-", 0) == 0) {
        // Entity creation (Phase 4.1) already assigned vertex-N as id
        point.vertex_id = point.id;
      } else {
        // Legacy point-* format — assign fresh vertex-N
        point.vertex_id = "vertex-" + std::to_string(parameters.next_vertex_index++);
      }
    }
  }

  // ── Phase 5: populate *_vertex_id on entities ───────────────
  auto vertex_id_of = [&](const std::string& point_id) -> std::string {
    for (const auto& p : parameters.vertices)
      if (p.id == point_id) return p.vertex_id;
    return "";
  };

  for (auto& line : parameters.lines) {
    line.start_vertex_id = vertex_id_of(line.start_vertex_id);
    line.end_vertex_id   = vertex_id_of(line.end_vertex_id);
  }
  for (auto& arc : parameters.arcs) {
    arc.start_vertex_id = vertex_id_of(arc.start_vertex_id);
    arc.end_vertex_id   = vertex_id_of(arc.end_vertex_id);
    // center_vertex_id already set in the center-emit loop above;
    // this maps it through the vertex table to keep vertex_id in sync.
    if (!arc.center_vertex_id.empty())
      arc.center_vertex_id = vertex_id_of(arc.center_vertex_id);
  }
  for (auto& circle : parameters.circles) {
    if (!circle.center_vertex_id.empty())
      circle.center_vertex_id = vertex_id_of(circle.center_vertex_id);
  }
}

// ── Phase 5: sync entity cached coords from canonical vertices ──
// After rebuild_sketch_vertices creates/updates the vertex table,
// copy vertex coordinates back to the entity structs so the cached
// fields stay in sync.  This makes vertices the single source of
// truth; entity fields are a read-optimised cache.
void sync_entity_coords_from_vertices(SketchFeatureParameters& parameters) {
  // Helper: find a vertex by its primary id.
  auto find_v = [&](const std::string& vid) -> const SketchVertex* {
    for (const auto& v : parameters.vertices)
      if (v.id == vid) return &v;
    return nullptr;
  };

  for (auto& line : parameters.lines) {
    if (const auto* sv = find_v(line.start_vertex_id)) {
      line.start_x = sv->x; line.start_y = sv->y;
    }
    if (const auto* ev = find_v(line.end_vertex_id)) {
      line.end_x = ev->x; line.end_y = ev->y;
    }
  }

  for (auto& arc : parameters.arcs) {
    if (const auto* sv = find_v(arc.start_vertex_id)) {
      arc.start_x = sv->x; arc.start_y = sv->y;
    }
    if (const auto* ev = find_v(arc.end_vertex_id)) {
      arc.end_x = ev->x; arc.end_y = ev->y;
    }
    // Arc center: use center_vertex_id (vertex-N, Phase 6)
    if (!arc.center_vertex_id.empty()) {
      if (const auto* cv = find_v(arc.center_vertex_id)) {
        arc.center_x = cv->x; arc.center_y = cv->y;
      }
    }
  }

  for (auto& circle : parameters.circles) {
    const auto center_id = circle_center_vertex_id(circle);
    if (const auto* cv = find_v(center_id)) {
      circle.center_x = cv->x; circle.center_y = cv->y;
    }
  }
}

// Force projected points to stay fixed. Projected points are derived
// from 3D body geometry — moving them would break the projection link.
void enforce_derived_points_fixed(SketchFeatureParameters& parameters) {
  for (auto& point : parameters.vertices) {
    if (point.kind == "projected") {
      point.is_fixed = true;
    }
  }
}

void sync_fixed_point_flags(SketchFeatureParameters& parameters,
                            const std::vector<SketchVertex>& previous_points) {
  for (auto& point : parameters.vertices) {
    const auto previous_it = std::find_if(
        previous_points.begin(),
        previous_points.end(),
        [&](const SketchVertex& previous) { return previous.id == point.id; });
    if (previous_it != previous_points.end()) {
      point.is_fixed = previous_it->is_fixed;
    }
  }
}

void rebuild_sketch_profiles(SketchFeatureParameters& parameters) {
  parameters.profiles = build_sketch_profile_regions(parameters);
}

// For each `SketchFillet`, re-derive the trim point coords + the
// generated arc's cached params from the *current* line endpoints.
// Runs as part of `refresh_sketch_derived_state` after every sketch
// edit, so dragging the "far" endpoint of a filleted line keeps the
// fillet tangent.
//
// Per-fillet algorithm:
//   1. Locate lines A and B and identify which endpoint of each is
//      the fillet's trim point. The opposite endpoint is "far".
//   2. Project each line as an infinite ray from `far` through its
//      *current* trim coord — that direction continues to the
//      virtual corner. Solve the two-line intersection in 2D for
//      the virtual corner coordinates.
//   3. With `theta` = angle between the two outgoing directions
//      (corner→far_a) and (corner→far_b), compute trim distance
//      `d = radius / tan(theta/2)` and shift the trim points to
//      `corner + d * (far - corner) / |far - corner|`.
//   4. Push the new trim coords back onto the lines' cached
//      endpoint fields and onto the arc's cached params (center,
//      radius, ccw, start/end).
//
// If validation fails for any reason (line missing, lines became
// parallel, trim no longer fits) we leave the fillet's cached
// geometry as-is and skip — the previous frame's render survives,
// which is preferable to a corrupted partial update. The fillet
// still survives; the user can drag the lines back into a valid
