// Restore solver-modified geometry that no constraint can legitimately reach.
//
// planegcs' DogLeg does not converge to a minimum-change solution on an
// underdetermined system: a driven dimension on ONE entity also drags
// UNCONSTRAINED geometry around, because those parameters are in the system with
// nothing pulling on them. Measured on a sketch with a rectangle and two loose
// lines, driving the rectangle's width from 40 to 60:
//
//   vertex-5 moved (0, +30)      a free line collapsed onto its other end
//   vertex-6 moved (+20, -20)
//   vertex-7 moved (+30, +25)
//   vertex-8 moved (-110, -35)   a free line jumped to the origin
//
// That is the "I changed one dimension and the solver destroyed my sketch"
// report, and no amount of ad-hoc enforcement can undo it.
//
// The rule here is geometric, not heuristic: a vertex may move only when it is
// CONNECTED — through shared vertices — to an entity that carries a
// solver-registered constraint (an axis constraint, a relation, a driving
// dimension, a constraint record, an anchor, or a fixed point). Everything
// outside that closure has no say in the solve, so its post-solve value is noise
// and is restored from the pre-solve snapshot. Shared-vertex connectivity makes
// the closure sound: a loose line welded to a driven line is reachable and keeps
// following it.

struct SolverVertexSnapshot {
  double x;
  double y;
};

struct SolverLineSnapshot {
  double start_x;
  double start_y;
  double end_x;
  double end_y;
};

struct SolverCircleSnapshot {
  double center_x;
  double center_y;
  double radius;
};

struct SolverArcSnapshot {
  double center_x;
  double center_y;
  double radius;
  double start_x;
  double start_y;
  double end_x;
  double end_y;
};

// Everything the solver may move, captured before the solve so the unreachable
// parts can be put back. Sized to the parameter arrays by index.
struct SolverGeometrySnapshot {
  std::vector<SolverLineSnapshot> lines;
  std::vector<SolverCircleSnapshot> circles;
  std::vector<SolverArcSnapshot> arcs;
  std::vector<std::optional<SolverVertexSnapshot>> vertices;
  std::unordered_set<std::string> reachable_vertices;
};

bool line_has_driving_dimension(const SketchFeatureParameters& parameters,
                                const std::string& line_id) {
  for (const auto& dimension : parameters.dimensions) {
    if (dimension.driven) continue;
    // Auto dimensions (display-only) are not fed to the solver.
    if (dimension.is_auto && dimension.expression.empty()) continue;
    if (dimension.entity_id == line_id ||
        dimension.secondary_entity_id == line_id) {
      return true;
    }
  }
  return false;
}

bool circle_has_driving_dimension(const SketchFeatureParameters& parameters,
                                  const std::string& entity_id) {
  for (const auto& dimension : parameters.dimensions) {
    if (dimension.driven) continue;
    if (dimension.is_auto && dimension.expression.empty()) continue;
    if (dimension.entity_id == entity_id) return true;
  }
  return false;
}

bool entity_is_solver_seed(const SketchFeatureParameters& parameters,
                           const std::string& entity_id) {
  if (line_has_driving_dimension(parameters, entity_id) ||
      circle_has_driving_dimension(parameters, entity_id)) {
    return true;
  }
  for (const auto& constraint : parameters.constraints) {
    for (const auto& target : constraint.target_ids) {
      if (target == entity_id) return true;
    }
  }
  return false;
}

