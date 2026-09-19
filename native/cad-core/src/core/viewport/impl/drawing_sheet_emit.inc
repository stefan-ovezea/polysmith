  // ── Drawing sheets from the active drawing ─────────────────────
  // The drawing workspace draws the active drawing's sheets from the
  // P5 flattened stream (core/drawing/drawing_sheet.h): view geometry
  // in sheet-mm with ISO 128-2 line styles APPLIED (dash patterns
  // computed in the core with the Annex-A corner rule), plus the
  // ISO 5457 furniture (frame, centring marks, grid-reference ticks,
  // projection symbol).  The stream is memory-only — flattened from
  // the runtime projection cache on every payload.  Emitted
  // unconditionally; the UI gates visibility by workspace (the CAM
  // toolpath pattern).
  std::vector<ViewportDrawingSheet> drawing_sheets;
  if (document.has_value() && !document->drawing.drawings.empty()) {
    const Drawing* drawing = nullptr;
    if (document->drawing.active_drawing_id.has_value()) {
      for (const auto& d : document->drawing.drawings) {
        if (d.drawing_id == document->drawing.active_drawing_id.value()) {
          drawing = &d;
          break;
        }
      }
    }
    if (drawing == nullptr) {
      drawing = &document->drawing.drawings.front();
    }

    int sheet_index = 0;
    for (const auto& sheet : drawing->sheets) {
      ++sheet_index;
      const auto flat = flatten_sheet(*document, drawing->drawing_id,
                                      sheet.sheet_id);
      if (!flat.has_value()) {
        continue;
      }
      ViewportDrawingSheet out_sheet;
      out_sheet.sheet_id = flat->sheet_id;
      out_sheet.drawing_id = flat->drawing_id;
      out_sheet.name = sheet.name.empty()
                           ? "Sheet " + std::to_string(sheet_index)
                           : sheet.name;
      out_sheet.width_mm = flat->width_mm;
      out_sheet.height_mm = flat->height_mm;
      for (const auto& p : flat->primitives) {
        ViewportDrawingCurve curve;
        curve.kind = p.kind;
        curve.line_class = p.line_class;
        // The flattened stream carries the purpose (the P3
        // curve_class vocabulary collapsed into it).
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
        out_sheet.curves.push_back(std::move(curve));
      }
      for (const auto& b : flat->views) {
        ViewportDrawingView out_view;
        out_view.view_id = b.view_id;
        out_view.label = b.label;
        out_view.scale = b.scale;
        out_view.origin = b.origin;
        out_view.min = b.min;
        out_view.max = b.max;
        out_view.stale = b.stale;
        out_view.warning = b.warning;
        out_sheet.views.push_back(std::move(out_view));
      }
      for (const auto& t : flat->texts) {
        ViewportDrawingText out_text;
        out_text.text = t.text;
        out_text.position = t.position;
        out_text.height_mm = t.height_mm;
        out_text.angle_deg = t.angle_deg;
        out_text.h_align = t.h_align;
        out_text.purpose = t.purpose;
        out_text.stale = t.stale;
        out_sheet.texts.push_back(std::move(out_text));
      }
      drawing_sheets.push_back(std::move(out_sheet));
    }
  }
