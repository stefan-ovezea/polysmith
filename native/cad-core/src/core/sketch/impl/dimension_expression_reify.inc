void reify_dimension_expressions(
    FeatureEntry& feature,
    const std::vector<struct ParameterEntry>& parameters) {
  if (!feature.sketch_parameters.has_value()) {
    return;
  }

  struct PendingDimensionUpdate {
    std::string id;
    std::string expression;
    double value;
  };
  std::vector<PendingDimensionUpdate> pending_updates;

  for (const auto& dim : feature.sketch_parameters->dimensions) {
    if (dim.expression.empty()) {
      continue;
    }

    const bool dim_is_angle =
        (dim.kind == "angle" || dim.kind == "line_angle");

    // Build a resolver against the current parameter table
    auto resolver = [&parameters,
                      dim_is_angle](const std::string& name) -> double {
      for (const auto& p : parameters) {
        if (p.name == name) {
          if (p.has_error) {
            throw std::runtime_error("Parameter '" + name +
                                     "' has an unresolved expression");
          }
          // Angle-type parameter referenced in a non-angle (length)
          // dimension — the numeric value would be misinterpreted.
          if (p.kind == "angle" && !dim_is_angle) {
            throw std::runtime_error("Angle parameter '" + name +
                "' cannot be used in a length dimension");
          }
          return p.resolved_value;
        }
      }
      throw std::runtime_error("Unknown parameter: '" + name + "'");
    };

    try {
      double resolved = evaluate_formula(dim.expression, resolver);
      if (resolved <= 0.0) {
        // Keep last good value for invalid dimension values
        continue;
      }
      // Angle dimensions store radians, but expressions are authored
      // in degrees (both literal numbers and angle-kind parameter
      // references). Convert here so the dimension's internal value
      // stays in radians while the expression stays human-readable.
      if (dim.kind == "angle" || dim.kind == "line_angle") {
        resolved = resolved * (M_PI / 180.0);
      } else if (dim.kind == "circle_radius") {
        resolved = resolved / 2.0;
      }
      // Preserve the orientation quadrant for line_angle dimensions so
      // a parameter re-eval doesn't flip the line's direction.
      if (dim.kind == "line_angle" && dim.value < 0.0) {
        resolved = -resolved;
      }
      pending_updates.push_back(PendingDimensionUpdate{
          .id = dim.id,
          .expression = dim.expression,
          .value = resolved,
      });
    } catch (const std::exception&) {
      // Silently keep last good value — expression resolution
      // failures shouldn't nuke the dimension's working value.
    }
  }

  for (const auto& update : pending_updates) {
    try {
      update_sketch_dimension(feature,
                              update.id,
                              update.value,
                              update.expression);
    } catch (const std::exception&) {
      // Keep the last valid geometry when a stored expression can no
      // longer be driven, e.g. because a referenced parameter was
      // deleted or the target entity was removed.
    }
  }
}
