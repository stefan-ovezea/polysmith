DocumentState DocumentManager::extrude_open_entities(
    const std::vector<std::string>& entity_ids,
    double depth,
    const std::string& mode,
    const std::optional<std::string>& target_body_id,
    const std::optional<ExtrudeFeatureParameters>& parameters) {
  require_document();
  if (entity_ids.empty()) {
    throw std::runtime_error("No sketch entities selected");
  }

  FeatureEntry* sketch_feature = nullptr;
  std::vector<SketchProfilePoint> chain;
  for (const auto& entity_id : entity_ids) {
    FeatureEntry* owner = nullptr;
    const SketchLine* line = nullptr;
    const SketchArc* arc = nullptr;
    for (auto& feature : document_->feature_history) {
      if (feature.kind != "sketch" || !feature.sketch_parameters.has_value()) {
        continue;
      }
      const auto& sketch = feature.sketch_parameters.value();
      const auto line_it = std::find_if(
          sketch.lines.begin(),
          sketch.lines.end(),
          [&](const SketchLine& candidate) { return candidate.id == entity_id; });
      if (line_it != sketch.lines.end()) {
        owner = &feature;
        line = &(*line_it);
        break;
      }
      const auto arc_it = std::find_if(
          sketch.arcs.begin(),
          sketch.arcs.end(),
          [&](const SketchArc& candidate) { return candidate.id == entity_id; });
      if (arc_it != sketch.arcs.end()) {
        owner = &feature;
        arc = &(*arc_it);
        break;
      }
    }
    if (owner == nullptr) {
      throw std::runtime_error("Sketch entity not found: " + entity_id);
    }
    if (sketch_feature != nullptr && sketch_feature->id != owner->id) {
      throw std::runtime_error(
          "Open thin extrude entities must belong to the same sketch");
    }
    sketch_feature = owner;

    auto append_point = [&](double x, double y) {
      if (!chain.empty()) {
        const auto& last = chain.back();
        const double dx = last.x - x;
        const double dy = last.y - y;
        if (std::sqrt(dx * dx + dy * dy) <= 1.0e-6) {
          return;
        }
      }
      chain.push_back({.x = x, .y = y});
    };

    if (line != nullptr) {
      append_point(line->start_x, line->start_y);
      append_point(line->end_x, line->end_y);
    } else if (arc != nullptr) {
      const double start_angle =
          std::atan2(arc->start_y - arc->center_y, arc->start_x - arc->center_x);
      double end_angle =
          std::atan2(arc->end_y - arc->center_y, arc->end_x - arc->center_x);
      if (arc->ccw && end_angle < start_angle) {
        end_angle += 2.0 * 3.14159265358979323846;
      } else if (!arc->ccw && end_angle > start_angle) {
        end_angle -= 2.0 * 3.14159265358979323846;
      }
      constexpr int kArcSegments = 16;
      for (int index = 0; index <= kArcSegments; ++index) {
        const double t = static_cast<double>(index) / kArcSegments;
        const double angle = start_angle + (end_angle - start_angle) * t;
        append_point(arc->center_x + std::cos(angle) * arc->radius,
                     arc->center_y + std::sin(angle) * arc->radius);
      }
    }
  }

  if (sketch_feature == nullptr || !sketch_feature->sketch_parameters.has_value()) {
    throw std::runtime_error("Open thin extrude source sketch not found");
  }
  if (chain.size() < 2) {
    throw std::runtime_error("Open thin extrude requires a connected chain");
  }

  const auto& sketch = sketch_feature->sketch_parameters.value();
  const std::string plane_id =
      sketch.plane_frame.has_value() ? plane_id_from_frame(sketch.plane_frame.value())
                                     : sketch.plane_id;
  const std::optional<PlaneFrame> plane_frame =
      sketch.plane_frame.has_value()
          ? std::optional<PlaneFrame>(make_plane_frame(sketch.plane_frame.value()))
          : std::nullopt;

  ExtrudeFeatureParameters extrude_parameters =
      parameters.value_or(ExtrudeFeatureParameters{});
  extrude_parameters.sketch_feature_id = sketch_feature->id;
  extrude_parameters.profile_id = "open-chain";
  extrude_parameters.profile_ids = {};
  extrude_parameters.open_entity_ids = entity_ids;
  extrude_parameters.plane_id = plane_id;
  extrude_parameters.plane_frame = plane_frame;
  extrude_parameters.profile_kind = "open_chain";
  extrude_parameters.start_x = 0.0;
  extrude_parameters.start_y = 0.0;
  extrude_parameters.width = 0.0;
  extrude_parameters.height = 0.0;
  extrude_parameters.radius = 0.0;
  extrude_parameters.profile_points = chain;
  extrude_parameters.inner_loops.clear();
  extrude_parameters.additional_profile_points.clear();
  extrude_parameters.additional_inner_loops.clear();
  extrude_parameters.depth = depth;
  extrude_parameters.mode = mode;
  if (mode.empty()) {
    extrude_parameters.mode = "new_body";
    extrude_parameters.operation = "new_body";
  } else if (extrude_parameters.operation == "new_body" && mode != "new_body") {
    extrude_parameters.operation = mode;
  }
  extrude_parameters.target_body_id = target_body_id;
  extrude_parameters.thin.enabled = true;
  normalize_extrude_parameters(*document_, extrude_parameters);

  push_undo_state();
  clear_redo_stack();
  document_->feature_history.push_back(
      create_extrude_feature(next_feature_id_++, extrude_parameters));
  document_->selected_feature_id = document_->feature_history.back().id;
  document_->selected_reference_id = std::nullopt;
  document_->selected_sketch_entity_ids.clear();
  document_->selected_sketch_entity_id = std::nullopt;
  document_->selected_sketch_profile_id = std::nullopt;
  document_->selected_sketch_profile_ids.clear();
  document_->selected_sketch_entity_ids.clear();
  document_->selected_sketch_vertex_ids.clear();
  document_->selected_sketch_text_id = std::nullopt;
  document_->selected_face_id = std::nullopt;
  document_->selected_edge_ids.clear();
  document_->selected_vertex_ids.clear();
  bump_geometry_revision();
  return document_.value();
}
