// Diagnostic for D-008: report what the solver actually did to every
// `point_on_circle` constraint, in solver space vs the `params` record.
//
// Included at namespace scope in constraint_solver.cpp (like the lifecycle .inc),
// so it defines `ConstraintSolver::point_on_circle_trace`. It is NOT env-gated
// here: the app call site gates logging behind PS_TRACE_POINT_ON_CIRCLE, and the
// unit test calls the method directly. The method only reads; it never mutates.
//
// The probes answer the two open D-008 questions without guessing:
//   1. Is the vertex in planegcs' plist?  -> `declared_unknown_ids_` (what WE
//      pushed to `unknown_params`; planegcs' plist is private).
//   2. Did the solver satisfy dist(point, centre) == radius, and did that survive
//      into `params`? -> `solver_xy` / `solver_residual` vs `params_xy` /
//      `params_residual`, plus `rad_solver` vs `rad_slot` (the off-by-2 suspicion
//      in the circle radius pointer).
std::string ConstraintSolver::point_on_circle_trace(
    const SketchFeatureParameters& params, const char* stage) const {
  std::ostringstream out;

  bool any = false;
  for (const auto& constraint : params.constraints) {
    if (constraint.kind != "point_on_circle" ||
        constraint.target_ids.size() < 2) {
      continue;
    }
    any = true;
    const std::string& vertex_id = constraint.target_ids[0];
    const std::string& circle_id = constraint.target_ids[1];
    const auto pit = point_id_to_index_.find(vertex_id);
    const auto cit = circle_id_to_index_.find(circle_id);

    out << stage << " " << vertex_id << "/" << circle_id;

    if (pit == point_id_to_index_.end()) {
      out << " vertex=UNMAPPED";
    } else {
      const size_t idx = pit->second;
      out << " declared_unknown="
          << (declared_unknown_ids_.count(vertex_id) ? "yes" : "no");
      out << " solver_xy=(" << param_storage_[idx * 2] << ","
          << param_storage_[idx * 2 + 1] << ")";
      if (idx < params.vertices.size()) {
        out << " params_xy=(" << params.vertices[idx].x << ","
            << params.vertices[idx].y << ")";
      }
    }

    if (cit == circle_id_to_index_.end()) {
      out << " circle=UNMAPPED";
    } else {
      const size_t ci = cit->second;
      const auto& gc = gcs_circles_[ci];
      out << " center_solver=(" << *gc.center.x << "," << *gc.center.y << ")";
      out << " rad_solver=" << *gc.rad;
      const size_t radius_base = params.vertices.size() * 2;
      out << " rad_slot=" << param_storage_[radius_base + ci];
    }

    // Residuals against the params record (the circle as apply() left it).
    const SketchCircle* circle = nullptr;
    for (const auto& candidate : params.circles) {
      if (candidate.id == circle_id) {
        circle = &candidate;
        break;
      }
    }
    if (circle != nullptr && pit != point_id_to_index_.end() &&
        cit != circle_id_to_index_.end()) {
      const size_t idx = pit->second;
      const size_t ci = cit->second;
      const double sx = param_storage_[idx * 2];
      const double sy = param_storage_[idx * 2 + 1];
      const double cx = *gcs_circles_[ci].center.x;
      const double cy = *gcs_circles_[ci].center.y;
      const double solver_residual =
          std::abs(std::hypot(sx - cx, sy - cy) - *gcs_circles_[ci].rad);
      out << " solver_residual=" << solver_residual;
      const double params_residual =
          std::abs(std::hypot(params.vertices[idx].x - circle->center_x,
                              params.vertices[idx].y - circle->center_y) -
                   circle->radius);
      out << " params_residual=" << params_residual;
    }

    out << "\n";
  }

  if (!any) return std::string();
  return out.str();
}
