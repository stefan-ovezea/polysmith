// ── DRAWING mutators ──────────────────────────────────────────────
//
// Drawing state lives in DocumentState::drawing (drawing_types.h).
// Every mutator follows the canonical document shape: validate and
// throw BEFORE mutating, then push_undo_state → clear_redo_stack →
// mutate → bump_geometry_revision.  Generated projections are NOT
// touched here — they are memory-only (drawing_runtime) and a
// geometry bump invalidates them through the refresh pass.

namespace {

// Returns the drawing with the given id, or nullptr.
polysmith::core::Drawing* find_drawing(DocumentState& document,
                                       const std::string& drawing_id) {
  for (auto& drawing : document.drawing.drawings) {
    if (drawing.drawing_id == drawing_id) {
      return &drawing;
    }
  }
  return nullptr;
}

// Returns the sheet with the given id inside the given drawing, or
// nullptr.
polysmith::core::DrawingSheet* find_sheet(Drawing& drawing,
                                          const std::string& sheet_id) {
  for (auto& sheet : drawing.sheets) {
    if (sheet.sheet_id == sheet_id) {
      return &sheet;
    }
  }
  return nullptr;
}

// Returns the view with the given id inside the given drawing, or
// nullptr.
polysmith::core::DrawingView* find_view(Drawing& drawing,
                                        const std::string& view_id) {
  for (auto& view : drawing.views) {
    if (view.view_id == view_id) {
      return &view;
    }
  }
  return nullptr;
}

// Shared view-definition validation — throws BEFORE any undo push
// (the canonical mutator shape).  Kind/frame rules:
//   - "projection": standard_view or custom_frame required.
//   - "axonometric": custom_frame required.
//   - "section": a section definition required — the frame derives
//     from the cutting plane (never a standard view or custom frame,
//     never the uncut body).
void validate_view_definition(const DrawingView& view) {
  if (view.kind != "projection" && view.kind != "axonometric" &&
      view.kind != "section") {
    throw std::runtime_error("Unknown view kind: " + view.kind);
  }
  if (view.kind == "section") {
    if (!view.section.has_value()) {
      throw std::runtime_error("Section views need a section definition");
    }
    const auto& normal = view.section->cutting_plane_normal;
    const double length = std::sqrt(normal[0] * normal[0] +
                                    normal[1] * normal[1] +
                                    normal[2] * normal[2]);
    if (length < 1e-9) {
      throw std::runtime_error("The cutting plane normal is degenerate");
    }
    if (!(view.section->hatch_spacing_mm > 0.0)) {
      throw std::runtime_error("The hatch spacing must be positive");
    }
  } else {
    if (!view.standard_view.empty() &&
        !polysmith::core::standard_view_frame(view.standard_view)
             .has_value()) {
      throw std::runtime_error("Unknown standard view: " +
                               view.standard_view);
    }
    if (view.kind == "axonometric" && !view.custom_frame.has_value()) {
      throw std::runtime_error(
          "Axonometric views need a custom frame");
    }
    if (view.standard_view.empty() && !view.custom_frame.has_value()) {
      throw std::runtime_error(
          "The view needs a standard view or a custom frame");
    }
  }
  if (view.source_body_ids.empty()) {
    throw std::runtime_error("The view needs at least one source body");
  }
}

}  // namespace

DocumentState DocumentManager::drawing_create(const Drawing& drawing) {
  require_document();
  if (drawing.name.empty()) {
    throw std::runtime_error("The drawing needs a name");
  }
  if (!drawing.drawing_id.empty()) {
    for (const auto& existing : document_->drawing.drawings) {
      if (existing.drawing_id == drawing.drawing_id) {
        throw std::runtime_error("A drawing with this id already exists");
      }
    }
  }
  if (drawing.sheets.empty()) {
    throw std::runtime_error("A drawing needs at least one sheet");
  }

  push_undo_state();
  clear_redo_stack();
  Drawing stored = drawing;
  if (stored.drawing_id.empty()) {
    stored.drawing_id = "drawing-" + std::to_string(next_drawing_id_++);
  }
  // Mint sheet/view/annotation ids when the caller left them empty
  // (the UI sends structural data; the core owns identity).
  for (auto& sheet : stored.sheets) {
    if (sheet.sheet_id.empty()) {
      sheet.sheet_id = "drawing-sheet-" + std::to_string(next_drawing_sheet_id_++);
    }
  }
  for (auto& view : stored.views) {
    if (view.view_id.empty()) {
      view.view_id = "drawing-view-" + std::to_string(next_drawing_view_id_++);
    }
  }
  for (auto& annotation : stored.annotations) {
    if (annotation.annotation_id.empty()) {
      annotation.annotation_id =
          "drawing-annotation-" + std::to_string(next_drawing_annotation_id_++);
    }
  }
  document_->drawing.drawings.push_back(stored);
  document_->drawing.active_drawing_id = stored.drawing_id;
  bump_geometry_revision();
  return document_.value();
}

