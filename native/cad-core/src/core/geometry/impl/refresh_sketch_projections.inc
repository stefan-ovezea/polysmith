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
      if (!projection.generated_vertex_id.empty()) {
        patch_projected_point(parameters, projection.generated_vertex_id,
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
