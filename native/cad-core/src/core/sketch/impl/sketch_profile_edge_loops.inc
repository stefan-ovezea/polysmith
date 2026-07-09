std::optional<EdgeLoopCandidate> detect_edge_loop(
    const std::vector<ProfileEdge>& edges) {
  if (edges.size() < 2) {
    // A polygon needs at least 3 edges, but a loop made of two arcs
    // (e.g. a stadium half) is also valid. Two lines can't enclose
    // an area, so 2-line components fail the area test below; arcs
    // can pass on the area test, so we accept >=2 here and let the
    // signed-area gate filter degenerate cases.
    return std::nullopt;
  }

  std::map<std::string, SketchProfilePoint> nodes;
  std::map<std::string, std::vector<size_t>> adjacency;
  std::vector<std::pair<std::string, std::string>> edge_nodes;

  for (size_t index = 0; index < edges.size(); ++index) {
    const auto& edge = edges[index];
    const SketchProfilePoint start{.x = edge.start_x, .y = edge.start_y};
    const SketchProfilePoint end{.x = edge.end_x, .y = edge.end_y};

    if (points_match(start, end)) {
      return std::nullopt;
    }

    const std::string start_key = edge_node_id(edge, true);
    const std::string end_key = edge_node_id(edge, false);
    const auto [start_it, inserted_start] = nodes.emplace(start_key, start);
    const auto [end_it, inserted_end] = nodes.emplace(end_key, end);
    const bool start_uses_fallback_key = edge.start_vertex_id.empty();
    const bool end_uses_fallback_key = edge.end_vertex_id.empty();
    if ((!inserted_start && start_uses_fallback_key &&
         !points_match(start_it->second, start)) ||
        (!inserted_end && end_uses_fallback_key &&
         !points_match(end_it->second, end))) {
      return std::nullopt;
    }
    adjacency[start_key].push_back(index);
    adjacency[end_key].push_back(index);
    edge_nodes.push_back({start_key, end_key});
  }

  if (nodes.size() != edges.size()) {
    return std::nullopt;
  }

  for (const auto& [node_key, incident_edges] : adjacency) {
    if (incident_edges.size() != 2) {
      return std::nullopt;
    }
  }

  std::vector<SketchProfilePoint> ordered_points;
  std::vector<std::string> ordered_point_ids;
  std::vector<std::string> ordered_edge_ids;
  std::set<size_t> visited_edges;

  std::string current_node = edge_nodes.front().first;

  while (visited_edges.size() < edges.size()) {
    const auto adjacency_it = adjacency.find(current_node);
    if (adjacency_it == adjacency.end()) {
      return std::nullopt;
    }

    const auto next_edge_it = std::find_if(
        adjacency_it->second.begin(),
        adjacency_it->second.end(),
        [&](size_t edge_index) { return !visited_edges.contains(edge_index); });

    if (next_edge_it == adjacency_it->second.end()) {
      return std::nullopt;
    }

    const size_t edge_index = *next_edge_it;
    visited_edges.insert(edge_index);
    const auto& edge = edges[edge_index];

    // Push the start vertex of this edge in walk direction. The next
    // edge's iteration will push the next vertex, and so on around
    // the loop.
    ordered_points.push_back(nodes.at(current_node));
    ordered_point_ids.push_back(current_node);
    ordered_edge_ids.push_back(edge.id);

    // For arc edges, also push the interior samples so the polygon
    // approximates the arc when extruded. Direction matters: if the
    // loop walk enters at start_vertex_id we sample forward; if it
    // enters at end_vertex_id we sample reversed.
    if (edge.kind == ProfileEdge::Kind::Arc) {
      const bool from_start = current_node == edge_nodes[edge_index].first;
      const auto interior = sample_arc_interior(edge, from_start);
      // Interior samples don't have stable point ids — use a synthetic
      // one anchored to the arc id and the sample index so consumers
      // that group by point id can still distinguish them. They're
      // not user-meaningful and don't appear as snap targets.
      for (size_t i = 0; i < interior.size(); ++i) {
        ordered_points.push_back(interior[i]);
        ordered_point_ids.push_back(
            "arc-sample-" + edge.id + "-" + std::to_string(i));
      }
    }

    const auto& [start_key, end_key] = edge_nodes[edge_index];
    current_node = start_key == current_node ? end_key : start_key;
  }

  if (!points_match(nodes.at(edge_nodes.front().first),
                    nodes.at(current_node))) {
    return std::nullopt;
  }

  const double area = polygon_signed_area(ordered_points);
  if (std::abs(area) <= kProfileTolerance) {
    return std::nullopt;
  }

  if (area < 0.0) {
    std::reverse(ordered_points.begin(), ordered_points.end());
  }

  return EdgeLoopCandidate{
      .points = ordered_points,
      .vertex_ids = ordered_point_ids,
      .edge_ids = ordered_edge_ids,
  };
}

std::vector<std::vector<ProfileEdge>> split_edge_components(
    const std::vector<ProfileEdge>& edges) {
  std::map<std::string, std::vector<size_t>> node_to_edges;
  std::vector<std::pair<std::string, std::string>> edge_nodes;

  for (size_t index = 0; index < edges.size(); ++index) {
    const auto& edge = edges[index];
    const std::string start_key = edge_node_id(edge, true);
    const std::string end_key = edge_node_id(edge, false);
    node_to_edges[start_key].push_back(index);
    node_to_edges[end_key].push_back(index);
    edge_nodes.push_back({start_key, end_key});
  }

  std::set<size_t> visited_edges;
  std::vector<std::vector<ProfileEdge>> components;

  for (size_t start_index = 0; start_index < edges.size(); ++start_index) {
    if (visited_edges.contains(start_index)) {
      continue;
    }

    std::deque<size_t> frontier = {start_index};
    std::vector<ProfileEdge> component;

    while (!frontier.empty()) {
      const size_t edge_index = frontier.front();
      frontier.pop_front();

      if (visited_edges.contains(edge_index)) {
        continue;
      }

      visited_edges.insert(edge_index);
      component.push_back(edges[edge_index]);

      const auto& [start_key, end_key] = edge_nodes[edge_index];
      for (const auto& node_key : {start_key, end_key}) {
        const auto adjacency_it = node_to_edges.find(node_key);
        if (adjacency_it == node_to_edges.end()) {
          continue;
        }

        for (size_t adjacent_edge_index : adjacency_it->second) {
          if (!visited_edges.contains(adjacent_edge_index)) {
            frontier.push_back(adjacent_edge_index);
          }
        }
      }
    }

    components.push_back(component);
  }

  return components;
}
