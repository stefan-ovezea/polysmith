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

TopoDS_Shape unify_same_domain(const TopoDS_Shape& shape) {
  if (shape.IsNull()) {
    return shape;
  }
  try {
    ShapeUpgrade_UnifySameDomain unify(shape,
                                       /*UnifyEdges=*/true,
                                       /*UnifyFaces=*/true,
                                       /*ConcatBSplines=*/false);
    unify.Build();
    const TopoDS_Shape unified = unify.Shape();
    if (!unified.IsNull()) {
      return unified;
    }
    fprintf(stderr, "[WARN] unify_same_domain: Build() succeeded but Shape() is null — keeping original\n");
  } catch (const std::exception& e) {
    fprintf(stderr, "[WARN] unify_same_domain: exception: %s — keeping original\n", e.what());
  }
  return shape;
}
