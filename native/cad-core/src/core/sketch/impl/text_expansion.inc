// ── Sketch text expansion ─────────────────────────────────────
// Re-derives glyph geometry from the parametric `texts` records on
// every recompute. Generated lines use deterministic ids
//   line-text-<text-id>-c<contour>-s<segment>
// with shared per-contour vertex ids
//   vertex-text-<text-id>-c<contour>-s<index>
// (the vertex id names the segment START; adjacent segments share the
// join vertex, the contour closes through segment 0's start). Because
// the tessellation tolerance is proportional to the font height, edits
// to height / angle / position / alignment / spacing keep the same id
// set, so profile ids (`profile-poly-<sorted entity ids>`) survive and
// linked extrudes re-snapshot instead of breaking. Changing the string
// changes the glyph set and degrades linked extrudes through the normal
// dependency_broken path.
//
// The deterministic ids live OUTSIDE the user id counters
// (next_sketch_line_id_ / next_vertex_index), so text re-expansion can
// never collide with or consume user entity ids.

bool is_generated_entity(const SketchFeatureParameters& parameters,
                         const std::string& entity_id) {
  const auto line_it = std::find_if(
      parameters.lines.begin(), parameters.lines.end(),
      [&](const SketchLine& line) { return line.id == entity_id; });
  if (line_it != parameters.lines.end()) {
    return line_it->generated_by.has_value();
  }
  const auto circle_it = std::find_if(
      parameters.circles.begin(), parameters.circles.end(),
      [&](const SketchCircle& circle) { return circle.id == entity_id; });
  if (circle_it != parameters.circles.end()) {
    return circle_it->generated_by.has_value();
  }
  const auto arc_it = std::find_if(
      parameters.arcs.begin(), parameters.arcs.end(),
      [&](const SketchArc& arc) { return arc.id == entity_id; });
  if (arc_it != parameters.arcs.end()) {
    return arc_it->generated_by.has_value();
  }
  return false;
}

void require_user_line(const SketchFeatureParameters& parameters,
                       const std::string& line_id) {
  const auto line_it = std::find_if(
      parameters.lines.begin(), parameters.lines.end(),
      [&](const SketchLine& line) { return line.id == line_id; });
  if (line_it == parameters.lines.end()) {
    throw std::runtime_error("Sketch line not found: " + line_id);
  }
  if (line_it->generated_by.has_value()) {
    throw std::runtime_error("Generated sketch entity is not editable: " +
                             line_id);
  }
}

void ensure_user_editable_entity(const SketchFeatureParameters& parameters,
                                 const std::string& entity_id) {
  if (is_generated_entity(parameters, entity_id)) {
    throw std::runtime_error("Generated sketch entity is not editable: " +
                             entity_id);
  }
}

