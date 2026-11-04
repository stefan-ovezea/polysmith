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
  validate_line(line_it->start_x, line_it->start_y, line_it->end_x, line_it->end_y, "updating a line vertex");

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

// ── Drag policy: the release position of the dragged vertex wins ──────────
//
// `update_sketch_vertex` is the drag/move path for a single vertex.
//
//   1. A vertex bound to a CURVE (`point_on_circle` / `point_on_arc`) can only
//      exist ON that curve, so clamping it onto a line's H/V axis makes the
//      sketch unsatisfiable: measured, dragging an r=10 circle's bound endpoint
//      from (10,0) to (0,-10) had its y clamped onto the line's other end,
//      landing the point on the circle's CENTRE — where point_on_circle has no
//      gradient — and the solve then failed (status=2, dofs=5). The axis is now
//      enforced on the line's FREE end instead (moved_point_bound_to_curve in
//      private_connected_point_move.inc), which keeps the badge AND keeps the
//      point on its curve: the line follows the dragged point. Fusion does the
//      same — a horizontal line bound to a circle rides up with it and keeps
//      its "H", and only stretches when the circle moves along the line.
//   2. Dragging the CONTACT point of a `tangent_line_circle` relation is
//      REFUSED, and the relation is kept (Fusion's behaviour, chosen by the user
//      2026-11-04). With the line's far end fixed there is exactly one place on
//      the circle where the line can be tangent, so the drag has nowhere to go.
//      This file used to drop the relation instead, which let the point follow
//      the cursor — the user's verdict on that: the sketch would "behave poorly,
//      moving the circle, the point on the circle and the line end all together
//      in unpredictable ways". Removing the tangency is the USER's decision; once
//      it is gone the guard no longer applies and the drag is ordinary.
//
// Rule 1's helper (`vertex_is_bound_to_curve`) lives in
// private_connected_point_move.inc, where the axis is enforced: that file is
// included inside the sketch's anonymous namespace, so the definition is
// deliberately kept next to its only caller.

