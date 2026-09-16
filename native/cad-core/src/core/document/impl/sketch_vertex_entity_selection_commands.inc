DocumentState DocumentManager::select_sketch_vertex(const std::string& vertex_id,
                                                   bool additive) {
  require_document();

  if (!document_->active_sketch_feature_id.has_value()) {
    throw std::runtime_error("No active sketch");
  }

  const auto feature_it = std::find_if(
      document_->feature_history.begin(),
      document_->feature_history.end(),
      [&](const FeatureEntry& feature) {
        return feature.id == document_->active_sketch_feature_id.value();
      });

  if (feature_it == document_->feature_history.end() ||
      !feature_it->sketch_parameters.has_value()) {
    throw std::runtime_error("Active sketch feature not found");
  }

  const auto vertex_it = std::find_if(
      feature_it->sketch_parameters->vertices.begin(),
      feature_it->sketch_parameters->vertices.end(),
      [&](const SketchVertex& vtx) { return vtx.id == vertex_id; });
  if (vertex_it == feature_it->sketch_parameters->vertices.end()) {
    throw std::runtime_error("Sketch vertex not found: " + vertex_id);
  }

  document_->selected_feature_id = feature_it->id;
  document_->selected_sketch_entity_ids.clear();
  document_->selected_sketch_entity_id = std::nullopt;
  document_->selected_sketch_dimension_id = std::nullopt;
  document_->selected_sketch_profile_id = std::nullopt;
  document_->selected_sketch_profile_ids.clear();
  if (additive) {
    auto& vertex_ids = document_->selected_sketch_vertex_ids;
    const auto existing =
        std::find(vertex_ids.begin(), vertex_ids.end(), vertex_id);
    if (existing == vertex_ids.end()) {
      vertex_ids.push_back(vertex_id);
    } else {
      vertex_ids.erase(existing);
    }
  } else {
    document_->selected_sketch_vertex_ids = {vertex_id};
  }
  document_->selected_sketch_vertex_id =
      document_->selected_sketch_vertex_ids.empty()
          ? std::nullopt
          : std::make_optional(document_->selected_sketch_vertex_ids.back());
  return document_.value();
}

DocumentState DocumentManager::select_sketch_entity(
    const std::string& entity_id, bool additive) {
  require_document();

  if (!document_->active_sketch_feature_id.has_value()) {
    throw std::runtime_error("No active sketch");
  }

  const auto feature_it = std::find_if(
      document_->feature_history.begin(),
      document_->feature_history.end(),
      [&](const FeatureEntry& feature) {
        return feature.id == document_->active_sketch_feature_id.value();
      });

  if (feature_it == document_->feature_history.end() ||
      !feature_it->sketch_parameters.has_value()) {
    throw std::runtime_error("Active sketch feature not found");
  }

  const bool has_line = std::any_of(
      feature_it->sketch_parameters->lines.begin(),
      feature_it->sketch_parameters->lines.end(),
      [&](const SketchLine& line) { return line.id == entity_id; });
  const bool has_circle = std::any_of(
      feature_it->sketch_parameters->circles.begin(),
      feature_it->sketch_parameters->circles.end(),
      [&](const SketchCircle& circle) { return circle.id == entity_id; });
  const bool has_arc = std::any_of(
      feature_it->sketch_parameters->arcs.begin(),
      feature_it->sketch_parameters->arcs.end(),
      [&](const SketchArc& arc) { return arc.id == entity_id; });
  const bool has_polygon = std::any_of(
      feature_it->sketch_parameters->polygons.begin(),
      feature_it->sketch_parameters->polygons.end(),
      [&](const SketchPolygon& polygon) { return polygon.id == entity_id; });

  if (!has_line && !has_circle && !has_arc && !has_polygon) {
    throw std::runtime_error("Sketch entity not found: " + entity_id);
  }

  const bool is_projected_entity = std::any_of(
      feature_it->sketch_parameters->projections.begin(),
      feature_it->sketch_parameters->projections.end(),
      [&](const SketchProjection& projection) {
        return projection_generates_entity(projection, entity_id);
      });

  document_->selected_feature_id = feature_it->id;
  document_->selected_sketch_vertex_ids.clear();
  document_->selected_sketch_vertex_id = std::nullopt;
  if (additive) {
    auto& entity_ids = document_->selected_sketch_entity_ids;
    const auto existing =
        std::find(entity_ids.begin(), entity_ids.end(), entity_id);
    if (existing == entity_ids.end()) {
      entity_ids.push_back(entity_id);
    } else {
      entity_ids.erase(existing);
    }
  } else {
    document_->selected_sketch_entity_ids = {entity_id};
  }
  document_->selected_sketch_entity_id =
      document_->selected_sketch_entity_ids.empty()
          ? std::nullopt
          : std::make_optional(document_->selected_sketch_entity_ids.back());
  const auto dimension_it = std::find_if(
      feature_it->sketch_parameters->dimensions.begin(),
      feature_it->sketch_parameters->dimensions.end(),
      [&](const SketchDimension& dimension) {
        return !is_projected_entity &&
               document_->selected_sketch_entity_ids.size() == 1 &&
               dimension.entity_id == document_->selected_sketch_entity_ids.front();
      });
  document_->selected_sketch_dimension_id =
      dimension_it != feature_it->sketch_parameters->dimensions.end()
          ? std::make_optional(dimension_it->id)
          : std::nullopt;
  document_->selected_sketch_profile_id = std::nullopt;
  document_->selected_sketch_profile_ids.clear();
  document_->selected_sketch_vertex_ids.clear();
  return document_.value();
}

