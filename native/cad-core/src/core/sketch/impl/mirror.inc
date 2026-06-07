std::pair<double, double> reflect_point_across_line(double px, double py,
                                                    double ax, double ay,
                                                    double bx, double by) {
  const double dx = bx - ax;
  const double dy = by - ay;
  const double len_sq = dx * dx + dy * dy;
  // Defensive: zero-length axis can't reflect. Return the source
  // unchanged so callers get a benign no-op rather than NaNs.
  if (len_sq <= 0.0) {
    return {px, py};
  }
  const double t = ((px - ax) * dx + (py - ay) * dy) / len_sq;
  const double foot_x = ax + t * dx;
  const double foot_y = ay + t * dy;
  return {2.0 * foot_x - px, 2.0 * foot_y - py};
}

// Regenerate `pending_mirror.generated_lines/circles` from the
// current `axis_line_id` + `object_ids` selection. Called on every
// preview parameter change. Cheap to run: it just walks the
// object list, reflects each entity's geometry across the axis,
// and pushes a transient SketchLine / SketchCircle into the
// pending state. The transient ids use a `pending-mirror-...`
// prefix so they never collide with committed entities.
//
// If the axis is unset, isn't actually a line, or has zero length,
// the generated arrays are simply cleared (no preview to draw).
void regenerate_mirror_preview(SketchFeatureParameters& parameters) {
  if (!parameters.pending_mirror.has_value()) {
    return;
  }
  auto& pending = *parameters.pending_mirror;
  pending.generated_lines.clear();
  pending.generated_circles.clear();

  if (!pending.axis_line_id.has_value()) {
    return;
  }
  const auto& axis_id = *pending.axis_line_id;
  const auto axis_it = std::find_if(
      parameters.lines.begin(), parameters.lines.end(),
      [&](const SketchLine& line) { return line.id == axis_id; });
  if (axis_it == parameters.lines.end()) {
    return;
  }
  const double ax = axis_it->start_x;
  const double ay = axis_it->start_y;
  const double bx = axis_it->end_x;
  const double by = axis_it->end_y;
  // Zero-length axis can't reflect; bail out so the user sees an
  // empty preview rather than a misleading copy.
  if ((bx - ax) * (bx - ax) + (by - ay) * (by - ay) <= 0.0) {
    return;
  }

  int local_line_counter = 0;
  int local_circle_counter = 0;
  for (const auto& object_id : pending.object_ids) {
    if (object_id == axis_id) {
      continue;  // Axis to itself is a no-op.
    }
    const auto line_it = std::find_if(
        parameters.lines.begin(), parameters.lines.end(),
        [&](const SketchLine& line) { return line.id == object_id; });
    if (line_it != parameters.lines.end()) {
      const auto [new_start_x, new_start_y] = reflect_point_across_line(
          line_it->start_x, line_it->start_y, ax, ay, bx, by);
      const auto [new_end_x, new_end_y] = reflect_point_across_line(
          line_it->end_x, line_it->end_y, ax, ay, bx, by);
      SketchLine reflected{};
      const std::string suffix = std::to_string(local_line_counter++);
      reflected.id = "pending-mirror-line-" + suffix;
      reflected.start_point_id = reflected.id + "-start";
      reflected.end_point_id = reflected.id + "-end";
      reflected.start_x = new_start_x;
      reflected.start_y = new_start_y;
      reflected.end_x = new_end_x;
      reflected.end_y = new_end_y;
      reflected.constraint = std::nullopt;
      reflected.is_construction = line_it->is_construction;
      pending.generated_lines.push_back(reflected);
      continue;
    }
    const auto circle_it = std::find_if(
        parameters.circles.begin(), parameters.circles.end(),
        [&](const SketchCircle& circle) { return circle.id == object_id; });
    if (circle_it != parameters.circles.end()) {
      const auto [new_cx, new_cy] = reflect_point_across_line(
          circle_it->center_x, circle_it->center_y, ax, ay, bx, by);
      SketchCircle reflected{};
      reflected.id =
          "pending-mirror-circle-" + std::to_string(local_circle_counter++);
      reflected.center_x = new_cx;
      reflected.center_y = new_cy;
      reflected.radius = circle_it->radius;
      reflected.is_construction = circle_it->is_construction;
      pending.generated_circles.push_back(reflected);
      continue;
    }
    // Unknown id — silently skip. The UI may have stale selections
    // and we'd rather draw a partial preview than throw.
  }
}

void start_mirror_preview(FeatureEntry& feature) {
  if (feature.kind != "sketch" || !feature.sketch_parameters.has_value()) {
    throw std::runtime_error("Only sketch features can host a mirror tool");
  }
  // Idempotent: re-arming the mirror tool while a preview is
  // already in progress just resets the selections. Saves the
  // caller from having to cancel + start.
  feature.sketch_parameters->pending_mirror =
      SketchFeatureParameters::PendingMirror{};
}

