  // ── Drawing templates (CREATE DRAWING dialog) ───────────────────
  //
  // Setup-only JSON files {name, sheets:[...]} — the core owns the file
  // I/O (the cam_tool_import_file/export_file precedent); the UI picks
  // the path and feeds the loaded template back through the normal
  // drawing_create path.

  if (command.type == "drawing_template_save") {
    const std::string path = read_string(command.payload, "file_path");
    if (!command.payload.contains("template") ||
        !command.payload.at("template").is_object()) {
      polysmith::protocol::write_message(
          polysmith::protocol::make_error_event(
              command.id, "DRAWING_TEMPLATE_SAVE_FAILED",
              "drawing_template_save needs a template object"));
      return;
    }
    // The template round-trips through the same lenient parse +
    // validation as a loaded file, so a bad save payload can never
    // write an invalid template.
    std::string error;
    const polysmith::core::DrawingTemplate template_ =
        polysmith::core::drawing_template_from_json(
            command.payload.at("template"), error);
    if (!error.empty()) {
      polysmith::protocol::write_message(
          polysmith::protocol::make_error_event(
              command.id, "DRAWING_TEMPLATE_SAVE_FAILED", error));
      return;
    }
    std::ofstream stream(path);
    if (!stream) {
      polysmith::protocol::write_message(
          polysmith::protocol::make_error_event(
              command.id, "DRAWING_TEMPLATE_SAVE_FAILED",
              "Could not write the template file: " + path));
      return;
    }
    stream << polysmith::core::drawing_template_to_json(template_).dump(2);
    stream << "\n";
    polysmith::core::log_info(
        "drawing", "template saved: " + path + " (" +
                       std::to_string(template_.sheets.size()) +
                       " sheet(s))");
    polysmith::protocol::write_message(
        polysmith::protocol::make_drawing_template_save_result_event(
            command.id, polysmith::protocol::json{{"file_path", path}}));
    return;
  }

  if (command.type == "drawing_template_load") {
    const std::string path = read_string(command.payload, "file_path");
    std::ifstream stream(path);
    if (!stream) {
      polysmith::protocol::write_message(
          polysmith::protocol::make_error_event(
              command.id, "DRAWING_TEMPLATE_LOAD_FAILED",
              "Could not read the template file: " + path));
      return;
    }
    polysmith::protocol::json payload;
    try {
      stream >> payload;
    } catch (const std::exception& e) {
      polysmith::protocol::write_message(
          polysmith::protocol::make_error_event(
              command.id, "DRAWING_TEMPLATE_LOAD_FAILED",
              "The template file is not valid JSON: " +
                  std::string(e.what())));
      return;
    }
    std::string error;
    const polysmith::core::DrawingTemplate template_ =
        polysmith::core::drawing_template_from_json(payload, error);
    if (!error.empty()) {
      polysmith::protocol::write_message(
          polysmith::protocol::make_error_event(
              command.id, "DRAWING_TEMPLATE_LOAD_FAILED", error));
      return;
    }
    // The full sheet payload rides along (ids empty — the core mints
    // them at drawing_create time; title blocks come with it).
    polysmith::protocol::json sheets = polysmith::protocol::json::array();
    for (const auto& sheet : template_.sheets) {
      sheets.push_back(polysmith::protocol::to_payload(sheet));
    }
    polysmith::protocol::write_message(
        polysmith::protocol::make_drawing_template_load_result_event(
            command.id,
            polysmith::protocol::json{
                {"template",
                 polysmith::protocol::json{
                     {"name", template_.name},
                     {"sheets", std::move(sheets)},
                 }},
            }));
    return;
  }
