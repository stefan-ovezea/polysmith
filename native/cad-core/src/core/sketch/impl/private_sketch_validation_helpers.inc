std::string make_parameters_summary(const SketchFeatureParameters& parameters) {
  std::ostringstream stream;
  stream << parameters.plane_id << " · " << parameters.lines.size() << " line";
  if (parameters.lines.size() != 1) {
    stream << "s";
  }
  if (!parameters.circles.empty()) {
    stream << " · " << parameters.circles.size() << " circle";
    if (parameters.circles.size() != 1) {
      stream << "s";
    }
  }
  if (!parameters.texts.empty()) {
    stream << " · " << parameters.texts.size() << " text";
    if (parameters.texts.size() != 1) {
      stream << "s";
    }
  }
  return stream.str();
}

void validate_line(double start_x,
                   double start_y,
                   double end_x,
                   double end_y) {
  const double dx = end_x - start_x;
  const double dy = end_y - start_y;
  if (std::sqrt(dx * dx + dy * dy) <= 0.001) {
    throw std::runtime_error("Sketch lines must have non-zero length");
  }
}

std::optional<std::string> infer_constraint_hint(double start_x,
                                                 double start_y,
                                                 double end_x,
                                                 double end_y) {
  const double dx = std::abs(end_x - start_x);
  const double dy = std::abs(end_y - start_y);
  constexpr double kConstraintTolerance = 0.01;

  if (dx <= kConstraintTolerance) {
    return std::string("vertical");
  }

  if (dy <= kConstraintTolerance) {
    return std::string("horizontal");
  }

  return std::nullopt;
}

void validate_constraint(const std::optional<std::string>& constraint) {
  if (!constraint.has_value()) {
    return;
  }

  if (constraint.value() != "horizontal" && constraint.value() != "vertical") {
    throw std::runtime_error("Unsupported sketch constraint: " + constraint.value());
  }
}

void validate_tool(const std::string& tool) {
  if (tool != "select" && tool != "line" && tool != "rectangle" &&
      tool != "circle" && tool != "polygon" && tool != "arc" && tool != "fillet" &&
      tool != "trim" && tool != "project" && tool != "dimension" &&
      tool != "move" && tool != "text") {
    throw std::runtime_error("Unsupported sketch tool: " + tool + " (validate_tool)");
  }
}
