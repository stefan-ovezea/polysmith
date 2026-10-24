// Env-gated trace of the sketch solver's input and output geometry.
//
// Constraint problems that only *look* wrong ("the rectangle jumped", "the
// dimension did nothing") are impossible to reason about from the outside: the
// solver's answer depends on the exact geometry it was handed, and the ad-hoc
// enforcement passes then move things again. Set PS_TRACE_SKETCH_SOLVE=1 and the
// Logs panel shows the sketch before the solve, immediately after it, and at the
// end of the refresh — the same "turn it on and read the structured log"
// contract as PS_TRACE_FACES for the profile walk.
bool sketch_solve_trace_enabled() {
  static const bool enabled = std::getenv("PS_TRACE_SKETCH_SOLVE") != nullptr;
  return enabled;
}

std::string sketch_geometry_summary(const SketchFeatureParameters& parameters) {
  std::ostringstream summary;
  for (const auto& line : parameters.lines) {
    summary << " | " << line.id << " (" << line.start_x << "," << line.start_y
            << ")->(" << line.end_x << "," << line.end_y << ")";
  }
  for (const auto& circle : parameters.circles) {
    summary << " | " << circle.id << " c(" << circle.center_x << ","
            << circle.center_y << ") r" << circle.radius;
  }
  for (const auto& arc : parameters.arcs) {
    summary << " | " << arc.id << " c(" << arc.center_x << "," << arc.center_y
            << ") r" << arc.radius;
  }
  return summary.str();
}

void trace_sketch_geometry(const char* stage,
                           const SketchFeatureParameters& parameters) {
  polysmith::core::log_info(
      "sketch_trace",
      std::string(stage) + ": " + sketch_geometry_summary(parameters));
}
