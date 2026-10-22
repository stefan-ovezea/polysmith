#include "app/drawing_command_handlers.h"

#include <array>
#include <optional>
#include <string>
#include <vector>

#include "app/command_context.h"
#include "core/document/document.h"
#include "core/drawing/drawing_annotation_geometry.h"
#include "core/drawing/drawing_dimension_geometry.h"
#include "core/drawing/drawing_resolution.h"
#include "core/drawing/drawing_runtime.h"
#include "core/drawing/drawing_sheet.h"
#include "core/drawing/drawing_types.h"
#include "core/viewport/viewport.h"

namespace polysmith {

namespace {

using polysmith::protocol::json;

json serialize_preview_curve(const polysmith::core::SheetPrimitive& p) {
  json curve = {
      {"kind", p.kind},
      {"line_class", p.line_class},
      {"curve_class", p.purpose},
      {"purpose", p.purpose},
      {"width_mm", p.style.width_mm},
      {"p0", json::array({p.p0[0], p.p0[1]})},
      {"p1", json::array({p.p1[0], p.p1[1]})},
      {"start_angle", p.start_angle},
      {"end_angle", p.end_angle},
  };
  if (p.center.has_value()) {
    curve["center"] = json::array({p.center.value()[0], p.center.value()[1]});
  }
  if (p.radius.has_value()) {
    curve["radius"] = p.radius.value();
  }
  if (!p.points.empty()) {
    json points = json::array();
    for (const auto& point : p.points) {
      points.push_back(json::array({point[0], point[1]}));
    }
    curve["points"] = std::move(points);
  }
  return curve;
}

json serialize_preview_text(const polysmith::core::SheetText& t) {
  return json{
      {"text", t.text},
      {"position", json::array({t.position[0], t.position[1]})},
      {"height_mm", t.height_mm},
      {"angle_deg", t.angle_deg},
      {"h_align", t.h_align},
      {"purpose", t.purpose},
      {"stale", t.stale},
  };
}

std::optional<std::array<double, 2>> read_optional_pick(
    const json& payload) {
  std::optional<std::array<double, 2>> pick;
  if (payload.contains("pick_2")) {
    const auto pick_json = payload.at("pick_2");
    if (pick_json.is_array() && pick_json.size() >= 2 &&
        pick_json.at(0).is_number() && pick_json.at(1).is_number()) {
      pick = std::array<double, 2>{pick_json.at(0).get<double>(),
                                   pick_json.at(1).get<double>()};
    }
  }
  return pick;
}

std::optional<std::array<double, 2>> read_optional_position(
    const json& payload, const char* key) {
  std::optional<std::array<double, 2>> result;
  if (payload.contains(key)) {
    const auto value = payload.at(key);
    if (value.is_array() && value.size() >= 2 && value.at(0).is_number() &&
        value.at(1).is_number()) {
      result = std::array<double, 2>{value.at(0).get<double>(),
                                     value.at(1).get<double>()};
    }
  }
  return result;
}

}  // namespace

bool handle_drawing_command(const polysmith::protocol::CommandMessage& command,
                            polysmith::core::DocumentManager& manager) {
  if (command.type == "drawing_create") {
    const auto drawing =
        polysmith::protocol::drawing_from_payload(command.payload);
    const auto document = manager.drawing_create(drawing);
    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return true;
  }

  if (command.type == "drawing_delete") {
    const auto drawing_id = read_string(command.payload, "drawing_id");
    const auto document = manager.drawing_delete(drawing_id);
    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return true;
  }

  if (command.type == "drawing_set_active") {
    const auto drawing_id = read_string(command.payload, "drawing_id");
    const auto document = manager.drawing_set_active(drawing_id);
    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return true;
  }

  if (command.type == "drawing_sheet_create") {
    const auto drawing_id = read_string(command.payload, "drawing_id");
    const auto sheet = polysmith::protocol::drawing_sheet_from_payload(
        command.payload.at("sheet"));
    const auto document = manager.drawing_sheet_create(drawing_id, sheet);
    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return true;
  }

  if (command.type == "drawing_sheet_delete") {
    const auto drawing_id = read_string(command.payload, "drawing_id");
    const auto sheet_id = read_string(command.payload, "sheet_id");
    const auto document = manager.drawing_sheet_delete(drawing_id, sheet_id);
    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return true;
  }

  if (command.type == "drawing_sheet_update") {
    const auto drawing_id = read_string(command.payload, "drawing_id");
    const auto sheet_id = read_string(command.payload, "sheet_id");
    const auto paper_size = read_string(command.payload, "paper_size");
    const auto orientation = read_string(command.payload, "orientation");
    const auto projection_angle =
        read_string(command.payload, "projection_angle");
    const auto name = read_string(command.payload, "name");
    const auto document = manager.drawing_sheet_update(
        drawing_id, sheet_id, paper_size, orientation, projection_angle, name);
    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return true;
  }

  if (command.type == "drawing_view_create") {
    const auto drawing_id = read_string(command.payload, "drawing_id");
    const auto sheet_id = read_string(command.payload, "sheet_id");
    const auto view = polysmith::protocol::drawing_view_from_payload(
        command.payload.at("view"));
    const auto document =
        manager.drawing_view_create(drawing_id, sheet_id, view);
    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return true;
  }

  if (command.type == "drawing_view_update") {
    const auto drawing_id = read_string(command.payload, "drawing_id");
    const auto view = polysmith::protocol::drawing_view_from_payload(
        command.payload.at("view"));
    const auto document = manager.drawing_view_update(drawing_id, view);
    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return true;
  }

  if (command.type == "drawing_view_delete") {
    const auto drawing_id = read_string(command.payload, "drawing_id");
    const auto view_id = read_string(command.payload, "view_id");
    const auto document = manager.drawing_view_delete(drawing_id, view_id);
    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return true;
  }

  if (command.type == "drawing_view_move") {
    const auto drawing_id = read_string(command.payload, "drawing_id");
    const auto view_id = read_string(command.payload, "view_id");
    const auto position = command.payload.at("sheet_position");
    if (!position.is_array() || position.size() < 2 ||
        !position.at(0).is_number() || !position.at(1).is_number()) {
      polysmith::protocol::write_message(polysmith::protocol::make_error_event(
          command.id, "INVALID_PAYLOAD",
          "drawing_view_move needs sheet_position [x, y]"));
      return true;
    }
    const std::array<double, 2> sheet_position = {
        position.at(0).get<double>(), position.at(1).get<double>()};
    const auto document =
        manager.drawing_view_move(drawing_id, view_id, sheet_position);
    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return true;
  }

  if (command.type == "drawing_view_preview") {
    const auto drawing_id = read_string(command.payload, "drawing_id");
    const auto sheet_id = read_string(command.payload, "sheet_id");
    const auto def = polysmith::protocol::drawing_view_from_payload(
        command.payload.at("view"));

    const auto document = manager.get_document();
    if (!document.has_value()) {
      polysmith::protocol::write_message(polysmith::protocol::make_error_event(
          command.id, "NO_ACTIVE_DOCUMENT", "No active document"));
      return true;
    }
    const auto preview = polysmith::core::preview_view_geometry(
        document.value(), drawing_id, def);
    if (!preview.has_value()) {
      polysmith::protocol::write_message(polysmith::protocol::make_error_event(
          command.id, "UNKNOWN_DRAWING", "The drawing was not found."));
      return true;
    }

    json curves = json::array();
    for (const auto& p : preview->primitives) {
      polysmith::core::ViewportDrawingCurve curve;
      curve.kind = p.kind;
      curve.line_class = p.line_class;
      curve.curve_class = p.purpose;
      curve.purpose = p.purpose;
      curve.width_mm = p.style.width_mm;
      curve.p0 = p.p0;
      curve.p1 = p.p1;
      curve.center = p.center;
      curve.radius = p.radius;
      curve.major_dir = p.major_dir;
      curve.major_radius = p.major_radius;
      curve.minor_radius = p.minor_radius;
      curve.start_angle = p.start_angle;
      curve.end_angle = p.end_angle;
      curve.points = p.points;
      curves.push_back(polysmith::protocol::to_payload(curve));
    }
    json texts = json::array();
    for (const auto& t : preview->texts) {
      polysmith::core::ViewportDrawingText text;
      text.text = t.text;
      text.position = t.position;
      text.height_mm = t.height_mm;
      text.angle_deg = t.angle_deg;
      text.h_align = t.h_align;
      text.purpose = t.purpose;
      text.stale = t.stale;
      texts.push_back(polysmith::protocol::to_payload(text));
    }

    json reply = {
        {"drawing_id", drawing_id},
        {"sheet_id", sheet_id},
        {"curves", curves},
        {"texts", texts},
        {"view",
         {{"view_id", ""},
          {"label",
           !def.standard_view.empty() ? def.standard_view : def.kind},
          {"scale", def.scale},
          {"origin", json::array(
                         {def.sheet_position[0], def.sheet_position[1]})},
          {"min", preview->min.has_value()
                      ? json::array({preview->min.value()[0],
                                     preview->min.value()[1]})
                      : json::array({0.0, 0.0})},
          {"max", preview->max.has_value()
                      ? json::array({preview->max.value()[0],
                                     preview->max.value()[1]})
                      : json::array({0.0, 0.0})},
          {"stale", false},
          {"warning", preview->warning}}},
    };
    polysmith::protocol::write_message(
        polysmith::protocol::make_drawing_view_preview_event(command.id, reply));
    return true;
  }

  if (command.type == "drawing_section_update") {
    const auto drawing_id = read_string(command.payload, "drawing_id");
    const auto view_id = read_string(command.payload, "view_id");
    const auto section = polysmith::protocol::section_definition_from_payload(
        command.payload.at("section"));
    const auto document =
        manager.drawing_section_update(drawing_id, view_id, section);
    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return true;
  }

  if (command.type == "drawing_title_block_update") {
    const auto drawing_id = read_string(command.payload, "drawing_id");
    const auto sheet_id = read_string(command.payload, "sheet_id");
    const auto title_block = polysmith::protocol::title_block_from_payload(
        command.payload.at("title_block"));
    const auto document =
        manager.drawing_title_block_update(drawing_id, sheet_id, title_block);
    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return true;
  }

  if (command.type == "drawing_export") {
    const auto drawing_id = read_string(command.payload, "drawing_id");
    const auto sheet_id = read_string(command.payload, "sheet_id");
    const auto format = read_string(command.payload, "format");
    const auto file_path = read_string(command.payload, "file_path");
    const auto dxf_mode = command.payload.contains("dxf_mode")
                              ? read_string(command.payload, "dxf_mode")
                              : std::string("geometry");
    const auto export_result = manager.export_drawing_sheet(
        drawing_id, sheet_id, format, file_path, dxf_mode);
    polysmith::protocol::write_message(
        polysmith::protocol::make_document_exported_event(
            command.id, json{{"file_path", export_result.file_path},
                             {"format", export_result.format},
                             {"exported_feature_count",
                              export_result.exported_feature_count}}));
    return true;
  }

  if (command.type == "drawing_dimension_create") {
    const auto drawing_id = read_string(command.payload, "drawing_id");
    const auto view_id = read_string(command.payload, "view_id");
    const auto dim_type = read_string(command.payload, "dim_type");
    const auto pick_json = command.payload.at("pick");
    if (!pick_json.is_array() || pick_json.size() < 2 ||
        !pick_json.at(0).is_number() || !pick_json.at(1).is_number()) {
      polysmith::protocol::write_message(polysmith::protocol::make_error_event(
          command.id, "INVALID_PAYLOAD",
          "drawing_dimension_create needs pick [x, y] in sheet-mm"));
      return true;
    }
    const std::array<double, 2> pick = {pick_json.at(0).get<double>(),
                                        pick_json.at(1).get<double>()};
    const auto pick_2 = read_optional_pick(command.payload);
    std::optional<std::string> annotation_id;
    if (command.payload.contains("annotation_id") &&
        command.payload.at("annotation_id").is_string() &&
        !command.payload.at("annotation_id").get<std::string>().empty()) {
      annotation_id = command.payload.at("annotation_id").get<std::string>();
    }
    const auto document = manager.drawing_dimension_create(
        drawing_id, view_id, dim_type, pick, pick_2, annotation_id);
    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return true;
  }

  if (command.type == "drawing_dimension_update") {
    const auto drawing_id = read_string(command.payload, "drawing_id");
    const auto annotation_id = read_string(command.payload, "annotation_id");
    std::optional<std::string> text_override;
    if (command.payload.contains("text_override") &&
        command.payload.at("text_override").is_string()) {
      text_override = command.payload.at("text_override").get<std::string>();
    }
    std::optional<std::string> prefix;
    if (command.payload.contains("prefix") &&
        command.payload.at("prefix").is_string()) {
      prefix = command.payload.at("prefix").get<std::string>();
    }
    std::optional<bool> arrow_flip;
    if (command.payload.contains("arrow_flip") &&
        command.payload.at("arrow_flip").is_boolean()) {
      arrow_flip = command.payload.at("arrow_flip").get<bool>();
    }
    const auto text_offset =
        read_optional_position(command.payload, "text_offset");
    const auto document = manager.drawing_dimension_update(
        drawing_id, annotation_id, text_override, prefix, arrow_flip,
        text_offset);
    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return true;
  }

  if (command.type == "drawing_dimension_delete") {
    const auto drawing_id = read_string(command.payload, "drawing_id");
    const auto annotation_id = read_string(command.payload, "annotation_id");
    const auto document =
        manager.drawing_dimension_delete(drawing_id, annotation_id);
    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return true;
  }

  if (command.type == "drawing_dimension_preview") {
    const auto drawing_id = read_string(command.payload, "drawing_id");
    const auto view_id = read_string(command.payload, "view_id");
    const auto dim_type = read_string(command.payload, "dim_type");
    const auto pick_json = command.payload.at("pick");
    if (!pick_json.is_array() || pick_json.size() < 2 ||
        !pick_json.at(0).is_number() || !pick_json.at(1).is_number()) {
      polysmith::protocol::write_message(polysmith::protocol::make_error_event(
          command.id, "INVALID_PAYLOAD",
          "drawing_dimension_preview needs pick [x, y] in sheet-mm"));
      return true;
    }
    const std::array<double, 2> pick = {pick_json.at(0).get<double>(),
                                        pick_json.at(1).get<double>()};
    const auto pick_2 = read_optional_pick(command.payload);

    const auto document = manager.get_document();
    if (!document.has_value()) {
      polysmith::protocol::write_message(polysmith::protocol::make_error_event(
          command.id, "NO_ACTIVE_DOCUMENT", "No active document"));
      return true;
    }
    const polysmith::core::Drawing* drawing = nullptr;
    for (const auto& d : document->drawing.drawings) {
      if (d.drawing_id == drawing_id) {
        drawing = &d;
        break;
      }
    }
    const polysmith::core::DrawingView* view = nullptr;
    if (drawing != nullptr) {
      for (const auto& v : drawing->views) {
        if (v.view_id == view_id) {
          view = &v;
          break;
        }
      }
    }
    const polysmith::core::ProjectionResult* projection =
        view != nullptr ? polysmith::core::drawing_runtime::cached_projection(
                              document.value(), view_id)
                        : nullptr;
    if (drawing == nullptr || view == nullptr || projection == nullptr) {
      polysmith::protocol::write_message(polysmith::protocol::make_error_event(
          command.id, "UNKNOWN_VIEW",
          "The view or its projection was not found."));
      return true;
    }

    polysmith::core::Annotation synthetic;
    synthetic.kind = dim_type;
    synthetic.view_id = view_id;
    if (dim_type == "diameter") {
      synthetic.prefix = "\xE2\x8C\x80";
    } else if (dim_type == "radius") {
      synthetic.prefix = "R";
    }
    std::string error;
    const auto measured = polysmith::core::measure_from_picks(
        *projection, *view, dim_type, pick, pick_2,
        /*pick_tolerance_sheetmm=*/5.0, document->drawing.decimal_separator,
        &synthetic, &error);
    json reply = {
        {"drawing_id", drawing_id},
        {"view_id", view_id},
        {"dim_type", dim_type},
        {"pick", json::array({pick[0], pick[1]})},
    };
    if (pick_2.has_value()) {
      reply["pick_2"] =
          json::array({pick_2.value()[0], pick_2.value()[1]});
    }
    if (!measured.has_value()) {
      reply["error"] = error;
      polysmith::protocol::write_message(
          polysmith::protocol::make_drawing_dimension_preview_event(
              command.id, reply));
      return true;
    }
    const auto graphics = polysmith::core::build_dimension_graphics(
        measured.value(), synthetic, *view, /*stale=*/false);
    json curves = json::array();
    for (const auto& p : graphics.primitives) {
      curves.push_back(serialize_preview_curve(p));
    }
    reply["curves"] = std::move(curves);
    if (graphics.text.has_value()) {
      reply["text"] = serialize_preview_text(graphics.text.value());
    }
    reply["value"] = measured.value().value;
    reply["text_value"] = measured.value().text;
    polysmith::protocol::write_message(
        polysmith::protocol::make_drawing_dimension_preview_event(
            command.id, reply));
    return true;
  }

  if (command.type == "drawing_note_create") {
    const auto drawing_id = read_string(command.payload, "drawing_id");
    const auto sheet_id = read_string(command.payload, "sheet_id");
    const auto text = read_string(command.payload, "text");
    const auto position_json = command.payload.at("position");
    if (!position_json.is_array() || position_json.size() < 2 ||
        !position_json.at(0).is_number() || !position_json.at(1).is_number()) {
      polysmith::protocol::write_message(polysmith::protocol::make_error_event(
          command.id, "INVALID_PAYLOAD",
          "drawing_note_create needs position [x, y] in sheet-mm"));
      return true;
    }
    const std::array<double, 2> position = {
        position_json.at(0).get<double>(), position_json.at(1).get<double>()};
    std::optional<double> height_mm;
    if (command.payload.contains("height_mm") &&
        command.payload.at("height_mm").is_number()) {
      height_mm = command.payload.at("height_mm").get<double>();
    }
    std::optional<double> angle_deg;
    if (command.payload.contains("angle_deg") &&
        command.payload.at("angle_deg").is_number()) {
      angle_deg = command.payload.at("angle_deg").get<double>();
    }
    std::optional<std::string> h_align;
    if (command.payload.contains("h_align") &&
        command.payload.at("h_align").is_string()) {
      h_align = command.payload.at("h_align").get<std::string>();
    }
    const auto document = manager.drawing_note_create(
        drawing_id, sheet_id, text, position, height_mm, angle_deg, h_align);
    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return true;
  }

  if (command.type == "drawing_note_update") {
    const auto drawing_id = read_string(command.payload, "drawing_id");
    const auto note_id = read_string(command.payload, "note_id");
    std::optional<std::string> text;
    if (command.payload.contains("text") &&
        command.payload.at("text").is_string()) {
      text = command.payload.at("text").get<std::string>();
    }
    const auto position = read_optional_position(command.payload, "position");
    std::optional<double> height_mm;
    if (command.payload.contains("height_mm") &&
        command.payload.at("height_mm").is_number()) {
      height_mm = command.payload.at("height_mm").get<double>();
    }
    std::optional<double> angle_deg;
    if (command.payload.contains("angle_deg") &&
        command.payload.at("angle_deg").is_number()) {
      angle_deg = command.payload.at("angle_deg").get<double>();
    }
    std::optional<std::string> h_align;
    if (command.payload.contains("h_align") &&
        command.payload.at("h_align").is_string()) {
      h_align = command.payload.at("h_align").get<std::string>();
    }
    const auto document = manager.drawing_note_update(
        drawing_id, note_id, text, position, height_mm, angle_deg, h_align);
    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return true;
  }

  if (command.type == "drawing_note_delete") {
    const auto drawing_id = read_string(command.payload, "drawing_id");
    const auto note_id = read_string(command.payload, "note_id");
    const auto document = manager.drawing_note_delete(drawing_id, note_id);
    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return true;
  }

  if (command.type == "drawing_annotation_create") {
    const auto drawing_id = read_string(command.payload, "drawing_id");
    const auto view_id = read_string(command.payload, "view_id");
    const auto kind = read_string(command.payload, "kind");
    const auto pick_json = command.payload.at("pick");
    if (!pick_json.is_array() || pick_json.size() < 2 ||
        !pick_json.at(0).is_number() || !pick_json.at(1).is_number()) {
      polysmith::protocol::write_message(polysmith::protocol::make_error_event(
          command.id, "INVALID_PAYLOAD",
          "drawing_annotation_create needs pick [x, y] in sheet-mm"));
      return true;
    }
    const std::array<double, 2> pick = {pick_json.at(0).get<double>(),
                                        pick_json.at(1).get<double>()};
    const auto pick_2 = read_optional_pick(command.payload);
    std::optional<std::string> text_override;
    if (command.payload.contains("text_override") &&
        command.payload.at("text_override").is_string()) {
      text_override = command.payload.at("text_override").get<std::string>();
    }
    std::optional<std::string> prefix;
    if (command.payload.contains("prefix") &&
        command.payload.at("prefix").is_string()) {
      prefix = command.payload.at("prefix").get<std::string>();
    }
    std::vector<polysmith::core::AnnotationExtension> extensions;
    if (command.payload.contains("extensions") &&
        command.payload.at("extensions").is_array()) {
      for (const auto& ext : command.payload.at("extensions")) {
        if (ext.is_object()) {
          extensions.push_back(
              polysmith::protocol::annotation_extension_from_payload(ext));
        }
      }
    }
    const auto placement =
        read_optional_position(command.payload, "placement");
    const auto document = manager.drawing_annotation_create(
        drawing_id, view_id, kind, pick, pick_2, text_override, prefix,
        extensions, placement);
    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return true;
  }

  if (command.type == "drawing_annotation_update") {
    const auto drawing_id = read_string(command.payload, "drawing_id");
    const auto annotation_id = read_string(command.payload, "annotation_id");
    std::optional<std::string> text_override;
    if (command.payload.contains("text_override") &&
        command.payload.at("text_override").is_string()) {
      text_override = command.payload.at("text_override").get<std::string>();
    }
    std::optional<std::string> prefix;
    if (command.payload.contains("prefix") &&
        command.payload.at("prefix").is_string()) {
      prefix = command.payload.at("prefix").get<std::string>();
    }
    std::optional<bool> arrow_flip;
    if (command.payload.contains("arrow_flip") &&
        command.payload.at("arrow_flip").is_boolean()) {
      arrow_flip = command.payload.at("arrow_flip").get<bool>();
    }
    const auto text_offset =
        read_optional_position(command.payload, "text_offset");
    std::optional<std::vector<polysmith::core::AnnotationExtension>> extensions;
    if (command.payload.contains("extensions") &&
        command.payload.at("extensions").is_array()) {
      std::vector<polysmith::core::AnnotationExtension> parsed;
      for (const auto& ext : command.payload.at("extensions")) {
        if (ext.is_object()) {
          parsed.push_back(
              polysmith::protocol::annotation_extension_from_payload(ext));
        }
      }
      extensions = std::move(parsed);
    }
    std::optional<double> attach_param;
    if (command.payload.contains("attach_param") &&
        command.payload.at("attach_param").is_number()) {
      attach_param = command.payload.at("attach_param").get<double>();
    }
    const auto document = manager.drawing_annotation_update(
        drawing_id, annotation_id, text_override, prefix, arrow_flip,
        text_offset, extensions, attach_param);
    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return true;
  }

  if (command.type == "drawing_annotation_delete") {
    const auto drawing_id = read_string(command.payload, "drawing_id");
    const auto annotation_id = read_string(command.payload, "annotation_id");
    const auto document =
        manager.drawing_annotation_delete(drawing_id, annotation_id);
    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return true;
  }

  if (command.type == "drawing_annotation_preview") {
    const auto drawing_id = read_string(command.payload, "drawing_id");
    const auto view_id = read_string(command.payload, "view_id");
    const auto kind = read_string(command.payload, "kind");
    const auto pick_json = command.payload.at("pick");
    if (!pick_json.is_array() || pick_json.size() < 2 ||
        !pick_json.at(0).is_number() || !pick_json.at(1).is_number()) {
      polysmith::protocol::write_message(polysmith::protocol::make_error_event(
          command.id, "INVALID_PAYLOAD",
          "drawing_annotation_preview needs pick [x, y] in sheet-mm"));
      return true;
    }
    const std::array<double, 2> pick = {pick_json.at(0).get<double>(),
                                        pick_json.at(1).get<double>()};
    const auto pick_2 = read_optional_pick(command.payload);
    std::optional<std::string> text_override;
    if (command.payload.contains("text_override") &&
        command.payload.at("text_override").is_string()) {
      text_override = command.payload.at("text_override").get<std::string>();
    }
    std::optional<std::string> prefix;
    if (command.payload.contains("prefix") &&
        command.payload.at("prefix").is_string()) {
      prefix = command.payload.at("prefix").get<std::string>();
    }

    const auto document = manager.get_document();
    if (!document.has_value()) {
      polysmith::protocol::write_message(polysmith::protocol::make_error_event(
          command.id, "NO_ACTIVE_DOCUMENT", "No active document"));
      return true;
    }
    const polysmith::core::Drawing* drawing = nullptr;
    for (const auto& d : document->drawing.drawings) {
      if (d.drawing_id == drawing_id) {
        drawing = &d;
        break;
      }
    }
    const polysmith::core::DrawingView* view = nullptr;
    if (drawing != nullptr) {
      for (const auto& v : drawing->views) {
        if (v.view_id == view_id) {
          view = &v;
          break;
        }
      }
    }
    const polysmith::core::ProjectionResult* projection =
        view != nullptr ? polysmith::core::drawing_runtime::cached_projection(
                              document.value(), view_id)
                        : nullptr;
    if (drawing == nullptr || view == nullptr || projection == nullptr) {
      polysmith::protocol::write_message(polysmith::protocol::make_error_event(
          command.id, "UNKNOWN_VIEW",
          "The view or its projection was not found."));
      return true;
    }

    polysmith::core::Annotation synthetic;
    synthetic.kind = kind;
    synthetic.view_id = view_id;
    synthetic.text_override = text_override;
    synthetic.prefix = prefix.value_or("");
    std::string error;
    const auto resolved = polysmith::core::resolve_annotation_picks(
        *projection, *view, kind, pick, pick_2,
        /*pick_tolerance_sheetmm=*/5.0, text_override, prefix.value_or(""),
        &error);
    json reply = {
        {"drawing_id", drawing_id},
        {"view_id", view_id},
        {"kind", kind},
        {"pick", json::array({pick[0], pick[1]})},
    };
    if (pick_2.has_value()) {
      reply["pick_2"] =
          json::array({pick_2.value()[0], pick_2.value()[1]});
    }
    if (!resolved.has_value()) {
      reply["error"] = error;
      polysmith::protocol::write_message(
          polysmith::protocol::make_drawing_annotation_preview_event(
              command.id, reply));
      return true;
    }
    const auto graphics = polysmith::core::build_annotation_graphics(
        resolved.value(), synthetic, *view, /*stale=*/false);
    json curves = json::array();
    for (const auto& p : graphics.primitives) {
      curves.push_back(serialize_preview_curve(p));
    }
    reply["curves"] = std::move(curves);
    if (graphics.text.has_value()) {
      reply["text"] = serialize_preview_text(graphics.text.value());
    }
    reply["text_value"] = resolved.value().text;
    polysmith::protocol::write_message(
        polysmith::protocol::make_drawing_annotation_preview_event(
            command.id, reply));
    return true;
  }

  return false;
}

}  // namespace polysmith
