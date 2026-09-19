  // ── DRAWING ──────────────────────────────────────────────────
  // Drawing state lives in DocumentState::drawing.  Implementations
  // in session_drawing_commands.inc.  Mutators follow the canonical
  // document shape (require → validate → push undo → mutate → bump
  // revision).

  // Creates a drawing.  A drawing always starts with one sheet so
  // the drawing is never sheet-less (views need a sheet to live on).
  DocumentState drawing_create(const Drawing& drawing);
  // Deletes the drawing AND its sheets/views/annotations.  Clears
  // active/selection ids when they pointed inside the drawing.
  DocumentState drawing_delete(const std::string& drawing_id);
  DocumentState drawing_set_active(const std::string& drawing_id);
  // Appends a sheet to the drawing.  Throws when the drawing id or
  // the sheet id already resolves / does not resolve.
  DocumentState drawing_sheet_create(const std::string& drawing_id,
                                     const DrawingSheet& sheet);
  // Deletes the sheet AND its views (annotations attached to those
  // views die with them).
  DocumentState drawing_sheet_delete(const std::string& drawing_id,
                                     const std::string& sheet_id);

  // Creates a view on the sheet.  Validates the view kind (sections
  // land in P4), the standard view name (resolved through
  // standard_view_frame), the frame presence, and the source body ids
  // — all before the undo push.  Mints the view id when empty.  The
  // bump re-projects the view through the drawing refresh pass.
  DocumentState drawing_view_create(const std::string& drawing_id,
                                    const std::string& sheet_id,
                                    const DrawingView& view);
  // Replaces the view's definition (same id).  The view keeps its
  // sheet; annotations keep their attachments and re-resolve on the
  // next refresh.
  DocumentState drawing_view_update(const std::string& drawing_id,
                                    const DrawingView& view);
  // Removes the view, its annotations, and its id from every sheet's
  // view_ids list.
  DocumentState drawing_view_delete(const std::string& drawing_id,
                                    const std::string& view_id);
  // Moves the view origin on its sheet (sheet-mm).  Purely cosmetic —
  // never re-projects.
  DocumentState drawing_view_move(const std::string& drawing_id,
                                  const std::string& view_id,
                                  const std::array<double, 2>& sheet_position);
  // Replaces a section view's SectionDefinition (label, cut direction,
  // hatch angle/spacing).  The view must be kind "section"; the bump
  // re-cuts and re-projects through the refresh pass.
  DocumentState drawing_section_update(const std::string& drawing_id,
                                       const std::string& view_id,
                                       const SectionDefinition& section);
