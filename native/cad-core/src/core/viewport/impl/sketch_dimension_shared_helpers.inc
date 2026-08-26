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
    label = "(" + label + ")";
  }
  return label;
}

// A sketch-local 2D point. Used for label placement, which every
// dimension kind stores as an absolute sketch-local coordinate pair
// (SketchDimension::label_x / label_y).
struct SketchLabelPoint2D {
  double x;
  double y;
};

// The stored label when the user has dragged this dimension, otherwise
// the caller's default placement. Radial kinds ignored the stored value
// entirely before this existed, so a dragged label snapped back to the
// default on the next viewport refetch.
inline SketchLabelPoint2D resolve_dimension_label_local(
    const SketchDimension& dimension, double default_x, double default_y) {
  if (dimension.label_x.has_value() && dimension.label_y.has_value()) {
    return SketchLabelPoint2D{*dimension.label_x, *dimension.label_y};
  }
  return SketchLabelPoint2D{default_x, default_y};
}

// Unit direction from a centre toward a label. Falls back to +X when the
// label sits on the centre, so the emitted geometry is never NaN.
inline SketchLabelPoint2D radial_contact_direction(double center_x,
                                                   double center_y,
                                                   double label_x,
                                                   double label_y) {
  const double dx = label_x - center_x;
  const double dy = label_y - center_y;
  const double length = std::sqrt(dx * dx + dy * dy);
  if (length <= 1e-9) {
    return SketchLabelPoint2D{1.0, 0.0};
  }
  return SketchLabelPoint2D{dx / length, dy / length};
}

// Leader geometry shared by every centre+radius dimension: circle
// radius/diameter, arc radius, and polygon circumradius.
struct RadialLeader2D {
  SketchLabelPoint2D label;     // resolved label placement
  SketchLabelPoint2D unit;      // centre -> label, normalized
  SketchLabelPoint2D contact;   // on the curve toward the label
  SketchLabelPoint2D quadrant;  // a quarter turn from the contact
};

// `contact` is where the arrowhead lands. `quadrant` is a direction
// reference for the renderer: the centre, contact and label are
// collinear, so there is no cross product to recover the sketch plane
// from when orienting the flat arrowhead triangles.
inline RadialLeader2D compute_radial_leader(const SketchDimension& dimension,
                                            double center_x, double center_y,
                                            double radius) {
  const SketchLabelPoint2D label = resolve_dimension_label_local(
      dimension, center_x + radius + kCircleDimensionOffset, center_y);
  const SketchLabelPoint2D unit =
      radial_contact_direction(center_x, center_y, label.x, label.y);
  return RadialLeader2D{
      label,
      unit,
      SketchLabelPoint2D{center_x + unit.x * radius,
                         center_y + unit.y * radius},
      SketchLabelPoint2D{center_x - unit.y * radius,
                         center_y + unit.x * radius},
  };
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