DocumentState DocumentManager::drawing_delete(const std::string& drawing_id) {
  require_document();
  auto& drawings = document_->drawing.drawings;
  const auto found =
      std::find_if(drawings.begin(), drawings.end(),
                   [&](const Drawing& d) { return d.drawing_id == drawing_id; });
  if (found == drawings.end()) {
    throw std::runtime_error("Unknown drawing: " + drawing_id);
  }

  push_undo_state();
  clear_redo_stack();
  drawings.erase(found);
  // Selection/active ids that pointed inside the deleted drawing are
  // dead — drop them rather than leaving dangling references.
  if (document_->drawing.active_drawing_id == drawing_id) {
    document_->drawing.active_drawing_id.reset();
  }
  if (document_->drawing.selected_view_id.has_value()) {
    const bool dead = std::none_of(
        drawings.begin(), drawings.end(), [&](const Drawing& d) {
          return std::any_of(
              d.views.begin(), d.views.end(), [&](const DrawingView& v) {
                return v.view_id ==
                       document_->drawing.selected_view_id.value();
              });
        });
    if (dead) {
      document_->drawing.selected_view_id.reset();
    }
  }
  if (document_->drawing.selected_annotation_id.has_value()) {
    const bool dead = std::none_of(
        drawings.begin(), drawings.end(), [&](const Drawing& d) {
          return std::any_of(
              d.annotations.begin(), d.annotations.end(),
              [&](const Annotation& a) {
                return a.annotation_id ==
                       document_->drawing.selected_annotation_id.value();
              });
        });
    if (dead) {
      document_->drawing.selected_annotation_id.reset();
    }
  }
  bump_geometry_revision();
  return document_.value();
}

DocumentState DocumentManager::drawing_set_active(
    const std::string& drawing_id) {
  require_document();
  const bool exists =
      find_drawing(document_.value(), drawing_id) != nullptr;
  if (!exists) {
    throw std::runtime_error("Unknown drawing: " + drawing_id);
  }
  push_undo_state();
  clear_redo_stack();
  document_->drawing.active_drawing_id = drawing_id;
  bump_geometry_revision();
  return document_.value();
}

DocumentState DocumentManager::drawing_sheet_create(
    const std::string& drawing_id, const DrawingSheet& sheet) {
  require_document();
  Drawing* drawing = find_drawing(document_.value(), drawing_id);
  if (drawing == nullptr) {
    throw std::runtime_error("Unknown drawing: " + drawing_id);
  }
  if (!sheet.sheet_id.empty() && find_sheet(*drawing, sheet.sheet_id) != nullptr) {
    throw std::runtime_error("A sheet with this id already exists");
  }
  // Only reference views that actually exist in this drawing — a
  // dangling view id would render as a silently missing view.
  // Validated BEFORE the undo push (canonical: throw before mutate).
  for (const auto& view_id : sheet.view_ids) {
    const bool exists = std::any_of(
        drawing->views.begin(), drawing->views.end(),
        [&](const DrawingView& v) { return v.view_id == view_id; });
    if (!exists) {
      throw std::runtime_error("Unknown view: " + view_id);
    }
  }

  push_undo_state();
  clear_redo_stack();
  DrawingSheet stored = sheet;
  if (stored.sheet_id.empty()) {
    stored.sheet_id =
        "drawing-sheet-" + std::to_string(next_drawing_sheet_id_++);
  }
  drawing->sheets.push_back(stored);
  bump_geometry_revision();
  return document_.value();
}

