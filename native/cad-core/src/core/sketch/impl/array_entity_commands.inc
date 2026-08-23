// ================================================================
// Array tools: create_linear_array / create_circular_array
//
// Exploded copies (Fusion's accepted alternative to parametric
// patterns): the command commits `count - 1` copies of the source
// entities through transform_sketch_entities(copy=true), so copies
// get fresh ids, preserve the source's vertex topology among
// themselves, and never adopt coincident vertices.
//
// v1: direct commit — the mirror-style pending preview workflow is
// deferred (undo is the adjust path).
// ================================================================

void create_linear_array(FeatureEntry& feature,
                         const std::vector<std::string>& entity_ids,
                         double dx,
                         double dy,
                         int count) {
  if (count < 2) {
    throw std::runtime_error("Sketch array: count must be at least 2");
  }
  if (count > 100) {
    throw std::runtime_error("Sketch array: count must be at most 100");
  }
  for (int i = 1; i < count; ++i) {
    transform_sketch_entities(feature, entity_ids, dx * i, dy * i,
                              /*center_x=*/0.0, /*center_y=*/0.0,
                              /*angle_deg=*/0.0, /*scale=*/1.0,
                              /*copy=*/true);
  }
}

void create_circular_array(FeatureEntry& feature,
                           const std::vector<std::string>& entity_ids,
                           double center_x,
                           double center_y,
                           int count,
                           double total_angle_deg) {
  if (count < 2) {
    throw std::runtime_error("Sketch array: count must be at least 2");
  }
  if (count > 100) {
    throw std::runtime_error("Sketch array: count must be at most 100");
  }
  if (count == 2 && std::abs(total_angle_deg) < 1.0e-9) {
    throw std::runtime_error(
        "Sketch array: two instances need a non-zero total angle");
  }
  // Fusion-style spacing: `count` instances INCLUDING the original,
  // evenly distributed over the total sweep — copy i sits at
  // total * i / count, so the last copy stops one step short of the
  // wrap (the source occupies angle 0 and must not be overlapped).
  const double step_deg = total_angle_deg / static_cast<double>(count);
  for (int i = 1; i < count; ++i) {
    transform_sketch_entities(feature, entity_ids, /*dx=*/0.0, /*dy=*/0.0,
                              center_x, center_y,
                              step_deg * i, /*scale=*/1.0,
                              /*copy=*/true);
  }
}
