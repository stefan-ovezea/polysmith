  if (command.type == "update_sketch_circle") {
    const auto document = document_manager().update_sketch_circle(
        read_string(command.payload, "circle_id"),
        read_dimension(command.payload, "center_x"),
        read_dimension(command.payload, "center_y"),
        read_dimension(command.payload, "radius"));

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "update_sketch_dimension") {
    // Accept either a numeric value (backward compatible) or a string
    // expression like "width * 2". If it's a string, evaluate against
    // the current parameter table, then pass the resolved value and
    // expression to the core for storage.
    double resolved_value = 0.0;
    std::optional<std::string> expression;

    // Look up the target dimension kind up front: circle_radius
    // dimensions carry the DISPLAYED diameter over IPC (the stored
    // value stays the radius — same convention as the expression
    // path below), and angle kinds convert degrees to radians.
    bool dim_is_angle = false;
    bool dim_is_circle_radius = false;
    {
      const auto doc = document_manager().get_document();
      if (doc.has_value() && doc->active_sketch_feature_id.has_value()) {
        const std::string dim_id =
            read_string(command.payload, "dimension_id");
        const auto feat_it = std::find_if(
            doc->feature_history.begin(), doc->feature_history.end(),
            [&](const core::FeatureEntry& f) {
              return f.id == doc->active_sketch_feature_id.value();
            });
        if (feat_it != doc->feature_history.end() &&
            feat_it->sketch_parameters.has_value()) {
          const auto& dims = feat_it->sketch_parameters->dimensions;
          const auto dim_it = std::find_if(
              dims.begin(), dims.end(),
              [&](const core::SketchDimension& d) { return d.id == dim_id; });
          if (dim_it != dims.end()) {
            dim_is_angle =
                dim_it->kind == "angle" || dim_it->kind == "line_angle";
            dim_is_circle_radius = dim_it->kind == "circle_radius";
          }
        }
      }
    }

    if (command.payload.contains("value")) {
      if (command.payload.at("value").is_number()) {
        resolved_value = command.payload.at("value").get<double>();
        if (dim_is_angle) {
          resolved_value = resolved_value * (M_PI / 180.0);
        } else if (dim_is_circle_radius) {
          resolved_value = resolved_value / 2.0;
        }
      } else if (command.payload.at("value").is_string()) {
        std::string expr_str =
            command.payload.at("value").get<std::string>();
        expression = expr_str;

        // Evaluate against current parameters
        const auto doc = document_manager().get_document();
        std::vector<core::ParameterEntry> params;
        if (doc.has_value()) {
          params = doc->parameters;
        }

        auto resolver = [&params,
                          dim_is_angle](const std::string& name) -> double {
          for (const auto& p : params) {
            if (p.name == name) {
              if (p.has_error) {
                throw std::runtime_error("Parameter '" + name +
                                         "' has an unresolved expression");
              }
              // Angle-type parameter referenced in a non-angle (length)
              // dimension - the numeric value would be misinterpreted.
              if (p.kind == "angle" && !dim_is_angle) {
                throw std::runtime_error(
                    "Angle parameter '" + name +
                    "' cannot be used in a length dimension");
              }
              return p.resolved_value;
            }
          }
          throw std::runtime_error("Unknown parameter: '" + name + "'");
        };

        try {
          resolved_value = core::evaluate_formula(expr_str, resolver);
          // Angle dimensions store radians internally, but expressions
          // authored by the user are in degrees (matching how plain
          // numeric angle edits are converted by the UI).  If this
          // dimension is an angle kind, convert degrees → radians so
          // the stored value matches what reify_dimension_expressions
          // produces.
          if (dim_is_angle) {
            resolved_value = resolved_value * (M_PI / 180.0);
          } else if (dim_is_circle_radius) {
            resolved_value = resolved_value / 2.0;
          }
        } catch (const std::exception& e) {
          throw std::runtime_error(
              std::string("Dimension expression error: ") + e.what());
        }
      } else {
        throw std::runtime_error(
            "Dimension 'value' must be a number or string expression");
      }
    } else {
      throw std::runtime_error("Dimension command missing 'value' field");
    }

    const auto document = document_manager().update_sketch_dimension(
        read_string(command.payload, "dimension_id"),
        resolved_value,
        expression);

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }
