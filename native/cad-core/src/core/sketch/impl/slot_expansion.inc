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
//   arc-slot-<id>-left     tl -> bl   (ccw=false: angle +90°→−90° about c_left)
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

    // slot.id already carries the "slot-N" form; the generated ids are
    // "line-slot-N-...", "arc-slot-N-...", "vertex-slot-N-...".
    const std::string prefix = slot.id;
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
    // Left arc: tl -> bl, clockwise about c_left.
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
        .ccw = false,
        .is_construction = false,
        .generated_by = "slot:" + slot.id,
    });
  }
}