// Build the snapshot and the reachability closure in one pass.
SolverGeometrySnapshot capture_solver_geometry(
    const SketchFeatureParameters& parameters) {
  SolverGeometrySnapshot snapshot;
  snapshot.lines.reserve(parameters.lines.size());
  for (const auto& line : parameters.lines) {
    snapshot.lines.push_back(SolverLineSnapshot{
        line.start_x, line.start_y, line.end_x, line.end_y});
  }
  snapshot.circles.reserve(parameters.circles.size());
  for (const auto& circle : parameters.circles) {
    snapshot.circles.push_back(SolverCircleSnapshot{
        circle.center_x, circle.center_y, circle.radius});
  }
  snapshot.arcs.reserve(parameters.arcs.size());
  for (const auto& arc : parameters.arcs) {
    snapshot.arcs.push_back(SolverArcSnapshot{
        arc.center_x, arc.center_y, arc.radius,
        arc.start_x, arc.start_y, arc.end_x, arc.end_y});
  }
  snapshot.vertices.resize(parameters.vertices.size());
  for (size_t i = 0; i < parameters.vertices.size(); ++i) {
    const auto& vertex = parameters.vertices[i];
    snapshot.vertices[i] = SolverVertexSnapshot{vertex.x, vertex.y};
    // A fixed point is a solver constraint in its own right: it must stay put,
    // and whatever shares it must be allowed to follow.
    if (vertex.is_fixed) snapshot.reachable_vertices.insert(vertex.id);
  }
  for (const auto& anchor : parameters.midpoint_anchors) {
    snapshot.reachable_vertices.insert(anchor.vertex_id);
  }
  for (const auto& anchor : parameters.point_line_anchors) {
    snapshot.reachable_vertices.insert(anchor.vertex_id);
  }
  // A constraint record may name a VERTEX directly - `point_on_circle` is
  // {vertex, curve}. That vertex is part of the constrained component by definition, but
  // it was not seeded here, so the reachability guard put it back after every solve and
  // the constraint could never be satisfied: measured, "solver moved unconstrained
  // geometry by 16.47 mm; restored it" immediately followed by "solver solution rejected
  // (point_on_circle vertex-2/circle-1 off by 48.99 mm)". Seeding every target id is
  // enough; ids that are not vertices simply never match one.
  for (const auto& constraint : parameters.constraints) {
    for (const auto& target : constraint.target_ids) {
      snapshot.reachable_vertices.insert(target);
    }
  }

  // Seed every line that carries a constraint of its own.
  for (const auto& line : parameters.lines) {
    const bool constrained_axis = line.constraint.has_value();
    const bool in_relation = std::any_of(
        parameters.line_relations.begin(), parameters.line_relations.end(),
        [&](const SketchLineRelation& relation) {
          return relation.first_line_id == line.id ||
                 relation.second_line_id == line.id;
        });
    if (!constrained_axis && !in_relation &&
        !entity_is_solver_seed(parameters, line.id)) {
      continue;
    }
    snapshot.reachable_vertices.insert(line.start_vertex_id);
    snapshot.reachable_vertices.insert(line.end_vertex_id);
  }
  for (const auto& circle : parameters.circles) {
    if (!entity_is_solver_seed(parameters, circle.id)) continue;
    snapshot.reachable_vertices.insert(
        circle_center_vertex_id(circle));
  }
  for (const auto& arc : parameters.arcs) {
    if (!entity_is_solver_seed(parameters, arc.id)) continue;
    snapshot.reachable_vertices.insert(arc.center_vertex_id);
    snapshot.reachable_vertices.insert(arc.start_vertex_id);
    snapshot.reachable_vertices.insert(arc.end_vertex_id);
  }

  // Expand through shared vertices until nothing new is reachable, so a loose
  // line welded to a driven one keeps following it.
  bool changed = true;
  while (changed) {
    changed = false;
    const auto absorb = [&](const std::string& id) {
      if (id.empty()) return;
      if (snapshot.reachable_vertices.insert(id).second) changed = true;
    };
    for (const auto& line : parameters.lines) {
      const bool start_reachable =
          snapshot.reachable_vertices.count(line.start_vertex_id) > 0;
      const bool end_reachable =
          snapshot.reachable_vertices.count(line.end_vertex_id) > 0;
      if (!start_reachable && !end_reachable) continue;
      absorb(line.start_vertex_id);
      absorb(line.end_vertex_id);
    }
    for (const auto& arc : parameters.arcs) {
      const bool reachable =
          snapshot.reachable_vertices.count(arc.start_vertex_id) > 0 ||
          snapshot.reachable_vertices.count(arc.end_vertex_id) > 0 ||
          snapshot.reachable_vertices.count(arc.center_vertex_id) > 0;
      if (!reachable) continue;
      absorb(arc.center_vertex_id);
      absorb(arc.start_vertex_id);
      absorb(arc.end_vertex_id);
    }
    for (const auto& circle : parameters.circles) {
      if (snapshot.reachable_vertices.count(circle_center_vertex_id(circle)) >
          0) {
        continue;
      }
      // A circle centre that coincides with a reachable vertex is reachable too
      // (the centre vertex id is its own, so match by position).
      for (const auto& vertex : parameters.vertices) {
        if (snapshot.reachable_vertices.count(vertex.id) == 0) continue;
        if (!points_match(vertex.x, vertex.y, circle.center_x, circle.center_y)) {
          continue;
        }
        absorb(circle_center_vertex_id(circle));
        break;
      }
    }
  }
  return snapshot;
}

// Put back every entity the solver could not legitimately have moved, and report
// the largest such displacement so the caller can say so in the log.
double restore_unreachable_solver_geometry(
    SketchFeatureParameters& parameters,
    const SolverGeometrySnapshot& snapshot) {
  double largest_restored = 0.0;
  const auto unreachable = [&](const std::string& vertex_id) {
    return !vertex_id.empty() &&
           snapshot.reachable_vertices.count(vertex_id) == 0;
  };

  for (size_t i = 0; i < parameters.lines.size() && i < snapshot.lines.size();
       ++i) {
    auto& line = parameters.lines[i];
    if (!unreachable(line.start_vertex_id) && !unreachable(line.end_vertex_id)) {
      continue;
    }
    // Only fully unreachable lines are restored: a line sharing one vertex with
    // the solved component must keep that vertex's solved position.
    if (!(unreachable(line.start_vertex_id) &&
          unreachable(line.end_vertex_id))) {
      continue;
    }
    const auto& saved = snapshot.lines[i];
    largest_restored = std::max(
        {largest_restored,
         std::abs(line.start_x - saved.start_x),
         std::abs(line.start_y - saved.start_y),
         std::abs(line.end_x - saved.end_x),
         std::abs(line.end_y - saved.end_y)});
    line.start_x = saved.start_x;
    line.start_y = saved.start_y;
    line.end_x = saved.end_x;
    line.end_y = saved.end_y;
  }

  for (size_t i = 0; i < parameters.circles.size() &&
                     i < snapshot.circles.size();
       ++i) {
    auto& circle = parameters.circles[i];
    if (!unreachable(circle_center_vertex_id(circle))) continue;
    const auto& saved = snapshot.circles[i];
    largest_restored = std::max(
        {largest_restored,
         std::abs(circle.center_x - saved.center_x),
         std::abs(circle.center_y - saved.center_y),
         std::abs(circle.radius - saved.radius)});
    circle.center_x = saved.center_x;
    circle.center_y = saved.center_y;
    circle.radius = saved.radius;
  }

  for (size_t i = 0; i < parameters.arcs.size() && i < snapshot.arcs.size();
       ++i) {
    auto& arc = parameters.arcs[i];
    if (!(unreachable(arc.start_vertex_id) && unreachable(arc.end_vertex_id) &&
          unreachable(arc.center_vertex_id))) {
      continue;
    }
    const auto& saved = snapshot.arcs[i];
    largest_restored = std::max(
        {largest_restored,
         std::abs(arc.center_x - saved.center_x),
         std::abs(arc.center_y - saved.center_y),
         std::abs(arc.radius - saved.radius),
         std::abs(arc.start_x - saved.start_x),
         std::abs(arc.start_y - saved.start_y),
         std::abs(arc.end_x - saved.end_x),
         std::abs(arc.end_y - saved.end_y)});
    arc.center_x = saved.center_x;
    arc.center_y = saved.center_y;
    arc.radius = saved.radius;
    arc.start_x = saved.start_x;
    arc.start_y = saved.start_y;
    arc.end_x = saved.end_x;
    arc.end_y = saved.end_y;
  }

  for (size_t i = 0; i < parameters.vertices.size() &&
                     i < snapshot.vertices.size();
       ++i) {
    auto& vertex = parameters.vertices[i];
    if (!unreachable(vertex.id) || !snapshot.vertices[i].has_value()) continue;
    const auto& saved = snapshot.vertices[i].value();
    largest_restored = std::max(
        {largest_restored,
         std::abs(vertex.x - saved.x),
         std::abs(vertex.y - saved.y)});
    vertex.x = saved.x;
    vertex.y = saved.y;
  }

  return largest_restored;
}

// ── Solution verification ────────────────────────────────────────────
//
// planegcs is not used here as a black box any more, because it is not one:
// `System::initSolution()` runs `diagnose()` before solving, and diagnose not
// only analyses redundancy but REMOVES the redundant constraints from the
// system it then solves (GCS.cpp builds `clistR` without them). Whatever the
// solver converges to therefore satisfies a SUBSET of what the sketch asks for —
// the dropped constraints are free to drift, which is exactly what "the solver
// randomly deformed my sketch" looks like. On a rectangle with dimensions,
// relations and automatic dimensions there is plenty to drop.
//
// So every solution is verified against the constraints we can evaluate. A
// solution that violates one is thrown away and the pre-solve geometry is kept:
// the sketch does not move rather than moving wrongly, and the log names the
// constraint that could not be met.

