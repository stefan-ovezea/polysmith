// ── Drawing Serialization ─────────────────────────────────────────
//
// `to_payload` for every drawing_types.h struct that is PERSISTED.
// ProjectionResult / ProjectedEdgeRecord / HatchRegion are runtime-
// only and deliberately have no overload here — generated projections
// must never enter a saved document (the toolpath contract).

namespace {

json drawing_array2(const std::array<double, 2>& a) {
  return json::array({a[0], a[1]});
}

}  // namespace

// ── DrawingViewFrame ──────────────────────────────────────────────

json to_payload(const polysmith::core::DrawingViewFrame& frame) {
  json j;
  j["origin"] = array3(frame.origin);
  j["normal"] = array3(frame.normal);
  j["x_direction"] = array3(frame.x_direction);
  return j;
}

// ── SourceEdgeWitness ─────────────────────────────────────────────

json to_payload(const polysmith::core::SourceEdgeWitness& witness) {
  json j;
  j["body_id"] = witness.body_id;
  j["src_edge_index"] = witness.src_edge_index;
  j["start_point"] = array3(witness.start_point);
  j["end_point"] = array3(witness.end_point);
  j["length"] = witness.length;
  j["tangent"] = array3(witness.tangent);
  j["curve_kind"] = witness.curve_kind;
  // Circle witness — emitted only when present (full-circle rims and
  // arcs; lines/ellipses/splines leave these unset).
  if (witness.center.has_value()) j["center"] = array3(witness.center.value());
  if (witness.axis.has_value()) j["axis"] = array3(witness.axis.value());
  if (witness.radius.has_value()) j["radius"] = witness.radius.value();
  j["param_range"] = drawing_array2(witness.param_range);
  return j;
}

// ── SectionDefinition ─────────────────────────────────────────────

json to_payload(const polysmith::core::SectionDefinition& section) {
  json j;
  j["cutting_plane_point"] = array3(section.cutting_plane_point);
  j["cutting_plane_normal"] = array3(section.cutting_plane_normal);
  j["cut_away"] = section.cut_away;
  j["label"] = section.label;
  j["hatch_angle_deg"] = section.hatch_angle_deg;
  j["hatch_spacing_mm"] = section.hatch_spacing_mm;
  if (section.construction_plane_id.has_value()) {
    j["construction_plane_id"] = section.construction_plane_id.value();
  }
  return j;
}

// ── TitleBlock ────────────────────────────────────────────────────

json to_payload(const polysmith::core::TitleBlock& title_block) {
  json j{
      {"legal_owner", title_block.legal_owner},
      {"identification", title_block.identification},
      {"date", title_block.date},
      {"title", title_block.title},
      {"approver", title_block.approver},
      {"creator", title_block.creator},
      {"document_type", title_block.document_type},
  };
  json rows = json::array();
  for (const auto& row : title_block.revision_rows) {
    rows.push_back({row[0], row[1], row[2], row[3], row[4]});
  }
  j["revision_rows"] = rows;
  return j;
}

// ── DrawingSheet ──────────────────────────────────────────────────

json to_payload(const polysmith::core::DrawingSheet& sheet) {
  return {
      {"sheet_id", sheet.sheet_id},
      {"name", sheet.name},
      {"paper_size", sheet.paper_size},
      {"orientation", sheet.orientation},
      {"projection_angle", sheet.projection_angle},
      {"view_ids", sheet.view_ids},
      {"title_block", to_payload(sheet.title_block)},
  };
}

// ── AnnotationExtension ───────────────────────────────────────────

json to_payload(const polysmith::core::AnnotationExtension& extension) {
  json j;
  j["kind"] = extension.kind;
  json fields = json::array();
  for (const auto& field : extension.fields) {
    fields.push_back({field[0], field[1]});
  }
  j["fields"] = fields;
  return j;
}

// ── Annotation ────────────────────────────────────────────────────

