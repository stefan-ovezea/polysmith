// Flatten a world-space point onto a sketch plane's local (u, v).
// Mirror of `world_to_sketch_local` in document.cpp — pulled in here
// so the live-projection refresher doesn't have to reach across
// translation units. Identical maths.
std::pair<double, double> world_to_sketch_local(
    const SketchFeatureParameters::SketchPlaneFrame& frame,
    double wx, double wy, double wz) {
  const double dx = wx - frame.origin_x;
  const double dy = wy - frame.origin_y;
  const double dz = wz - frame.origin_z;
  const double sx =
      dx * frame.x_axis_x + dy * frame.x_axis_y + dz * frame.x_axis_z;
  const double sy =
      dx * frame.y_axis_x + dy * frame.y_axis_y + dz * frame.y_axis_z;
  return {sx, sy};
}

std::array<double, 3> sketch_local_to_world(const PlaneFrame& frame,
                                            double local_x,
                                            double local_y) {
  return {
      frame.origin_x + local_x * frame.x_axis_x + local_y * frame.y_axis_x,
      frame.origin_y + local_x * frame.x_axis_y + local_y * frame.y_axis_y,
      frame.origin_z + local_x * frame.x_axis_z + local_y * frame.y_axis_z,
  };
}

std::vector<double> make_refreshed_helix_points(
    const ConstructionAxisFeatureParameters& axis,
    const HelixFeatureParameters& params) {
  constexpr double kPi = 3.14159265358979323846;
  const double ax = axis.end_x - axis.start_x;
  const double ay = axis.end_y - axis.start_y;
  const double az = axis.end_z - axis.start_z;
  const double length = std::sqrt(ax * ax + ay * ay + az * az);
  if (length <= 1.0e-9 || params.radius <= 0.0 || params.pitch <= 0.0 ||
      params.height <= 0.0) {
    return {};
  }

  const std::array<double, 3> dir{ax / length, ay / length, az / length};
  const std::array<double, 3> seed =
      std::abs(dir[0]) < 0.9 ? std::array<double, 3>{1.0, 0.0, 0.0}
                             : std::array<double, 3>{0.0, 1.0, 0.0};
  std::array<double, 3> u{
      dir[1] * seed[2] - dir[2] * seed[1],
      dir[2] * seed[0] - dir[0] * seed[2],
      dir[0] * seed[1] - dir[1] * seed[0],
  };
  const double u_len = std::sqrt(u[0] * u[0] + u[1] * u[1] + u[2] * u[2]);
  if (u_len <= 1.0e-9) {
    return {};
  }
  u = {u[0] / u_len, u[1] / u_len, u[2] / u_len};
  const std::array<double, 3> v{
      dir[1] * u[2] - dir[2] * u[1],
      dir[2] * u[0] - dir[0] * u[2],
      dir[0] * u[1] - dir[1] * u[0],
  };

  const double turns = params.height / params.pitch;
  const int samples = std::max(24, static_cast<int>(std::ceil(turns * 48.0)));
  const double handed = params.handedness == "left" ? -1.0 : 1.0;
  const double start = params.start_angle_degrees * kPi / 180.0;
  std::vector<double> points;
  points.reserve(static_cast<size_t>(samples + 1) * 3);
  for (int i = 0; i <= samples; ++i) {
    const double t = static_cast<double>(i) / static_cast<double>(samples);
    const double along = params.height * t;
    const double angle = start + handed * 2.0 * kPi * turns * t;
    points.push_back(axis.start_x + dir[0] * along +
                     u[0] * std::cos(angle) * params.radius +
                     v[0] * std::sin(angle) * params.radius);
    points.push_back(axis.start_y + dir[1] * along +
                     u[1] * std::cos(angle) * params.radius +
                     v[1] * std::sin(angle) * params.radius);
    points.push_back(axis.start_z + dir[2] * along +
                     u[2] * std::cos(angle) * params.radius +
                     v[2] * std::sin(angle) * params.radius);
  }
  return points;
}

struct SweepPathEntity {
  std::string id;
  std::string kind;
  std::string start_point_id;
  std::string end_point_id;
  const SketchLine* line = nullptr;
  const SketchArc* arc = nullptr;
};