// True when the vertex is the CONTACT point of a live `tangent_line_circle`
// relation. The relation's contract (see set_sketch_tangent_constraint) is that
// the line's START is the external anchor and its END is the contact point, so
// only an END drag is affected — dragging the start is the case the per-refresh
// re-projection exists for.
bool vertex_is_tangent_contact_point(const SketchFeatureParameters& parameters,
                                     const std::string& vertex_id) {
  for (const auto& line : parameters.lines) {
    if (line.end_vertex_id != vertex_id) continue;
    for (const auto& relation : parameters.line_relations) {
      if (relation.kind == "tangent_line_circle" &&
          relation.first_line_id == line.id) {
        return true;
      }
    }
  }
  return false;
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

  // Dragging the CONTACT point of a tangency is refused, and the relation is
  // kept — Fusion's behaviour, which the user asked for (2026-11-04): with the
  // far end fixed there is only one place on the circle where this line can be
  // tangent, so the drag has nowhere to go. The alternative — dropping the
  // relation so the point can follow the cursor — is what this branch used to
  // do, and the user's verdict on the resulting free geometry was "behaves
  // poorly, moving the circle, the point on the circle and the line end all
  // together in unpredictable ways". A user who wants the point to move removes
  // the tangency themselves; then this guard is gone and the drag is ordinary
  // and predictable.
  if (!points_match(point->x, point->y, x, y) &&
      vertex_is_tangent_contact_point(parameters, point_id)) {
    polysmith::core::log_info(
        "sketch",
        "drag refused: the point is the contact point of a tangency (remove the "
        "tangent constraint to move it)");
    return;
  }

  if (point->is_fixed && !points_match(point->x, point->y, x, y)) {
    throw std::runtime_error("Cannot move a fixed sketch point");
  }

  const std::vector<std::string> affected_line_ids =
      collect_line_ids_for_vertex(parameters, point_id);

  // Remember where the affected lines' midpoints were, so the dimension labels
  // measuring them can be carried along with the geometry (see the shift below).
  std::vector<std::pair<std::string, std::pair<double, double>>>
      line_midpoints_before;
  for (const auto& line_id : affected_line_ids) {
    for (const auto& line : parameters.lines) {
      if (line.id != line_id) continue;
      line_midpoints_before.push_back(
          {line_id,
           {(line.start_x + line.end_x) / 2.0, (line.start_y + line.end_y) / 2.0}});
      break;
    }
  }

  if (point->kind == "center") {
    // Circle center: use geometry_owner_ids lookup (Phase 4.2)
    if (const auto* circle = find_circle_for_point(parameters, point_id)) {
      // Need mutable access to update coords
      for (auto& c : parameters.circles) {
        if (c.id == circle->id) {
          const double old_x = c.center_x;
          const double old_y = c.center_y;
          c.center_x = x;
          c.center_y = y;
          // Translate the point_on_circle-attached points by the centre's delta so
          // they ride the circle at their same relative spot. Collect them so the
          // solve can FREEZE them along with the centre: with a driving line-length
          // dimension an otherwise-free point gets pulled off-angle around the
          // circle ("the end wanders on the circle").
          //
          // A line drawn onto the circle is covered by this loop like any other
          // point: its endpoint carries a real point_on_circle, never an id
          // adopted from the curve (see find_coincident_endpoint).
          // A tangency OWNS its contact point's position: once the centre has
          // moved, the new tangent point is the only correct spot on the
          // circle. Freezing the contact point along with the other riders
          // (below) pins it at its old relative angle, where the line is no
          // longer tangent — and `enforce_tangent_line_circle_relations` runs
          // AFTER the solve, so a frozen contact point makes that pass silently
          // no-op (`propagate_connected_point_move` refuses a fixed vertex) and
          // the "T" badge survives a broken tangency. Measured 2026-11-04 on a
          // saved document: d − r reached −0.48 mm and −2.30 mm on an r = 32
          // circle, the error accumulating on every further drag, with both
          // badges still up.
          //
          // The freeze is only a solve-time guard ("freeze ... for the solve,
          // then restore" below); the attachment is the `point_on_circle`
          // constraint, and the tangent point lies ON the circle by
          // construction, so leaving the contact point out of the freeze cannot
          // detach the line — the apex is untouched and the endpoint stays on
          // the boundary.
          std::vector<std::string> tangent_contact_ids;
          for (const auto& relation : parameters.line_relations) {
            if (relation.kind != "tangent_line_circle") continue;
            if (relation.second_line_id != c.id) continue;
            for (const auto& line : parameters.lines) {
              if (line.id != relation.first_line_id) continue;
              tangent_contact_ids.push_back(line.end_vertex_id);
            }
          }
          std::vector<std::string> ride_ids;
          for (const auto& constraint : parameters.constraints) {
            if (constraint.kind != "point_on_circle") continue;
            if (constraint.target_ids.size() < 2) continue;
            if (constraint.target_ids[1] != c.id) continue;
            const auto pos = point_position_of(parameters, constraint.target_ids[0]);
            if (!pos.has_value()) continue;
            propagate_connected_point_move(parameters, constraint.target_ids[0],
                                           pos->first + (x - old_x),
                                           pos->second + (y - old_y));
            // A tangent contact point is driven by its relation, not by the
            // ride, so it is deliberately NOT frozen below.
            if (std::find(tangent_contact_ids.begin(), tangent_contact_ids.end(),
                          constraint.target_ids[0]) ==
                tangent_contact_ids.end()) {
              ride_ids.push_back(constraint.target_ids[0]);
            }
          }
          // Dragging OVERRIDES the dimensions, exactly as it does when the
          // user drags a line's free end (update_line_vertex re-measures after
          // the edit): the geometry the user is holding wins and the dimension
          // records the new measurement. Re-measure BEFORE the solve, while the
          // free end is still where it was, so a driving length dimension has
          // nothing left to enforce and cannot push the far end of the line
          // ("the line free end moves to keep the original length").
          sync_all_line_dimensions(parameters);
          // Freeze the centre AND every riding point for the solve, then restore.
          std::vector<std::pair<std::string, bool>> saved_fixed;
          const auto save_fixed = [&](const std::string& id) {
            for (const auto& v : parameters.vertices) {
              if (v.id == id) { saved_fixed.push_back({id, v.is_fixed}); return; }
            }
          };
          const auto set_fixed = [&](const std::string& id, bool fixed) {
            for (auto& v : parameters.vertices) {
              if (v.id == id) { v.is_fixed = fixed; return; }
            }
          };
          save_fixed(point_id);
          for (const auto& id : ride_ids) save_fixed(id);
          set_fixed(point_id, true);
          for (const auto& id : ride_ids) set_fixed(id, true);
          refresh_sketch_derived_state(feature);
          for (const auto& [id, was_fixed] : saved_fixed) set_fixed(id, was_fixed);
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
          // A trimmed arc's endpoints are frozen by the trim command to
          // protect the loop it welded against the solver. A deliberate
          // centre drag must still move them — and the lines welded to
          // them — or the centre moves alone, the radius drifts off the
          // endpoints and the attached lines detach ("extra geometry on
          // centre drag"). Lift the flag for the translate + propagate,
          // then restore it so the loop stays protected.
          auto set_vertex_fixed = [&](const std::string& id, bool fixed) {
            for (auto& v : parameters.vertices) {
              if (v.id == id) {
                v.is_fixed = fixed;
                return;
              }
            }
          };
          const bool start_was_fixed =
              vertex_is_fixed(parameters, a.start_vertex_id);
          const bool end_was_fixed =
              vertex_is_fixed(parameters, a.end_vertex_id);
          set_vertex_fixed(a.start_vertex_id, false);
          set_vertex_fixed(a.end_vertex_id, false);
          // Move connected lines that share the arc's endpoints.
          propagate_connected_point_move(parameters, a.start_vertex_id,
                                         a.start_x, a.start_y);
          propagate_connected_point_move(parameters, a.end_vertex_id,
                                         a.end_x, a.end_y);
          set_vertex_fixed(a.start_vertex_id, start_was_fixed);
          set_vertex_fixed(a.end_vertex_id, end_was_fixed);
          // Translate the point_on_arc-attached points by the centre's delta
          // so they ride the arc at their same relative spot — the same
          // contract the circle path above applies to point_on_circle.
          for (const auto& constraint : parameters.constraints) {
            if (constraint.kind != "point_on_arc") continue;
            if (constraint.target_ids.size() < 2) continue;
            if (constraint.target_ids[1] != a.id) continue;
            // The arc's own endpoints already translated with the arc
            // (a.start_x/y, a.end_x/y) above. A point_on_arc naming one of
            // them is redundant, and translating it here would shift it a
            // SECOND (dx,dy) — the endpoint then rides at 2·delta while the
            // centre moved 1·delta, so |endpoint − centre| drifts off the
            // radius and the drawn arc no longer reaches the welded lines
            // ("lines detach a bit more on every centre-drag commit").
            if (constraint.target_ids[0] == a.start_vertex_id ||
                constraint.target_ids[0] == a.end_vertex_id) {
              continue;
            }
            const auto pos =
                point_position_of(parameters, constraint.target_ids[0]);
            if (!pos.has_value()) continue;
            propagate_connected_point_move(parameters, constraint.target_ids[0],
                                           pos->first + dx, pos->second + dy);
          }
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

  // Standalone Point-tool point (kind "point"). No curve owns it, so the
  // generic propagation below would find nothing to move. Update the record
  // AND the vertex-table entry: the table is authoritative for user points
  // across a rebuild (rebuild_sketch_vertices reads it back into the record),
  // so touching only the record would be reverted by the next refresh.
  if (point->kind == "point") {
    for (auto& user_point : parameters.points) {
      if (user_point.id != point_id) continue;
      user_point.x = x;
      user_point.y = y;
      for (auto& vertex : parameters.vertices) {
        if (vertex.id == point_id) {
          vertex.x = x;
          vertex.y = y;
        }
      }
      refresh_sketch_derived_state(feature);
      return;
    }
  }

  // A tangent contact point never reaches here: the guard at the top of this
  // function refuses that drag and keeps the relation (see its comment). A
  // curve-bound vertex needs no help either — the axis is enforced on the line's
  // free end instead of clamping the point (see
  // private_connected_point_move.inc), so the H/V badge survives the drag.

  // A curve-bound point can only exist ON its curve, so project the release
  // position onto it BEFORE the solve. Without this, a drag onto the circle's
  // CENTRE leaves point_on_circle with no gradient to follow and the solve fails
  // (measured 2026-11-04: `status=2`, and the point stayed on the centre — the
  // sketch was then poisoned for every later refresh). Dragging toward the centre
  // now slides the point along the rim instead.
  for (const auto& constraint : parameters.constraints) {
    if (constraint.kind != "point_on_circle" ||
        constraint.target_ids.size() < 2 ||
        constraint.target_ids[0] != point_id) {
      continue;
    }
    const auto circle_it = std::find_if(
        parameters.circles.begin(), parameters.circles.end(),
        [&](const SketchCircle& circle) {
          return circle.id == constraint.target_ids[1];
        });
    if (circle_it == parameters.circles.end()) continue;
    if (circle_it->radius <= kMinimumSketchDimensionValue) continue;
    double dx = x - circle_it->center_x;
    double dy = y - circle_it->center_y;
    if (std::hypot(dx, dy) <= kMinimumSketchDimensionValue) {
      // Degenerate target: keep the direction the point already has.
      dx = point->x - circle_it->center_x;
      dy = point->y - circle_it->center_y;
      if (std::hypot(dx, dy) <= kMinimumSketchDimensionValue) {
        dx = 1.0;
        dy = 0.0;
      }
    }
    const double length = std::hypot(dx, dy);
    x = circle_it->center_x + circle_it->radius * (dx / length);
    y = circle_it->center_y + circle_it->radius * (dy / length);
    break;
  }

  // A line that carries a DRIVING dimension between its own two endpoints behaves
  // rigidly under a drag: checked against Fusion at the user's request
  // (2026-11-04) — "I draw a line, make dimension and drag one end, [it] just
  // moves the whole line". Carrying the far endpoint by the same delta is what
  // keeps the length (and therefore the dimension) unchanged. Without it the drag
  // stretched the line and the solver pulled it straight back; on a line bound to a
  // circle that pull-back also walked the circle centre around.
  //
  // Anything attached to the far end rides the translation (the refresh's
  // point_on_circle pass moves the circle with its point), which is why a
  // circle-bound line's drag moves the circle instead of resizing it.
  for (const auto& line : parameters.lines) {
    const bool is_start = line.start_vertex_id == point_id;
    const bool is_end = line.end_vertex_id == point_id;
    if (!is_start && !is_end) continue;
    bool dimensioned = false;
    for (const auto& dimension : parameters.dimensions) {
      if (dimension.driven) continue;
      // AUTO dimensions are bookkeeping the sketch writes back on every refresh
      // (every line gets one), not a constraint the user placed — a drag must keep
      // stretching those lines. Only an explicit dimension (is_auto == false, the
      // state set when the user creates or edits one) makes the line rigid.
      if (dimension.is_auto) continue;
      if (dimension.kind == "line_length" && dimension.entity_id == line.id) {
        dimensioned = true;
        break;
      }
      const bool owns_endpoints =
          (dimension.entity_id == line.start_vertex_id &&
           dimension.secondary_entity_id == line.end_vertex_id) ||
          (dimension.entity_id == line.end_vertex_id &&
           dimension.secondary_entity_id == line.start_vertex_id);
      if (dimension.kind == "point_distance" && owns_endpoints) {
        dimensioned = true;
        break;
      }
    }
    if (!dimensioned) continue;

    const double current_x = is_start ? line.start_x : line.end_x;
    const double current_y = is_start ? line.start_y : line.end_y;
    const double delta_x = x - current_x;
    const double delta_y = y - current_y;
    if (std::abs(delta_x) <= kMinimumSketchDimensionValue &&
        std::abs(delta_y) <= kMinimumSketchDimensionValue) {
      break;
    }
    // Copy what the call needs: propagate_connected_point_move mutates the line.
    const std::string far_id =
        is_start ? line.end_vertex_id : line.start_vertex_id;
    const double far_x = is_start ? line.end_x : line.start_x;
    const double far_y = is_start ? line.end_y : line.start_y;
    // The bound circle travels with the line. A point_on_circle endpoint that is
    // translated must keep its place ON the rim, so the circle moves by the same
    // delta — Fusion: "if the line with dimension started from the circle to canvas,
    // on free-end drag [it] moves the circle". Without this the point_on_circle
    // repair pulled the translated point back onto a stationary rim, which shortened
    // the line and made the free end lag behind the cursor (measured: 67.75 instead
    // of 70).
    for (const auto& constraint : parameters.constraints) {
      if (constraint.kind != "point_on_circle" ||
          constraint.target_ids.size() < 2) {
        continue;
      }
      if (constraint.target_ids[0] != line.start_vertex_id &&
          constraint.target_ids[0] != line.end_vertex_id) {
        continue;
      }
      for (auto& circle : parameters.circles) {
        if (circle.id != constraint.target_ids[1]) continue;
        circle.center_x += delta_x;
        circle.center_y += delta_y;
        // Every OTHER point bound to this circle rides it, exactly like a
        // circle-centre drag: a sketch can have two lines on one circle (the
        // user's does), and translating the circle away from the second one
        // leaves it off the rim — the solver then redistributed everything and
        // the dragged end lagged the cursor by 1.6 mm instead of following it.
        for (const auto& rider : parameters.constraints) {
          if (rider.kind != "point_on_circle" || rider.target_ids.size() < 2) {
            continue;
          }
          if (rider.target_ids[1] != circle.id) continue;
          if (rider.target_ids[0] == line.start_vertex_id ||
              rider.target_ids[0] == line.end_vertex_id) {
            continue;
          }
          const auto rider_position =
              point_position_of(parameters, rider.target_ids[0]);
          if (!rider_position.has_value()) continue;
          propagate_connected_point_move(parameters, rider.target_ids[0],
                                         rider_position->first + delta_x,
                                         rider_position->second + delta_y);
        }
        break;
      }
    }
    propagate_connected_point_move(parameters, far_id, far_x + delta_x,
                                   far_y + delta_y);
    break;
  }

  propagate_connected_point_move(parameters, point_id, x, y);
  sync_all_line_dimensions(parameters);

  // A dimension annotation must stay attached to the geometry it measures. Its
  // label position is a sketch-space point (the UI stores one as soon as the
  // label is placed), so moving the line without moving the label grows the gap —
  // the user's "the dimension label does not move with the line and increases the
  // distance to it" (2026-11-04). The label follows the measured span's midpoint
  // displacement, which keeps the offset under a pure translation AND under a
  // stretch.
  for (const auto& [line_id, before_midpoint] : line_midpoints_before) {
    const auto line_it = std::find_if(
        parameters.lines.begin(), parameters.lines.end(),
        [&](const SketchLine& line) { return line.id == line_id; });
    if (line_it == parameters.lines.end()) continue;
    const double shift_x =
        (line_it->start_x + line_it->end_x) / 2.0 - before_midpoint.first;
    const double shift_y =
        (line_it->start_y + line_it->end_y) / 2.0 - before_midpoint.second;
    if (std::abs(shift_x) < 1e-9 && std::abs(shift_y) < 1e-9) continue;
    for (auto& dimension : parameters.dimensions) {
      if (!dimension.label_x.has_value() || !dimension.label_y.has_value()) {
        continue;
      }
      const bool measures_line =
          (dimension.kind == "line_length" && dimension.entity_id == line_id) ||
          (dimension.kind == "line_angle" && dimension.entity_id == line_id) ||
          (dimension.entity_id == line_it->start_vertex_id &&
           dimension.secondary_entity_id == line_it->end_vertex_id) ||
          (dimension.entity_id == line_it->end_vertex_id &&
           dimension.secondary_entity_id == line_it->start_vertex_id);
      if (!measures_line) continue;
      dimension.label_x = dimension.label_x.value() + shift_x;
      dimension.label_y = dimension.label_y.value() + shift_y;
    }
  }

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
            // The sweep is stored in the ellipse's own frame, which
            // rotates with the shape — angles and ccw are invariant
            // under the transform.
            .has_sweep = ellipse.has_sweep,
            .sweep_start_angle = ellipse.sweep_start_angle,
            .sweep_end_angle = ellipse.sweep_end_angle,
            .ccw = ellipse.ccw,
            .start_vertex_id = copy_vertex(ellipse.start_vertex_id),
            .end_vertex_id = copy_vertex(ellipse.end_vertex_id),
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

  // A moved line is no longer the line the tangency was measured on: the
  // relation's contract pins the line's END at the tangent point, so moving the
  // line only lets the next refresh project that end back onto the stale
  // tangent point — the line deforms instead of moving. Moving is an offending
  // operation, exactly like the endpoint drag (see the drag-policy comment at
  // the top of this file): tangency yields, the relation is dropped and the "T"
  // badge goes with it. Moving the CIRCLE is not affected — the line riding a
  // moved circle is the case the re-projection exists for.
  for (const auto& line_id : moved_line_ids) {
    parameters.line_relations.erase(
        std::remove_if(parameters.line_relations.begin(),
                       parameters.line_relations.end(),
                       [&](const SketchLineRelation& relation) {
                         return relation.kind == "tangent_line_circle" &&
                                relation.first_line_id == line_id;
                       }),
        parameters.line_relations.end());
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
