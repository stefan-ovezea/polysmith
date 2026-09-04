json to_payload(const polysmith::core::ViewportToolpathPrimitive& primitive) {
  json points = json::array();
  for (const auto& p : primitive.points) {
    points.push_back({
        {"x", p.x},
        {"y", p.y},
        {"z", p.z},
        {"is_rapid", p.is_rapid},
        {"pierce", p.pierce},
    });
  }
  return {
      {"id", primitive.id},
      {"label", primitive.label},
      {"points", points},
  };
}