double normalize_sweep_arc_angle(double start_angle,
                                 double end_angle,
                                 bool ccw) {
  constexpr double kPi = 3.14159265358979323846;
  double sweep = end_angle - start_angle;
  if (ccw) {
    while (sweep <= 0.0) {
      sweep += 2.0 * kPi;
    }
  } else {
    while (sweep >= 0.0) {
      sweep -= 2.0 * kPi;
    }
  }
  return sweep;
}

std::vector<SweepPathEntity> order_sweep_path_entities(
    const SketchFeatureParameters& sketch,
    const std::string& seed_entity_id) {
  std::vector<SweepPathEntity> entities;
  for (const auto& line : sketch.lines) {
    if (!line.is_construction) {
      entities.push_back(SweepPathEntity{.id = line.id,
                                         .kind = "line",
                                         .start_point_id = line.start_point_id,
                                         .end_point_id = line.end_point_id,
                                         .line = &line});
    }
  }
  for (const auto& arc : sketch.arcs) {
    if (!arc.is_construction) {
      entities.push_back(SweepPathEntity{.id = arc.id,
                                         .kind = "arc",
                                         .start_point_id = arc.start_point_id,
                                         .end_point_id = arc.end_point_id,
                                         .arc = &arc});
    }
  }
  const auto seed_it = std::find_if(
      entities.begin(), entities.end(),
      [&](const SweepPathEntity& entity) { return entity.id == seed_entity_id; });
  if (seed_it == entities.end()) {
    return {};
  }
  const size_t seed_index =
      static_cast<size_t>(std::distance(entities.begin(), seed_it));
  std::unordered_map<std::string, std::vector<size_t>> by_point;
  for (size_t index = 0; index < entities.size(); ++index) {
    by_point[entities[index].start_point_id].push_back(index);
    by_point[entities[index].end_point_id].push_back(index);
  }

  std::vector<size_t> stack{seed_index};
  std::unordered_set<size_t> component;
  while (!stack.empty()) {
    const size_t index = stack.back();
    stack.pop_back();
    if (!component.insert(index).second) {
      continue;
    }
    for (const auto& point_id :
         {entities[index].start_point_id, entities[index].end_point_id}) {
      for (const size_t connected : by_point[point_id]) {
        if (!component.count(connected)) {
          stack.push_back(connected);
        }
      }
    }
  }
  for (const size_t index : component) {
    const auto& entity = entities[index];
    if (by_point[entity.start_point_id].size() > 2 ||
        by_point[entity.end_point_id].size() > 2) {
      return {};
    }
  }

  size_t start_index = seed_index;
  std::string current_point = entities[seed_index].start_point_id;
  for (const size_t index : component) {
    const auto& entity = entities[index];
    if (by_point[entity.start_point_id].size() == 1) {
      start_index = index;
      current_point = entity.start_point_id;
      break;
    }
    if (by_point[entity.end_point_id].size() == 1) {
      start_index = index;
      current_point = entity.end_point_id;
      break;
    }
  }

  std::vector<SweepPathEntity> ordered;
  std::unordered_set<size_t> used;
  size_t current_index = start_index;
  while (used.size() < component.size()) {
    const auto& entity = entities[current_index];
    SweepPathEntity oriented = entity;
    if (entity.end_point_id == current_point) {
      std::swap(oriented.start_point_id, oriented.end_point_id);
    }
    ordered.push_back(oriented);
    used.insert(current_index);
    current_point = oriented.end_point_id;

    std::optional<size_t> next_index;
    for (const size_t connected : by_point[current_point]) {
      if (component.count(connected) && !used.count(connected)) {
        next_index = connected;
        break;
      }
    }
    if (!next_index.has_value()) {
      break;
    }
    current_index = next_index.value();
  }
  return ordered.size() == component.size() ? ordered : std::vector<SweepPathEntity>{};
}

