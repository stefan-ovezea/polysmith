#include "app/sketch_utility_command_handlers.h"

#include <algorithm>

#include "app/command_context.h"
#include "core/document/document.h"

namespace polysmith {

bool handle_sketch_utility_command(
    const polysmith::protocol::CommandMessage& command,
    polysmith::core::DocumentManager& manager) {
  // ── Parameters / selection filter / trim ────────────────────────
  if (command.type == "add_parameter") {
    std::string kind = "length";
    if (command.payload.contains("kind") &&
        command.payload.at("kind").is_string()) {
      kind = command.payload.at("kind").get<std::string>();
    }
    const auto document = manager.add_parameter(
        read_string(command.payload, "name"),
        read_string(command.payload, "expression"), kind);
    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return true;
  }

  if (command.type == "update_parameter") {
    std::string kind = "length";
    if (command.payload.contains("kind") &&
        command.payload.at("kind").is_string()) {
      kind = command.payload.at("kind").get<std::string>();
    }
    const auto document = manager.update_parameter(
        read_string(command.payload, "name"),
        read_string(command.payload, "expression"), kind);
    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return true;
  }

  if (command.type == "update_selection_filter") {
    auto filter = polysmith::core::SelectionFilter{};
    if (command.payload.contains("select_curves") &&
        command.payload.at("select_curves").is_boolean()) {
      filter.select_curves = command.payload.at("select_curves").get<bool>();
    }
    if (command.payload.contains("select_points") &&
        command.payload.at("select_points").is_boolean()) {
      filter.select_points = command.payload.at("select_points").get<bool>();
    }
    if (command.payload.contains("select_construction") &&
        command.payload.at("select_construction").is_boolean()) {
      filter.select_construction =
          command.payload.at("select_construction").get<bool>();
    }
    if (command.payload.contains("select_constraints") &&
        command.payload.at("select_constraints").is_boolean()) {
      filter.select_constraints =
          command.payload.at("select_constraints").get<bool>();
    }
    if (command.payload.contains("snap_endpoint") &&
        command.payload.at("snap_endpoint").is_boolean()) {
      filter.snap_endpoint = command.payload.at("snap_endpoint").get<bool>();
    }
    if (command.payload.contains("snap_midpoint") &&
        command.payload.at("snap_midpoint").is_boolean()) {
      filter.snap_midpoint = command.payload.at("snap_midpoint").get<bool>();
    }
    if (command.payload.contains("snap_center") &&
        command.payload.at("snap_center").is_boolean()) {
      filter.snap_center = command.payload.at("snap_center").get<bool>();
    }
    if (command.payload.contains("snap_intersection") &&
        command.payload.at("snap_intersection").is_boolean()) {
      filter.snap_intersection =
          command.payload.at("snap_intersection").get<bool>();
    }
    if (command.payload.contains("snap_nearest") &&
        command.payload.at("snap_nearest").is_boolean()) {
      filter.snap_nearest = command.payload.at("snap_nearest").get<bool>();
    }
    if (command.payload.contains("snap_quadrant") &&
        command.payload.at("snap_quadrant").is_boolean()) {
      filter.snap_quadrant = command.payload.at("snap_quadrant").get<bool>();
    }
    if (command.payload.contains("snap_perpendicular") &&
        command.payload.at("snap_perpendicular").is_boolean()) {
      filter.snap_perpendicular =
          command.payload.at("snap_perpendicular").get<bool>();
    }
    if (command.payload.contains("snap_parallel") &&
        command.payload.at("snap_parallel").is_boolean()) {
      filter.snap_parallel = command.payload.at("snap_parallel").get<bool>();
    }
    if (command.payload.contains("snap_tangent") &&
        command.payload.at("snap_tangent").is_boolean()) {
      filter.snap_tangent = command.payload.at("snap_tangent").get<bool>();
    }
    if (command.payload.contains("snap_grid") &&
        command.payload.at("snap_grid").is_boolean()) {
      filter.snap_grid = command.payload.at("snap_grid").get<bool>();
    }
    if (command.payload.contains("snap_grid_line") &&
        command.payload.at("snap_grid_line").is_boolean()) {
      filter.snap_grid_line = command.payload.at("snap_grid_line").get<bool>();
    }
    if (command.payload.contains("snap_polar") &&
        command.payload.at("snap_polar").is_boolean()) {
      filter.snap_polar = command.payload.at("snap_polar").get<bool>();
    }
    if (command.payload.contains("magnetic_pull") &&
        command.payload.at("magnetic_pull").is_boolean()) {
      filter.magnetic_pull = command.payload.at("magnetic_pull").get<bool>();
    }
    if (command.payload.contains("tolerance_px") &&
        command.payload.at("tolerance_px").is_number()) {
      filter.tolerance_px = command.payload.at("tolerance_px").get<int>();
    }
    const auto document = manager.update_selection_filter(filter);
    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return true;
  }

  if (command.type == "delete_parameter") {
    const auto document = manager.delete_parameter(
        read_string(command.payload, "name"));
    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return true;
  }

  if (command.type == "trim_sketch_entity") {
    std::optional<int> segment_index = std::nullopt;
    if (command.payload.contains("segment_index") &&
        command.payload["segment_index"].is_number()) {
      segment_index = command.payload["segment_index"].get<int>();
    }
    std::optional<int> expected_revision = std::nullopt;
    if (command.payload.contains("expected_revision") &&
        command.payload["expected_revision"].is_number()) {
      expected_revision = command.payload["expected_revision"].get<int>();
    }
    const auto document = manager.trim_sketch_entity(
        read_string(command.payload, "entity_id"),
        read_dimension(command.payload, "click_x"),
        read_dimension(command.payload, "click_y"), segment_index,
        nullptr /* minted_vertex_ids */, expected_revision);
    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return true;
  }

  // ── Arrays ──────────────────────────────────────────────────────
  if (command.type == "create_linear_array" ||
      command.type == "create_circular_array") {
    std::vector<std::string> entity_ids;
    if (command.payload.contains("entity_ids") &&
        command.payload.at("entity_ids").is_array()) {
      for (const auto& entry : command.payload.at("entity_ids")) {
        if (entry.is_string()) {
          entity_ids.push_back(entry.get<std::string>());
        }
      }
    }
    if (command.type == "create_linear_array") {
      const auto document = manager.create_linear_array(
          entity_ids, read_dimension(command.payload, "dx"),
          read_dimension(command.payload, "dy"),
          read_int(command.payload, "count"));
      polysmith::protocol::write_message(
          polysmith::protocol::make_document_state_event(
              command.id, polysmith::protocol::to_payload(document)));
      return true;
    }
    const auto document = manager.create_circular_array(
        entity_ids, read_dimension(command.payload, "center_x"),
        read_dimension(command.payload, "center_y"),
        read_int(command.payload, "count"),
        read_dimension(command.payload, "total_angle_deg"));
    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return true;
  }

  // ── Text ────────────────────────────────────────────────────────
  if (command.type == "add_sketch_text") {
    polysmith::core::SketchText text{};
    text.text = command.payload.contains("text") &&
                        command.payload.at("text").is_string()
                    ? command.payload.at("text").get<std::string>()
                    : std::string("Text");
    text.font_path = read_optional_string(command.payload, "font_path").value_or("");
    text.height_mm = command.payload.contains("height_mm") &&
                             command.payload.at("height_mm").is_number()
                         ? command.payload.at("height_mm").get<double>()
                         : 10.0;
    text.angle_deg = command.payload.contains("angle_deg") &&
                             command.payload.at("angle_deg").is_number()
                         ? command.payload.at("angle_deg").get<double>()
                         : 0.0;
    text.anchor_x = read_dimension(command.payload, "anchor_x");
    text.anchor_y = read_dimension(command.payload, "anchor_y");
    text.h_align = command.payload.contains("h_align") &&
                           command.payload.at("h_align").is_string()
                       ? command.payload.at("h_align").get<std::string>()
                       : std::string("center");
    text.v_align = command.payload.contains("v_align") &&
                           command.payload.at("v_align").is_string()
                       ? command.payload.at("v_align").get<std::string>()
                       : std::string("middle");
    text.char_spacing = command.payload.contains("char_spacing") &&
                                command.payload.at("char_spacing").is_number()
                            ? command.payload.at("char_spacing").get<double>()
                            : 0.0;
    text.path_entity_id = read_optional_string(command.payload, "path_entity_id");
    if (command.payload.contains("path_offset") &&
        command.payload.at("path_offset").is_number()) {
      text.path_offset = command.payload.at("path_offset").get<double>();
    }
    const auto document = manager.add_sketch_text(text);
    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return true;
  }

  if (command.type == "update_sketch_text") {
    const std::string text_id = read_string(command.payload, "text_id");
    polysmith::core::SketchText merged{};
    const auto current = manager.get_document();
    if (current.has_value() &&
        current->active_sketch_feature_id.has_value()) {
      const auto feature_it = std::find_if(
          current->feature_history.begin(), current->feature_history.end(),
          [&](const polysmith::core::FeatureEntry& feature) {
            return feature.id == current->active_sketch_feature_id.value();
          });
      if (feature_it != current->feature_history.end() &&
          feature_it->sketch_parameters.has_value()) {
        const auto text_it = std::find_if(
            feature_it->sketch_parameters->texts.begin(),
            feature_it->sketch_parameters->texts.end(),
            [&](const polysmith::core::SketchText& entry) {
              return entry.id == text_id;
            });
        if (text_it != feature_it->sketch_parameters->texts.end()) {
          merged = *text_it;
        }
      }
    }
    if (merged.id.empty()) {
      throw std::runtime_error("Sketch text not found: " + text_id);
    }
    if (command.payload.contains("text") &&
        command.payload.at("text").is_string()) {
      merged.text = command.payload.at("text").get<std::string>();
    }
    if (command.payload.contains("font_path")) {
      merged.font_path =
          read_optional_string(command.payload, "font_path").value_or("");
    }
    if (command.payload.contains("height_mm") &&
        command.payload.at("height_mm").is_number()) {
      merged.height_mm = command.payload.at("height_mm").get<double>();
    }
    if (command.payload.contains("angle_deg") &&
        command.payload.at("angle_deg").is_number()) {
      merged.angle_deg = command.payload.at("angle_deg").get<double>();
    }
    if (command.payload.contains("anchor_x") &&
        command.payload.at("anchor_x").is_number()) {
      merged.anchor_x = command.payload.at("anchor_x").get<double>();
    }
    if (command.payload.contains("anchor_y") &&
        command.payload.at("anchor_y").is_number()) {
      merged.anchor_y = command.payload.at("anchor_y").get<double>();
    }
    if (command.payload.contains("h_align") &&
        command.payload.at("h_align").is_string()) {
      merged.h_align = command.payload.at("h_align").get<std::string>();
    }
    if (command.payload.contains("v_align") &&
        command.payload.at("v_align").is_string()) {
      merged.v_align = command.payload.at("v_align").get<std::string>();
    }
    if (command.payload.contains("char_spacing") &&
        command.payload.at("char_spacing").is_number()) {
      merged.char_spacing = command.payload.at("char_spacing").get<double>();
    }
    if (command.payload.contains("path_entity_id")) {
      merged.path_entity_id =
          read_optional_string(command.payload, "path_entity_id");
    }
    if (command.payload.contains("path_offset")) {
      merged.path_offset =
          command.payload.at("path_offset").is_number()
              ? std::optional<double>(
                    command.payload.at("path_offset").get<double>())
              : std::nullopt;
    }
    const auto document = manager.update_sketch_text(text_id, merged);
    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return true;
  }

  if (command.type == "delete_sketch_text") {
    const auto before_document = manager.get_document();
    const auto document = manager.delete_sketch_text(
        read_string(command.payload, "text_id"));
    if (emit_sketch_entities_changed(command.id, before_document, document)) {
      return true;
    }
    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return true;
  }

  return false;
}

}  // namespace polysmith