DocumentState DocumentManager::drawing_sheet_delete(
    const std::string& drawing_id, const std::string& sheet_id) {
  require_document();
  Drawing* drawing = find_drawing(document_.value(), drawing_id);
  if (drawing == nullptr) {
    throw std::runtime_error("Unknown drawing: " + drawing_id);
  }
  DrawingSheet* sheet = find_sheet(*drawing, sheet_id);
  if (sheet == nullptr) {
    throw std::runtime_error("Unknown sheet: " + sheet_id);
  }

  push_undo_state();
  clear_redo_stack();
  const std::vector<std::string> dying_view_ids = sheet->view_ids;
  // Delete the sheet and its views; annotations attached to those
  // views die with them (an annotation without its view is
  // meaningless — the view owns the projected geometry it measures).
  drawing->sheets.erase(
      std::remove_if(drawing->sheets.begin(), drawing->sheets.end(),
                     [&](const DrawingSheet& s) {
                       return s.sheet_id == sheet_id;
                     }),
      drawing->sheets.end());
  for (const auto& view_id : dying_view_ids) {
    drawing->views.erase(
        std::remove_if(drawing->views.begin(), drawing->views.end(),
                       [&](const DrawingView& v) {
                         return v.view_id == view_id;
                       }),
        drawing->views.end());
  }
  for (const auto& annotation : drawing->annotations) {
    if (std::find(dying_view_ids.begin(), dying_view_ids.end(),
                  annotation.view_id) != dying_view_ids.end()) {
      drawing_runtime::erase_dimension(document_->id,
                                       annotation.annotation_id);
    }
  }
  drawing->annotations.erase(
      std::remove_if(drawing->annotations.begin(), drawing->annotations.end(),
                     [&](const Annotation& a) {
                       return std::find(dying_view_ids.begin(),
                                        dying_view_ids.end(),
                                        a.view_id) != dying_view_ids.end();
                     }),
      drawing->annotations.end());
  // Clear selection ids that pointed at the deleted sheet's content.
  if (document_->drawing.selected_view_id.has_value() &&
      std::find(dying_view_ids.begin(), dying_view_ids.end(),
                document_->drawing.selected_view_id.value()) !=
          dying_view_ids.end()) {
    document_->drawing.selected_view_id.reset();
  }
  if (document_->drawing.selected_annotation_id.has_value()) {
    const auto& selected = document_->drawing.selected_annotation_id.value();
    const bool dead = std::none_of(
        drawing->annotations.begin(), drawing->annotations.end(),
        [&](const Annotation& a) { return a.annotation_id == selected; });
    if (dead) {
      document_->drawing.selected_annotation_id.reset();
    }
  }
  bump_geometry_revision();
  return document_.value();
}

DocumentState DocumentManager::drawing_sheet_update(
    const std::string& drawing_id, const std::string& sheet_id,
    const std::string& paper_size, const std::string& orientation,
    const std::string& projection_angle, const std::string& name) {
  require_document();
  Drawing* drawing = find_drawing(document_.value(), drawing_id);
  if (drawing == nullptr) {
    throw std::runtime_error("Unknown drawing: " + drawing_id);
  }
  DrawingSheet* sheet = find_sheet(*drawing, sheet_id);
  if (sheet == nullptr) {
    throw std::runtime_error("Unknown sheet: " + sheet_id);
  }
  // Validate BEFORE the undo push (canonical mutator shape).
  static const std::vector<std::string> kPaperSizes = {
      "A0", "A1", "A2", "A3", "A4"};
  if (std::find(kPaperSizes.begin(), kPaperSizes.end(), paper_size) ==
      kPaperSizes.end()) {
    throw std::runtime_error("Unknown paper size: " + paper_size);
  }
  if (orientation != "portrait" && orientation != "landscape") {
    throw std::runtime_error("Unknown orientation: " + orientation);
  }
  if (projection_angle != "first_angle" &&
      projection_angle != "third_angle") {
    throw std::runtime_error("Unknown projection angle: " +
                             projection_angle);
  }

  push_undo_state();
  clear_redo_stack();
  sheet->paper_size = paper_size;
  sheet->orientation = orientation;
  sheet->projection_angle = projection_angle;
  sheet->name = name;
  // The sheet definition changed (not any view's projection) — the
  // bump keeps the payload honest.
  bump_geometry_revision();
  return document_.value();
}