std::vector<SweepFeatureParameters::PathSegment> make_sweep_path_segments(
    const SketchFeatureParameters& sketch,
    const PlaneFrame& path_frame,
    const std::string& seed_entity_id) {
  const auto ordered = order_sweep_path_entities(sketch, seed_entity_id);
  std::vector<SweepFeatureParameters::PathSegment> segments;
  for (const auto& entity : ordered) {
    if (entity.kind == "line" && entity.line != nullptr) {
      const bool reversed = entity.start_point_id != entity.line->start_point_id;
      const auto start = sketch_local_to_world(
          path_frame, reversed ? entity.line->end_x : entity.line->start_x,
          reversed ? entity.line->end_y : entity.line->start_y);
      const auto end = sketch_local_to_world(
          path_frame, reversed ? entity.line->start_x : entity.line->end_x,
          reversed ? entity.line->start_y : entity.line->end_y);
      segments.push_back(SweepFeatureParameters::PathSegment{
          .entity_id = entity.id,
          .kind = "line",
          .start_x = start[0],
          .start_y = start[1],
          .start_z = start[2],
          .end_x = end[0],
          .end_y = end[1],
          .end_z = end[2],
      });
      continue;
    }
    if (entity.kind == "arc" && entity.arc != nullptr) {
      const bool reversed = entity.start_point_id != entity.arc->start_point_id;
      const bool ccw = reversed ? !entity.arc->ccw : entity.arc->ccw;
      const double start_x = reversed ? entity.arc->end_x : entity.arc->start_x;
      const double start_y = reversed ? entity.arc->end_y : entity.arc->start_y;
      const double end_x = reversed ? entity.arc->start_x : entity.arc->end_x;
      const double end_y = reversed ? entity.arc->start_y : entity.arc->end_y;
      const double start_angle =
          std::atan2(start_y - entity.arc->center_y,
                     start_x - entity.arc->center_x);
      const double end_angle =
          std::atan2(end_y - entity.arc->center_y,
                     end_x - entity.arc->center_x);
      const double mid_angle =
          start_angle +
          normalize_sweep_arc_angle(start_angle, end_angle, ccw) * 0.5;
      const double mid_x =
          entity.arc->center_x + entity.arc->radius * std::cos(mid_angle);
      const double mid_y =
          entity.arc->center_y + entity.arc->radius * std::sin(mid_angle);
      const auto start = sketch_local_to_world(path_frame, start_x, start_y);
      const auto end = sketch_local_to_world(path_frame, end_x, end_y);
      const auto center = sketch_local_to_world(
          path_frame, entity.arc->center_x, entity.arc->center_y);
      const auto mid = sketch_local_to_world(path_frame, mid_x, mid_y);
      segments.push_back(SweepFeatureParameters::PathSegment{
          .entity_id = entity.id,
          .kind = "arc",
          .start_x = start[0],
          .start_y = start[1],
          .start_z = start[2],
          .end_x = end[0],
          .end_y = end[1],
          .end_z = end[2],
          .center_x = center[0],
          .center_y = center[1],
          .center_z = center[2],
          .mid_x = mid[0],
          .mid_y = mid[1],
          .mid_z = mid[2],
          .radius = entity.arc->radius,
          .ccw = ccw,
      });
    }
  }
  return segments;
}

// Find a sketch line by id and patch its endpoint coords. The mutator
// updates the cached (start_x, start_y) / (end_x, end_y); the points
// table picks up the new coords via the next
// `refresh_sketch_derived_state` pass.
bool patch_sketch_line(SketchFeatureParameters& parameters,
                       const std::string& line_id,
                       double start_x, double start_y,
                       double end_x, double end_y) {
  for (auto& line : parameters.lines) {
    if (line.id == line_id) {
      line.start_x = start_x;
      line.start_y = start_y;
      line.end_x = end_x;
      line.end_y = end_y;
      return true;
    }
  }
  return false;
}

bool patch_sketch_circle(SketchFeatureParameters& parameters,
                         const std::string& circle_id,
                         double center_x, double center_y, double radius) {
  for (auto& circle : parameters.circles) {
    if (circle.id == circle_id) {
      circle.center_x = center_x;
      circle.center_y = center_y;
      circle.radius = radius;
      return true;
    }
  }
  return false;
}

bool patch_sketch_arc(SketchFeatureParameters& parameters,
                      const std::string& arc_id,
                      double start_x, double start_y,
                      double end_x, double end_y,
                      double center_x, double center_y,
                      double radius, bool ccw) {
  for (auto& arc : parameters.arcs) {
    if (arc.id == arc_id) {
      arc.start_x = start_x;
      arc.start_y = start_y;
      arc.end_x = end_x;
      arc.end_y = end_y;
      arc.center_x = center_x;
      arc.center_y = center_y;
      arc.radius = radius;
      arc.ccw = ccw;
      return true;
    }
  }
  return false;
}

