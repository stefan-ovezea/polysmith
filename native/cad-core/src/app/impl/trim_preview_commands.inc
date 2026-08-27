  if (command.type == "trim_preview") {
    const std::string entity_id = read_string(command.payload, "entity_id");
    const double cursor_x = read_dimension(command.payload, "cursor_x");
    const double cursor_y = read_dimension(command.payload, "cursor_y");

    const auto doc_opt = document_manager().get_document();
    if (!doc_opt.has_value() ||
        !doc_opt->active_sketch_feature_id.has_value()) {
      polysmith::protocol::write_message(
          { { "id", command.id },
            { "type", "trim_preview_result" },
            { "payload", nullptr } });
      return;
    }

    const auto feature_it = std::find_if(
        doc_opt->feature_history.begin(),
        doc_opt->feature_history.end(),
        [&](const auto& f) {
          return f.id == doc_opt->active_sketch_feature_id.value();
        });

    if (feature_it == doc_opt->feature_history.end() ||
        !feature_it->sketch_parameters.has_value()) {
      polysmith::protocol::write_message(
          { { "id", command.id },
            { "type", "trim_preview_result" },
            { "payload", nullptr } });
      return;
    }

    const auto& params = feature_it->sketch_parameters.value();
    nlohmann::json result;
    result["entity_id"] = entity_id;
    // The document revision this preview was computed against. The UI
    // echoes it back with the trim; a mismatch means the preview
    // described geometry that no longer exists, and the core falls
    // back to re-deriving the segment from the click point.
    result["revision"] = doc_opt->revision;

    // Find and preview the entity.
    const auto line_it = std::find_if(
        params.lines.begin(), params.lines.end(),
        [&](const auto& l) { return l.id == entity_id; });
    if (line_it != params.lines.end()) {
      auto isects = polysmith::core::find_all_intersections(*line_it, params);
      if (isects.empty()) {
        polysmith::protocol::write_message(
            { { "id", command.id },
              { "type", "trim_preview_result" },
              { "payload", nullptr } });
        return;
      }
      auto segs = polysmith::core::split_line_at_intersections(*line_it, isects);
      int idx = polysmith::core::select_clicked_segment(
          segs, *line_it, cursor_x, cursor_y);
      result["entity_kind"] = "line";
      result["hovered_index"] = idx;
      auto& jsegs = result["segments"] = nlohmann::json::array();
      for (const auto& s : segs) {
        nlohmann::json seg;
        seg["start"] = {s.start_x, s.start_y};
        seg["end"]   = {s.end_x,   s.end_y};
        jsegs.push_back(std::move(seg));
      }
      polysmith::protocol::write_message(
          { { "id", command.id },
            { "type", "trim_preview_result" },
            { "payload", result } });
      return;
    }

    const auto circle_it = std::find_if(
        params.circles.begin(), params.circles.end(),
        [&](const auto& c) { return c.id == entity_id; });
    if (circle_it != params.circles.end()) {
      auto isects = polysmith::core::find_all_intersections(*circle_it, params);
      auto r = nlohmann::json::object();
      r["entity_id"] = entity_id;
      r["entity_kind"] = "circle";
      // Same revision stamp as the line branch — the UI's schema
      // requires it on every non-null preview payload.
      r["revision"] = doc_opt->revision;
      if (isects.empty()) {
        r["hovered_index"] = 0;
        r["full_circle"] = true;
      } else {
        auto segs = polysmith::core::split_circle_at_intersections(
            *circle_it, isects);
        int idx = polysmith::core::select_clicked_segment(
            segs, *circle_it, cursor_x, cursor_y);
        r["hovered_index"] = idx;
        r["full_circle"] = false;
        // Return segment angles so the UI can sample.
        auto& jsegs = r["segments"] = nlohmann::json::array();
        for (const auto& s : segs) {
          nlohmann::json seg;
          seg["param_start"] = s.param_start;
          seg["param_end"]   = s.param_end;
          jsegs.push_back(std::move(seg));
        }
      }
      polysmith::protocol::write_message(
          { { "id", command.id },
            { "type", "trim_preview_result" },
            { "payload", r } });
      return;
    }

    const auto arc_it = std::find_if(
        params.arcs.begin(), params.arcs.end(),
        [&](const auto& a) { return a.id == entity_id; });
    if (arc_it != params.arcs.end()) {
      auto isects = polysmith::core::find_all_intersections(*arc_it, params);
      auto r = nlohmann::json::object();
      r["entity_id"] = entity_id;
      r["entity_kind"] = "arc";
      // Same revision stamp as the line branch — the UI's schema
      // requires it on every non-null preview payload.
      r["revision"] = doc_opt->revision;
      if (isects.empty()) {
        r["hovered_index"] = 0;
        r["full_arc"] = true;
      } else {
        auto segs = polysmith::core::split_arc_at_intersections(
            *arc_it, isects);
        int idx = polysmith::core::select_clicked_segment(
            segs, *arc_it, cursor_x, cursor_y);
        r["hovered_index"] = idx;
        r["full_arc"] = false;
        auto& jsegs = r["segments"] = nlohmann::json::array();
        for (const auto& s : segs) {
          nlohmann::json seg;
          seg["param_start"] = s.param_start;
          seg["param_end"]   = s.param_end;
          jsegs.push_back(std::move(seg));
        }
      }
      polysmith::protocol::write_message(
          { { "id", command.id },
            { "type", "trim_preview_result" },
            { "payload", r } });
      return;
    }

    polysmith::protocol::write_message(
        { { "id", command.id },
          { "type", "trim_preview_result" },
          { "payload", nullptr } });
    return;
  }
