DocumentState DocumentManager::add_sketch_circle(
    double center_x,
    double center_y,
    double radius,
    bool is_construction,
    const std::string& mode,
    double p1_x,
    double p1_y,
    double p2_x,
    double p2_y,
    double p3_x,
    double p3_y,
    const std::string& line_a_id,
    const std::string& line_b_id,
    const std::string& line_c_id,
    double hint_x,
    double hint_y) {
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

  if (feature_it == document_->feature_history.end()) {
    throw std::runtime_error("Active sketch feature not found");
  }

  // Mode resolution (mirrors the arc wrapper's mode handling): the
  // UI sends raw click inputs and this wrapper derives the single
  // source of truth (center, radius) plus tangent relations for the
  // defining lines.
  double resolved_cx = center_x;
  double resolved_cy = center_y;
  double resolved_r = radius;
  std::vector<std::string> tangent_line_ids;
  // Same dimension floor as the arc wrapper — the canonical constant
  // is TU-local to sketch_feature.cpp.
  constexpr double kCircleMinimumDimension = 0.001;

  const auto require_mode_line = [&](const std::string& line_id,
                                     const char* which) -> const SketchLine& {
    if (line_id.empty()) {
      throw std::runtime_error(std::string("Circle mode: missing ") + which);
    }
    for (const auto& line : feature_it->sketch_parameters->lines) {
      if (line.id == line_id) {
        return line;
      }
    }
    throw std::runtime_error(std::string("Circle mode: line not found: ") +
                             line_id);
  };

  if (mode == "two_point") {
    resolved_cx = (p1_x + p2_x) / 2.0;
    resolved_cy = (p1_y + p2_y) / 2.0;
    resolved_r = std::hypot(p2_x - p1_x, p2_y - p1_y) / 2.0;
    if (resolved_r <= kCircleMinimumDimension) {
      throw std::runtime_error("Two-point circle: points coincide");
    }
  } else if (mode == "three_point") {
    // Circumcenter via the perpendicular-bisector intersection
    // (same form as the three-point arc).
    const double d = 2.0 *
        (p1_x * (p2_y - p3_y) + p2_x * (p3_y - p1_y) + p3_x * (p1_y - p2_y));
    if (std::abs(d) < 1e-9) {
      throw std::runtime_error("Three-point circle: points are colinear");
    }
    const double s1 = p1_x * p1_x + p1_y * p1_y;
    const double s2 = p2_x * p2_x + p2_y * p2_y;
    const double s3 = p3_x * p3_x + p3_y * p3_y;
    resolved_cx = (s1 * (p2_y - p3_y) + s2 * (p3_y - p1_y) +
                   s3 * (p1_y - p2_y)) / d;
    resolved_cy = (s1 * (p3_x - p2_x) + s2 * (p1_x - p3_x) +
                   s3 * (p2_x - p1_x)) / d;
    resolved_r = std::hypot(p1_x - resolved_cx, p1_y - resolved_cy);
  } else if (mode == "tangent_two_lines" || mode == "tangent_three_lines") {
    const SketchLine& line_a = require_mode_line(line_a_id, "first line");
    const SketchLine& line_b = require_mode_line(line_b_id, "second line");
    tangent_line_ids = {line_a_id, line_b_id};
    const auto support_intersection = [&](const SketchLine& l1,
                                          const SketchLine& l2) {
      const double ax = l1.start_x, ay = l1.start_y;
      const double bx = l1.end_x - ax, by = l1.end_y - ay;
      const double cx = l2.start_x, cy = l2.start_y;
      const double dx = l2.end_x - cx, dy = l2.end_y - cy;
      const double denom = bx * dy - by * dx;
      if (std::abs(denom) < 1e-9) {
        throw std::runtime_error(
            "Circle tangent mode: the defining lines are parallel");
      }
      const double t = ((cx - ax) * dy - (cy - ay) * dx) / denom;
      return std::make_pair(ax + t * bx, ay + t * by);
    };
    const auto unit_dir = [&](const SketchLine& l) {
      const double dx = l.end_x - l.start_x;
      const double dy = l.end_y - l.start_y;
      const double len = std::hypot(dx, dy);
      if (len <= kCircleMinimumDimension) {
        throw std::runtime_error("Circle tangent mode: zero-length line");
      }
      return std::make_pair(dx / len, dy / len);
    };
    const auto distance_to_line = [&](double cx, double cy,
                                      const SketchLine& l) {
      const double abx = l.end_x - l.start_x;
      const double aby = l.end_y - l.start_y;
      return std::abs(abx * (cy - l.start_y) - aby * (cx - l.start_x)) /
             std::hypot(abx, aby);
    };

    if (mode == "tangent_two_lines") {
      // Center on the angle bisector through the corner; the hint
      // point selects the wedge side and its projection onto the
      // bisector sets the size.
      const auto [cx0, cy0] = support_intersection(line_a, line_b);
      const auto [uax, uay] = unit_dir(line_a);
      const auto [ubx, uby] = unit_dir(line_b);
      // The two angle bisectors are normalize(u1 + u2) and
      // normalize(u1 - u2) — the hint point selects which wedge the
      // circle sits in (the one with the larger projection).
      const auto normalize = [](double x, double y) {
        const double len = std::hypot(x, y);
        if (len < 1e-9) {
          throw std::runtime_error(
              "Circle tangent mode: the defining lines are anti-parallel");
        }
        return std::make_pair(x / len, y / len);
      };
      const auto [bis1_x, bis1_y] = normalize(uax + ubx, uay + uby);
      const auto [bis2_x, bis2_y] = normalize(uax - ubx, uay - uby);
      // Choose the bisector with the larger ABSOLUTE projection and
      // point it toward the hint — the signed max fails when the
      // defining lines are stored pointing toward the corner (both
      // projections negative; the "max" then picks the one pointing
      // AWAY from the wedge).
      const double t1 = (hint_x - cx0) * bis1_x + (hint_y - cy0) * bis1_y;
      const double t2 = (hint_x - cx0) * bis2_x + (hint_y - cy0) * bis2_y;
      const bool use_first = std::abs(t1) >= std::abs(t2);
      const double t = std::abs(use_first ? t1 : t2);
      const bool flip = (use_first ? t1 : t2) < 0.0;
      if (t <= kCircleMinimumDimension) {
        throw std::runtime_error(
            "Tangent circle: the placement point is too close to the corner");
      }
      const double bis_x = (use_first ? bis1_x : bis2_x) * (flip ? -1.0 : 1.0);
      const double bis_y = (use_first ? bis1_y : bis2_y) * (flip ? -1.0 : 1.0);
      resolved_cx = cx0 + bis_x * t;
      resolved_cy = cy0 + bis_y * t;
      resolved_r = distance_to_line(resolved_cx, resolved_cy, line_a);
    } else {
      // Three lines: the triangle's incenter. Pairwise support
      // intersections are the vertices; the incenter is the
      // side-length-weighted average.
      const SketchLine& line_c = require_mode_line(line_c_id, "third line");
      tangent_line_ids.push_back(line_c_id);
      const auto [vax, vay] = support_intersection(line_a, line_b);
      const auto [vbx, vby] = support_intersection(line_b, line_c);
      const auto [vcx, vcy] = support_intersection(line_c, line_a);
      const double a = std::hypot(vbx - vcx, vby - vcy);
      const double b = std::hypot(vcx - vax, vcy - vay);
      const double c = std::hypot(vax - vbx, vay - vby);
      const double perimeter = a + b + c;
      if (perimeter <= kCircleMinimumDimension) {
        throw std::runtime_error(
            "Tangent circle: the three lines do not form a triangle");
      }
      resolved_cx = (a * vax + b * vbx + c * vcx) / perimeter;
      resolved_cy = (a * vay + b * vby + c * vcy) / perimeter;
      resolved_r = distance_to_line(resolved_cx, resolved_cy, line_a);
    }
    if (resolved_r <= kCircleMinimumDimension) {
      throw std::runtime_error(
          "Tangent circle: resolved radius is too small");
    }
  } else if (!mode.empty() && mode != "center_radius") {
    // The IPC layer sends an empty mode for the plain center+radius
    // flow — treat it as the default.
    throw std::runtime_error("Unknown circle creation mode: " + mode);
  }

  const int circle_index = next_sketch_circle_id_;
  push_undo_state();
  clear_redo_stack();
  polysmith::core::add_sketch_circle(
      *feature_it, next_sketch_circle_id_++, resolved_cx, resolved_cy,
      resolved_r, is_construction);
  if (!tangent_line_ids.empty()) {
    polysmith::core::attach_circle_tangent_relations(
        *feature_it, "circle-" + std::to_string(circle_index),
        tangent_line_ids);
    // Re-run the derived-state refresh so the new relations'
    // enforcement measures the radius immediately.
    polysmith::core::refresh_sketch_derived_state(*feature_it);
  }
  refresh_linked_extrudes(*document_, *feature_it);
  document_->selected_feature_id = feature_it->id;
  document_->selected_sketch_vertex_id = std::nullopt;
  document_->selected_sketch_entity_id =
      feature_it->sketch_parameters->circles.back().id;
  document_->selected_sketch_dimension_id =
      is_construction ? std::nullopt
                      : std::make_optional(
                            feature_it->sketch_parameters->dimensions.back().id);
  document_->selected_sketch_profile_id = std::nullopt;
  document_->selected_sketch_profile_ids.clear();
  document_->selected_sketch_vertex_ids.clear();
  document_->selected_sketch_entity_ids.clear();
  document_->active_sketch_tool = "circle";
  bump_geometry_revision();
  return document_.value();
}

DocumentState DocumentManager::add_sketch_polygon(int sides,
                                                  const std::string& mode,
                                                  double start_x,
                                                  double start_y,
                                                  double end_x,
                                                  double end_y,
                                                  bool is_construction) {
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

  if (feature_it == document_->feature_history.end()) {
    throw std::runtime_error("Active sketch feature not found");
  }

  push_undo_state();
  clear_redo_stack();
  polysmith::core::add_sketch_polygon(
      *feature_it, next_sketch_polygon_id_++, sides, mode, start_x, start_y,
      end_x, end_y, is_construction);
  refresh_linked_extrudes(*document_, *feature_it);
  document_->selected_feature_id = feature_it->id;
  document_->selected_sketch_vertex_id = std::nullopt;
  document_->selected_sketch_entity_id =
      feature_it->sketch_parameters->polygons.back().id;
  document_->selected_sketch_dimension_id = std::nullopt;
  document_->selected_sketch_profile_id = std::nullopt;
  document_->selected_sketch_profile_ids.clear();
  document_->selected_sketch_vertex_ids.clear();
  document_->selected_sketch_entity_ids.clear();
  document_->active_sketch_tool = "polygon";
  bump_geometry_revision();
  return document_.value();
}