bool patch_projected_point(SketchFeatureParameters& parameters,
                           const std::string& point_id,
                           double x, double y) {
  for (auto& projected : parameters.projected_points) {
    if (projected.id == point_id) {
      projected.x = x;
      projected.y = y;
      return true;
    }
  }
  return false;
}

// Re-derive every projection on `feature` against `prefix` (the
// document state with this sketch's *upstream* features only) and
// patch the matching generated entities in place. The `prefix` is
// already body-compiled to the latest geometry by the surrounding
// `refresh_history_dependencies` walker, so the sources we resolve
// reflect every recent edit. Returns true iff at least one
// projection's source could not be resolved (caller surfaces this
// as a feature-level dependency_broken).
bool refresh_sketch_projections(const DocumentState& prefix,
                                FeatureEntry& feature) {
  if (!feature.sketch_parameters.has_value() ||
      !feature.sketch_parameters->plane_frame.has_value()) {
    return false;
  }
  auto& parameters = feature.sketch_parameters.value();
  const auto& frame = parameters.plane_frame.value();

  bool any_broken = false;
  for (auto& projection : parameters.projections) {
    projection.dependency_broken = false;
    projection.dependency_warning.clear();

    if (projection.source_kind == "vertex") {
      const auto position =
          compute_vertex_position(prefix, projection.source_id);
      if (!position.has_value()) {
        projection.dependency_broken = true;
        projection.dependency_warning =
            "Projected vertex source is no longer available.";
        any_broken = true;
        continue;
      }
      const auto local = world_to_sketch_local(
          frame, position->x, position->y, position->z);
      if (!projection.generated_point_id.empty()) {
        patch_projected_point(parameters, projection.generated_point_id,
                              local.first, local.second);
      }
    } else if (projection.source_kind == "edge") {
      const auto edge =
          compute_edge_geometry(prefix, projection.source_id);
      if (!edge.has_value() || edge->kind == "unsupported") {
        projection.dependency_broken = true;
        projection.dependency_warning =
            "Projected edge source is no longer available or its "
            "curve type is no longer supported.";
        any_broken = true;
        continue;
      }
      if (edge->kind == "line") {
        if (projection.generated_line_ids.size() != 1) {
          // Source-shape mismatch: the original projection was a
          // single straight line but now we'd be patching the wrong
          // entity count. Flag broken and leave the cached coords
          // alone so nothing visually jumps until the user re-projects.
          projection.dependency_broken = true;
          projection.dependency_warning =
              "Projected source changed shape (line). Re-project to "
              "update.";
          any_broken = true;
          continue;
        }
        const auto a = world_to_sketch_local(
            frame, edge->start.x, edge->start.y, edge->start.z);
        const auto b = world_to_sketch_local(
            frame, edge->end.x, edge->end.y, edge->end.z);
        patch_sketch_line(parameters, projection.generated_line_ids.front(),
                          a.first, a.second, b.first, b.second);
      } else if (edge->kind == "circle") {
        if (projection.generated_circle_ids.size() != 1) {
          projection.dependency_broken = true;
          projection.dependency_warning =
              "Projected source changed shape (circle). Re-project to "
              "update.";
          any_broken = true;
          continue;
        }
        const auto center = world_to_sketch_local(
            frame, edge->center.x, edge->center.y, edge->center.z);
        patch_sketch_circle(parameters,
                            projection.generated_circle_ids.front(),
                            center.first, center.second, edge->radius);
      } else if (edge->kind == "arc") {
        if (projection.generated_arc_ids.size() != 1) {
          projection.dependency_broken = true;
          projection.dependency_warning =
              "Projected source changed shape (arc). Re-project to "
              "update.";
          any_broken = true;
          continue;
        }
        const auto start = world_to_sketch_local(
            frame, edge->start.x, edge->start.y, edge->start.z);
        const auto end = world_to_sketch_local(
            frame, edge->end.x, edge->end.y, edge->end.z);
        const auto center = world_to_sketch_local(
            frame, edge->center.x, edge->center.y, edge->center.z);
        const double dot = edge->axis.x * frame.normal_x +
                           edge->axis.y * frame.normal_y +
                           edge->axis.z * frame.normal_z;
        const bool ccw = dot > 0.0;
        patch_sketch_arc(parameters,
                         projection.generated_arc_ids.front(),
                         start.first, start.second,
                         end.first, end.second,
                         center.first, center.second,
                         edge->radius, ccw);
      }
    } else if (projection.source_kind == "face") {
      const auto outline =
          compute_face_outline(prefix, projection.source_id);
      if (!outline.has_value()) {
        projection.dependency_broken = true;
        projection.dependency_warning =
            "Projected face source is no longer available.";
        any_broken = true;
        continue;
      }
      // Face outlines map onto a fixed number of generated entities
      // recorded at projection time. If the upstream face's vertex
      // count or shape category changed (e.g. a polygon gained a
      // vertex after a chamfer), the existing generated_line /
      // generated_circle ids no longer cover the new outline. v1
      // flags this as broken; the user re-projects to re-emit the
      // updated geometry.
      if (outline->kind == "rectangle") {
        if (projection.generated_line_ids.size() != 4) {
          projection.dependency_broken = true;
          projection.dependency_warning =
              "Projected face changed shape. Re-project to update.";
          any_broken = true;
          continue;
        }
        std::array<std::pair<double, double>, 4> local{};
        for (size_t k = 0; k < 4; ++k) {
          const auto& corner = outline->rectangle_corners[k];
          local[k] = world_to_sketch_local(frame, corner.x, corner.y, corner.z);
        }
        for (size_t k = 0; k < 4; ++k) {
          const auto& a = local[k];
          const auto& b = local[(k + 1) % 4];
          patch_sketch_line(parameters, projection.generated_line_ids[k],
                            a.first, a.second, b.first, b.second);
        }
      } else if (outline->kind == "polygon") {
        size_t expected_line_count = outline->polygon_corners.size();
        for (const auto& inner_loop : outline->inner_loops) {
          expected_line_count += inner_loop.size();
        }
        if (projection.generated_line_ids.size() != expected_line_count) {
          projection.dependency_broken = true;
          projection.dependency_warning =
              "Projected face vertex count changed. Re-project to update.";
          any_broken = true;
          continue;
        }
        size_t line_id_index = 0;
        auto patch_loop =
            [&](const std::vector<FaceOutlinePoint>& loop) {
              std::vector<std::pair<double, double>> local;
              local.reserve(loop.size());
              for (const auto& corner : loop) {
                local.push_back(world_to_sketch_local(
                    frame, corner.x, corner.y, corner.z));
              }
              for (size_t k = 0; k < local.size(); ++k) {
                const auto& a = local[k];
                const auto& b = local[(k + 1) % local.size()];
                patch_sketch_line(parameters,
                                  projection.generated_line_ids[line_id_index++],
                                  a.first, a.second, b.first, b.second);
              }
            };
        patch_loop(outline->polygon_corners);
        for (const auto& inner_loop : outline->inner_loops) {
          patch_loop(inner_loop);
        }
      } else if (outline->kind == "circle") {
        const size_t expected_circle_count = 1 + outline->inner_circles.size();
        if (projection.generated_circle_ids.size() != expected_circle_count) {
          projection.dependency_broken = true;
          projection.dependency_warning =
              "Projected face changed shape. Re-project to update.";
          any_broken = true;
          continue;
        }
        auto patch_projected_circle =
            [&](size_t index, const FaceOutlinePoint& center, double radius) {
          const auto local = world_to_sketch_local(
              frame, center.x, center.y, center.z);
          patch_sketch_circle(parameters, projection.generated_circle_ids[index],
                              local.first, local.second, radius);
        };
        patch_projected_circle(0, outline->circle_center,
                               outline->circle_radius);
        for (size_t index = 0; index < outline->inner_circles.size(); ++index) {
          const auto& inner_circle = outline->inner_circles[index];
          patch_projected_circle(index + 1, inner_circle.center,
                                 inner_circle.radius);
        }
      }
    }
  }
  return any_broken;
}
