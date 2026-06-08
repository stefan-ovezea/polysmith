namespace {

// Helper: which endpoint of `line` references `point_id`. Returns
// `true` for the start endpoint, `false` for the end endpoint.
// Caller must have already verified one of them does match.
bool line_start_matches(const SketchLine &line, const std::string &point_id) {
  return line.start_point_id == point_id;
}

} // namespace
