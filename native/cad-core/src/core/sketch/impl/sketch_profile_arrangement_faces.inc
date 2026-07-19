std::vector<ArrangementFace> detect_arrangement_faces(
    const std::vector<LineSegment>& segments) {
  std::vector<std::vector<SketchProfilePoint>> split_points(segments.size());
  for (size_t index = 0; index < segments.size(); ++index) {
    split_points[index].push_back(segments[index].start);
    split_points[index].push_back(segments[index].end);
  }

  for (size_t left = 0; left < segments.size(); ++left) {
    for (size_t right = left + 1; right < segments.size(); ++right) {
      if (const auto intersection =
              line_intersection(segments[left], segments[right]);
          intersection.has_value()) {
        split_points[left].push_back(intersection.value());
        split_points[right].push_back(intersection.value());
      }
    }
  }

  std::map<std::string, int> node_by_key;
  std::vector<SketchProfilePoint> nodes;
  auto node_index_for = [&](const SketchProfilePoint& point) {
    const std::string key = make_node_key(point);
    const auto existing = node_by_key.find(key);
    if (existing != node_by_key.end()) {
      return existing->second;
    }
    const int next_index = static_cast<int>(nodes.size());
    node_by_key[key] = next_index;
    nodes.push_back(point);
    return next_index;
  };

  std::vector<ArrangementEdge> edges;
  std::set<std::string> edge_keys;
  for (size_t index = 0; index < segments.size(); ++index) {
    auto points = split_points[index];
    std::sort(points.begin(), points.end(), [&](const auto& left, const auto& right) {
      return segment_parameter(segments[index], left) <
             segment_parameter(segments[index], right);
    });
    points.erase(std::unique(points.begin(), points.end(), points_match),
                 points.end());

    for (size_t point_index = 1; point_index < points.size(); ++point_index) {
      const int start_node = node_index_for(points[point_index - 1]);
      const int end_node = node_index_for(points[point_index]);
      if (start_node == end_node) {
        continue;
      }
      const int low = std::min(start_node, end_node);
      const int high = std::max(start_node, end_node);
      const std::string key = std::to_string(low) + ":" + std::to_string(high);
      if (edge_keys.contains(key)) {
        continue;
      }
      edge_keys.insert(key);
      edges.push_back(ArrangementEdge{
          .line_id = segments[index].line_id,
          .start_node = start_node,
          .end_node = end_node,
      });
    }
  }

  std::vector<std::vector<int>> adjacency(nodes.size());
  for (const auto& edge : edges) {
    adjacency[edge.start_node].push_back(edge.end_node);
    adjacency[edge.end_node].push_back(edge.start_node);
  }
  for (size_t node_index = 0; node_index < adjacency.size(); ++node_index) {
    auto& neighbors = adjacency[node_index];
    std::sort(neighbors.begin(), neighbors.end(), [&](int left, int right) {
      const auto& origin = nodes[node_index];
      const double left_angle =
          std::atan2(nodes[left].y - origin.y, nodes[left].x - origin.x);
      const double right_angle =
          std::atan2(nodes[right].y - origin.y, nodes[right].x - origin.x);
      return left_angle < right_angle;
    });
  }

  std::map<std::pair<int, int>, std::string> line_for_directed_edge;
  for (const auto& edge : edges) {
    line_for_directed_edge[{edge.start_node, edge.end_node}] = edge.line_id;
    line_for_directed_edge[{edge.end_node, edge.start_node}] = edge.line_id;
  }

  std::set<std::pair<int, int>> visited;
  std::vector<ArrangementFace> faces;
  for (const auto& edge : edges) {
    for (const auto& directed :
         {std::pair<int, int>{edge.start_node, edge.end_node},
          std::pair<int, int>{edge.end_node, edge.start_node}}) {
      if (visited.contains(directed)) {
        continue;
      }

      std::vector<int> face_nodes;
      std::vector<std::string> face_lines;
      std::pair<int, int> current = directed;
      bool closed = false;
      for (size_t guard = 0; guard < edges.size() * 4 + 8; ++guard) {
        if (visited.contains(current)) {
          break;
        }
        visited.insert(current);
        face_nodes.push_back(current.first);
        face_lines.push_back(line_for_directed_edge[current]);

        const int at = current.second;
        const int from = current.first;
        const auto& neighbors = adjacency[at];
        const auto reverse_it = std::find(neighbors.begin(), neighbors.end(), from);
        if (reverse_it == neighbors.end() || neighbors.empty()) {
          break;
        }
        const size_t reverse_index =
            static_cast<size_t>(std::distance(neighbors.begin(), reverse_it));
        const size_t next_index =
            (reverse_index + neighbors.size() - 1) % neighbors.size();
        current = {at, neighbors[next_index]};
        if (current == directed) {
          closed = true;
          break;
        }
      }

      if (!closed || face_nodes.size() < 3) {
        continue;
      }

      std::vector<SketchProfilePoint> face_points;
      std::vector<std::string> face_point_ids;
      for (const int node : face_nodes) {
        face_points.push_back(nodes[node]);
        face_point_ids.push_back(make_node_key(nodes[node]));
      }
      const double area = polygon_signed_area(face_points);
      if (area <= kProfileTolerance) {
        continue;
      }
      std::sort(face_lines.begin(), face_lines.end());
      face_lines.erase(std::unique(face_lines.begin(), face_lines.end()),
                       face_lines.end());
      faces.push_back(ArrangementFace{
          .points = face_points,
          .vertex_ids = face_point_ids,
          .line_ids = face_lines,
          .area = area,
      });
    }
  }

  return faces;
}

std::vector<ArrangementFace> detect_line_arrangement_faces(
    const SketchFeatureParameters& parameters) {
  return detect_arrangement_faces(solid_line_segments(parameters));
}
