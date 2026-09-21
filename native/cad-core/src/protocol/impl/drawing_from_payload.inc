// ── Drawing Deserialization ───────────────────────────────────────
//
// Inverses of the drawing `to_payload` functions above.  Loaded
// documents carry the "drawing" key; everything here mirrors the
// target-schema types in drawing_types.h.  Missing keys fall back to
// the struct defaults so documents saved before a field existed still
// load (the cam_from_payload convention).

namespace {

double drawing_read_number(const json& payload, const char* key,
                           double fallback) {
  if (!payload.contains(key) || payload.at(key).is_null()) {
    return fallback;
  }
  return payload.at(key).get<double>();
}

int drawing_read_int(const json& payload, const char* key, int fallback) {
  if (!payload.contains(key) || payload.at(key).is_null()) {
    return fallback;
  }
  return payload.at(key).get<int>();
}

bool drawing_read_bool(const json& payload, const char* key, bool fallback) {
  if (!payload.contains(key) || payload.at(key).is_null()) {
    return fallback;
  }
  return payload.at(key).get<bool>();
}

std::string drawing_read_string(const json& payload, const char* key,
                                const std::string& fallback) {
  if (!payload.contains(key) || payload.at(key).is_null()) {
    return fallback;
  }
  return payload.at(key).get<std::string>();
}

std::array<double, 3> drawing_read_array3(const json& payload, const char* key,
                                          std::array<double, 3> fallback) {
  if (!payload.contains(key) || !payload.at(key).is_array()) {
    return fallback;
  }
  const auto& arr = payload.at(key);
  if (arr.size() < 3) {
    return fallback;
  }
  return {arr.at(0).get<double>(), arr.at(1).get<double>(),
          arr.at(2).get<double>()};
}

std::array<double, 2> drawing_read_array2(const json& payload, const char* key,
                                          std::array<double, 2> fallback) {
  if (!payload.contains(key) || !payload.at(key).is_array()) {
    return fallback;
  }
  const auto& arr = payload.at(key);
  if (arr.size() < 2) {
    return fallback;
  }
  return {arr.at(0).get<double>(), arr.at(1).get<double>()};
}

std::vector<std::string> drawing_read_string_list(const json& payload,
                                                  const char* key) {
  std::vector<std::string> result;
  if (!payload.contains(key) || !payload.at(key).is_array()) {
    return result;
  }
  for (const auto& entry : payload.at(key)) {
    if (entry.is_string()) {
      result.push_back(entry.get<std::string>());
    }
  }
  return result;
}

}  // namespace

// ── DrawingViewFrame ──────────────────────────────────────────────

polysmith::core::DrawingViewFrame drawing_view_frame_from_payload(
    const json& payload) {
  polysmith::core::DrawingViewFrame frame;
  frame.origin = drawing_read_array3(payload, "origin", frame.origin);
  frame.normal = drawing_read_array3(payload, "normal", frame.normal);
  frame.x_direction =
      drawing_read_array3(payload, "x_direction", frame.x_direction);
  return frame;
}

// ── SourceEdgeWitness ─────────────────────────────────────────────

polysmith::core::SourceEdgeWitness source_edge_witness_from_payload(
    const json& payload) {
  polysmith::core::SourceEdgeWitness witness;
  witness.body_id = drawing_read_string(payload, "body_id", "");
  witness.src_edge_index = drawing_read_int(payload, "src_edge_index", -1);
  witness.start_point =
      drawing_read_array3(payload, "start_point", witness.start_point);
  witness.end_point = drawing_read_array3(payload, "end_point", witness.end_point);
  witness.length = drawing_read_number(payload, "length", 0.0);
  witness.tangent = drawing_read_array3(payload, "tangent", witness.tangent);
  witness.curve_kind = drawing_read_string(payload, "curve_kind", "line");
  if (payload.contains("center") && payload.at("center").is_array()) {
    witness.center = drawing_read_array3(payload, "center", {0.0, 0.0, 0.0});
  }
  if (payload.contains("axis") && payload.at("axis").is_array()) {
    witness.axis = drawing_read_array3(payload, "axis", {0.0, 0.0, 1.0});
  }
  if (payload.contains("radius") && !payload.at("radius").is_null()) {
    witness.radius = payload.at("radius").get<double>();
  }
  witness.param_range = drawing_read_array2(payload, "param_range", {0.0, 0.0});
  return witness;
}

// ── SectionDefinition ─────────────────────────────────────────────

polysmith::core::SectionDefinition section_definition_from_payload(
    const json& payload) {
  polysmith::core::SectionDefinition section;
  section.cutting_plane_point = drawing_read_array3(
      payload, "cutting_plane_point", section.cutting_plane_point);
  section.cutting_plane_normal = drawing_read_array3(
      payload, "cutting_plane_normal", section.cutting_plane_normal);
  section.cut_away = drawing_read_bool(payload, "cut_away", true);
  section.label = drawing_read_string(payload, "label", "A");
  section.hatch_angle_deg =
      drawing_read_number(payload, "hatch_angle_deg", 45.0);
  section.hatch_spacing_mm =
      drawing_read_number(payload, "hatch_spacing_mm", 3.0);
  if (payload.contains("construction_plane_id") &&
      payload.at("construction_plane_id").is_string()) {
    section.construction_plane_id =
        payload.at("construction_plane_id").get<std::string>();
  }
  return section;
}

