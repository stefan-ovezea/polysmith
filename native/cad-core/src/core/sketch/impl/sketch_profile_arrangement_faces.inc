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
      // For directed segments, each sub-edge is a distinct directed edge;
      // don't deduplicate by node pair because the reverse direction is
      // not equivalent.
      if (!segments[index].directed) {
        const int low = std::min(start_node, end_node);
        const int high = std::max(start_node, end_node);
        const std::string key =
            std::to_string(low) + ":" + std::to_string(high);
        if (edge_keys.contains(key)) {
          continue;
        }
        edge_keys.insert(key);
      }
      edges.push_back(ArrangementEdge{
          .line_id = segments[index].line_id,
          .start_node = start_node,
          .end_node = end_node,
          .directed = segments[index].directed,
      });
    }
  }

  std::vector<std::vector<int>> adjacency(nodes.size());
  for (const auto& edge : edges) {
    adjacency[edge.start_node].push_back(edge.end_node);
    if (!edge.directed) {
      adjacency[edge.end_node].push_back(edge.start_node);
    }
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
  std::set<std::pair<int, int>> directed_edge_set;
  for (const auto& edge : edges) {
    line_for_directed_edge[{edge.start_node, edge.end_node}] = edge.line_id;
    if (!edge.directed) {
      line_for_directed_edge[{edge.end_node, edge.start_node}] = edge.line_id;
    } else {
      directed_edge_set.insert({edge.start_node, edge.end_node});
    }
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
      bool used_directed_edge = false;
      // Track the walk locally: a walk that dead-ends (directed arc
      // chains, tangent junctions) must not poison the global visited
      // set, otherwise the pairs it consumed can no longer be walked by
      // the walk that actually closes the face.
      std::set<std::pair<int, int>> walk_visited;
      for (size_t guard = 0; guard < edges.size() * 4 + 8; ++guard) {
        if (walk_visited.contains(current)) {
          break;
        }
        walk_visited.insert(current);
        if (directed_edge_set.contains(current)) {
          used_directed_edge = true;
        }
        face_nodes.push_back(current.first);
        face_lines.push_back(line_for_directed_edge[current]);

        const int at = current.second;
        const int from = current.first;
        const auto& neighbors = adjacency[at];
        if (neighbors.empty()) {
          break;
        }

        // Face walk with the interior on the left: among the edges
        // leaving `at`, take the one with the smallest counter-clockwise
        // turn from the incoming direction (the reverse edge has turn 0
        // and is excluded).  At tangent junctions a candidate continuing
        // the incoming edge's own curve (same edge id) wins when its turn
        // is within a few degrees of the best — polyline sampling makes
        // a straight-on tangent line nearly indistinguishable from the
        // curve continuation by angle alone, and without this preference
        // the walker would leave a circle at its tangent point instead of
        // tracing the circle interior.
        const auto& origin = nodes[at];
        const double incoming_angle =
            std::atan2(origin.y - nodes[from].y, origin.x - nodes[from].x);
        const std::string incoming_id = line_for_directed_edge[current];
        int next_node = -1;
        double best_left = std::numeric_limits<double>::max();
        bool best_is_same_id = false;
        for (const int candidate : neighbors) {
          const double angle =
              std::atan2(nodes[candidate].y - origin.y,
                         nodes[candidate].x - origin.x);
          double left = angle - incoming_angle;
          while (left <= 0.0) left += 2.0 * kPi;
          while (left > 2.0 * kPi) left -= 2.0 * kPi;
          const bool same_id =
              !incoming_id.empty() &&
              line_for_directed_edge[{at, candidate}] == incoming_id;
          constexpr double kTangentTieBreak = 5.0 * kPi / 180.0;
          if (next_node < 0 || left < best_left ||
              (same_id && !best_is_same_id &&
               left - best_left <= kTangentTieBreak)) {
            best_left = left;
            next_node = candidate;
            best_is_same_id = same_id;
          }
        }
        if (next_node < 0) {
          break;
        }
        current = {at, next_node};
        if (current == directed) {
          closed = true;
          break;
        }
      }

      if (!closed || face_nodes.size() < 3) {
        continue;
      }
      visited.insert(walk_visited.begin(), walk_visited.end());

      std::vector<SketchProfilePoint> face_points;
      std::vector<std::string> face_point_ids;
      for (const int node : face_nodes) {
        face_points.push_back(nodes[node]);
        face_point_ids.push_back(make_node_key(nodes[node]));
      }
      const double area = polygon_signed_area(face_points);
      // A CW walk of an undirected cycle is the unbounded exterior face —
      // drop it.  A face whose walk was forced through a directed arc can
      // only be walked in one orientation, which may be CW; normalise it
      // to CCW so downstream consumers see positive orientation.
      if (area < 0.0 && !used_directed_edge) {
        continue;
      }
      if (std::abs(area) <= kProfileTolerance) {
        continue;
      }
      if (area < 0.0) {
        std::reverse(face_points.begin(), face_points.end());
        std::reverse(face_point_ids.begin(), face_point_ids.end());
        std::reverse(face_lines.begin(), face_lines.end());
      }
      std::vector<std::string> ordered_face_lines = face_lines;
      std::sort(face_lines.begin(), face_lines.end());
      face_lines.erase(std::unique(face_lines.begin(), face_lines.end()),
                       face_lines.end());
      faces.push_back(ArrangementFace{
          .points = face_points,
          .vertex_ids = face_point_ids,
          .line_ids = face_lines,
          .ordered_line_ids = ordered_face_lines,
          .area = std::abs(area),
      });
    }
  }

  return faces;
}

std::vector<ArrangementFace> detect_line_arrangement_faces(
    const SketchFeatureParameters& parameters) {
  return detect_arrangement_faces(solid_line_segments(parameters));
}
