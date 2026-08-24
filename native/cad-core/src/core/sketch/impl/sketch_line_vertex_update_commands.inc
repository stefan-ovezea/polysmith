void update_sketch_line(FeatureEntry& feature,
                        const std::string& line_id,
                        double start_x,
                        double start_y,
                        double end_x,
                        double end_y) {
  if (feature.kind != "sketch" || !feature.sketch_parameters.has_value()) {
    throw std::runtime_error("Only sketch features can update sketch lines");
  }

  // Text glyph segments are derived geometry — edit the text entity,
  // not its generated lines.
  require_user_line(*feature.sketch_parameters, line_id);

  const auto line_it = std::find_if(
      feature.sketch_parameters->lines.begin(),
      feature.sketch_parameters->lines.end(),
      [&](const SketchLine& line) { return line.id == line_id; });

  if (line_it == feature.sketch_parameters->lines.end()) {
    throw std::runtime_error("Sketch line not found: " + line_id);
  }

  const double previous_start_x = line_it->start_x;
  const double previous_start_y = line_it->start_y;
  const double previous_end_x = line_it->end_x;
  const double previous_end_y = line_it->end_y;

  line_it->start_x = start_x;
  line_it->start_y = start_y;
  line_it->end_x = end_x;
  line_it->end_y = end_y;
  restore_fixed_line_endpoints(*feature.sketch_parameters,
                               *line_it,
                               previous_start_x,
                               previous_start_y,
                               previous_end_x,
                               previous_end_y);
  apply_line_constraint_respecting_fixed_points(
      *feature.sketch_parameters, *line_it);
  restore_fixed_line_endpoints(*feature.sketch_parameters,
                               *line_it,
                               previous_start_x,
                               previous_start_y,
                               previous_end_x,
                               previous_end_y);
  snap_line_endpoints_to_coincident_geometry(*feature.sketch_parameters, *line_it);
  apply_line_constraint_respecting_fixed_points(
      *feature.sketch_parameters, *line_it);
  validate_line(line_it->start_x, line_it->start_y, line_it->end_x, line_it->end_y);

  if (!points_match(previous_start_x,
                    previous_start_y,
                    line_it->start_x,
                    line_it->start_y)) {
    propagate_connected_point_move(*feature.sketch_parameters,
                                   line_it->start_vertex_id,
                                   line_it->start_x,
                                   line_it->start_y);
  }
  if (!points_match(previous_end_x,
                    previous_end_y,
                    line_it->end_x,
                    line_it->end_y)) {
    propagate_connected_point_move(*feature.sketch_parameters,
                                   line_it->end_vertex_id,
                                   line_it->end_x,
                                   line_it->end_y);
  }
  sync_all_line_dimensions(*feature.sketch_parameters);
  enforce_equal_length_relations(*feature.sketch_parameters, line_it->id);
  enforce_perpendicular_relations(*feature.sketch_parameters, line_it->id);
  enforce_parallel_relations(*feature.sketch_parameters, line_it->id);
  sync_all_line_dimensions(*feature.sketch_parameters);
  refresh_sketch_derived_state(feature);
}