// ── TitleBlock ────────────────────────────────────────────────────

polysmith::core::TitleBlock title_block_from_payload(const json& payload) {
  polysmith::core::TitleBlock title_block;
  title_block.legal_owner = drawing_read_string(payload, "legal_owner", "");
  title_block.identification =
      drawing_read_string(payload, "identification", "");
  title_block.date = drawing_read_string(payload, "date", "");
  title_block.title = drawing_read_string(payload, "title", "");
  title_block.approver = drawing_read_string(payload, "approver", "");
  title_block.creator = drawing_read_string(payload, "creator", "");
  title_block.document_type =
      drawing_read_string(payload, "document_type", "");
  if (payload.contains("revision_rows") &&
      payload.at("revision_rows").is_array()) {
    for (const auto& row : payload.at("revision_rows")) {
      if (!row.is_array() || row.size() < 5) {
        continue;
      }
      title_block.revision_rows.push_back(
          {row.at(0).get<std::string>(), row.at(1).get<std::string>(),
           row.at(2).get<std::string>(), row.at(3).get<std::string>(),
           row.at(4).get<std::string>()});
    }
  }
  return title_block;
}

// ── DrawingSheet ──────────────────────────────────────────────────

polysmith::core::DrawingSheet drawing_sheet_from_payload(
    const json& payload) {
  polysmith::core::DrawingSheet sheet;
  sheet.sheet_id = drawing_read_string(payload, "sheet_id", "");
  sheet.name = drawing_read_string(payload, "name", "");
  sheet.paper_size = drawing_read_string(payload, "paper_size", "A4");
  sheet.orientation = drawing_read_string(payload, "orientation", "landscape");
  sheet.projection_angle =
      drawing_read_string(payload, "projection_angle", "first_angle");
  sheet.view_ids = drawing_read_string_list(payload, "view_ids");
  if (payload.contains("title_block") && payload.at("title_block").is_object()) {
    sheet.title_block =
        title_block_from_payload(payload.at("title_block"));
  }
  return sheet;
}

// ── AnnotationExtension ───────────────────────────────────────────

polysmith::core::AnnotationExtension annotation_extension_from_payload(
    const json& payload) {
  polysmith::core::AnnotationExtension extension;
  extension.kind = drawing_read_string(payload, "kind", "");
  if (payload.contains("fields") && payload.at("fields").is_array()) {
    for (const auto& field : payload.at("fields")) {
      if (field.is_array() && field.size() >= 2 &&
          field.at(0).is_string() && field.at(1).is_string()) {
        extension.fields.push_back(
            {field.at(0).get<std::string>(), field.at(1).get<std::string>()});
      }
    }
  }
  return extension;
}

// ── Annotation ────────────────────────────────────────────────────

polysmith::core::Annotation annotation_from_payload(const json& payload) {
  polysmith::core::Annotation annotation;
  annotation.annotation_id =
      drawing_read_string(payload, "annotation_id", "");
  annotation.kind = drawing_read_string(payload, "kind", "linear");
  annotation.view_id = drawing_read_string(payload, "view_id", "");
  annotation.source_edge_id =
      drawing_read_string(payload, "source_edge_id", "");
  if (payload.contains("witness") && payload.at("witness").is_object()) {
    annotation.witness =
        source_edge_witness_from_payload(payload.at("witness"));
  }
  if (payload.contains("witness_2") && payload.at("witness_2").is_object()) {
    annotation.witness_2 =
        source_edge_witness_from_payload(payload.at("witness_2"));
  }
  if (payload.contains("attach_param") &&
      !payload.at("attach_param").is_null()) {
    annotation.attach_param = payload.at("attach_param").get<double>();
  }
  if (payload.contains("text_override") &&
      payload.at("text_override").is_string()) {
    annotation.text_override =
        payload.at("text_override").get<std::string>();
  }
  annotation.prefix = drawing_read_string(payload, "prefix", "");
  if (payload.contains("extensions") && payload.at("extensions").is_array()) {
    for (const auto& ext : payload.at("extensions")) {
      if (ext.is_object()) {
        annotation.extensions.push_back(
            annotation_extension_from_payload(ext));
      }
    }
  }
  annotation.dependency_broken =
      drawing_read_bool(payload, "dependency_broken", false);
  annotation.warning = drawing_read_string(payload, "warning", "");
  if (payload.contains("text_offset") && payload.at("text_offset").is_array()) {
    annotation.text_offset =
        drawing_read_array2(payload, "text_offset", {0.0, 0.0});
  }
  annotation.arrow_flip = drawing_read_bool(payload, "arrow_flip", false);
  return annotation;
}

// ── DrawingView ───────────────────────────────────────────────────

