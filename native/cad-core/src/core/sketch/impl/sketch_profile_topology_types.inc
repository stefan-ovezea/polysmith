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
  std::vector<std::string> line_ids;
  std::vector<std::string> ordered_line_ids;
  double area;
};