void update_sketch_vertex(FeatureEntry& feature,
                         const std::string& point_id,
                         double x,
                         double y) {
  if (feature.kind != "sketch" || !feature.sketch_parameters.has_value()) {
    throw std::runtime_error("Only sketch features can update sketch points");
  }

  // Text glyph and slot boundary vertices are derived geometry — edit
  // the owning entity, not its generated vertices.
  if (point_id.rfind("vertex-text-", 0) == 0 ||
      point_id.rfind("vertex-slot-", 0) == 0) {
    throw std::runtime_error("Generated sketch entity is not editable: " +
                             point_id);
  }

  auto& parameters = feature.sketch_parameters.value();
  const SketchVertex* point = find_sketch_vertex(parameters, point_id);
  if (point == nullptr) {
    throw std::runtime_error("Sketch point not found: " + point_id);
  }

  if (point->is_fixed && !points_match(point->x, point->y, x, y)) {
    throw std::runtime_error("Cannot move a fixed sketch point");
  }

  const std::vector<std::string> affected_line_ids =
      collect_line_ids_for_vertex(parameters, point_id);

  if (point->kind == "center") {
    // Circle center: use geometry_owner_ids lookup (Phase 4.2)
    if (const auto* circle = find_circle_for_point(parameters, point_id)) {
      // Need mutable access to update coords
      for (auto& c : parameters.circles) {
        if (c.id == circle->id) {
          c.center_x = x;
          c.center_y = y;
          refresh_sketch_derived_state(feature);
          return;
        }
      }
      throw std::runtime_error("Circle not found for center point: " + point_id);
    }

    // Arc center: use geometry_owner_ids lookup (Phase 4.2)
    if (const auto* arc = find_arc_for_point(parameters, point_id)) {
      for (auto& a : parameters.arcs) {
        if (a.id == arc->id) {
          const double dx = x - a.center_x;
          const double dy = y - a.center_y;
          a.center_x = x;
          a.center_y = y;
          a.start_x += dx;
          a.start_y += dy;
          a.end_x += dx;
          a.end_y += dy;
          // Move connected lines that share the arc's endpoints.
          propagate_connected_point_move(parameters, a.start_vertex_id,
                                         a.start_x, a.start_y);
          propagate_connected_point_move(parameters, a.end_vertex_id,
                                         a.end_x, a.end_y);
          sync_all_line_dimensions(parameters);
          refresh_sketch_derived_state(feature);
          return;
        }
      }
      throw std::runtime_error("Arc not found for center point: " + point_id);
    }

    // Slot center: move the slot record's center — the expansion pass
    // regenerates the whole boundary from it.
    for (auto& slot : parameters.slots) {
      if (slot.center_vertex_id == point_id) {
        slot.center_x = x;
        slot.center_y = y;
        refresh_sketch_derived_state(feature);
        return;
      }
    }

    // Ellipse center: move the ellipse (the axis points stay fixed).
    for (auto& ellipse : parameters.ellipses) {
      if (ellipse.center_vertex_id == point_id) {
        ellipse.center_x = x;
        ellipse.center_y = y;
        refresh_sketch_derived_state(feature);
        return;
      }
    }

    throw std::runtime_error("Unsupported sketch center point: " + point_id);
  }

  propagate_connected_point_move(parameters, point_id, x, y);
  sync_all_line_dimensions(parameters);

  for (const auto& line_id : affected_line_ids) {
    enforce_equal_length_relations(parameters, line_id);
    enforce_perpendicular_relations(parameters, line_id);
    enforce_parallel_relations(parameters, line_id);
  }

  sync_all_line_dimensions(parameters);
  refresh_sketch_derived_state(feature);
}

