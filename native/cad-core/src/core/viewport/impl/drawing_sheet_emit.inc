  // ── Drawing sheets from the active drawing ─────────────────────
  // The drawing workspace draws the active drawing's sheets: the
  // projection curves come from the drawing_runtime cache (memory-
  // only, revision-validated) and are transformed to sheet-mm here so
  // the UI renderer needs no drawing math.  Emitted unconditionally —
  // the UI gates visibility by workspace (the CAM toolpath pattern).
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

    // ISO 5457 trimmed sizes (portrait): A0..A4.
    const auto paper_size_mm = [](const std::string& paper_size) {
      if (paper_size == "A0") return std::array<double, 2>{841.0, 1189.0};
      if (paper_size == "A1") return std::array<double, 2>{594.0, 841.0};
      if (paper_size == "A2") return std::array<double, 2>{420.0, 594.0};
      if (paper_size == "A3") return std::array<double, 2>{297.0, 420.0};
      return std::array<double, 2>{210.0, 297.0};  // A4
    };

    int sheet_index = 0;
    for (const auto& sheet : drawing->sheets) {
      ++sheet_index;
      ViewportDrawingSheet out_sheet;
      out_sheet.sheet_id = sheet.sheet_id;
      out_sheet.drawing_id = drawing->drawing_id;
      out_sheet.name = sheet.name.empty() ? "Sheet " + std::to_string(sheet_index)
                                          : sheet.name;
      const auto size = paper_size_mm(sheet.paper_size);
      out_sheet.width_mm = sheet.orientation == "landscape" ? size[1]
                                                            : size[0];
      out_sheet.height_mm = sheet.orientation == "landscape" ? size[0]
                                                             : size[1];

      int view_index = 0;
      for (const auto& view_id : sheet.view_ids) {
        ++view_index;
        const DrawingView* view = nullptr;
        for (const auto& v : drawing->views) {
          if (v.view_id == view_id) {
            view = &v;
            break;
          }
        }
        if (view == nullptr) {
          continue;  // dangling reference — the mutators prevent it
        }
        const ProjectionResult* projection =
            drawing_runtime::cached_projection(*document, view->view_id);
        if (projection == nullptr) {
          continue;  // not projected yet — nothing to draw
        }

        ViewportDrawingView out_view;
        out_view.view_id = view->view_id;
        out_view.label = !view->standard_view.empty()
                             ? view->standard_view
                             : "View " + std::to_string(view_index);
        out_view.scale = view->scale;
        out_view.origin = view->sheet_position;
        out_view.stale = projection->stale;
        out_view.warning = view->warning;

        const double s = view->scale;
        const double ox = view->sheet_position[0];
        const double oy = view->sheet_position[1];
        bool have_bounds = false;
        for (const auto& rec : projection->edges) {
          ViewportDrawingCurve curve;
          curve.kind = rec.curve_kind;
          curve.line_class = rec.line_class;
          curve.curve_class = rec.curve_class;
          curve.p0 = {rec.p_start[0] * s + ox, rec.p_start[1] * s + oy};
          curve.p1 = {rec.p_end[0] * s + ox, rec.p_end[1] * s + oy};
          if (rec.circle_center.has_value()) {
            curve.center = {rec.circle_center.value()[0] * s + ox,
                            rec.circle_center.value()[1] * s + oy};
            curve.radius = rec.circle_radius.value() * s;
          }
          if (rec.ellipse_center.has_value()) {
            curve.center = {rec.ellipse_center.value()[0] * s + ox,
                            rec.ellipse_center.value()[1] * s + oy};
            curve.major_dir = rec.ellipse_major_dir;
            curve.major_radius = rec.ellipse_major_radius.value() * s;
            curve.minor_radius = rec.ellipse_minor_radius.value() * s;
          }
          curve.start_angle = rec.start_angle;
          curve.end_angle = rec.end_angle;
          out_sheet.curves.push_back(std::move(curve));

          if (!have_bounds) {
            out_view.min = {curve.p0[0], curve.p0[1]};
            out_view.max = {curve.p0[0], curve.p0[1]};
            have_bounds = true;
          }
          out_view.min[0] = std::min(out_view.min[0], curve.p1[0]);
          out_view.min[1] = std::min(out_view.min[1], curve.p1[1]);
          out_view.max[0] = std::max(out_view.max[0], curve.p1[0]);
          out_view.max[1] = std::max(out_view.max[1], curve.p1[1]);
        }

        // ── Section hatching (ISO 128-3 §7) ────────────────────
        // Continuous thin lines at the section's angle and spacing,
        // clipped to the hatch region polygon (outer loop minus
        // holes) in view space, transformed to sheet-mm like every
        // projection curve.  The scanline hatcher is the shared
        // engine function — the P5 flatten and the export backends
        // produce identical hatching.
        if (view->section.has_value()) {
          const auto& section = view->section.value();
          for (const auto& region : projection->hatch_regions) {
            const auto segments = compute_hatch_segments(
                region, section.hatch_angle_deg, section.hatch_spacing_mm);
            for (const auto& segment : segments) {
              ViewportDrawingCurve curve;
              curve.kind = "line";
              curve.line_class = "visible";
              curve.curve_class = "hatch";
              curve.p0 = {segment[0][0] * s + ox, segment[0][1] * s + oy};
              curve.p1 = {segment[1][0] * s + ox, segment[1][1] * s + oy};
              out_sheet.curves.push_back(std::move(curve));
            }
          }
        }
        out_sheet.views.push_back(std::move(out_view));
      }
      drawing_sheets.push_back(std::move(out_sheet));
    }
  }