void update_mirror_preview_axis(FeatureEntry& feature,
                                const std::string& axis_line_id) {
  if (feature.kind != "sketch" || !feature.sketch_parameters.has_value()) {
    throw std::runtime_error("Only sketch features can host a mirror tool");
  }
  auto& parameters = *feature.sketch_parameters;
  if (!parameters.pending_mirror.has_value()) {
    throw std::runtime_error(
        "Mirror preview must be started before setting the axis");
  }
  if (axis_line_id.empty()) {
    parameters.pending_mirror->axis_line_id = std::nullopt;
  } else {
    parameters.pending_mirror->axis_line_id = axis_line_id;
  }
  regenerate_mirror_preview(parameters);
}

void update_mirror_preview_objects(
    FeatureEntry& feature, const std::vector<std::string>& object_ids) {
  if (feature.kind != "sketch" || !feature.sketch_parameters.has_value()) {
    throw std::runtime_error("Only sketch features can host a mirror tool");
  }
  auto& parameters = *feature.sketch_parameters;
  if (!parameters.pending_mirror.has_value()) {
    throw std::runtime_error(
        "Mirror preview must be started before setting objects");
  }
  parameters.pending_mirror->object_ids = object_ids;
  regenerate_mirror_preview(parameters);
}

void commit_mirror_preview(FeatureEntry& feature,
                           int& next_line_index,
                           int& next_circle_index,
                           bool persistent) {
  if (feature.kind != "sketch" || !feature.sketch_parameters.has_value()) {
    throw std::runtime_error("Only sketch features can host a mirror tool");
  }
  auto& parameters = *feature.sketch_parameters;
  if (!parameters.pending_mirror.has_value()) {
    throw std::runtime_error("No mirror preview to commit");
  }
  // Snapshot the generated geometry by value before clearing the
  // pending state — `add_sketch_line` / `add_sketch_circle` will
  // also call `refresh_sketch_derived_state` which iterates over
  // sketch state and we don't want a half-cleared pending around.
  const auto generated_lines = parameters.pending_mirror->generated_lines;
  const auto generated_circles = parameters.pending_mirror->generated_circles;
  // Source ids in the order matching `generated_lines/circles` so we
  // can build a source -> mirror id map after committing. The
  // pending state has the source ids in `object_ids` but not split
  // by entity kind; we filter against the live arrays here.
  const auto object_ids = parameters.pending_mirror->object_ids;
  const auto axis_id = parameters.pending_mirror->axis_line_id;
  parameters.pending_mirror = std::nullopt;

  // Snapshot the relations BEFORE committing so we don't iterate
  // over relations we're about to add for the mirrored copies.
  const auto source_relations = parameters.line_relations;

  // Build source -> mirror id maps as we commit. The order of
  // `generated_lines` / `generated_circles` matches the order of
  // matching ids in `object_ids` (regenerate_mirror_preview walks
  // object_ids in order and pushes one entry per recognized id).
  std::unordered_map<std::string, std::string> source_to_mirror_line;
  std::unordered_map<std::string, std::string> source_to_mirror_circle;
  std::size_t generated_line_cursor = 0;
  std::size_t generated_circle_cursor = 0;
  for (const auto& source_id : object_ids) {
    const bool is_line = std::any_of(
        parameters.lines.begin(), parameters.lines.end(),
        [&](const SketchLine& line) { return line.id == source_id; });
    if (is_line) {
      if (generated_line_cursor >= generated_lines.size()) {
        continue;  // Defensive: stale id, no preview was generated.
      }
      const auto& line = generated_lines[generated_line_cursor++];
      const std::string new_id = "line-" + std::to_string(next_line_index);
      add_sketch_line(feature, next_line_index++, line.start_x, line.start_y,
                      line.end_x, line.end_y, line.is_construction);
      source_to_mirror_line.emplace(source_id, new_id);
      continue;
    }
    const bool is_circle = std::any_of(
        parameters.circles.begin(), parameters.circles.end(),
        [&](const SketchCircle& circle) { return circle.id == source_id; });
    if (is_circle) {
      if (generated_circle_cursor >= generated_circles.size()) {
        continue;
      }
      const auto& circle = generated_circles[generated_circle_cursor++];
      const std::string new_id = "circle-" + std::to_string(next_circle_index);
      add_sketch_circle(feature, next_circle_index++, circle.center_x,
                        circle.center_y, circle.radius,
                        circle.is_construction);
      source_to_mirror_circle.emplace(source_id, new_id);
    }
  }

  // Carry over line relations whose BOTH participants were
  // mirrored. Reflection preserves equal_length / perpendicular /
  // parallel / tangent_line_circle, so duplicating the relation on
  // the mirrored pair keeps the constraint network intact. Single-
  // sided relations (only one endpoint mirrored) are skipped on
  // purpose — coupling the mirror to the original would lock the
  // user out of editing one side independently.
  for (const auto& relation : source_relations) {
    if (relation.kind == "tangent_line_circle") {
      const auto line_it = source_to_mirror_line.find(relation.first_line_id);
      const auto circle_it =
          source_to_mirror_circle.find(relation.second_line_id);
      if (line_it == source_to_mirror_line.end() ||
          circle_it == source_to_mirror_circle.end()) {
        continue;
      }
      parameters.line_relations.push_back(SketchLineRelation{
          .id = "rel-tangent-" + line_it->second + "-" + circle_it->second,
          .kind = "tangent_line_circle",
          .first_line_id = line_it->second,
          .second_line_id = circle_it->second,
      });
      continue;
    }
    const auto first_it = source_to_mirror_line.find(relation.first_line_id);
    const auto second_it = source_to_mirror_line.find(relation.second_line_id);
    if (first_it == source_to_mirror_line.end() ||
        second_it == source_to_mirror_line.end()) {
      continue;
    }
    std::string id_prefix;
    if (relation.kind == "equal_length") {
      id_prefix = "rel-equal-length-";
    } else if (relation.kind == "perpendicular") {
      id_prefix = "rel-perpendicular-";
    } else if (relation.kind == "parallel") {
      id_prefix = "rel-parallel-";
    } else {
      continue;  // Unknown kind — leave it alone rather than guess.
    }
    parameters.line_relations.push_back(SketchLineRelation{
        .id = id_prefix + first_it->second,
        .kind = relation.kind,
        .first_line_id = first_it->second,
        .second_line_id = second_it->second,
    });
  }

  // Collect all mirrored entity IDs (both lines and circles) so we
  // can strip their auto-dimensions. Mirrored entities only show the
  // mirror badge — auto line-length / angle / radius dims are clutter.
  std::vector<std::string> mirrored_ids;
  for (const auto& [src, dst] : source_to_mirror_line) {
    mirrored_ids.push_back(dst);
  }
  for (const auto& [src, dst] : source_to_mirror_circle) {
    mirrored_ids.push_back(dst);
  }
  parameters.dimensions.erase(
      std::remove_if(parameters.dimensions.begin(), parameters.dimensions.end(),
                     [&](const SketchDimension& d) {
                       // Strip all auto-generated dimensions from mirrored
                       // entities — they only show the mirror badge. The
                       // id prefix (dim-line- / dim-line-angle- / dim-circle-)
                       // reliably identifies dimensions created by add_sketch_line
                       // and add_sketch_circle.
                       const bool is_auto_dim =
                           d.id.rfind("dim-line-", 0) == 0 ||
                           d.id.rfind("dim-line-angle-", 0) == 0 ||
                           d.id.rfind("dim-circle-", 0) == 0;
                       return is_auto_dim &&
                              std::find(mirrored_ids.begin(), mirrored_ids.end(),
                                        d.entity_id) != mirrored_ids.end();
                     }),
      parameters.dimensions.end());

  // Store persistent mirror relations when the user wants live updates.
  if (persistent && axis_id.has_value()) {
    const auto& ax_id = axis_id.value();
    for (const auto& [source_id, mirror_id] : source_to_mirror_line) {
      parameters.mirror_relations.push_back(SketchMirrorRelation{
          .id = "mirror-rel-" + source_id + "-" + mirror_id,
          .source_id = source_id,
          .mirror_id = mirror_id,
          .axis_line_id = ax_id,
      });
    }
    for (const auto& [source_id, mirror_id] : source_to_mirror_circle) {
      parameters.mirror_relations.push_back(SketchMirrorRelation{
          .id = "mirror-rel-" + source_id + "-" + mirror_id,
          .source_id = source_id,
          .mirror_id = mirror_id,
          .axis_line_id = ax_id,
      });
    }
  }

  // Re-run the derived-state refresh once after relation copying so
  // the constraint glyphs / profile detection see the new edges.
  refresh_sketch_derived_state(feature);
}

void cancel_mirror_preview(FeatureEntry& feature) {
  if (feature.kind != "sketch" || !feature.sketch_parameters.has_value()) {
    throw std::runtime_error("Only sketch features can host a mirror tool");
  }
  // Plain discard. `pending_mirror` was never written to the main
  // arrays so there's nothing to roll back.
  feature.sketch_parameters->pending_mirror = std::nullopt;
}