polysmith::core::DetailDefinition detail_definition_from_payload(
    const json& payload) {
  polysmith::core::DetailDefinition detail;
  detail.parent_view_id =
      drawing_read_string(payload, "parent_view_id", "");
  detail.center = drawing_read_array2(payload, "center", {0.0, 0.0});
  detail.radius = drawing_read_number(payload, "radius", 1.0);
  detail.label = drawing_read_string(payload, "label", "A");
  return detail;
}

polysmith::core::DrawingView drawing_view_from_payload(const json& payload) {
  polysmith::core::DrawingView view;
  view.view_id = drawing_read_string(payload, "view_id", "");
  view.kind = drawing_read_string(payload, "kind", "projection");
  view.standard_view = drawing_read_string(payload, "standard_view", "");
  if (payload.contains("custom_frame") &&
      payload.at("custom_frame").is_object()) {
    view.custom_frame =
        drawing_view_frame_from_payload(payload.at("custom_frame"));
  }
  view.source_body_ids = drawing_read_string_list(payload, "source_body_ids");
  view.scale = drawing_read_number(payload, "scale", 1.0);
  view.sheet_position =
      drawing_read_array2(payload, "sheet_position", {0.0, 0.0});
  view.show_hidden = drawing_read_bool(payload, "show_hidden", false);
  if (payload.contains("section") && payload.at("section").is_object()) {
    view.section = section_definition_from_payload(payload.at("section"));
  }
  if (payload.contains("detail") && payload.at("detail").is_object()) {
    view.detail = detail_definition_from_payload(payload.at("detail"));
  }
  if (payload.contains("broken_ref") && payload.at("broken_ref").is_string()) {
    view.broken_ref = payload.at("broken_ref").get<std::string>();
  }
  view.warning = drawing_read_string(payload, "warning", "");
  return view;
}

// ── SheetNote ──────────────────────────────────────────────────────

polysmith::core::SheetNote sheet_note_from_payload(const json& payload) {
  polysmith::core::SheetNote note;
  note.note_id = drawing_read_string(payload, "note_id", "");
  note.sheet_id = drawing_read_string(payload, "sheet_id", "");
  note.text = drawing_read_string(payload, "text", "");
  note.position = drawing_read_array2(payload, "position", {0.0, 0.0});
  note.height_mm = drawing_read_number(payload, "height_mm", 3.5);
  note.angle_deg = drawing_read_number(payload, "angle_deg", 0.0);
  note.h_align = drawing_read_string(payload, "h_align", "center");
  return note;
}

// ── Drawing ───────────────────────────────────────────────────────

polysmith::core::Drawing drawing_from_payload(const json& payload) {
  polysmith::core::Drawing drawing;
  drawing.drawing_id = drawing_read_string(payload, "drawing_id", "");
  drawing.name = drawing_read_string(payload, "name", "");
  if (payload.contains("sheets") && payload.at("sheets").is_array()) {
    for (const auto& sheet : payload.at("sheets")) {
      drawing.sheets.push_back(drawing_sheet_from_payload(sheet));
    }
  }
  if (payload.contains("views") && payload.at("views").is_array()) {
    for (const auto& view : payload.at("views")) {
      drawing.views.push_back(drawing_view_from_payload(view));
    }
  }
  if (payload.contains("annotations") &&
      payload.at("annotations").is_array()) {
    for (const auto& annotation : payload.at("annotations")) {
      drawing.annotations.push_back(annotation_from_payload(annotation));
    }
  }
  if (payload.contains("notes") && payload.at("notes").is_array()) {
    for (const auto& note : payload.at("notes")) {
      drawing.notes.push_back(sheet_note_from_payload(note));
    }
  }
  return drawing;
}

// ── DrawingDocumentData ───────────────────────────────────────────

polysmith::core::DrawingDocumentData drawing_document_data_from_payload(
    const json& payload) {
  polysmith::core::DrawingDocumentData drawing;
  if (payload.contains("drawings") && payload.at("drawings").is_array()) {
    for (const auto& d : payload.at("drawings")) {
      drawing.drawings.push_back(drawing_from_payload(d));
    }
  }
  if (payload.contains("active_drawing_id") &&
      payload.at("active_drawing_id").is_string()) {
    drawing.active_drawing_id =
        payload.at("active_drawing_id").get<std::string>();
  }
  if (payload.contains("selected_view_id") &&
      payload.at("selected_view_id").is_string()) {
    drawing.selected_view_id =
        payload.at("selected_view_id").get<std::string>();
  }
  if (payload.contains("selected_annotation_id") &&
      payload.at("selected_annotation_id").is_string()) {
    drawing.selected_annotation_id =
        payload.at("selected_annotation_id").get<std::string>();
  }
  drawing.decimal_separator =
      drawing_read_string(payload, "decimal_separator", ".");
  // Legacy migration: the field was never user-settable, so every
  // stored "," is the pre-dot DEFAULT, not a user choice — map it.
  if (drawing.decimal_separator == ",") {
    drawing.decimal_separator = ".";
  }
  return drawing;
}
