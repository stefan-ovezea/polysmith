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
  if (!feature.sketch_parameters.has_value()) {
    return false;
  }
  auto& parameters = feature.sketch_parameters.value();
  // Body projections are allowed on origin ref-plane sketches (whose
  // plane_frame is nullopt — the legacy axis mapping applies), so the
  // frame is materialized once from the stored frame or the origin
  // fallback and shared by every branch below.
  const bool has_body_projection = std::any_of(
      parameters.projections.begin(), parameters.projections.end(),
      [](const SketchProjection& projection) {
        return projection.source_kind == "body";
      });
  if (!parameters.plane_frame.has_value() && !has_body_projection) {
    return false;
  }
  const auto resolved_frame = resolve_sketch_projection_frame(parameters);
  if (!resolved_frame.has_value()) {
    return false;
  }
  SketchFeatureParameters::SketchPlaneFrame materialized_frame{
      .origin_x = resolved_frame->origin_x,
      .origin_y = resolved_frame->origin_y,
      .origin_z = resolved_frame->origin_z,
      .x_axis_x = resolved_frame->x_axis_x,
      .x_axis_y = resolved_frame->x_axis_y,
      .x_axis_z = resolved_frame->x_axis_z,
      .y_axis_x = resolved_frame->y_axis_x,
      .y_axis_y = resolved_frame->y_axis_y,
      .y_axis_z = resolved_frame->y_axis_z,
      .normal_x = resolved_frame->normal_x,
      .normal_y = resolved_frame->normal_y,
      .normal_z = resolved_frame->normal_z,
  };
  const auto& frame = materialized_frame;

  bool any_broken = false;
  for (auto& projection : parameters.projections) {
    projection.dependency_broken = false;
    projection.dependency_warning.clear();

    if (projection.source_kind == "body") {
      // source_id format: "body:<body_id>:<mode>".
      const std::string marker = "body:";
      const size_t mode_pos = projection.source_id.rfind(':');
      if (projection.source_id.rfind(marker, 0) != 0 ||
          mode_pos == std::string::npos || mode_pos <= marker.size()) {
        projection.dependency_broken = true;
        projection.dependency_warning =
            "Projected body source id is malformed.";
        any_broken = true;
        continue;
      }
      const std::string body_id =
          projection.source_id.substr(marker.size(), mode_pos - marker.size());
      const std::string mode = projection.source_id.substr(mode_pos + 1);

      const FeatureEntry* source = nullptr;
      for (const auto& candidate : prefix.feature_history) {
        if (candidate.id == body_id) {
          source = &candidate;
          break;
        }
      }
      const bool source_is_import =
          source != nullptr && source->kind == "mesh_import" &&
          source->mesh_import_parameters.has_value();
      const bool source_is_converted =
          source != nullptr && source->kind == "mesh_to_body";
      if (!source_is_import && !source_is_converted) {
        projection.dependency_broken = true;
        projection.dependency_warning =
            "Projected mesh body is no longer available.";
        any_broken = true;
        continue;
      }
      // Imported meshes re-read the STL; converted meshes resolve via
      // the compiled prefix (creation-time snapshot).
      TopoDS_Shape shape;
      if (source_is_import) {
        shape = build_feature_shape(*source);
      } else {
        const CompiledBodies compiled =
            compile_bodies(prefix, /*include_meshes=*/false);
        const auto compiled_it = std::find_if(
            compiled.bodies.begin(), compiled.bodies.end(),
            [&](const CompiledBody& body) { return body.id == body_id; });
        if (compiled_it != compiled.bodies.end()) {
          shape = compiled_it->shape;
        }
      }
      if (shape.IsNull()) {
        projection.dependency_broken = true;
        projection.dependency_warning =
            "Projected mesh body source is no longer available.";
        any_broken = true;
        continue;
      }
      const auto polylines =
          mode == "section"
              ? compute_mesh_section_polylines(shape, resolved_frame.value())
              : compute_mesh_silhouette_polylines(shape,
                                                  resolved_frame.value());
      // A polyline of N points emits N-1 lines (closed loops keep the
      // duplicated closing point) — same emission rule as
      // project_body_into_sketch.
      size_t expected_line_count = 0;
      for (const auto& polyline : polylines) {
        if (polyline.size() >= 2) {
          expected_line_count += polyline.size() - 1;
        }
      }
      if (projection.generated_line_ids.size() != expected_line_count) {
        projection.dependency_broken = true;
        projection.dependency_warning =
            "Projected mesh body changed shape. Re-project to update.";
        any_broken = true;
        continue;
      }
      size_t line_index = 0;
      for (const auto& polyline : polylines) {
        for (size_t k = 0; k + 1 < polyline.size(); ++k) {
          const auto a = world_to_sketch_local(
              frame, polyline[k].X(), polyline[k].Y(), polyline[k].Z());
          const auto b = world_to_sketch_local(
              frame, polyline[k + 1].X(), polyline[k + 1].Y(),
              polyline[k + 1].Z());
          patch_sketch_line(parameters,
                            projection.generated_line_ids[line_index++],
                            a.first, a.second, b.first, b.second);
        }
      }
    } else if (projection.source_kind == "vertex") {
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
        // Expected entity counts mirror the emission order in
        // project_face_into_sketch: outer segments (lines/arcs mixed),
        // then inner-loop segments, then the circular through-holes.
        size_t expected_line_count = 0;
        size_t expected_arc_count = 0;
        const auto count_loop = [&](const auto& segment_arcs) {
          for (const auto& arc : segment_arcs) {
            if (arc.has_value()) {
              ++expected_arc_count;
            } else {
              ++expected_line_count;
            }
          }
        };
        count_loop(outline->polygon_segment_arcs);
        for (const auto& inner_arcs : outline->inner_segment_arcs) {
          count_loop(inner_arcs);
        }
        if (projection.generated_line_ids.size() != expected_line_count ||
            projection.generated_arc_ids.size() != expected_arc_count ||
            projection.generated_circle_ids.size() !=
                outline->inner_circles.size()) {
          projection.dependency_broken = true;
          projection.dependency_warning =
              "Projected face vertex count changed. Re-project to update.";
          any_broken = true;
          continue;
        }
        size_t line_id_index = 0;
        size_t arc_id_index = 0;
        auto patch_loop =
            [&](const std::vector<FaceOutlinePoint>& loop,
                const std::vector<std::optional<FaceOutlineArc>>& segment_arcs) {
              std::vector<std::pair<double, double>> local;
              local.reserve(loop.size());
              for (const auto& corner : loop) {
                local.push_back(world_to_sketch_local(
                    frame, corner.x, corner.y, corner.z));
              }
              for (size_t k = 0; k < local.size(); ++k) {
                const auto& a = local[k];
                const auto& b = local[(k + 1) % local.size()];
                const auto& arc = segment_arcs[k];
                if (arc.has_value() &&
                    circle_axis_parallel_to_sketch(frame, arc->axis)) {
                  const auto center = world_to_sketch_local(
                      frame, arc->center.x, arc->center.y, arc->center.z);
                  const auto mid = world_to_sketch_local(
                      frame, arc->mid.x, arc->mid.y, arc->mid.z);
                  patch_sketch_arc(
                      parameters,
                      projection.generated_arc_ids[arc_id_index++],
                      a.first, a.second, b.first, b.second,
                      center.first, center.second, arc->radius,
                      arc_ccw_from_local(a, b, mid));
                } else {
                  patch_sketch_line(
                      parameters,
                      projection.generated_line_ids[line_id_index++],
                      a.first, a.second, b.first, b.second);
                }
              }
            };
        patch_loop(outline->polygon_corners,
                   outline->polygon_segment_arcs);
        for (size_t loop_index = 0; loop_index < outline->inner_loops.size();
             ++loop_index) {
          patch_loop(outline->inner_loops[loop_index],
                     outline->inner_segment_arcs[loop_index]);
        }
        for (size_t index = 0; index < outline->inner_circles.size(); ++index) {
          const auto& inner_circle = outline->inner_circles[index];
          const auto center = world_to_sketch_local(
              frame, inner_circle.center.x, inner_circle.center.y,
              inner_circle.center.z);
          patch_sketch_circle(parameters,
                              projection.generated_circle_ids[index],
                              center.first, center.second,
                              inner_circle.radius);
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
