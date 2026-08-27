// ================================================================
// Offset tool: offset_sketch_entity(entity_id, distance)
//
// Non-parametric single-entity offset. Creates a NEW entity at the
// signed distance from the source via the standard creation
// constructors (auto dimensions disabled — the offset is a derived
// copy, not a dimensioned entity):
//   line   -> parallel line shifted along the left normal of
//             start->end (positive distance = left side)
//   circle -> concentric circle with radius + distance
//   arc    -> same center and sweep angles, radius + distance
//             (ccw preserved)
//
// Chain / loop offset is deferred — only single entities.
// ================================================================

namespace {

// Next free numeric suffix for an entity id — same scheme as
// next_trim_entity_index so offsets never collide with existing ids
// (ids are unique keys for the wire builder and profile logic).
template <typename Entity>
int next_offset_entity_index(const std::vector<Entity>& entities) {
  int highest = 0;
  for (const auto& entity : entities) {
    const size_t dash = entity.id.rfind('-');
    if (dash == std::string::npos || dash + 1 >= entity.id.size()) {
      continue;
    }
    const std::string suffix = entity.id.substr(dash + 1);
    if (suffix.find_first_not_of("0123456789") != std::string::npos) {
      continue;
    }
    highest = std::max(highest, std::stoi(suffix));
  }
  return highest + 1;
}

}  // namespace

void offset_sketch_entity(FeatureEntry& feature,
                          const std::string& entity_id,
                          double distance) {
  if (feature.kind != "sketch" || !feature.sketch_parameters.has_value()) {
    throw std::runtime_error("Only sketch features can offset entities");
  }
  if (std::abs(distance) <= kMinimumSketchDimensionValue) {
    throw std::runtime_error("Sketch offset: distance must be non-zero");
  }

  auto& params = *feature.sketch_parameters;

  // Generated geometry (text glyphs, slot outlines, fillet/chamfer
  // lines) is derived — offsetting it would just be overwritten by
  // the expansion pass.
  ensure_user_editable_entity(params, entity_id);

  // ── Line ─────────────────────────────────────────────────────
  const auto line_it = std::find_if(
      params.lines.begin(), params.lines.end(),
      [&](const SketchLine& line) { return line.id == entity_id; });

  if (line_it != params.lines.end()) {
    if (line_it->is_construction) {
      throw std::runtime_error(
          "Sketch offset: construction entities cannot be offset");
    }
    const double dx = line_it->end_x - line_it->start_x;
    const double dy = line_it->end_y - line_it->start_y;
    const double length = std::hypot(dx, dy);
    if (length <= kMinimumSketchDimensionValue) {
      throw std::runtime_error("Sketch offset: line has zero length");
    }
    // Left normal of start->end: positive distance lands on the left
    // side, negative on the right.
    const double nx = -dy / length;
    const double ny = dx / length;
    const int next_index = next_offset_entity_index(params.lines);
    add_sketch_line(feature, next_index,
                    line_it->start_x + distance * nx,
                    line_it->start_y + distance * ny,
                    line_it->end_x + distance * nx,
                    line_it->end_y + distance * ny,
                    /*is_construction=*/false,
                    /*snap_start=*/true,
                    /*auto_dimensions=*/false,
                    /*refresh_immediately=*/true,
                    /*infer_constraints=*/false);
    return;
  }

  // ── Circle ───────────────────────────────────────────────────
  const auto circle_it = std::find_if(
      params.circles.begin(), params.circles.end(),
      [&](const SketchCircle& circle) { return circle.id == entity_id; });

  if (circle_it != params.circles.end()) {
    if (circle_it->is_construction) {
      throw std::runtime_error(
          "Sketch offset: construction entities cannot be offset");
    }
    const double new_radius = circle_it->radius + distance;
    if (new_radius <= kMinimumSketchDimensionValue) {
      throw std::runtime_error(
          "Sketch offset: distance would collapse or invert the circle");
    }
    const int next_index = next_offset_entity_index(params.circles);
    add_sketch_circle(feature, next_index,
                      circle_it->center_x, circle_it->center_y,
                      new_radius,
                      /*is_construction=*/false,
                      /*auto_dimensions=*/false,
                      /*refresh_immediately=*/true);
    return;
  }

  // ── Arc ──────────────────────────────────────────────────────
  const auto arc_it = std::find_if(
      params.arcs.begin(), params.arcs.end(),
      [&](const SketchArc& arc) { return arc.id == entity_id; });

  if (arc_it != params.arcs.end()) {
    if (arc_it->is_construction) {
      throw std::runtime_error(
          "Sketch offset: construction entities cannot be offset");
    }
    const double new_radius = arc_it->radius + distance;
    if (new_radius <= kMinimumSketchDimensionValue) {
      throw std::runtime_error(
          "Sketch offset: distance would collapse or invert the arc");
    }
    // Preserve the sweep: same start/end angles around the same
    // center at the new radius, ccw untouched.
    const double start_angle = std::atan2(arc_it->start_y - arc_it->center_y,
                                          arc_it->start_x - arc_it->center_x);
    const double end_angle = std::atan2(arc_it->end_y - arc_it->center_y,
                                        arc_it->end_x - arc_it->center_x);
    const int next_index = next_offset_entity_index(params.arcs);
    add_sketch_arc(feature, next_index,
                   /*start_point_index=*/params.next_vertex_index,
                   /*end_point_index=*/params.next_vertex_index,
                   arc_it->center_x + new_radius * std::cos(start_angle),
                   arc_it->center_y + new_radius * std::sin(start_angle),
                   arc_it->center_x + new_radius * std::cos(end_angle),
                   arc_it->center_y + new_radius * std::sin(end_angle),
                   arc_it->center_x, arc_it->center_y,
                   new_radius,
                   arc_it->ccw,
                   /*is_construction=*/false,
                   /*auto_dimensions=*/false,
                   /*refresh_immediately=*/true);
    return;
  }

  throw std::runtime_error(
      "Sketch offset: entity not found or not offsettable (lines, circles "
      "and arcs only)");
}