DocumentState DocumentManager::drawing_title_block_update(
    const std::string& drawing_id, const std::string& sheet_id,
    const TitleBlock& title_block) {
  require_document();
  Drawing* drawing = find_drawing(document_.value(), drawing_id);
  if (drawing == nullptr) {
    throw std::runtime_error("Unknown drawing: " + drawing_id);
  }
  DrawingSheet* sheet = find_sheet(*drawing, sheet_id);
  if (sheet == nullptr) {
    throw std::runtime_error("Unknown sheet: " + sheet_id);
  }
  // Nothing to validate beyond what the payload parser already did —
  // every field is a string.  Canonical mutator shape still applies.
  push_undo_state();
  clear_redo_stack();
  sheet->title_block = title_block;
  // Cosmetic: the bump only re-flattens, nothing re-projects.
  bump_geometry_revision();
  return document_.value();
}

ExportResult DocumentManager::export_drawing_sheet(
    const std::string& drawing_id, const std::string& sheet_id,
    const std::string& format, const std::string& file_path,
    const std::string& dxf_mode) const {
  require_document();
  // Non-mutating: no undo push, no bump — the export reads the
  // current flattened state (the invariant: every mutation bumps the
  // revision and the refresh pass keeps the runtime projections
  // current, so the flatten is live).
  return polysmith::core::export_drawing_sheet(document_.value(),
                                               drawing_id, sheet_id, format,
                                               file_path, dxf_mode);
}

DocumentState DocumentManager::drawing_view_create(
    const std::string& drawing_id, const std::string& sheet_id,
    const DrawingView& view) {
  require_document();
  Drawing* drawing = find_drawing(document_.value(), drawing_id);
  if (drawing == nullptr) {
    throw std::runtime_error("Unknown drawing: " + drawing_id);
  }
  if (find_sheet(*drawing, sheet_id) == nullptr) {
    throw std::runtime_error("Unknown sheet: " + sheet_id);
  }
  if (!view.view_id.empty() && find_view(*drawing, view.view_id) != nullptr) {
    throw std::runtime_error("A view with this id already exists");
  }
  validate_view_definition(view);

  push_undo_state();
  clear_redo_stack();
  DrawingView stored = view;
  if (stored.view_id.empty()) {
    stored.view_id = "drawing-view-" + std::to_string(next_drawing_view_id_++);
  }
  drawing->views.push_back(stored);
  // The sheet owns the view ordering (first-angle layout reads it).
  find_sheet(*drawing, sheet_id)->view_ids.push_back(stored.view_id);
  bump_geometry_revision();
  return document_.value();
}

DocumentState DocumentManager::drawing_view_update(
    const std::string& drawing_id, const DrawingView& view) {
  require_document();
  Drawing* drawing = find_drawing(document_.value(), drawing_id);
  if (drawing == nullptr) {
    throw std::runtime_error("Unknown drawing: " + drawing_id);
  }
  DrawingView* existing = find_view(*drawing, view.view_id);
  if (existing == nullptr) {
    throw std::runtime_error("Unknown view: " + view.view_id);
  }
  validate_view_definition(view);

  push_undo_state();
  clear_redo_stack();
  // Replace the definition; the view keeps its id and its sheet.
  *existing = view;
  bump_geometry_revision();
  return document_.value();
}

DocumentState DocumentManager::drawing_view_delete(
    const std::string& drawing_id, const std::string& view_id) {
  require_document();
  Drawing* drawing = find_drawing(document_.value(), drawing_id);
  if (drawing == nullptr) {
    throw std::runtime_error("Unknown drawing: " + drawing_id);
  }
  if (find_view(*drawing, view_id) == nullptr) {
    throw std::runtime_error("Unknown view: " + view_id);
  }

  push_undo_state();
  clear_redo_stack();
  drawing->views.erase(
      std::remove_if(drawing->views.begin(), drawing->views.end(),
                     [&](const DrawingView& v) {
                       return v.view_id == view_id;
                     }),
      drawing->views.end());
  // Annotations attached to the view die with it.
  for (const auto& annotation : drawing->annotations) {
    if (annotation.view_id == view_id) {
      drawing_runtime::erase_dimension(document_->id,
                                       annotation.annotation_id);
    }
  }
  drawing->annotations.erase(
      std::remove_if(drawing->annotations.begin(), drawing->annotations.end(),
                     [&](const Annotation& a) {
                       return a.view_id == view_id;
                     }),
      drawing->annotations.end());
  // Remove the id from every sheet's ordering list.
  for (auto& sheet : drawing->sheets) {
    sheet.view_ids.erase(
        std::remove(sheet.view_ids.begin(), sheet.view_ids.end(), view_id),
        sheet.view_ids.end());
  }
  if (document_->drawing.selected_view_id == view_id) {
    document_->drawing.selected_view_id.reset();
  }
  bump_geometry_revision();
  return document_.value();
}

