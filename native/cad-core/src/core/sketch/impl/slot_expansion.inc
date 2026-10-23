// ── Sketch slot expansion ─────────────────────────────────────
// Re-derives slot boundary geometry from the parametric `slots` records
// on every recompute — the same generated-entity pattern as the text
// expansion (text_expansion.inc). Each slot expands into two lines and
// two arcs that are tangent by construction:
//
//   corners:  tl = c_left  + R·n      tr = c_right + R·n
//             bl = c_left  − R·n      br = c_right − R·n
//   where c_left/c_right are the arc centers at distance L/2 from the
//   slot center along the axis direction u, and n is the axis normal.
//
//   line-slot-<id>-bottom  bl -> br   (CCW loop)
//   arc-slot-<id>-right    br -> tr   (ccw=true:  angle −90°→+90° about c_right)
//   line-slot-<id>-top     tr -> tl
//   arc-slot-<id>-left     tl -> bl   (ccw=true:  angle +90°→+270° about c_left)
//
// The generated ids are deterministic and live OUTSIDE the user id
// counters, so slot re-expansion never collides with user entity ids.
// Editing guards: ensure_user_editable_entity rejects generated ids for
// edit/trim/move/dim commands; the vertex rebuild re-marks the
// "vertex-slot-" corner/arc-center vertices fixed.
//
// IMPORTANT ordering contract: refresh_sketch_texts runs FIRST in the
// refresh pipeline and strips ALL generated entities (any generated_by
// value). This pass then strips only its own "slot:"-generated entities
// and re-pushes them, so both expansions can coexist.

