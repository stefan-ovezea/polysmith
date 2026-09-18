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