DocumentState DocumentManager::select_sketch_entities(
    const std::vector<std::string>& entity_ids, bool additive) {
  require_document();

  if (!document_->active_sketch_feature_id.has_value()) {
    throw std::runtime_error("No active sketch");
  }

  const auto feature_it = std::find_if(
      document_->feature_history.begin(),
      document_->feature_history.end(),
      [&](const FeatureEntry& feature) {
        return feature.id == document_->active_sketch_feature_id.value();
      });

  if (feature_it == document_->feature_history.end() ||
      !feature_it->sketch_parameters.has_value()) {
    throw std::runtime_error("Active sketch feature not found");
  }

  const auto entity_exists = [&](const std::string& entity_id) {
    const auto& parameters = *feature_it->sketch_parameters;
    const auto in_collection = [&](const auto& collection) {
      return std::any_of(collection.begin(), collection.end(),
                         [&](const auto& entry) {
                           return entry.id == entity_id;
                         });
    };
    return in_collection(parameters.lines) ||
           in_collection(parameters.circles) ||
           in_collection(parameters.arcs) ||
           in_collection(parameters.polygons);
  };

  std::vector<std::string> valid_ids;
  std::vector<std::string> unknown_ids;
  for (const std::string& entity_id : entity_ids) {
    if (entity_exists(entity_id)) {
      valid_ids.push_back(entity_id);
    } else {
      unknown_ids.push_back(entity_id);
    }
  }
  if (!unknown_ids.empty()) {
    log_warn("sketch_selection",
             "select_sketch_entities: " +
                 std::to_string(unknown_ids.size()) +
                 " unknown entity id(s) skipped");
  }

  document_->selected_feature_id = feature_it->id;
  document_->selected_sketch_vertex_ids.clear();
  document_->selected_sketch_vertex_id = std::nullopt;
  document_->selected_sketch_profile_id = std::nullopt;
  document_->selected_sketch_profile_ids.clear();
  document_->selected_sketch_dimension_id = std::nullopt;
  if (additive) {
    auto& selected_ids = document_->selected_sketch_entity_ids;
    for (const std::string& entity_id : valid_ids) {
      const auto existing =
          std::find(selected_ids.begin(), selected_ids.end(), entity_id);
      if (existing == selected_ids.end()) {
        selected_ids.push_back(entity_id);
      } else {
        selected_ids.erase(existing);
      }
    }
  } else {
    document_->selected_sketch_entity_ids = valid_ids;
  }
  document_->selected_sketch_entity_id =
      document_->selected_sketch_entity_ids.empty()
          ? std::nullopt
          : std::make_optional(document_->selected_sketch_entity_ids.back());
  return document_.value();
}

