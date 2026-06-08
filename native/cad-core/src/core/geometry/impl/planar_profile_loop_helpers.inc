std::vector<SketchProfilePoint> to_local_profile_loop(
    const PlaneFrame& frame,
    const std::vector<FaceOutlinePoint>& loop) {
  std::vector<SketchProfilePoint> local;
  local.reserve(loop.size());
  for (const auto& point : loop) {
    local.push_back(to_local_profile_point(frame, point));
  }
  return local;
}
