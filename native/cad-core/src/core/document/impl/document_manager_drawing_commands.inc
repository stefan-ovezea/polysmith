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
  // Replaces the sheet's furniture settings: paper size (A0–A4),
  // orientation, per-sheet projection angle, and name.  Values are
  // validated before the undo push.
  DocumentState drawing_sheet_update(const std::string& drawing_id,
                                     const std::string& sheet_id,
                                     const std::string& paper_size,
                                     const std::string& orientation,
                                     const std::string& projection_angle,
                                     const std::string& name);

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
  // Replaces the sheet's ISO 7200 title block data (the eight
  // mandatory fields + revision rows).  Purely cosmetic — the bump
  // only re-flattens, nothing re-projects.
  DocumentState drawing_title_block_update(const std::string& drawing_id,
                                           const std::string& sheet_id,
                                           const TitleBlock& title_block);
  // Exports one sheet of a drawing ("svg" | "dxf" — PDF lands in P9).
  // NON-mutating: flattens from the current runtime projections (an
  // export never re-projects, never pushes undo, never bumps the
  // revision).  Throws on an unknown drawing/sheet/format or I/O
  // failure.
  ExportResult export_drawing_sheet(const std::string& drawing_id,
                                    const std::string& sheet_id,
                                    const std::string& format,
                                    const std::string& file_path,
                                    const std::string& dxf_mode = "geometry") const;

  // ── Dimensions (P6) ───────────────────────────────────────────
  // The core mints the witness from the pick: picks are sheet-mm
  // points resolved against the view's CURRENT projection (never
  // stored ordinals — the TNP mantra).  With an annotation_id the
  // command repairs that annotation (witness + kind re-captured,
  // cosmetics kept).  Validation (pick resolution, witness minting,
  // kind-vs-geometry) throws BEFORE the undo push.
  DocumentState drawing_dimension_create(
      const std::string& drawing_id, const std::string& view_id,
      const std::string& dim_type, const std::array<double, 2>& pick,
      const std::optional<std::array<double, 2>>& pick_2,
      const std::optional<std::string>& annotation_id);
  // Cosmetic edits only: text override, prefix, arrow flip, text
  // offset.  Never re-projects, never re-resolves — the bump only
  // re-flattens.
  DocumentState drawing_dimension_update(
      const std::string& drawing_id, const std::string& annotation_id,
      const std::optional<std::string>& text_override,
      const std::optional<std::string>& prefix,
      const std::optional<bool>& arrow_flip,
      const std::optional<std::array<double, 2>>& text_offset);
  DocumentState drawing_dimension_delete(const std::string& drawing_id,
                                         const std::string& annotation_id);