DocumentState DocumentManager::drawing_view_move(
    const std::string& drawing_id, const std::string& view_id,
    const std::array<double, 2>& sheet_position) {
  require_document();
  Drawing* drawing = find_drawing(document_.value(), drawing_id);
  if (drawing == nullptr) {
    throw std::runtime_error("Unknown drawing: " + drawing_id);
  }
  DrawingView* view = find_view(*drawing, view_id);
  if (view == nullptr) {
    throw std::runtime_error("Unknown view: " + view_id);
  }

  push_undo_state();
  clear_redo_stack();
  view->sheet_position = sheet_position;
  // Cosmetic move only — the projection itself does not change, but
  // the document state does, so the bump keeps the payload honest.
  bump_geometry_revision();
  return document_.value();
}

DocumentState DocumentManager::drawing_section_update(
    const std::string& drawing_id, const std::string& view_id,
    const SectionDefinition& section) {
  require_document();
  Drawing* drawing = find_drawing(document_.value(), drawing_id);
  if (drawing == nullptr) {
    throw std::runtime_error("Unknown drawing: " + drawing_id);
  }
  DrawingView* view = find_view(*drawing, view_id);
  if (view == nullptr) {
    throw std::runtime_error("Unknown view: " + view_id);
  }
  if (view->kind != "section") {
    throw std::runtime_error("drawing_section_update needs a section view");
  }
  // Validate the patched view BEFORE the undo push (canonical shape).
  DrawingView patched = *view;
  patched.section = section;
  validate_view_definition(patched);

  push_undo_state();
  clear_redo_stack();
  view->section = section;
  bump_geometry_revision();
  return document_.value();
}

// ── Dimensions (P6) ───────────────────────────────────────────────
//
// The core mints the witness from the pick: the pick (sheet-mm) is
// resolved against the view's CURRENT projection, the record's
// provenance becomes the annotation's SourceEdgeWitness, and the
// persistent edge reference id follows the cam_capture_edge_reference
// precedent.  A pick is never a stored ordinal — re-projection cannot
// break it (the TNP mantra).

DocumentState DocumentManager::drawing_dimension_create(
    const std::string& drawing_id, const std::string& view_id,
    const std::string& dim_type, const std::array<double, 2>& pick,
    const std::optional<std::array<double, 2>>& pick_2,
    const std::optional<std::string>& annotation_id) {
  require_document();
  Drawing* drawing = find_drawing(document_.value(), drawing_id);
  if (drawing == nullptr) {
    throw std::runtime_error("Unknown drawing: " + drawing_id);
  }
  DrawingView* view = find_view(*drawing, view_id);
  if (view == nullptr) {
    throw std::runtime_error("Unknown view: " + view_id);
  }
  if (dim_type != "linear" && dim_type != "angular" &&
      dim_type != "radius" && dim_type != "diameter") {
    throw std::runtime_error("Unknown dimension kind: " + dim_type);
  }
  const ProjectionResult* projection =
      drawing_runtime::cached_projection(document_.value(), view_id);
  if (projection == nullptr) {
    throw std::runtime_error(
        "The view has no projection yet — create the view first.");
  }

  // Resolve the picks + measure BEFORE the undo push (canonical
  // shape): witness minting and kind validation can throw.
  Annotation built;
  built.kind = dim_type;
  built.view_id = view_id;
  if (annotation_id.has_value()) {
    // Repair: the annotation keeps its cosmetic fields; the witness
    // and kind are re-captured from the fresh picks.
    auto found = std::find_if(
        drawing->annotations.begin(), drawing->annotations.end(),
        [&](const Annotation& a) {
          return a.annotation_id == annotation_id.value();
        });
    if (found == drawing->annotations.end()) {
      throw std::runtime_error("Unknown annotation: " + annotation_id.value());
    }
    if (found->view_id != view_id) {
      throw std::runtime_error(
          "The annotation belongs to a different view.");
    }
    built = *found;
    built.kind = dim_type;
    built.witness_2.reset();
    built.dependency_broken = false;
    built.warning.clear();
  } else if (dim_type == "diameter") {
    built.prefix = "\xE2\x8C\x80";  // ⌀ — ISO 129-1
  } else if (dim_type == "radius") {
    built.prefix = "R";             // R is mandatory for radii
  }

  std::string error;
  std::vector<const ProjectedEdgeRecord*> records;
  const auto measured = measure_from_picks(
      *projection, *view, dim_type, pick, pick_2,
      /*pick_tolerance_sheetmm=*/5.0, document_->drawing.decimal_separator,
      &built, &error, &records);
  if (!measured.has_value()) {
    throw std::runtime_error(error);
  }
  if (records.empty()) {
    throw std::runtime_error("The pick resolved to no edge.");
  }
  if (!witness_from_record(*records[0], &built.witness, &error)) {
    throw std::runtime_error(error);
  }
  if (records.size() > 1) {
    SourceEdgeWitness second;
    if (!witness_from_record(*records[1], &second, &error)) {
      throw std::runtime_error(error);
    }
    built.witness_2 = second;
  }

  push_undo_state();
  clear_redo_stack();
  built.source_edge_id =
      "drawing-edge-" + std::to_string(next_drawing_edge_ref_id_++);
  if (annotation_id.has_value()) {
    auto found = std::find_if(
        drawing->annotations.begin(), drawing->annotations.end(),
        [&](const Annotation& a) {
          return a.annotation_id == annotation_id.value();
        });
    *found = std::move(built);
    document_->drawing.selected_annotation_id = annotation_id;
  } else {
    built.annotation_id =
        "drawing-annotation-" + std::to_string(next_drawing_annotation_id_++);
    document_->drawing.selected_annotation_id = built.annotation_id;
    drawing->annotations.push_back(std::move(built));
  }
  bump_geometry_revision();
  return document_.value();
}

