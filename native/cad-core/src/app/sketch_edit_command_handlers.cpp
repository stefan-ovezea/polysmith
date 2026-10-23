#include "app/sketch_edit_command_handlers.h"

#define _USE_MATH_DEFINES
#include <algorithm>
#include <cmath>

#include "app/command_context.h"
#include "core/document/document.h"
#include "core/sketch/formula_eval.h"

namespace polysmith {

bool handle_sketch_edit_command(
    const polysmith::protocol::CommandMessage& command,
    polysmith::core::DocumentManager& manager) {
  // ── Sketch session ──────────────────────────────────────────────
  if (command.type == "start_sketch_on_plane") {
    const auto document = manager.start_sketch_on_plane(
        read_string(command.payload, "reference_id"));
    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return true;
  }

  if (command.type == "start_sketch_on_face") {
    const auto document = manager.start_sketch_on_face(
        read_string(command.payload, "face_id"),
        read_plane_frame(command.payload, "plane_frame"));
    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return true;
  }

  if (command.type == "redefine_sketch_plane") {
    const auto document = manager.redefine_sketch_plane(
        read_string(command.payload, "feature_id"),
        read_string(command.payload, "plane_id"));
    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return true;
  }

  if (command.type == "set_sketch_tool") {
    const auto document =
        manager.set_sketch_tool(read_string(command.payload, "tool"));
    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return true;
  }

  // ── Geometry update ─────────────────────────────────────────────
  if (command.type == "update_sketch_line") {
    const auto before_document = manager.get_document();
    const auto document = manager.update_sketch_line(
        read_string(command.payload, "line_id"),
        read_dimension(command.payload, "start_x"),
        read_dimension(command.payload, "start_y"),
        read_dimension(command.payload, "end_x"),
        read_dimension(command.payload, "end_y"));
    if (emit_sketch_entities_changed(command.id, before_document, document)) {
      return true;
    }
    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return true;
  }

  if (command.type == "update_sketch_vertex") {
    const auto before_document = manager.get_document();
    // `bind_to_vertex_id` (optional): the drag ended on a standalone
    // Point-tool point, so the move must also bind — and absorb — that point
    // in the same undo step. See DocumentManager::update_sketch_vertex.
    const auto document = manager.update_sketch_vertex(
        read_string(command.payload, "vertex_id"),
        read_dimension(command.payload, "x"),
        read_dimension(command.payload, "y"),
        read_optional_string_value(command.payload, "bind_to_vertex_id", ""));
    if (emit_sketch_entities_changed(command.id, before_document, document)) {
      return true;
    }
    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return true;
  }

  if (command.type == "move_sketch_entities" ||
      command.type == "transform_sketch_entities") {
    std::vector<std::string> entity_ids;
    if (command.payload.contains("entity_ids") &&
        command.payload.at("entity_ids").is_array()) {
      for (const auto& entry : command.payload.at("entity_ids")) {
        if (entry.is_string()) {
          entity_ids.push_back(entry.get<std::string>());
        }
      }
    }
    if (command.type == "transform_sketch_entities") {
      bool copy = false;
      if (command.payload.contains("copy") &&
          command.payload.at("copy").is_boolean()) {
        copy = command.payload.at("copy").get<bool>();
      }
      const auto before_document = manager.get_document();
      const auto document = manager.transform_sketch_entities(
          entity_ids, read_dimension(command.payload, "dx"),
          read_dimension(command.payload, "dy"),
          read_dimension(command.payload, "center_x"),
          read_dimension(command.payload, "center_y"),
          read_dimension(command.payload, "angle_deg"),
          read_dimension(command.payload, "scale"), copy);
      if (emit_sketch_entities_changed(command.id, before_document, document)) {
        return true;
      }
      polysmith::protocol::write_message(
          polysmith::protocol::make_document_state_event(
              command.id, polysmith::protocol::to_payload(document)));
      return true;
    }
    const auto before_document = manager.get_document();
    const auto document = manager.move_sketch_entities(
        entity_ids, read_dimension(command.payload, "dx"),
        read_dimension(command.payload, "dy"),
        read_dimension(command.payload, "center_x"),
        read_dimension(command.payload, "center_y"),
        read_dimension(command.payload, "angle_deg"));
    if (emit_sketch_entities_changed(command.id, before_document, document)) {
      return true;
    }
    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return true;
  }

  // ── Line constraints ────────────────────────────────────────────
  if (command.type == "set_sketch_line_constraint") {
    const std::string constraint = read_string(command.payload, "constraint");
    const auto document = manager.set_sketch_line_constraint(
        read_string(command.payload, "line_id"),
        constraint == "none" ? std::nullopt
                             : std::make_optional(constraint));
    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return true;
  }

  if (command.type == "clear_sketch_line_constraints") {
    const auto document = manager.clear_sketch_line_constraints(
        read_string(command.payload, "line_id"));
    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return true;
  }

  if (command.type == "set_sketch_equal_length_constraint") {
    const std::string other_line_id =
        read_string(command.payload, "other_line_id");
    const auto document = manager.set_sketch_equal_length_constraint(
        read_string(command.payload, "line_id"),
        other_line_id == "none" ? std::nullopt
                                : std::make_optional(other_line_id));
    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return true;
  }

  if (command.type == "set_sketch_perpendicular_constraint") {
    const std::string other_line_id =
        read_string(command.payload, "other_line_id");
    const auto document = manager.set_sketch_perpendicular_constraint(
        read_string(command.payload, "line_id"),
        other_line_id == "none" ? std::nullopt
                                : std::make_optional(other_line_id));
    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return true;
  }

  // ── Mirror ──────────────────────────────────────────────────────
  if (command.type == "start_mirror_preview") {
    const auto document = manager.start_mirror_preview();
    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return true;
  }

  if (command.type == "update_mirror_preview_axis") {
    const auto document = manager.update_mirror_preview_axis(
        read_string(command.payload, "axis_line_id"));
    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return true;
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
    const auto document = manager.update_mirror_preview_objects(object_ids);
    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return true;
  }

  if (command.type == "commit_mirror_preview") {
    bool persistent = false;
    if (command.payload.contains("persistent")) {
      persistent = command.payload["persistent"].get<bool>();
    }
    const auto document = manager.commit_mirror_preview(persistent);
    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return true;
  }

  if (command.type == "cancel_mirror_preview") {
    const auto document = manager.cancel_mirror_preview();
    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return true;
  }

  // ── Relation constraints ────────────────────────────────────────
  if (command.type == "set_sketch_tangent_constraint") {
    const auto document = manager.set_sketch_tangent_constraint(
        read_string(command.payload, "line_id"),
        read_string(command.payload, "circle_id"));
    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return true;
  }

  if (command.type == "set_sketch_parallel_constraint") {
    const std::string other_line_id =
        read_string(command.payload, "other_line_id");
    const auto document = manager.set_sketch_parallel_constraint(
        read_string(command.payload, "line_id"),
        other_line_id == "none" ? std::nullopt
                                : std::make_optional(other_line_id));
    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return true;
  }

  if (command.type == "set_sketch_coincident_constraint") {
    const auto document = manager.set_sketch_coincident_constraint(
        read_string(command.payload, "vertex_id"),
        read_string(command.payload, "other_vertex_id"));
    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return true;
  }

  if (command.type == "delete_sketch_coincident_constraint") {
    const auto document = manager.delete_sketch_coincident_constraint(
        read_string(command.payload, "constraint_id"));
    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return true;
  }

  if (command.type == "set_sketch_symmetric_constraint") {
    const auto document = manager.set_sketch_symmetric_constraint(
        read_string(command.payload, "point_id"),
        read_string(command.payload, "other_point_id"),
        read_string(command.payload, "axis_line_id"));
    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return true;
  }

  if (command.type == "set_sketch_midpoint_constraint") {
    const auto document = manager.set_sketch_midpoint_constraint(
        read_string(command.payload, "vertex_id"),
        read_string(command.payload, "line_id"));
    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return true;
  }

  if (command.type == "set_sketch_collinear_constraint") {
    const auto document = manager.set_sketch_collinear_constraint(
        read_string(command.payload, "line_id"),
        read_string(command.payload, "other_line_id"));
    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return true;
  }

  if (command.type == "set_sketch_tangent_pair_constraint") {
    const auto document = manager.set_sketch_tangent_pair_constraint(
        read_string(command.payload, "first_id"),
        read_string(command.payload, "second_id"));
    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return true;
  }

  // ── Dimension create / label ────────────────────────────────────
  if (command.type == "add_sketch_angle_dimension") {
    std::optional<double> angle_value;
    if (command.payload.contains("value") &&
        command.payload.at("value").is_number()) {
      angle_value = command.payload.at("value").get<double>();
    }
    const auto document = manager.add_sketch_angle_dimension(
        read_string(command.payload, "first_line_id"),
        read_string(command.payload, "second_line_id"), angle_value);
    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return true;
  }

  if (command.type == "add_sketch_distance_dimension") {
    const auto document = manager.add_sketch_distance_dimension(
        read_string(command.payload, "first_entity_id"),
        read_string(command.payload, "second_entity_id"));
    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return true;
  }

  if (command.type == "update_sketch_dimension_label_position") {
    const auto document = manager.update_sketch_dimension_label_position(
        read_string(command.payload, "dimension_id"),
        read_dimension(command.payload, "label_x"),
        read_dimension(command.payload, "label_y"));
    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return true;
  }

  if (command.type == "add_sketch_line_length_dimension") {
    const auto document = manager.add_sketch_line_length_dimension(
        read_string(command.payload, "line_id"));
    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return true;
  }

  if (command.type == "add_sketch_line_angle_dimension") {
    const auto document = manager.add_sketch_line_angle_dimension(
        read_string(command.payload, "line_id"));
    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return true;
  }

  if (command.type == "add_sketch_circle_radius_dimension") {
    std::optional<std::string> display_as;
    if (command.payload.contains("display_as") &&
        command.payload.at("display_as").is_string()) {
      display_as = command.payload.at("display_as").get<std::string>();
    }
    const auto document = manager.add_sketch_circle_radius_dimension(
        read_string(command.payload, "circle_id"), display_as);
    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return true;
  }

  if (command.type == "add_sketch_arc_radius_dimension") {
    const auto document = manager.add_sketch_arc_radius_dimension(
        read_string(command.payload, "arc_id"));
    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return true;
  }

  if (command.type == "add_sketch_arc_angle_dimension") {
    const auto document = manager.add_sketch_arc_angle_dimension(
        read_string(command.payload, "arc_id"));
    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return true;
  }

  if (command.type == "add_sketch_arc_length_dimension") {
    const auto document = manager.add_sketch_arc_length_dimension(
        read_string(command.payload, "arc_id"));
    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return true;
  }

  if (command.type == "add_sketch_vertex_distance_dimension") {
    std::optional<std::string> axis;
    if (command.payload.contains("axis") &&
        command.payload.at("axis").is_string()) {
      axis = command.payload.at("axis").get<std::string>();
    }
    const auto document = manager.add_sketch_vertex_distance_dimension(
        read_string(command.payload, "vertex_a_id"),
        read_string(command.payload, "vertex_b_id"), axis);
    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return true;
  }

  if (command.type == "add_sketch_polygon_radius_dimension") {
    const auto document = manager.add_sketch_polygon_radius_dimension(
        read_string(command.payload, "polygon_id"));
    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return true;
  }

  if (command.type == "update_sketch_circle") {
    const auto document = manager.update_sketch_circle(
        read_string(command.payload, "circle_id"),
        read_dimension(command.payload, "center_x"),
        read_dimension(command.payload, "center_y"),
        read_dimension(command.payload, "radius"));
    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return true;
  }

  if (command.type == "update_sketch_dimension") {
    double resolved_value = 0.0;
    std::optional<std::string> expression;

    bool dim_is_angle = false;
    bool dim_is_diameter_display = false;
    {
      const auto doc = manager.get_document();
      if (doc.has_value() && doc->active_sketch_feature_id.has_value()) {
        const std::string dim_id =
            read_string(command.payload, "dimension_id");
        const auto feat_it = std::find_if(
            doc->feature_history.begin(), doc->feature_history.end(),
            [&](const polysmith::core::FeatureEntry& f) {
              return f.id == doc->active_sketch_feature_id.value();
            });
        if (feat_it != doc->feature_history.end() &&
            feat_it->sketch_parameters.has_value()) {
          const auto& dims = feat_it->sketch_parameters->dimensions;
          const auto dim_it = std::find_if(
              dims.begin(), dims.end(),
              [&](const polysmith::core::SketchDimension& d) {
                return d.id == dim_id;
              });
          if (dim_it != dims.end()) {
            dim_is_angle =
                dim_it->kind == "angle" || dim_it->kind == "line_angle";
            dim_is_diameter_display = dim_it->kind == "circle_radius" &&
                                      dim_it->display_as != "radius";
          }
        }
      }
    }

    if (command.payload.contains("value")) {
      if (command.payload.at("value").is_number()) {
        resolved_value = command.payload.at("value").get<double>();
        if (dim_is_angle) {
          resolved_value = resolved_value * (M_PI / 180.0);
        }
        if (dim_is_diameter_display) {
          resolved_value = resolved_value / 2.0;
        }
      } else if (command.payload.at("value").is_string()) {
        std::string expr_str = command.payload.at("value").get<std::string>();
        expression = expr_str;

        const auto doc = manager.get_document();
        std::vector<polysmith::core::ParameterEntry> params;
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
          resolved_value =
              polysmith::core::evaluate_formula(expr_str, resolver);
          if (dim_is_angle) {
            resolved_value = resolved_value * (M_PI / 180.0);
          } else if (dim_is_diameter_display) {
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

    const auto document = manager.update_sketch_dimension(
        read_string(command.payload, "dimension_id"), resolved_value,
        expression);
    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return true;
  }

  return false;
}

}  // namespace polysmith
