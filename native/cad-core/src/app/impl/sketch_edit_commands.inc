  if (command.type == "start_sketch_on_plane") {
    const auto document = document_manager().start_sketch_on_plane(
        read_string(command.payload, "reference_id"));

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "start_sketch_on_face") {
    const auto document = document_manager().start_sketch_on_face(
        read_string(command.payload, "face_id"),
        read_plane_frame(command.payload, "plane_frame"));

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "set_sketch_tool") {
    const auto document =
        document_manager().set_sketch_tool(read_string(command.payload, "tool"));

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "update_sketch_line") {
    const auto document = document_manager().update_sketch_line(
        read_string(command.payload, "line_id"),
        read_dimension(command.payload, "start_x"),
        read_dimension(command.payload, "start_y"),
        read_dimension(command.payload, "end_x"),
        read_dimension(command.payload, "end_y"));

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "update_sketch_point") {
    const auto document = document_manager().update_sketch_point(
        read_string(command.payload, "point_id"),
        read_dimension(command.payload, "x"),
        read_dimension(command.payload, "y"));

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "set_sketch_line_constraint") {
    const std::string constraint = read_string(command.payload, "constraint");
    const auto document = document_manager().set_sketch_line_constraint(
        read_string(command.payload, "line_id"),
        constraint == "none" ? std::nullopt
                              : std::make_optional(constraint));

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "clear_sketch_line_constraints") {
    const auto document = document_manager().clear_sketch_line_constraints(
        read_string(command.payload, "line_id"));

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "set_sketch_equal_length_constraint") {
    const std::string other_line_id =
        read_string(command.payload, "other_line_id");
    const auto document = document_manager().set_sketch_equal_length_constraint(
        read_string(command.payload, "line_id"),
        other_line_id == "none" ? std::nullopt
                                 : std::make_optional(other_line_id));

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "set_sketch_perpendicular_constraint") {
    const std::string other_line_id =
        read_string(command.payload, "other_line_id");
    const auto document = document_manager().set_sketch_perpendicular_constraint(
        read_string(command.payload, "line_id"),
        other_line_id == "none" ? std::nullopt
                                 : std::make_optional(other_line_id));

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "start_mirror_preview") {
    const auto document = document_manager().start_mirror_preview();
    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "update_mirror_preview_axis") {
    // Empty axis_line_id is a valid clear. read_string allows
    // empty strings; the core treats empty as "no axis".
    const auto document = document_manager().update_mirror_preview_axis(
        read_string(command.payload, "axis_line_id"));
    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "update_mirror_preview_objects") {
    std::vector<std::string> object_ids;
    if (command.payload.contains("object_ids") &&
        command.payload.at("object_ids").is_array()) {
      for (const auto& entry : command.payload.at("object_ids")) {
        if (entry.is_string()) {
          object_ids.push_back(entry.get<std::string>());
        }
      }
    }
    const auto document =
        document_manager().update_mirror_preview_objects(object_ids);
    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "commit_mirror_preview") {
    bool persistent = false;
    if (command.payload.contains("persistent")) {
      persistent = command.payload["persistent"].get<bool>();
    }
    const auto document = document_manager().commit_mirror_preview(persistent);
    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "cancel_mirror_preview") {
    const auto document = document_manager().cancel_mirror_preview();
    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "set_sketch_tangent_constraint") {
    const auto document = document_manager().set_sketch_tangent_constraint(
        read_string(command.payload, "line_id"),
        read_string(command.payload, "circle_id"));

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "set_sketch_parallel_constraint") {
    const std::string other_line_id =
        read_string(command.payload, "other_line_id");
    const auto document = document_manager().set_sketch_parallel_constraint(
        read_string(command.payload, "line_id"),
        other_line_id == "none" ? std::nullopt
                                 : std::make_optional(other_line_id));

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "set_sketch_coincident_constraint") {
    const auto document = document_manager().set_sketch_coincident_constraint(
        read_string(command.payload, "point_id"),
        read_string(command.payload, "other_point_id"));

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "delete_sketch_coincident_constraint") {
    const auto document =
        document_manager().delete_sketch_coincident_constraint(
            read_string(command.payload, "constraint_id"));

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

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

    if (command.payload.contains("value")) {
      if (command.payload.at("value").is_number()) {
        resolved_value = command.payload.at("value").get<double>();
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

        // Determine whether the target dimension is an angle kind so we
        // can type-check parameter references and apply the degrees->radians
        // conversion once, in a single pass.
        bool dim_is_angle = false;
        bool dim_is_circle_radius = false;
        if (doc.has_value() &&
            doc->active_sketch_feature_id.has_value()) {
          const auto& features = doc->feature_history;
          const std::string dim_id =
              read_string(command.payload, "dimension_id");
          const auto feat_it = std::find_if(
              features.begin(), features.end(),
              [&](const core::FeatureEntry& f) {
                return f.id == doc->active_sketch_feature_id.value();
              });
          if (feat_it != features.end() &&
              feat_it->sketch_parameters.has_value()) {
            const auto& dims = feat_it->sketch_parameters->dimensions;
            const auto dim_it = std::find_if(
                dims.begin(), dims.end(),
                [&](const core::SketchDimension& d) {
                  return d.id == dim_id;
                });
            if (dim_it != dims.end() &&
                (dim_it->kind == "angle" ||
                 dim_it->kind == "line_angle")) {
              dim_is_angle = true;
            }
            if (dim_it != dims.end() && dim_it->kind == "circle_radius") {
              dim_is_circle_radius = true;
            }
          }
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

  if (command.type == "add_sketch_angle_dimension") {
    const auto document = document_manager().add_sketch_angle_dimension(
        read_string(command.payload, "first_line_id"),
        read_string(command.payload, "second_line_id"));

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "add_sketch_distance_dimension") {
    const auto document = document_manager().add_sketch_distance_dimension(
        read_string(command.payload, "first_entity_id"),
        read_string(command.payload, "second_entity_id"));

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "update_sketch_dimension_label_position") {
    const auto document =
        document_manager().update_sketch_dimension_label_position(
            read_string(command.payload, "dimension_id"),
            read_dimension(command.payload, "label_x"),
            read_dimension(command.payload, "label_y"));

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "add_sketch_line_length_dimension") {
    const auto document =
        document_manager().add_sketch_line_length_dimension(
            read_string(command.payload, "line_id"));

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "add_sketch_circle_radius_dimension") {
    std::optional<std::string> display_as;
    if (command.payload.contains("display_as") &&
        command.payload.at("display_as").is_string()) {
      display_as = command.payload.at("display_as").get<std::string>();
    }
    const auto document =
        document_manager().add_sketch_circle_radius_dimension(
            read_string(command.payload, "circle_id"), display_as);

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "add_sketch_point_distance_dimension") {
    const auto document =
        document_manager().add_sketch_point_distance_dimension(
            read_string(command.payload, "point_a_id"),
            read_string(command.payload, "point_b_id"));

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "add_sketch_polygon_radius_dimension") {
    const auto document =
        document_manager().add_sketch_polygon_radius_dimension(
            read_string(command.payload, "polygon_id"));

    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }
