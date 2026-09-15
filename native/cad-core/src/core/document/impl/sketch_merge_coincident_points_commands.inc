// Merge coincident sketch points — heals split vertices left behind
// by legacy projections (created before the endpoint welding existed):
// two entities meet at a corner but hold two vertex ids a few microns
// apart.  The exact profile detector tolerates such splits (0.01 mm
// coordinate welding), but the fillet tool requires an exact shared
// vertex id — so these corners refused to fillet until this heal ran.
//
// The merge clusters every vertex referenced by the sketch by
// position (union-find over pairs within the coincident tolerance,
// 0.01 mm — the same tolerance the creation paths weld with), remaps
// ids onto the cluster representative (smallest id wins ties), and
// snaps entity cached coords to the representative's position.
// Fillet / chamfer corner + trim ids and coincident-constraint
// target ids are remapped as well so no record is left pointing at a
// dissolved vertex.  Single undoable action.

namespace {
constexpr double kMergeCoincidentTolerance = 0.01;  // mm
}  // namespace

DocumentState DocumentManager::merge_coincident_sketch_points(
    const std::string& feature_id) {
  require_document();

  auto feature_it = std::find_if(
      document_->feature_history.begin(),
      document_->feature_history.end(),
      [&](const FeatureEntry& feature) { return feature.id == feature_id; });
  if (feature_it == document_->feature_history.end() ||
      feature_it->kind != "sketch" ||
      !feature_it->sketch_parameters.has_value()) {
    throw std::runtime_error(
        "merge_coincident_sketch_points requires a sketch feature");
  }

  auto& parameters = *feature_it->sketch_parameters;

  // Gather every vertex id referenced by the sketch, with its cached
  // position.  Entities contribute their endpoint/center coords;
  // the vertex table fills in ids not covered there (ellipse axis
  // points, quadrant points, standalone projected points).
  struct VertexEntry {
    std::string id;
    double x;
    double y;
  };
  std::vector<VertexEntry> entries;
  const auto add_entry = [&](const std::string& id, double x, double y) {
    if (!id.empty()) entries.push_back(VertexEntry{id, x, y});
  };
  for (const auto& line : parameters.lines) {
    add_entry(line.start_vertex_id, line.start_x, line.start_y);
    add_entry(line.end_vertex_id, line.end_x, line.end_y);
  }
  for (const auto& arc : parameters.arcs) {
    add_entry(arc.start_vertex_id, arc.start_x, arc.start_y);
    add_entry(arc.end_vertex_id, arc.end_x, arc.end_y);
    add_entry(arc.center_vertex_id, arc.center_x, arc.center_y);
  }
  for (const auto& circle : parameters.circles) {
    add_entry(circle.center_vertex_id, circle.center_x, circle.center_y);
  }
  for (const auto& fillet : parameters.fillets) {
    add_entry(fillet.corner_vertex_id, fillet.corner_x, fillet.corner_y);
  }
  for (const auto& chamfer : parameters.chamfers) {
    add_entry(chamfer.corner_vertex_id, chamfer.corner_x, chamfer.corner_y);
  }
  for (const auto& vertex : parameters.vertices) {
    add_entry(vertex.id, vertex.x, vertex.y);
  }

  if (entries.empty()) {
    return document_.value();
  }

  // Union-find over pairs within the tolerance.  Representative =
  // smallest id in the cluster (deterministic, stable across runs).
  std::map<std::string, std::string> parent;
  const auto find_root = [&](const std::string& id) {
    std::string root = id;
    while (parent.count(root) && parent[root] != root) {
      root = parent[root];
    }
    return root;
  };
  for (const auto& entry : entries) {
    if (!parent.count(entry.id)) parent[entry.id] = entry.id;
  }
  for (size_t i = 0; i < entries.size(); ++i) {
    for (size_t j = i + 1; j < entries.size(); ++j) {
      if (entries[i].id == entries[j].id) continue;
      if (std::abs(entries[i].x - entries[j].x) >
              kMergeCoincidentTolerance ||
          std::abs(entries[i].y - entries[j].y) >
              kMergeCoincidentTolerance) {
        continue;
      }
      const std::string ri = find_root(entries[i].id);
      const std::string rj = find_root(entries[j].id);
      if (ri == rj) continue;
      const std::string rep = std::min(ri, rj);
      const std::string other = rep == ri ? rj : ri;
      parent[other] = rep;
      parent[rep] = rep;
    }
  }

  // Representative position per cluster = the position of the
  // smallest-id entry (the first time it appears in `entries`).
  std::map<std::string, std::pair<double, double>> pos_by_rep;
  for (const auto& entry : entries) {
    const std::string rep = find_root(entry.id);
    if (!pos_by_rep.count(rep)) {
      pos_by_rep[rep] = {entry.x, entry.y};
    }
  }

  push_undo_state();
  clear_redo_stack();

  const auto remap = [&](const std::string& id) {
    return parent.count(id) ? find_root(id) : id;
  };
  const auto rep_pos = [&](const std::string& id) {
    const std::string rep = remap(id);
    const auto it = pos_by_rep.find(rep);
    return it == pos_by_rep.end() ? std::pair{0.0, 0.0} : it->second;
  };

  for (auto& line : parameters.lines) {
    line.start_vertex_id = remap(line.start_vertex_id);
    line.end_vertex_id = remap(line.end_vertex_id);
    if (line.start_vertex_id == line.end_vertex_id) {
      // Degenerate after merge — collapse to a zero-length line so
      // the refresh's zero-length cleanup pass drops it.
      line.start_x = line.end_x;
      line.start_y = line.end_y;
    }
  }
  for (auto& arc : parameters.arcs) {
    arc.start_vertex_id = remap(arc.start_vertex_id);
    arc.end_vertex_id = remap(arc.end_vertex_id);
    arc.center_vertex_id = remap(arc.center_vertex_id);
    const auto sp = rep_pos(arc.start_vertex_id);
    const auto ep = rep_pos(arc.end_vertex_id);
    arc.start_x = sp.first;
    arc.start_y = sp.second;
    arc.end_x = ep.first;
    arc.end_y = ep.second;
  }
  for (auto& circle : parameters.circles) {
    circle.center_vertex_id = remap(circle.center_vertex_id);
  }
  for (auto& ellipse : parameters.ellipses) {
    ellipse.center_vertex_id = remap(ellipse.center_vertex_id);
    ellipse.axis_a_vertex_id = remap(ellipse.axis_a_vertex_id);
    ellipse.axis_b_vertex_id = remap(ellipse.axis_b_vertex_id);
    ellipse.start_vertex_id = remap(ellipse.start_vertex_id);
    ellipse.end_vertex_id = remap(ellipse.end_vertex_id);
  }
  for (auto& spline : parameters.splines) {
    for (auto& pole_id : spline.pole_vertex_ids) {
      pole_id = remap(pole_id);
    }
  }
  for (auto& fillet : parameters.fillets) {
    fillet.corner_vertex_id = remap(fillet.corner_vertex_id);
    fillet.trim_a_vertex_id = remap(fillet.trim_a_vertex_id);
    fillet.trim_b_vertex_id = remap(fillet.trim_b_vertex_id);
  }
  for (auto& chamfer : parameters.chamfers) {
    chamfer.corner_vertex_id = remap(chamfer.corner_vertex_id);
    chamfer.trim_a_vertex_id = remap(chamfer.trim_a_vertex_id);
    chamfer.trim_b_vertex_id = remap(chamfer.trim_b_vertex_id);
  }
  for (auto& anchor : parameters.midpoint_anchors) {
    anchor.vertex_id = remap(anchor.vertex_id);
  }
  for (auto& anchor : parameters.point_line_anchors) {
    anchor.vertex_id = remap(anchor.vertex_id);
  }
  // Coincident constraints reference point ids in target_ids — remap
  // them so the solver doesn't chase dissolved vertices.
  for (auto& constraint : parameters.constraints) {
    for (auto& target_id : constraint.target_ids) {
      if (parent.count(target_id)) {
        target_id = remap(target_id);
      }
    }
  }

  refresh_sketch_derived_state(*feature_it);
  bump_geometry_revision();
  return document_.value();
}
