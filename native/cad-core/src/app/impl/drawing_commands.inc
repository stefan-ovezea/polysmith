  // ── DRAWING commands ───────────────────────────────────────────
  //
  // All drawing state lives in DocumentState::drawing
  // (drawing_types.h).  Payloads are the serialized target-schema
  // structs; replies are document_state events like every other
  // mutator.  Validation errors from the DocumentManager bubble to
  // the run() catch and reply with an error event — handlers stay
  // thin.

  if (command.type == "drawing_create") {
    const auto drawing =
        polysmith::protocol::drawing_from_payload(command.payload);
    const auto document = document_manager().drawing_create(drawing);
    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "drawing_delete") {
    const auto drawing_id = read_string(
        command.payload, "drawing_id");
    const auto document = document_manager().drawing_delete(drawing_id);
    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "drawing_set_active") {
    const auto drawing_id = read_string(
        command.payload, "drawing_id");
    const auto document = document_manager().drawing_set_active(drawing_id);
    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "drawing_sheet_create") {
    const auto drawing_id = read_string(
        command.payload, "drawing_id");
    const auto sheet = polysmith::protocol::drawing_sheet_from_payload(
        command.payload.at("sheet"));
    const auto document =
        document_manager().drawing_sheet_create(drawing_id, sheet);
    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "drawing_sheet_delete") {
    const auto drawing_id = read_string(
        command.payload, "drawing_id");
    const auto sheet_id = read_string(
        command.payload, "sheet_id");
    const auto document =
        document_manager().drawing_sheet_delete(drawing_id, sheet_id);
    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "drawing_sheet_update") {
    const auto drawing_id = read_string(
        command.payload, "drawing_id");
    const auto sheet_id = read_string(
        command.payload, "sheet_id");
    const auto paper_size = read_string(
        command.payload, "paper_size");
    const auto orientation = read_string(
        command.payload, "orientation");
    const auto projection_angle = read_string(
        command.payload, "projection_angle");
    const auto name = read_string(
        command.payload, "name");
    const auto document = document_manager().drawing_sheet_update(
        drawing_id, sheet_id, paper_size, orientation, projection_angle,
        name);
    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "drawing_view_create") {
    const auto drawing_id = read_string(
        command.payload, "drawing_id");
    const auto sheet_id = read_string(
        command.payload, "sheet_id");
    const auto view = polysmith::protocol::drawing_view_from_payload(
        command.payload.at("view"));
    const auto document =
        document_manager().drawing_view_create(drawing_id, sheet_id, view);
    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "drawing_view_update") {
    const auto drawing_id = read_string(
        command.payload, "drawing_id");
    const auto view = polysmith::protocol::drawing_view_from_payload(
        command.payload.at("view"));
    const auto document =
        document_manager().drawing_view_update(drawing_id, view);
    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "drawing_view_delete") {
    const auto drawing_id = read_string(
        command.payload, "drawing_id");
    const auto view_id = read_string(
        command.payload, "view_id");
    const auto document =
        document_manager().drawing_view_delete(drawing_id, view_id);
    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "drawing_view_move") {
    const auto drawing_id = read_string(
        command.payload, "drawing_id");
    const auto view_id = read_string(
        command.payload, "view_id");
    const auto position = command.payload.at("sheet_position");
    if (!position.is_array() || position.size() < 2 ||
        !position.at(0).is_number() || !position.at(1).is_number()) {
      polysmith::protocol::write_message(polysmith::protocol::make_error_event(
          command.id, "INVALID_PAYLOAD",
          "drawing_view_move needs sheet_position [x, y]"));
      return;
    }
    const std::array<double, 2> sheet_position = {
        position.at(0).get<double>(), position.at(1).get<double>()};
    const auto document = document_manager().drawing_view_move(
        drawing_id, view_id, sheet_position);
    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "drawing_view_preview") {
    // Non-mutating: project + flatten an UNCOMMITTED view definition
    // and reply with the ghost geometry (the Insert View panel renders
    // it live until Enter commits the real drawing_view_create).
    const auto drawing_id = read_string(command.payload, "drawing_id");
    const auto sheet_id = read_string(command.payload, "sheet_id");
    const auto def = polysmith::protocol::drawing_view_from_payload(
        command.payload.at("view"));

    const auto document = document_manager().get_document();
    if (!document.has_value()) {
      polysmith::protocol::write_message(polysmith::protocol::make_error_event(
          command.id, "NO_ACTIVE_DOCUMENT", "No active document"));
      return;
    }
    const auto preview = polysmith::core::preview_view_geometry(
        document.value(), drawing_id, def);
    if (!preview.has_value()) {
      polysmith::protocol::write_message(polysmith::protocol::make_error_event(
          command.id, "UNKNOWN_DRAWING",
          "The drawing was not found."));
      return;
    }

    // Mirror the sheet emission's primitive vocabulary so the UI
    // reuses the same parsers + scene builders as a committed view
    // (drawing_sheet_emit.inc — keep the two mappings in sync).
    polysmith::protocol::json curves = polysmith::protocol::json::array();
    for (const auto& p : preview->primitives) {
      polysmith::core::ViewportDrawingCurve curve;
      curve.kind = p.kind;
      curve.line_class = p.line_class;
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
      curves.push_back(polysmith::protocol::to_payload(curve));
    }
    polysmith::protocol::json texts = polysmith::protocol::json::array();
    for (const auto& t : preview->texts) {
      polysmith::core::ViewportDrawingText text;
      text.text = t.text;
      text.position = t.position;
      text.height_mm = t.height_mm;
      text.angle_deg = t.angle_deg;
      text.h_align = t.h_align;
      text.purpose = t.purpose;
      text.stale = t.stale;
      texts.push_back(polysmith::protocol::to_payload(text));
    }

    polysmith::protocol::json reply = {
        {"drawing_id", drawing_id},
        {"sheet_id", sheet_id},
        {"curves", curves},
        {"texts", texts},
        // A view record shaped like a committed view (empty id marks
        // it as the uncommitted preview).
        {"view",
         {
             {"view_id", ""},
             {"label", !def.standard_view.empty() ? def.standard_view
                                                  : def.kind},
             {"scale", def.scale},
             {"origin", polysmith::protocol::json::array(
                            {def.sheet_position[0], def.sheet_position[1]})},
             {"min", preview->min.has_value()
                         ? polysmith::protocol::json::array(
                               {preview->min.value()[0],
                                preview->min.value()[1]})
                         : polysmith::protocol::json::array({0.0, 0.0})},
             {"max", preview->max.has_value()
                         ? polysmith::protocol::json::array(
                               {preview->max.value()[0],
                                preview->max.value()[1]})
                         : polysmith::protocol::json::array({0.0, 0.0})},
             {"stale", false},
             {"warning", preview->warning},
         }},
    };
    polysmith::protocol::write_message(
        polysmith::protocol::make_drawing_view_preview_event(
            command.id, reply));
    return;
  }

  if (command.type == "drawing_section_update") {
    const auto drawing_id = read_string(
        command.payload, "drawing_id");
    const auto view_id = read_string(
        command.payload, "view_id");
    const auto section =
        polysmith::protocol::section_definition_from_payload(
            command.payload.at("section"));
    const auto document = document_manager().drawing_section_update(
        drawing_id, view_id, section);
    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "drawing_title_block_update") {
    const auto drawing_id = read_string(command.payload, "drawing_id");
    const auto sheet_id = read_string(command.payload, "sheet_id");
    const auto title_block =
        polysmith::protocol::title_block_from_payload(
            command.payload.at("title_block"));
    const auto document = document_manager().drawing_title_block_update(
        drawing_id, sheet_id, title_block);
    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  // ── Export (P8) ─────────────────────────────────────────────────
  // Non-mutating: replies with the document_exported event, never a
  // document state (nothing changed).

  if (command.type == "drawing_export") {
    const auto drawing_id = read_string(command.payload, "drawing_id");
    const auto sheet_id = read_string(command.payload, "sheet_id");
    const auto format = read_string(command.payload, "format");
    const auto file_path = read_string(command.payload, "file_path");
    // P9: dxf_mode selects the DXF entity fidelity; the default keeps
    // the P8 behavior ("geometry").
    const auto dxf_mode = command.payload.contains("dxf_mode")
                              ? read_string(command.payload, "dxf_mode")
                              : std::string("geometry");
    const auto export_result = document_manager().export_drawing_sheet(
        drawing_id, sheet_id, format, file_path, dxf_mode);
    polysmith::protocol::write_message(
        polysmith::protocol::make_document_exported_event(
            command.id, polysmith::protocol::json{
                            {"file_path", export_result.file_path},
                            {"format", export_result.format},
                            {"exported_feature_count",
                             export_result.exported_feature_count},
                        }));
    return;
  }

  // ── Dimensions (P6) ─────────────────────────────────────────────
  // Picks are sheet-mm points resolved against the view's CURRENT
  // projection; the core mints the witness from the record's
  // provenance.  Validation errors bubble to the run() catch.

  if (command.type == "drawing_dimension_create") {
    const auto drawing_id = read_string(command.payload, "drawing_id");
    const auto view_id = read_string(command.payload, "view_id");
    const auto dim_type = read_string(command.payload, "dim_type");
    const auto pick_json = command.payload.at("pick");
    if (!pick_json.is_array() || pick_json.size() < 2 ||
        !pick_json.at(0).is_number() || !pick_json.at(1).is_number()) {
      polysmith::protocol::write_message(polysmith::protocol::make_error_event(
          command.id, "INVALID_PAYLOAD",
          "drawing_dimension_create needs pick [x, y] in sheet-mm"));
      return;
    }
    const std::array<double, 2> pick = {pick_json.at(0).get<double>(),
                                        pick_json.at(1).get<double>()};
    std::optional<std::array<double, 2>> pick_2;
    if (command.payload.contains("pick_2")) {
      const auto pick_2_json = command.payload.at("pick_2");
      if (pick_2_json.is_array() && pick_2_json.size() >= 2 &&
          pick_2_json.at(0).is_number() && pick_2_json.at(1).is_number()) {
        pick_2 = std::array<double, 2>{pick_2_json.at(0).get<double>(),
                                       pick_2_json.at(1).get<double>()};
      }
    }
    std::optional<std::string> annotation_id;
    if (command.payload.contains("annotation_id") &&
        command.payload.at("annotation_id").is_string() &&
        !command.payload.at("annotation_id").get<std::string>().empty()) {
      annotation_id = command.payload.at("annotation_id").get<std::string>();
    }
    const auto document = document_manager().drawing_dimension_create(
        drawing_id, view_id, dim_type, pick, pick_2, annotation_id);
    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "drawing_dimension_update") {
    const auto drawing_id = read_string(command.payload, "drawing_id");
    const auto annotation_id = read_string(command.payload, "annotation_id");
    std::optional<std::string> text_override;
    if (command.payload.contains("text_override") &&
        command.payload.at("text_override").is_string()) {
      text_override = command.payload.at("text_override").get<std::string>();
    }
    std::optional<std::string> prefix;
    if (command.payload.contains("prefix") &&
        command.payload.at("prefix").is_string()) {
      prefix = command.payload.at("prefix").get<std::string>();
    }
    std::optional<bool> arrow_flip;
    if (command.payload.contains("arrow_flip") &&
        command.payload.at("arrow_flip").is_boolean()) {
      arrow_flip = command.payload.at("arrow_flip").get<bool>();
    }
    std::optional<std::array<double, 2>> text_offset;
    if (command.payload.contains("text_offset")) {
      const auto offset = command.payload.at("text_offset");
      if (offset.is_array() && offset.size() >= 2 && offset.at(0).is_number() &&
          offset.at(1).is_number()) {
        text_offset = std::array<double, 2>{offset.at(0).get<double>(),
                                            offset.at(1).get<double>()};
      }
    }
    const auto document = document_manager().drawing_dimension_update(
        drawing_id, annotation_id, text_override, prefix, arrow_flip,
        text_offset);
    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "drawing_dimension_delete") {
    const auto drawing_id = read_string(command.payload, "drawing_id");
    const auto annotation_id = read_string(command.payload, "annotation_id");
    const auto document = document_manager().drawing_dimension_delete(
        drawing_id, annotation_id);
    polysmith::protocol::write_message(
        polysmith::protocol::make_document_state_event(
            command.id, polysmith::protocol::to_payload(document)));
    return;
  }

  if (command.type == "drawing_dimension_preview") {
    // Non-mutating: resolve the pick against the CURRENT projection
    // and reply with the computed value + graphics (the UI renders it
    // until Enter commits the create).
    const auto drawing_id = read_string(command.payload, "drawing_id");
    const auto view_id = read_string(command.payload, "view_id");
    const auto dim_type = read_string(command.payload, "dim_type");
    const auto pick_json = command.payload.at("pick");
    if (!pick_json.is_array() || pick_json.size() < 2 ||
        !pick_json.at(0).is_number() || !pick_json.at(1).is_number()) {
      polysmith::protocol::write_message(polysmith::protocol::make_error_event(
          command.id, "INVALID_PAYLOAD",
          "drawing_dimension_preview needs pick [x, y] in sheet-mm"));
      return;
    }
    const std::array<double, 2> pick = {pick_json.at(0).get<double>(),
                                        pick_json.at(1).get<double>()};
    std::optional<std::array<double, 2>> pick_2;
    if (command.payload.contains("pick_2")) {
      const auto pick_2_json = command.payload.at("pick_2");
      if (pick_2_json.is_array() && pick_2_json.size() >= 2 &&
          pick_2_json.at(0).is_number() && pick_2_json.at(1).is_number()) {
        pick_2 = std::array<double, 2>{pick_2_json.at(0).get<double>(),
                                       pick_2_json.at(1).get<double>()};
      }
    }

    const auto document = document_manager().get_document();
    if (!document.has_value()) {
      polysmith::protocol::write_message(polysmith::protocol::make_error_event(
          command.id, "NO_ACTIVE_DOCUMENT", "No active document"));
      return;
    }
    const polysmith::core::Drawing* drawing = nullptr;
    for (const auto& d : document->drawing.drawings) {
      if (d.drawing_id == drawing_id) {
        drawing = &d;
        break;
      }
    }
    const polysmith::core::DrawingView* view = nullptr;
    if (drawing != nullptr) {
      for (const auto& v : drawing->views) {
        if (v.view_id == view_id) {
          view = &v;
          break;
        }
      }
    }
    const polysmith::core::ProjectionResult* projection =
        view != nullptr ? polysmith::core::drawing_runtime::cached_projection(
                              document.value(), view_id)
                        : nullptr;
    if (drawing == nullptr || view == nullptr || projection == nullptr) {
      polysmith::protocol::write_message(polysmith::protocol::make_error_event(
          command.id, "UNKNOWN_VIEW",
          "The view or its projection was not found."));
      return;
    }

    // A synthetic annotation carries the cosmetics for the preview
    // text (prefix defaults per kind).
    polysmith::core::Annotation synthetic;
    synthetic.kind = dim_type;
    synthetic.view_id = view_id;
    if (dim_type == "diameter") {
      synthetic.prefix = "\xE2\x8C\x80";
    } else if (dim_type == "radius") {
      synthetic.prefix = "R";
    }
    std::string error;
    const auto measured = polysmith::core::measure_from_picks(
        *projection, *view, dim_type, pick, pick_2,
        /*pick_tolerance_sheetmm=*/5.0, document->drawing.decimal_separator,
        &synthetic, &error);
    polysmith::protocol::json reply = {
        {"drawing_id", drawing_id},
        {"view_id", view_id},
        {"dim_type", dim_type},
        {"pick", polysmith::protocol::json::array({pick[0], pick[1]})},
    };
    if (pick_2.has_value()) {
      reply["pick_2"] = polysmith::protocol::json::array(
          {pick_2.value()[0], pick_2.value()[1]});
    }
    if (!measured.has_value()) {
      reply["error"] = error;
      polysmith::protocol::write_message(
          polysmith::protocol::make_drawing_dimension_preview_event(
              command.id, reply));
      return;
    }
    // Graphics: same primitives the flatten emits, serialized in the
    // viewport-curve payload shape so the UI reuses its types.
    const auto graphics = polysmith::core::build_dimension_graphics(
        measured.value(), synthetic, *view, /*stale=*/false);
    polysmith::protocol::json curves = polysmith::protocol::json::array();
    for (const auto& p : graphics.primitives) {
      polysmith::protocol::json curve = {
          {"kind", p.kind},
          {"line_class", p.line_class},
          {"curve_class", p.purpose},
          {"purpose", p.purpose},
          {"width_mm", p.style.width_mm},
          {"p0", polysmith::protocol::json::array({p.p0[0], p.p0[1]})},
          {"p1", polysmith::protocol::json::array({p.p1[0], p.p1[1]})},
          {"start_angle", p.start_angle},
          {"end_angle", p.end_angle},
      };
      if (p.center.has_value()) {
        curve["center"] = polysmith::protocol::json::array(
            {p.center.value()[0], p.center.value()[1]});
      }
      if (p.radius.has_value()) {
        curve["radius"] = p.radius.value();
      }
      if (!p.points.empty()) {
        polysmith::protocol::json points = polysmith::protocol::json::array();
        for (const auto& point : p.points) {
          points.push_back(
              polysmith::protocol::json::array({point[0], point[1]}));
        }
        curve["points"] = points;
      }
      curves.push_back(std::move(curve));
    }
    reply["curves"] = std::move(curves);
    if (graphics.text.has_value()) {
      reply["text"] = {
          {"text", graphics.text.value().text},
          {"position", polysmith::protocol::json::array(
                           {graphics.text.value().position[0],
                            graphics.text.value().position[1]})},
          {"height_mm", graphics.text.value().height_mm},
          {"angle_deg", graphics.text.value().angle_deg},
          {"h_align", graphics.text.value().h_align},
          {"purpose", graphics.text.value().purpose},
          {"stale", graphics.text.value().stale},
      };
    }
    reply["value"] = measured.value().value;
    reply["text_value"] = measured.value().text;
    polysmith::protocol::write_message(
        polysmith::protocol::make_drawing_dimension_preview_event(
            command.id, reply));
    return;
  }