json to_payload(const polysmith::core::Annotation& annotation) {
  json j;
  j["annotation_id"] = annotation.annotation_id;
  j["kind"] = annotation.kind;
  j["view_id"] = annotation.view_id;
  j["source_edge_id"] = annotation.source_edge_id;
  j["witness"] = to_payload(annotation.witness);
  if (annotation.witness_2.has_value()) {
    j["witness_2"] = to_payload(annotation.witness_2.value());
  }
  if (annotation.attach_param.has_value()) {
    j["attach_param"] = annotation.attach_param.value();
  }
  if (annotation.text_override.has_value()) {
    j["text_override"] = annotation.text_override.value();
  }
  j["prefix"] = annotation.prefix;
  json extensions = json::array();
  for (const auto& ext : annotation.extensions) {
    extensions.push_back(to_payload(ext));
  }
  j["extensions"] = extensions;
  j["dependency_broken"] = annotation.dependency_broken;
  j["warning"] = annotation.warning;
  if (annotation.text_offset.has_value()) {
    j["text_offset"] = drawing_array2(annotation.text_offset.value());
  }
  j["arrow_flip"] = annotation.arrow_flip;
  return j;
}

// ── SheetNote ──────────────────────────────────────────────────────

json to_payload(const polysmith::core::SheetNote& note) {
  json j;
  j["note_id"] = note.note_id;
  j["sheet_id"] = note.sheet_id;
  j["text"] = note.text;
  j["position"] = drawing_array2(note.position);
  j["height_mm"] = note.height_mm;
  j["angle_deg"] = note.angle_deg;
  j["h_align"] = note.h_align;
  return j;
}

// ── DrawingView ───────────────────────────────────────────────────

json to_payload(const polysmith::core::DetailDefinition& detail) {
  json j;
  j["parent_view_id"] = detail.parent_view_id;
  j["center"] = drawing_array2(detail.center);
  j["radius"] = detail.radius;
  j["label"] = detail.label;
  return j;
}

json to_payload(const polysmith::core::DrawingView& view) {
  json j;
  j["view_id"] = view.view_id;
  j["kind"] = view.kind;
  j["standard_view"] = view.standard_view;
  if (view.custom_frame.has_value()) {
    j["custom_frame"] = to_payload(view.custom_frame.value());
  }
  j["source_body_ids"] = view.source_body_ids;
  j["scale"] = view.scale;
  j["sheet_position"] = drawing_array2(view.sheet_position);
  j["show_hidden"] = view.show_hidden;
  if (view.section.has_value()) {
    j["section"] = to_payload(view.section.value());
  }
  if (view.detail.has_value()) {
    j["detail"] = to_payload(view.detail.value());
  }
  if (view.broken_ref.has_value()) {
    j["broken_ref"] = view.broken_ref.value();
  }
  j["warning"] = view.warning;
  return j;
}

// ── Drawing ───────────────────────────────────────────────────────

json to_payload(const polysmith::core::Drawing& drawing) {
  json j;
  j["drawing_id"] = drawing.drawing_id;
  j["name"] = drawing.name;
  json sheets = json::array();
  for (const auto& sheet : drawing.sheets) sheets.push_back(to_payload(sheet));
  j["sheets"] = sheets;
  json views = json::array();
  for (const auto& view : drawing.views) views.push_back(to_payload(view));
  j["views"] = views;
  json annotations = json::array();
  for (const auto& annotation : drawing.annotations) {
    annotations.push_back(to_payload(annotation));
  }
  j["annotations"] = annotations;
  json notes = json::array();
  for (const auto& note : drawing.notes) {
    notes.push_back(to_payload(note));
  }
  j["notes"] = notes;
  return j;
}

// ── DrawingDocumentData ───────────────────────────────────────────

json to_payload(const polysmith::core::DrawingDocumentData& drawing) {
  json j;
  json drawings = json::array();
  for (const auto& d : drawing.drawings) drawings.push_back(to_payload(d));
  j["drawings"] = drawings;
  j["active_drawing_id"] = drawing.active_drawing_id.has_value()
      ? json(drawing.active_drawing_id.value()) : json(nullptr);
  j["selected_view_id"] = drawing.selected_view_id.has_value()
      ? json(drawing.selected_view_id.value()) : json(nullptr);
  j["selected_annotation_id"] = drawing.selected_annotation_id.has_value()
      ? json(drawing.selected_annotation_id.value()) : json(nullptr);
  j["decimal_separator"] = drawing.decimal_separator;
  return j;
}
