// Render a dimension value as a short, human-friendly string with no
// trailing zeros: 12 -> "12", 12.5 -> "12.5", 12.50 -> "12.5",
// 12.345678 -> "12.35". std::to_string on a double produces six
// trailing decimals which makes the canvas-rendered dimension labels
// noisy ("12.500000 mm"). Two decimals of precision is enough for the
// viewport readout; the underlying value the core stores is unchanged.
std::string format_dimension_value(double value) {
  char buffer[64];
  std::snprintf(buffer, sizeof(buffer), "%.2f", value);
  std::string text = buffer;
  if (text.find('.') != std::string::npos) {
    while (!text.empty() && text.back() == '0') {
      text.pop_back();
    }
    if (!text.empty() && text.back() == '.') {
      text.pop_back();
    }
  }
  if (text == "-0") {
    text = "0";
  }
  return text;
}

// Build a viewport label for a dimension.  Wraps the value in parentheses
// when the dimension is driven (reference-only), e.g. "(12.35 mm)".
inline std::string format_dimension_label(const SketchDimension& dim,
                                          const std::string& suffix = " mm") {
  std::string label = format_dimension_value(dim.value) + suffix;
  if (dim.driven) {
    label = "(driven) " + label;
  }
  return label;
}

// Centroid (in sketch-local 2D coords) of every line midpoint and
// circle center in the sketch. Used as the "inside" reference when
// placing line dimensions so the dimension line lands on the side of
// the segment that points away from the rest of the sketch — i.e. on
// the outside of a closed profile such as a rectangle. Falls back to
// the line's own midpoint when the sketch has no other entities, in
// which case the offset direction does not matter.
struct SketchCentroid2D {
  double x;
  double y;
  bool has_value;
};

SketchCentroid2D
compute_sketch_centroid_2d(const SketchFeatureParameters &parameters) {
  double sum_x = 0.0;
  double sum_y = 0.0;
  int count = 0;
  for (const auto &other_line : parameters.lines) {
    sum_x += (other_line.start_x + other_line.end_x) / 2.0;
    sum_y += (other_line.start_y + other_line.end_y) / 2.0;
    count += 1;
  }
  for (const auto &other_circle : parameters.circles) {
    sum_x += other_circle.center_x;
    sum_y += other_circle.center_y;
    count += 1;
  }
  if (count == 0) {
    return SketchCentroid2D{.x = 0.0, .y = 0.0, .has_value = false};
  }
  return SketchCentroid2D{
      .x = sum_x / static_cast<double>(count),
      .y = sum_y / static_cast<double>(count),
      .has_value = true,
  };
}
