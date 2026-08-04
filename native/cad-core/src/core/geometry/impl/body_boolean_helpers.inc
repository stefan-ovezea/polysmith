std::optional<ParsedEdgeId> parse_edge_id(const std::string& id) {
  const std::string separator = ":edge:";
  const auto pos = id.find(separator);
  if (pos == std::string::npos) {
    return std::nullopt;
  }
  ParsedEdgeId parsed{};
  parsed.body_id = id.substr(0, pos);
  try {
    size_t consumed = 0;
    const std::string suffix = id.substr(pos + separator.size());
    parsed.index = std::stoi(suffix, &consumed);
    if (consumed != suffix.size() || parsed.index < 0) {
      return std::nullopt;
    }
  } catch (const std::exception&) {
    return std::nullopt;
  }
  return parsed;
}

TopoDS_Shape unify_same_domain(const TopoDS_Shape& shape,
                              bool unify_faces = true) {
  if (shape.IsNull()) {
    return shape;
  }
  try {
    ShapeUpgrade_UnifySameDomain unify(shape,
                                       /*UnifyEdges=*/true,
                                       /*UnifyFaces=*/unify_faces,
                                       /*ConcatBSplines=*/false);
    // Default tolerances (Precision::Confusion = 1e-7) are too tight
    // for boolean results in OCCT 8, where floating-point drift can
    // leave nominally coplanar faces just far enough apart that the
    // unifier skips them.  Use the same tolerances as SimplifyResult.
    unify.SetLinearTolerance(1e-5);
    unify.SetAngularTolerance(1e-5);
    unify.Build();
    const TopoDS_Shape unified = unify.Shape();
    if (!unified.IsNull()) {
      return unified;
    }
  } catch (const std::exception&) {
  }
  return shape;
}