void refresh_sketch_texts(SketchFeatureParameters& parameters) {
  // Strip every previously generated entity (owned by a text record, or
  // orphaned after a text was deleted / a render started failing). The
  // re-expansion below re-pushes fresh geometry for each live text.
  const auto is_generated = [](const auto& entity) {
    return entity.generated_by.has_value();
  };
  parameters.lines.erase(
      std::remove_if(parameters.lines.begin(), parameters.lines.end(),
                     is_generated),
      parameters.lines.end());
  parameters.circles.erase(
      std::remove_if(parameters.circles.begin(), parameters.circles.end(),
                     is_generated),
      parameters.circles.end());
  parameters.arcs.erase(
      std::remove_if(parameters.arcs.begin(), parameters.arcs.end(),
                     is_generated),
      parameters.arcs.end());

  if (parameters.texts.empty()) {
    return;
  }

  text::TextEngine& engine = text::TextEngine::instance();
  size_t total_segments = 0;

  for (auto& text_entry : parameters.texts) {
    const std::string font_path =
        text_entry.font_path.empty() ? text::TextEngine::bundled_font_path()
                                     : text_entry.font_path;

    text::TextStyle style{.font_path = font_path,
                          .height_mm = text_entry.height_mm,
                          .angle_deg = text_entry.angle_deg,
                          .char_spacing = text_entry.char_spacing,
                          .h_align = text_entry.h_align,
                          .v_align = text_entry.v_align};
    style.path_offset = text_entry.path_offset.value_or(0.0);

    // Text on path: resolve the referenced sketch curve — a user line
    // or arc (generated glyph segments can never be paths). The path
    // geometry is read fresh on every recompute, so dragging the path
    // entity moves the text along with it.
    if (text_entry.path_entity_id.has_value() &&
        !text_entry.path_entity_id->empty()) {
      const std::string& path_id = text_entry.path_entity_id.value();
      const auto path_line = std::find_if(
          parameters.lines.begin(), parameters.lines.end(),
          [&](const SketchLine& line) {
            return line.id == path_id && !line.generated_by.has_value();
          });
      if (path_line != parameters.lines.end()) {
        style.path = text::TextPath{
            .start_x = path_line->start_x,
            .start_y = path_line->start_y,
            .end_x = path_line->end_x,
            .end_y = path_line->end_y,
        };
      } else {
        const auto path_arc = std::find_if(
            parameters.arcs.begin(), parameters.arcs.end(),
            [&](const SketchArc& arc) {
              return arc.id == path_id && !arc.generated_by.has_value();
            });
        if (path_arc != parameters.arcs.end()) {
          const auto norm_angle = [](double a) {
            a = std::fmod(a, 2.0 * M_PI);
            if (a < 0.0) a += 2.0 * M_PI;
            return a;
          };
          const double start_angle =
              std::atan2(path_arc->start_y - path_arc->center_y,
                         path_arc->start_x - path_arc->center_x);
          const double end_angle =
              std::atan2(path_arc->end_y - path_arc->center_y,
                         path_arc->end_x - path_arc->center_x);
          double sweep = path_arc->ccw
                             ? norm_angle(end_angle - start_angle)
                             : norm_angle(start_angle - end_angle);
          if (sweep < 1.0e-9) {
            sweep = 2.0 * M_PI;  // full circle
          }
          style.path = text::TextPath{
              .is_arc = true,
              .center_x = path_arc->center_x,
              .center_y = path_arc->center_y,
              .radius = path_arc->radius,
              .start_angle = start_angle,
              .sweep_angle = sweep,
              .direction = path_arc->ccw ? 1 : -1,
          };
        } else {
          const std::string path_error =
              "text path entity not found: " + path_id;
          if (text_entry.render_error != path_error) {
            log_warn("sketch_text",
                     "text " + text_entry.id + ": " + path_error);
          }
          text_entry.render_error = path_error;
          continue;
        }
      }
    }

    text::TextLayout layout;
    std::string error;
    if (!engine.layout(text_entry.text, text_entry.anchor_x,
                       text_entry.anchor_y, style, &layout, &error)) {
      if (text_entry.render_error != error) {
        log_error("sketch_text",
                  "text " + text_entry.id + " failed to render: " + error);
      }
      text_entry.render_error = error;
      continue;
    }
    text_entry.render_error.clear();

    // Convert the placed contours into SketchLine structs. Pushed
    // directly (bulk pattern — no per-segment add_sketch_line, no
    // inference, no dimensions) with vertices that the vertex rebuild
    // re-marks as fixed (id prefix "vertex-text-"), keeping the solver
    // and the DOF counter away from glyph geometry.
    int contour_index = 0;
    for (const auto& contour : layout.contours) {
      const std::string contour_suffix =
          "-c" + std::to_string(contour_index);
      const size_t count = contour.points.size();
      if (count >= 3) {
        for (size_t s = 0; s < count; ++s) {
          const auto& start = contour.points[s];
          const auto& end = contour.points[(s + 1) % count];
          const std::string seg_suffix = "-s" + std::to_string(s);
          parameters.lines.push_back(SketchLine{
              .id = "line-text-" + text_entry.id + contour_suffix +
                    seg_suffix,
              .start_vertex_id = "vertex-text-" + text_entry.id +
                                 contour_suffix + seg_suffix,
              .end_vertex_id = "vertex-text-" + text_entry.id +
                               contour_suffix + "-s" +
                               std::to_string((s + 1) % count),
              .start_x = start.x,
              .start_y = start.y,
              .end_x = end.x,
              .end_y = end.y,
              .constraint = std::nullopt,
              .is_construction = false,
              .generated_by = "text:" + text_entry.id,
          });
          ++total_segments;
        }
      }
      ++contour_index;
    }
  }

  log_debug("sketch_text",
            "expanded " + std::to_string(parameters.texts.size()) +
                " text(s) into " + std::to_string(total_segments) +
                " glyph segments");
}
