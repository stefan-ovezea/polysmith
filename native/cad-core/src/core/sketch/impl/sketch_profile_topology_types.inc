struct EdgeLoopCandidate {
  std::vector<SketchProfilePoint> points;
  std::vector<std::string> vertex_ids;
  std::vector<std::string> edge_ids;
};

struct LineSegment {
  std::string line_id;
  SketchProfilePoint start;
  SketchProfilePoint end;
  // When true, this edge can only be traversed from start to end in
  // the face-detection graph.  Used for sampled arcs so the arrangement
  // never produces faces that walk the arc backwards.
  bool directed = false;
};

struct ArrangementEdge {
  std::string line_id;
  int start_node;
  int end_node;
  bool directed = false;
};

struct ArrangementFace {
  std::vector<SketchProfilePoint> points;
  std::vector<std::string> vertex_ids;
  std::vector<std::string> line_ids;
  std::vector<std::string> ordered_line_ids;
  double area;
};