DocumentState DocumentManager::select_sketch_rect(
    double x1, double y1, double x2, double y2, bool window_mode,
    bool additive) {
  require_document();

  if (!document_->active_sketch_feature_id.has_value()) {
    throw std::runtime_error("No active sketch");
  }

  const auto feature_it = std::find_if(
      document_->feature_history.begin(),
      document_->feature_history.end(),
      [&](const FeatureEntry& feature) {
        return feature.id == document_->active_sketch_feature_id.value();
      });

  if (feature_it == document_->feature_history.end() ||
      !feature_it->sketch_parameters.has_value()) {
    throw std::runtime_error("Active sketch feature not found");
  }

  const auto& parameters = *feature_it->sketch_parameters;
  const double rx1 = std::min(x1, x2);
  const double ry1 = std::min(y1, y2);
  const double rx2 = std::max(x1, x2);
  const double ry2 = std::max(y1, y2);

  const auto inside = [&](double px, double py) {
    return px >= rx1 && px <= rx2 && py >= ry1 && py <= ry2;
  };

  // 2D segment / rectangle crossing (the UI's old crossing rule,
  // moved here so it tests the exact geometry).
  const auto crosses = [&](double ax1, double ay1, double ax2, double ay2) {
    const auto cross2d = [](double px, double py, double qx, double qy,
                            double rx, double ry) {
      return (qx - px) * (ry - py) - (qy - py) * (rx - px);
    };
    const auto on_segment = [](double px, double py, double qx, double qy,
                               double rx, double ry) {
      return std::min(px, qx) <= rx && rx <= std::max(px, qx) &&
             std::min(py, qy) <= ry && ry <= std::max(py, qy);
    };
    const auto segments_intersect = [&](double p1x, double p1y, double p2x,
                                        double p2y, double p3x, double p3y,
                                        double p4x, double p4y) {
      const double d1 = cross2d(p1x, p1y, p2x, p2y, p3x, p3y);
      const double d2 = cross2d(p1x, p1y, p2x, p2y, p4x, p4y);
      const double d3 = cross2d(p3x, p3y, p4x, p4y, p1x, p1y);
      const double d4 = cross2d(p3x, p3y, p4x, p4y, p2x, p2y);
      if (((d1 > 0 && d2 < 0) || (d1 < 0 && d2 > 0)) &&
          ((d3 > 0 && d4 < 0) || (d3 < 0 && d4 > 0))) {
        return true;
      }
      if (d1 == 0 && on_segment(p1x, p1y, p2x, p2y, p3x, p3y)) return true;
      if (d2 == 0 && on_segment(p1x, p1y, p2x, p2y, p4x, p4y)) return true;
      if (d3 == 0 && on_segment(p3x, p3y, p4x, p4y, p1x, p1y)) return true;
      if (d4 == 0 && on_segment(p3x, p3y, p4x, p4y, p2x, p2y)) return true;
      return false;
    };
    return segments_intersect(ax1, ay1, ax2, ay2, rx1, ry1, rx2, ry1) ||
           segments_intersect(ax1, ay1, ax2, ay2, rx2, ry1, rx2, ry2) ||
           segments_intersect(ax1, ay1, ax2, ay2, rx2, ry2, rx1, ry2) ||
           segments_intersect(ax1, ay1, ax2, ay2, rx1, ry2, rx1, ry1);
  };

  std::vector<std::string> selected_ids;
  const auto append_if_selected = [&](const std::string& id, bool hit) {
    if (hit) {
      selected_ids.push_back(id);
    }
  };

  // Lines: window mode needs BOTH endpoints inside; crossing mode
  // accepts an endpoint inside or the segment crossing the rect.
  // Construction lines are skipped (they are drafting aids).
  for (const auto& line : parameters.lines) {
    if (line.is_construction) {
      continue;
    }
    const bool start_inside = inside(line.start_x, line.start_y);
    const bool end_inside = inside(line.end_x, line.end_y);
    const bool hit = window_mode
                         ? (start_inside && end_inside)
                         : (start_inside || end_inside ||
                            crosses(line.start_x, line.start_y, line.end_x,
                                    line.end_y));
    append_if_selected(line.id, hit);
  }

  // Circles: the bounding square. Window mode needs it fully inside;
  // crossing mode accepts any overlap. Construction circles are
  // selectable (the UI's old rule) — a construction circle is still
  // a deletable entity.
  for (const auto& circle : parameters.circles) {
    const double bx1 = circle.center_x - circle.radius;
    const double by1 = circle.center_y - circle.radius;
    const double bx2 = circle.center_x + circle.radius;
    const double by2 = circle.center_y + circle.radius;
    const bool hit = window_mode
                         ? (bx1 >= rx1 && bx2 <= rx2 && by1 >= ry1 &&
                            by2 <= ry2)
                         : !(bx2 < rx1 || bx1 > rx2 || by2 < ry1 ||
                             by1 > ry2);
    append_if_selected(circle.id, hit);
  }

  // Arcs: window mode needs BOTH endpoints inside; crossing mode
  // accepts an endpoint inside or the arc's bounding box touching the
  // rect. The box uses the four cardinal extremes of the full circle
  // (the UI's old approximation — it slightly over-selects chords but
  // never misses an arc).
  for (const auto& arc : parameters.arcs) {
    if (arc.is_construction) {
      continue;
    }
    const bool start_inside = inside(arc.start_x, arc.start_y);
    const bool end_inside = inside(arc.end_x, arc.end_y);
    if (window_mode) {
      append_if_selected(arc.id, start_inside && end_inside);
      continue;
    }
    double bx1 = std::min(arc.start_x, arc.end_x);
    double by1 = std::min(arc.start_y, arc.end_y);
    double bx2 = std::max(arc.start_x, arc.end_x);
    double by2 = std::max(arc.start_y, arc.end_y);
    const double radius =
        std::hypot(arc.start_x - arc.center_x, arc.start_y - arc.center_y);
    bx1 = std::min(bx1, arc.center_x - radius);
    by1 = std::min(by1, arc.center_y - radius);
    bx2 = std::max(bx2, arc.center_x + radius);
    by2 = std::max(by2, arc.center_y + radius);
    const bool box_hit = !(bx2 < rx1 || bx1 > rx2 || by2 < ry1 || by1 > ry2);
    append_if_selected(arc.id, start_inside || end_inside || box_hit);
  }

  // Ellipses: bounding square of the larger radius (the UI's old
  // approximation for the rotated major axis).
  for (const auto& ellipse : parameters.ellipses) {
    if (ellipse.is_construction) {
      continue;
    }
    const double max_radius = std::max(ellipse.a, ellipse.b);
    const double bx1 = ellipse.center_x - max_radius;
    const double by1 = ellipse.center_y - max_radius;
    const double bx2 = ellipse.center_x + max_radius;
    const double by2 = ellipse.center_y + max_radius;
    const bool hit = window_mode
                         ? (bx1 >= rx1 && bx2 <= rx2 && by1 >= ry1 &&
                            by2 <= ry2)
                         : !(bx2 < rx1 || bx1 > rx2 || by2 < ry1 ||
                             by1 > ry2);
    append_if_selected(ellipse.id, hit);
  }

  // Apply like select_sketch_entities: replace (or toggle per-id in
  // additive mode) the entity selection; vertex/profile/dimension
  // selections are cleared.
  document_->selected_feature_id = feature_it->id;
  document_->selected_sketch_vertex_ids.clear();
  document_->selected_sketch_vertex_id = std::nullopt;
  document_->selected_sketch_profile_id = std::nullopt;
  document_->selected_sketch_profile_ids.clear();
  document_->selected_sketch_dimension_id = std::nullopt;
  if (additive) {
    auto& existing_ids = document_->selected_sketch_entity_ids;
    for (const std::string& id : selected_ids) {
      const auto existing =
          std::find(existing_ids.begin(), existing_ids.end(), id);
      if (existing == existing_ids.end()) {
        existing_ids.push_back(id);
      } else {
        existing_ids.erase(existing);
      }
    }
  } else {
    document_->selected_sketch_entity_ids = selected_ids;
  }
  document_->selected_sketch_entity_id =
      document_->selected_sketch_entity_ids.empty()
          ? std::nullopt
          : std::make_optional(document_->selected_sketch_entity_ids.back());
  return document_.value();
}