// The verification itself lives in a header so it can be unit-tested without a
// solver or a document: core/sketch/solver_solution_check.h. What stays here is
// the part that needs the pre-solve snapshot - detecting degenerate geometry.
// Put every entity back the way it was before the solve.
void restore_all_solver_geometry(SketchFeatureParameters& parameters,
                                 const SolverGeometrySnapshot& snapshot) {
  for (size_t i = 0; i < parameters.lines.size() && i < snapshot.lines.size();
       ++i) {
    parameters.lines[i].start_x = snapshot.lines[i].start_x;
    parameters.lines[i].start_y = snapshot.lines[i].start_y;
    parameters.lines[i].end_x = snapshot.lines[i].end_x;
    parameters.lines[i].end_y = snapshot.lines[i].end_y;
  }
  for (size_t i = 0; i < parameters.circles.size() &&
                     i < snapshot.circles.size();
       ++i) {
    parameters.circles[i].center_x = snapshot.circles[i].center_x;
    parameters.circles[i].center_y = snapshot.circles[i].center_y;
    parameters.circles[i].radius = snapshot.circles[i].radius;
  }
  for (size_t i = 0; i < parameters.arcs.size() && i < snapshot.arcs.size();
       ++i) {
    parameters.arcs[i].center_x = snapshot.arcs[i].center_x;
    parameters.arcs[i].center_y = snapshot.arcs[i].center_y;
    parameters.arcs[i].radius = snapshot.arcs[i].radius;
    parameters.arcs[i].start_x = snapshot.arcs[i].start_x;
    parameters.arcs[i].start_y = snapshot.arcs[i].start_y;
    parameters.arcs[i].end_x = snapshot.arcs[i].end_x;
    parameters.arcs[i].end_y = snapshot.arcs[i].end_y;
  }
  for (size_t i = 0; i < parameters.vertices.size() &&
                     i < snapshot.vertices.size();
       ++i) {
    if (!snapshot.vertices[i].has_value()) continue;
    parameters.vertices[i].x = snapshot.vertices[i].value().x;
    parameters.vertices[i].y = snapshot.vertices[i].value().y;
  }
}

// True when a solve has destroyed geometry instead of solving it.
//
// A zero-length line is trivially parallel/perpendicular to everything, so a
// degenerate solution satisfies the relation constraints while destroying the
// sketch. That is exactly what was reported: "collapsed line line-2 removed 0
// dimension(s), 0 constraint(s) and 2 relation(s)" — a rectangle side shrank to
// nothing, the cleanup deleted it together with its relations, and the next
// command failed with "Sketch dimension not found". The verifier above could not
// see it, because a collapsed line has no length constraint to violate and the
// relations are technically met.
//
// Empty string = the geometry survived. Otherwise names the first casualty.
std::string solver_solution_collapses_geometry(
    const SketchFeatureParameters& parameters,
    const SolverGeometrySnapshot& snapshot) {
  for (size_t i = 0; i < parameters.lines.size() && i < snapshot.lines.size();
       ++i) {
    const auto& saved = snapshot.lines[i];
    const double before =
        std::hypot(saved.end_x - saved.start_x, saved.end_y - saved.start_y);
    if (before <= kMinimumSketchDimensionValue) continue;
    const auto& line = parameters.lines[i];
    const double after =
        std::hypot(line.end_x - line.start_x, line.end_y - line.start_y);
    if (after <= kMinimumSketchDimensionValue) {
      return "line " + line.id + " collapsed from " + std::to_string(before) +
             " mm to " + std::to_string(after) + " mm";
    }
  }
  for (size_t i = 0; i < parameters.circles.size() &&
                     i < snapshot.circles.size();
       ++i) {
    const auto& saved = snapshot.circles[i];
    if (saved.radius <= kMinimumSketchDimensionValue) continue;
    const auto& circle = parameters.circles[i];
    if (circle.radius <= kMinimumSketchDimensionValue) {
      return "circle " + circle.id + " collapsed from " +
             std::to_string(saved.radius) + " mm to " +
             std::to_string(circle.radius) + " mm";
    }
  }
  for (size_t i = 0; i < parameters.arcs.size() && i < snapshot.arcs.size();
       ++i) {
    const auto& saved = snapshot.arcs[i];
    if (saved.radius <= kMinimumSketchDimensionValue) continue;
    const auto& arc = parameters.arcs[i];
    if (arc.radius <= kMinimumSketchDimensionValue) {
      return "arc " + arc.id + " collapsed from " + std::to_string(saved.radius) +
             " mm to " + std::to_string(arc.radius) + " mm";
    }
  }
  return std::string();
}
