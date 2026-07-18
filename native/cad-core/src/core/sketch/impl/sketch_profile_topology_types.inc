struct EdgeLoopCandidate {
  std::vector<SketchProfilePoint> points;
  std::vector<std::string> vertex_ids;
  std::vector<std::string> edge_ids;
};

struct LineSegment {
  std::string line_id;
  SketchProfilePoint start;
  SketchProfilePoint end;
};

struct ArrangementEdge {
  std::string line_id;
  int start_node;
  int end_node;
};

struct ArrangementFace {
  std::vector<SketchProfilePoint> points;
  std::vector<std::string> vertex_ids;
  // Unique sorted set of edge IDs that form this face.
  std::vector<std::string> line_ids;
  // Edge IDs in boundary walk order (before sort/dedup — one per segment).
  std::vector<std::string> ordered_line_ids;
  double area;
};