DocumentState DocumentManager::drawing_dimension_update(
    const std::string& drawing_id, const std::string& annotation_id,
    const std::optional<std::string>& text_override,
    const std::optional<std::string>& prefix,
    const std::optional<bool>& arrow_flip,
    const std::optional<std::array<double, 2>>& text_offset) {
  require_document();
  Drawing* drawing = find_drawing(document_.value(), drawing_id);
  if (drawing == nullptr) {
    throw std::runtime_error("Unknown drawing: " + drawing_id);
  }
  auto found = std::find_if(
      drawing->annotations.begin(), drawing->annotations.end(),
      [&](const Annotation& a) { return a.annotation_id == annotation_id; });
  if (found == drawing->annotations.end()) {
    throw std::runtime_error("Unknown annotation: " + annotation_id);
  }

  // Cosmetic-only: never re-projects, never re-resolves (the plan's
  // projection-input vs cosmetic layer separation).  The bump
  // re-flattens so the new text/placement shows.
  push_undo_state();
  clear_redo_stack();
  if (text_override.has_value()) {
    found->text_override = text_override.value().empty()
                               ? std::optional<std::string>()
                               : text_override;
  }
  if (prefix.has_value()) {
    found->prefix = prefix.value();
  }
  if (arrow_flip.has_value()) {
    found->arrow_flip = arrow_flip.value();
  }
  if (text_offset.has_value()) {
    found->text_offset = text_offset;
  }
  bump_geometry_revision();
  return document_.value();
}

DocumentState DocumentManager::drawing_dimension_delete(
    const std::string& drawing_id, const std::string& annotation_id) {
  require_document();
  Drawing* drawing = find_drawing(document_.value(), drawing_id);
  if (drawing == nullptr) {
    throw std::runtime_error("Unknown drawing: " + drawing_id);
  }
  auto found = std::find_if(
      drawing->annotations.begin(), drawing->annotations.end(),
      [&](const Annotation& a) { return a.annotation_id == annotation_id; });
  if (found == drawing->annotations.end()) {
    throw std::runtime_error("Unknown annotation: " + annotation_id);
  }

  push_undo_state();
  clear_redo_stack();
  // The cached value is derived state — drop it with the annotation.
  drawing_runtime::erase_dimension(document_->id, annotation_id);
  if (document_->drawing.selected_annotation_id == annotation_id) {
    document_->drawing.selected_annotation_id.reset();
  }
  drawing->annotations.erase(found);
  bump_geometry_revision();
  return document_.value();
}