void transform_sketch_entities(FeatureEntry& feature,
                               const std::vector<std::string>& entity_ids,
                               double dx,
                               double dy,
                               double center_x,
                               double center_y,
                               double angle_deg,
                               double scale,
                               bool copy) {
  if (feature.kind != "sketch" || !feature.sketch_parameters.has_value()) {
    throw std::runtime_error(
        "Only sketch features can transform sketch entities");
  }

  auto& parameters = feature.sketch_parameters.value();

  // Generated glyph segments ride with their text entity — a move must
  // never relocate them independently (or it would fight the expansion
  // pass, which regenerates them from the text anchor on next refresh).
  for (const auto& entity_id : entity_ids) {
    ensure_user_editable_entity(parameters, entity_id);
  }

  constexpr double kPi = 3.14159265358979323846;
  const bool rotating = std::abs(angle_deg) > 1.0e-6;
  const bool scaling = std::abs(scale - 1.0) > 1.0e-9;
  if (scale <= kMinimumSketchDimensionValue) {
    throw std::runtime_error("Sketch transform: scale must be positive");
  }
  const double angle_rad = angle_deg * (kPi / 180.0);
  const double cos_a = std::cos(angle_rad);
  const double sin_a = std::sin(angle_rad);

  const auto transform_point = [&](double x, double y) {
    const double rel_x = x - center_x;
    const double rel_y = y - center_y;
    return std::make_pair(
        (rel_x * cos_a - rel_y * sin_a) * scale + center_x + dx,
        (rel_x * sin_a + rel_y * cos_a) * scale + center_y + dy);
  };

  // ── Copy mode: exploded copies with fresh ids ─────────────────
  // Pushed directly as raw records (not via add_sketch_*) so the
  // copies never adopt coincident vertex ids — a scale-1 copy lands
  // exactly on the originals and must stay independent. No auto
  // dimensions; H/V constraints are inferred only when the copy is
  // not rotated (mirrors the plan's "rotate+copy strips H/V").
  if (copy) {
    std::vector<std::string> line_ids, circle_ids, arc_ids, ellipse_ids,
        slot_ids, spline_ids;
    line_ids.reserve(parameters.lines.size());
    for (const auto& line : parameters.lines) line_ids.push_back(line.id);
    for (const auto& circle : parameters.circles) circle_ids.push_back(circle.id);
    for (const auto& arc : parameters.arcs) arc_ids.push_back(arc.id);
    for (const auto& ellipse : parameters.ellipses)
      ellipse_ids.push_back(ellipse.id);
    for (const auto& slot : parameters.slots) slot_ids.push_back(slot.id);
    for (const auto& spline : parameters.splines)
      spline_ids.push_back(spline.id);
    int next_line = next_trim_entity_index(line_ids);
    int next_circle = next_trim_entity_index(circle_ids);
    int next_arc = next_trim_entity_index(arc_ids);
    int next_ellipse = next_trim_entity_index(ellipse_ids);
    int next_slot = next_trim_entity_index(slot_ids);
    int next_spline = next_trim_entity_index(spline_ids);

    // Copies share vertices with EACH OTHER when the source entities
    // shared them (a copied rectangle keeps unified corners), but
    // never with the originals — a scale-1 copy lands exactly on the
    // source geometry and must stay independent.
    std::map<std::string, std::string> copied_vertex_ids;
    const auto copy_vertex = [&](const std::string& source_vertex_id) {
      auto& mapped = copied_vertex_ids[source_vertex_id];
      if (mapped.empty()) {
        mapped = "vertex-" + std::to_string(parameters.next_vertex_index++);
      }
      return mapped;
    };

    for (const auto& entity_id : entity_ids) {
      for (const auto& line : parameters.lines) {
        if (line.id != entity_id) continue;
        const auto s = transform_point(line.start_x, line.start_y);
        const auto e = transform_point(line.end_x, line.end_y);
        parameters.lines.push_back(SketchLine{
            .id = "line-" + std::to_string(next_line++),
            .start_vertex_id = copy_vertex(line.start_vertex_id),
            .end_vertex_id = copy_vertex(line.end_vertex_id),
            .start_x = s.first,
            .start_y = s.second,
            .end_x = e.first,
            .end_y = e.second,
            .constraint = rotating
                              ? std::nullopt
                              : infer_constraint_hint(s.first, s.second,
                                                      e.first, e.second),
            .is_construction = line.is_construction,
        });
      }
      for (const auto& circle : parameters.circles) {
        if (circle.id != entity_id) continue;
        const auto c = transform_point(circle.center_x, circle.center_y);
        parameters.circles.push_back(SketchCircle{
            .id = "circle-" + std::to_string(next_circle++),
            .center_vertex_id = copy_vertex(circle.center_vertex_id),
            .center_x = c.first,
            .center_y = c.second,
            .radius = circle.radius * scale,
            .is_construction = circle.is_construction,
        });
      }
      for (const auto& arc : parameters.arcs) {
        if (arc.id != entity_id) continue;
        const auto c = transform_point(arc.center_x, arc.center_y);
        const auto s = transform_point(arc.start_x, arc.start_y);
        const auto e = transform_point(arc.end_x, arc.end_y);
        parameters.arcs.push_back(SketchArc{
            .id = "arc-" + std::to_string(next_arc++),
            .start_vertex_id = copy_vertex(arc.start_vertex_id),
            .end_vertex_id = copy_vertex(arc.end_vertex_id),
            .center_vertex_id = copy_vertex(arc.center_vertex_id),
            .center_x = c.first,
            .center_y = c.second,
            .radius = arc.radius * scale,
            .start_x = s.first,
            .start_y = s.second,
            .end_x = e.first,
            .end_y = e.second,
            .ccw = arc.ccw,
            .is_construction = arc.is_construction,
        });
      }
      for (const auto& ellipse : parameters.ellipses) {
        if (ellipse.id != entity_id) continue;
        const auto c = transform_point(ellipse.center_x, ellipse.center_y);
        parameters.ellipses.push_back(SketchEllipse{
            .id = "ellipse-" + std::to_string(next_ellipse++),
            .center_vertex_id = copy_vertex(ellipse.center_vertex_id),
            .axis_a_vertex_id = copy_vertex(ellipse.axis_a_vertex_id),
            .axis_b_vertex_id = copy_vertex(ellipse.axis_b_vertex_id),
            .center_x = c.first,
            .center_y = c.second,
            .a = ellipse.a * scale,
            .b = ellipse.b * scale,
            .rotation = ellipse.rotation + angle_rad,
            .is_construction = ellipse.is_construction,
        });
      }
      for (const auto& slot : parameters.slots) {
        if (slot.id != entity_id) continue;
        const auto c = transform_point(slot.center_x, slot.center_y);
        parameters.slots.push_back(SketchSlot{
            .id = "slot-" + std::to_string(next_slot++),
            .center_vertex_id = copy_vertex(slot.center_vertex_id),
            .center_x = c.first,
            .center_y = c.second,
            .length = slot.length * scale,
            .radius = slot.radius * scale,
            .rotation = slot.rotation + angle_rad,
            .mode = slot.mode,
            .is_construction = slot.is_construction,
        });
      }
      for (const auto& spline : parameters.splines) {
        if (spline.id != entity_id) continue;
        SketchSpline copy;
        copy.id = "spline-" + std::to_string(next_spline++);
        copy.degree = spline.degree;
        copy.is_construction = spline.is_construction;
        copy.pole_vertex_ids.reserve(spline.pole_vertex_ids.size());
        copy.pole_xs.reserve(spline.pole_xs.size());
        copy.pole_ys.reserve(spline.pole_ys.size());
        for (size_t i = 0; i < spline.pole_vertex_ids.size(); ++i) {
          copy.pole_vertex_ids.push_back(
              copy_vertex(spline.pole_vertex_ids[i]));
          const auto p = transform_point(spline.pole_xs[i], spline.pole_ys[i]);
          copy.pole_xs.push_back(p.first);
          copy.pole_ys.push_back(p.second);
        }
        parameters.splines.push_back(std::move(copy));
      }
      // Standalone point ids are not copyable — ignored.
    }

    sync_all_line_dimensions(parameters);
    refresh_sketch_derived_state(feature);
    return;
  }

  // Collect every vertex belonging to the moved entities.  Entity ids
  // that match none of the geometry collections are standalone points:
  // the id IS the vertex id (mirrors the UI's sketchMoveEntityVertices).
  std::set<std::string> moved_vertex_ids;
  std::set<std::string> moved_line_ids;
  for (const auto& entity_id : entity_ids) {
    bool matched = false;
    for (const auto& line : parameters.lines) {
      if (line.id == entity_id) {
        moved_vertex_ids.insert(line.start_vertex_id);
        moved_vertex_ids.insert(line.end_vertex_id);
        moved_line_ids.insert(line.id);
        matched = true;
        break;
      }
    }
    if (matched) continue;
    for (const auto& circle : parameters.circles) {
      if (circle.id == entity_id) {
        if (!circle.center_vertex_id.empty()) {
          moved_vertex_ids.insert(circle.center_vertex_id);
        }
        matched = true;
        break;
      }
    }
    if (matched) continue;
    for (const auto& arc : parameters.arcs) {
      if (arc.id == entity_id) {
        moved_vertex_ids.insert(arc.start_vertex_id);
        moved_vertex_ids.insert(arc.end_vertex_id);
        if (!arc.center_vertex_id.empty()) {
          moved_vertex_ids.insert(arc.center_vertex_id);
        }
        matched = true;
        break;
      }
    }
    if (matched) continue;
    for (const auto& ellipse : parameters.ellipses) {
      if (ellipse.id == entity_id) {
        // Only the center is movable (the axis points are fixed); the
        // fixed-vertex filter below leaves them in place and the cache
        // shift keeps the shape rigid.
        moved_vertex_ids.insert(ellipse.center_vertex_id);
        matched = true;
        break;
      }
    }
    if (matched) continue;
    for (const auto& slot : parameters.slots) {
      if (slot.id == entity_id) {
        // The generated boundary entities are filtered by
        // ensure_user_editable_entity above; moving the slot moves its
        // center vertex and the expansion pass regenerates the shape.
        moved_vertex_ids.insert(slot.center_vertex_id);
        matched = true;
        break;
      }
    }
    if (matched) continue;
    for (const auto& spline : parameters.splines) {
      if (spline.id == entity_id) {
        // Every pole is movable — move the whole control polygon.
        for (const auto& vid : spline.pole_vertex_ids) {
          moved_vertex_ids.insert(vid);
        }
        matched = true;
        break;
      }
    }
    if (matched) continue;
    moved_vertex_ids.insert(entity_id);
  }

  // Fixed vertices stay put (consistent with update_sketch_vertex's
  // fixed-point rejection and the ripple's fixed-point semantics).
  std::vector<std::string> movable_vertex_ids;
  for (const auto& vertex_id : moved_vertex_ids) {
    const SketchVertex* vertex = find_sketch_vertex(parameters, vertex_id);
    if (vertex != nullptr && !vertex->is_fixed) {
      movable_vertex_ids.push_back(vertex_id);
    } else if (vertex == nullptr) {
      // Unknown vertex ids (e.g. a dangling reference) still get a
      // chance below — propagate validates against live geometry.
      movable_vertex_ids.push_back(vertex_id);
    }
  }
  if (movable_vertex_ids.empty()) {
    throw std::runtime_error("No movable vertices for sketch move");
  }

  // Rotation frees H/V constraints on moved lines: a rotated line is
  // genuinely no longer axis-aligned, and keeping the constraint would
  // snap it back inside refresh_sketch_derived_state.  Pure translation
  // preserves axis-alignment, so constraints stay.
  if (rotating) {
    for (auto& line : parameters.lines) {
      if (moved_line_ids.count(line.id)) {
        line.constraint = std::nullopt;
      }
    }
  }

  // Rigid transform.  Write each movable vertex's target into the
  // entity caches that own it (lines via endpoints, circles via
  // centers, arcs via center + endpoints — radii are preserved because
  // the motion is a pure rigid transform) and ripple connected lines
  // through propagate_connected_point_move, exactly like an endpoint
  // drag.  refresh_sketch_derived_state rebuilds the vertex table from
  // these caches afterwards.
  std::set<std::string> seen;
  for (const auto& vertex_id : movable_vertex_ids) {
    if (seen.count(vertex_id)) continue;
    seen.insert(vertex_id);
    const SketchVertex* vertex = find_sketch_vertex(parameters, vertex_id);
    if (vertex == nullptr) continue;
    const auto target = transform_point(vertex->x, vertex->y);
    if (points_match(vertex->x, vertex->y, target.first, target.second)) {
      continue;
    }

    for (auto& line : parameters.lines) {
      if (line.start_vertex_id == vertex_id) {
        line.start_x = target.first;
        line.start_y = target.second;
      }
      if (line.end_vertex_id == vertex_id) {
        line.end_x = target.first;
        line.end_y = target.second;
      }
    }
    for (auto& spline : parameters.splines) {
      for (size_t i = 0; i < spline.pole_vertex_ids.size(); ++i) {
        if (spline.pole_vertex_ids[i] == vertex_id) {
          spline.pole_xs[i] = target.first;
          spline.pole_ys[i] = target.second;
        }
      }
    }
    for (auto& circle : parameters.circles) {
      if (circle.center_vertex_id == vertex_id) {
        circle.center_x = target.first;
        circle.center_y = target.second;
      }
    }
    for (auto& arc : parameters.arcs) {
      if (arc.start_vertex_id == vertex_id) {
        arc.start_x = target.first;
        arc.start_y = target.second;
      }
      if (arc.end_vertex_id == vertex_id) {
        arc.end_x = target.first;
        arc.end_y = target.second;
      }
      if (arc.center_vertex_id == vertex_id) {
        arc.center_x = target.first;
        arc.center_y = target.second;
      }
    }
    for (auto& ellipse : parameters.ellipses) {
      if (ellipse.center_vertex_id == vertex_id) {
        ellipse.center_x = target.first;
        ellipse.center_y = target.second;
      }
    }
    for (auto& slot : parameters.slots) {
      if (slot.center_vertex_id == vertex_id) {
        slot.center_x = target.first;
        slot.center_y = target.second;
      }
    }
    // Standalone points (no owning entity): update the vertex entry
    // directly so the position survives the refresh.
    for (auto& vertex_entry : parameters.vertices) {
      if (vertex_entry.id == vertex_id) {
        vertex_entry.x = target.first;
        vertex_entry.y = target.second;
      }
    }

    // Ripple only when a connected line is NOT fully covered by this
    // move.  propagate_connected_point_move clamps the moved endpoint
    // of an H/V-constrained line back to its OTHER endpoint's axis —
    // for a wholly-moved line, processing the endpoints one at a time
    // would snap each one back to the other's pre-move coordinate and
    // revert the whole translation (a lone horizontal line was
    // immovable via the move tool for exactly this reason).
    bool needs_ripple = false;
    for (const auto& line : parameters.lines) {
      if (line.start_vertex_id != vertex_id && line.end_vertex_id != vertex_id) {
        continue;
      }
      const bool fully_moved =
          moved_vertex_ids.count(line.start_vertex_id) != 0 &&
          moved_vertex_ids.count(line.end_vertex_id) != 0;
      if (!fully_moved) needs_ripple = true;
    }
    if (needs_ripple) {
      propagate_connected_point_move(parameters, vertex_id, target.first,
                                     target.second);
    }
  }

  // Scale the moved entities' size parameters (a similarity transform
  // keeps the shape, so radii / a / b / slot dimensions scale with
  // the vertices). Driving radius dimensions on scaled entities no
  // longer match the geometry — flip them to driven so the solver
  // enforcement doesn't undo the scale.
  if (scaling) {
    for (auto& circle : parameters.circles) {
      if (moved_vertex_ids.count(circle.center_vertex_id)) {
        circle.radius *= scale;
      }
    }
    for (auto& arc : parameters.arcs) {
      if (moved_vertex_ids.count(arc.start_vertex_id) ||
          moved_vertex_ids.count(arc.end_vertex_id) ||
          moved_vertex_ids.count(arc.center_vertex_id)) {
        arc.radius *= scale;
      }
    }
    for (auto& ellipse : parameters.ellipses) {
      if (moved_vertex_ids.count(ellipse.center_vertex_id)) {
        ellipse.a *= scale;
        ellipse.b *= scale;
      }
    }
    for (auto& slot : parameters.slots) {
      if (moved_vertex_ids.count(slot.center_vertex_id)) {
        slot.length *= scale;
        slot.radius *= scale;
      }
    }
    for (auto& dim : parameters.dimensions) {
      const bool owns_entity =
          std::find(entity_ids.begin(), entity_ids.end(), dim.entity_id) !=
          entity_ids.end();
      if (owns_entity &&
          (dim.kind == "circle_radius" || dim.kind == "arc_radius")) {
        dim.driven = true;
      }
    }
  }

  sync_all_line_dimensions(parameters);
  for (const auto& line_id : moved_line_ids) {
    enforce_equal_length_relations(parameters, line_id);
    enforce_perpendicular_relations(parameters, line_id);
    enforce_parallel_relations(parameters, line_id);
  }
  sync_all_line_dimensions(parameters);
  refresh_sketch_derived_state(feature);
}

// Rigid translate/rotate wrapper — the pre-SK4 move IPC surface.
void move_sketch_entities(FeatureEntry& feature,
                          const std::vector<std::string>& entity_ids,
                          double dx,
                          double dy,
                          double center_x,
                          double center_y,
                          double angle_deg) {
  transform_sketch_entities(feature, entity_ids, dx, dy, center_x, center_y,
                            angle_deg, /*scale=*/1.0, /*copy=*/false);
}