void refresh_sketch_slots(SketchFeatureParameters& parameters) {
  // Strip previously generated slot entities (owned by a slot record,
  // or orphaned after a slot was deleted). Only slot-owned entities —
  // the text expansion already rebuilt text glyphs above this pass.
  const auto is_slot_generated = [](const auto& entity) {
    return entity.generated_by.has_value() &&
           entity.generated_by.value().rfind("slot:", 0) == 0;
  };
  parameters.lines.erase(
      std::remove_if(parameters.lines.begin(), parameters.lines.end(),
                     is_slot_generated),
      parameters.lines.end());
  parameters.arcs.erase(
      std::remove_if(parameters.arcs.begin(), parameters.arcs.end(),
                     is_slot_generated),
      parameters.arcs.end());

  for (const auto& slot : parameters.slots) {
    // slot.id already carries the "slot-N" form; the generated ids are
    // "line-slot-N-...", "arc-slot-N-...", "vertex-slot-N-...".
    const std::string prefix = slot.id;

    if (slot.mode == "arc") {
      // Curved slot: the centreline is an arc of radius `arc_radius` about
      // (center_x, center_y); the boundary is the outer arc (radius + half
      // width), the inner arc (radius - half width) and two straight RADIAL
      // end caps. The caps are perpendicular to both arcs, so the loop closes
      // with the same line/arc entity mix the profile walk already handles.
      const double r_out = slot.arc_radius + slot.radius;
      const double r_in = slot.arc_radius - slot.radius;
      const double a0 = slot.start_angle;
      const double a1 = slot.start_angle + slot.sweep_angle;
      const auto on_arc = [&](double angle, double r) {
        return std::pair<double, double>{
            slot.center_x + r * std::cos(angle),
            slot.center_y + r * std::sin(angle)};
      };
      const auto [outer_start_x, outer_start_y] = on_arc(a0, r_out);
      const auto [outer_end_x, outer_end_y] = on_arc(a1, r_out);
      const auto [inner_end_x, inner_end_y] = on_arc(a1, r_in);
      const auto [inner_start_x, inner_start_y] = on_arc(a0, r_in);

      const auto vertex = [&](const std::string& corner) {
        return "vertex-" + prefix + "-" + corner;
      };
      const std::string owner = "slot:" + slot.id;

      // Outer arc: outer_start -> outer_end, counter-clockwise.
      parameters.arcs.push_back(SketchArc{
          .id = "arc-" + prefix + "-outer",
          .start_vertex_id = vertex("outer-start"),
          .end_vertex_id = vertex("outer-end"),
          .center_vertex_id = vertex("arc-center"),
          .center_x = slot.center_x,
          .center_y = slot.center_y,
          .radius = r_out,
          .start_x = outer_start_x,
          .start_y = outer_start_y,
          .end_x = outer_end_x,
          .end_y = outer_end_y,
          .ccw = true,
          .is_construction = false,
          .generated_by = owner,
      });
      // End cap at the far angle: straight radial segment outer -> inner.
      parameters.lines.push_back(SketchLine{
          .id = "line-" + prefix + "-end",
          .start_vertex_id = vertex("outer-end"),
          .end_vertex_id = vertex("inner-end"),
          .start_x = outer_end_x,
          .start_y = outer_end_y,
          .end_x = inner_end_x,
          .end_y = inner_end_y,
          .constraint = std::nullopt,
          .is_construction = false,
          .generated_by = owner,
      });
      // Inner arc: inner_end -> inner_start, clockwise (walking the loop
      // back along the inner boundary).
      parameters.arcs.push_back(SketchArc{
          .id = "arc-" + prefix + "-inner",
          .start_vertex_id = vertex("inner-end"),
          .end_vertex_id = vertex("inner-start"),
          .center_vertex_id = vertex("arc-center"),
          .center_x = slot.center_x,
          .center_y = slot.center_y,
          .radius = r_in,
          .start_x = inner_end_x,
          .start_y = inner_end_y,
          .end_x = inner_start_x,
          .end_y = inner_start_y,
          .ccw = false,
          .is_construction = false,
          .generated_by = owner,
      });
      // End cap at the start angle: straight radial segment inner -> outer.
      parameters.lines.push_back(SketchLine{
          .id = "line-" + prefix + "-start",
          .start_vertex_id = vertex("inner-start"),
          .end_vertex_id = vertex("outer-start"),
          .start_x = inner_start_x,
          .start_y = inner_start_y,
          .end_x = outer_start_x,
          .end_y = outer_start_y,
          .constraint = std::nullopt,
          .is_construction = false,
          .generated_by = owner,
      });
      continue;
    }

    const double half_length = slot.length / 2.0;
    const double ux = std::cos(slot.rotation);
    const double uy = std::sin(slot.rotation);
    const double nx = -uy;
    const double ny = ux;

    const double left_cx = slot.center_x - half_length * ux;
    const double left_cy = slot.center_y - half_length * uy;
    const double right_cx = slot.center_x + half_length * ux;
    const double right_cy = slot.center_y + half_length * uy;

    const double tl_x = left_cx + slot.radius * nx;
    const double tl_y = left_cy + slot.radius * ny;
    const double tr_x = right_cx + slot.radius * nx;
    const double tr_y = right_cy + slot.radius * ny;
    const double bl_x = left_cx - slot.radius * nx;
    const double bl_y = left_cy - slot.radius * ny;
    const double br_x = right_cx - slot.radius * nx;
    const double br_y = right_cy - slot.radius * ny;

    const auto vertex = [&](const std::string& corner) {
      return "vertex-" + prefix + "-" + corner;
    };

    // Bottom line: bl -> br.
    parameters.lines.push_back(SketchLine{
        .id = "line-" + prefix + "-bottom",
        .start_vertex_id = vertex("bl"),
        .end_vertex_id = vertex("br"),
        .start_x = bl_x,
        .start_y = bl_y,
        .end_x = br_x,
        .end_y = br_y,
        .constraint = std::nullopt,
        .is_construction = false,
        .generated_by = "slot:" + slot.id,
    });
    // Top line: tr -> tl.
    parameters.lines.push_back(SketchLine{
        .id = "line-" + prefix + "-top",
        .start_vertex_id = vertex("tr"),
        .end_vertex_id = vertex("tl"),
        .start_x = tr_x,
        .start_y = tr_y,
        .end_x = tl_x,
        .end_y = tl_y,
        .constraint = std::nullopt,
        .is_construction = false,
        .generated_by = "slot:" + slot.id,
    });
    // Right arc: br -> tr, counter-clockwise about c_right.
    parameters.arcs.push_back(SketchArc{
        .id = "arc-" + prefix + "-right",
        .start_vertex_id = vertex("br"),
        .end_vertex_id = vertex("tr"),
        .center_vertex_id = vertex("right-center"),
        .center_x = right_cx,
        .center_y = right_cy,
        .radius = slot.radius,
        .start_x = br_x,
        .start_y = br_y,
        .end_x = tr_x,
        .end_y = tr_y,
        .ccw = true,
        .is_construction = false,
        .generated_by = "slot:" + slot.id,
    });
    // Left arc: tl -> bl, counter-clockwise about c_left — the sweep
    // +90° → +270° (through the west bulge).  A ccw=false arc with the
    // same endpoints sweeps the WRONG half (−90°: the east bulge), which
    // made the slot's west cap cut back INTO the slot interior.
    parameters.arcs.push_back(SketchArc{
        .id = "arc-" + prefix + "-left",
        .start_vertex_id = vertex("tl"),
        .end_vertex_id = vertex("bl"),
        .center_vertex_id = vertex("left-center"),
        .center_x = left_cx,
        .center_y = left_cy,
        .radius = slot.radius,
        .start_x = tl_x,
        .start_y = tl_y,
        .end_x = bl_x,
        .end_y = bl_y,
        .ccw = true,
        .is_construction = false,
        .generated_by = "slot:" + slot.id,
    });
  }
}
