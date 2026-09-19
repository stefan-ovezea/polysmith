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
